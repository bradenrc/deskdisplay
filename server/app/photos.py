"""Photo library: mirror a Google Drive folder (or a local folder) and serve screen-sized JPEGs.

Drive photos are downloaded once, shrunk to a "master" JPEG and the original is discarded,
so a large library stays small on disk. Renders are baseline JPEGs because the small JPEG
decoders used on microcontrollers cannot read progressive files.
"""

from __future__ import annotations

import hashlib
import json
import logging
import random
import re
import threading
from datetime import datetime
from pathlib import Path

from PIL import Image, ImageOps

try:  # HEIC/HEIF (iPhone photos)
    from pillow_heif import register_heif_opener

    register_heif_opener()
except Exception:  # pragma: no cover - optional
    pass

log = logging.getLogger("deskpal.photos")

DRIVE_API = "https://www.googleapis.com/drive/v3"
FOLDER_MIME = "application/vnd.google-apps.folder"
IMAGE_EXT = {".jpg", ".jpeg", ".png", ".webp", ".heic", ".heif", ".bmp", ".gif"}
ID_RE = re.compile(r"^[A-Za-z0-9_-]{10,}$")
MONTHS = ["January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"]


def parse_folder_id(text: str) -> str:
    """Accept a Drive folder link or a bare folder id; return the id or ''."""
    text = (text or "").strip()
    match = re.search(r"/folders/([A-Za-z0-9_-]{10,})", text) or re.search(
        r"[?&]id=([A-Za-z0-9_-]{10,})", text
    )
    if match:
        return match.group(1)
    return text if ID_RE.match(text) else ""


def _caption(folder: str, taken: str | None) -> str:
    when = ""
    if taken:
        match = re.match(r"(\d{4})[:-](\d{2})", taken)
        if match and 1 <= int(match.group(2)) <= 12:
            when = f"{MONTHS[int(match.group(2)) - 1]} {match.group(1)}"
    return " · ".join(part for part in (folder, when) if part)


class Library:
    def __init__(self, root: Path):
        self.root = root
        self.masters = root / "masters"
        self.cache = root / "cache"
        self.index_path = root / "index.json"
        self._lock = threading.Lock()
        self._index: dict[str, dict] = {}
        self._bag: list[str] = []
        for folder in (self.masters, self.cache):
            folder.mkdir(parents=True, exist_ok=True)
        if self.index_path.exists():
            try:
                self._index = json.loads(self.index_path.read_text())
            except (OSError, ValueError):
                self._index = {}

    # ---- reading -----------------------------------------------------------------

    def count(self) -> int:
        with self._lock:
            return len(self._index)

    def entries(self) -> list[dict]:
        with self._lock:
            return [
                {"id": pid, "caption": e.get("caption", "")}
                for pid, e in self._index.items()
            ]

    def get(self, pid: str) -> dict | None:
        with self._lock:
            entry = self._index.get(pid)
            return dict(entry) if entry else None

    def next_id(self) -> str | None:
        """Shuffle through every photo once before repeating any."""
        with self._lock:
            self._bag = [pid for pid in self._bag if pid in self._index]
            if not self._bag:
                self._bag = list(self._index)
                random.shuffle(self._bag)
            return self._bag.pop() if self._bag else None

    def render(
        self, pid: str, width: int, height: int, fit: str = "cover"
    ) -> Path | None:
        entry = self.get(pid)
        if not entry:
            return None
        source = Path(entry["path"])
        if not source.exists():
            return None
        fit = "contain" if fit == "contain" else "cover"
        out = self.cache / f"{pid}_{width}x{height}_{fit}.jpg"
        if out.exists() and out.stat().st_mtime >= source.stat().st_mtime:
            return out
        with Image.open(source) as image:
            image = ImageOps.exif_transpose(image).convert("RGB")
            if fit == "cover":
                image = ImageOps.fit(
                    image, (width, height), Image.LANCZOS, centering=(0.5, 0.4)
                )
            else:
                image = ImageOps.pad(
                    image, (width, height), Image.LANCZOS, color=(0, 0, 0)
                )
            tmp = out.with_suffix(".tmp")
            image.save(tmp, "JPEG", quality=85, progressive=False, subsampling="4:2:0")
            tmp.replace(out)
        return out

    # ---- writing -----------------------------------------------------------------

    def _save(self) -> None:
        tmp = self.index_path.with_suffix(".tmp")
        tmp.write_text(json.dumps(self._index))
        tmp.replace(self.index_path)

    def _replace_source(self, prefix: str, fresh: dict[str, dict]) -> None:
        """Swap in the entries for one source ('d_' Drive, 'l_' local) and drop stale files."""
        with self._lock:
            stale = [
                pid
                for pid in self._index
                if pid.startswith(prefix) and pid not in fresh
            ]
            for pid in stale:
                del self._index[pid]
            self._index.update(fresh)
            self._save()
        for pid in stale:
            for path in list(self.cache.glob(f"{pid}_*")) + [
                self.masters / f"{pid}.jpg"
            ]:
                path.unlink(missing_ok=True)

    def sync_local(self, folder: str | Path | None) -> int:
        fresh: dict[str, dict] = {}
        base = Path(folder) if folder else None
        if base and base.is_dir():
            for path in sorted(base.rglob("*")):
                if path.is_file() and path.suffix.lower() in IMAGE_EXT:
                    rel = path.relative_to(base)
                    pid = "l_" + hashlib.sha1(str(rel).encode()).hexdigest()[:16]
                    parent = rel.parent.name
                    fresh[pid] = {
                        "path": str(path),
                        "caption": parent,
                        "name": path.name,
                    }
        self._replace_source("l_", fresh)
        return len(fresh)

    def clear_drive(self) -> None:
        self._replace_source("d_", {})

    def sync_drive(
        self, sess, folder_id: str, master_max: int = 1920, max_photos: int = 2000
    ) -> int:
        if not ID_RE.match(folder_id or ""):
            raise ValueError(
                "Drive folder id looks wrong; paste the folder link on the setup page"
            )
        listing = self._drive_list(
            sess, folder_id, max_photos
        )  # raises before anything is removed
        fresh: dict[str, dict] = {}
        failed = 0
        for number, item in enumerate(listing, 1):
            pid = "d_" + item["id"]
            existing = self.get(pid)
            master = self.masters / f"{pid}.jpg"
            if (
                existing
                and existing.get("modified") == item["modified"]
                and master.exists()
            ):
                fresh[pid] = existing
                continue
            try:
                self._drive_download(sess, item["id"], master, master_max)
            except Exception as exc:
                failed += 1
                log.warning("could not fetch %s: %s", item["name"], exc)
                continue
            entry = {
                "path": str(master),
                "caption": _caption(item["folder"], item["taken"]),
                "name": item["name"],
                "modified": item["modified"],
            }
            fresh[pid] = entry
            with self._lock:  # make new photos available as they arrive
                self._index[pid] = entry
                if number % 20 == 0:
                    self._save()
        self._replace_source("d_", fresh)
        if failed:
            log.warning("%d photo(s) could not be downloaded this pass", failed)
        return len(fresh)

    def _drive_list(self, sess, folder_id: str, max_photos: int) -> list[dict]:
        resp = sess.get(
            f"{DRIVE_API}/files/{folder_id}",
            params={"fields": "name,mimeType", "supportsAllDrives": "true"},
            timeout=30,
        )
        if resp.status_code == 404:
            raise ValueError(
                "Drive folder not found, or this Google account cannot see it"
            )
        resp.raise_for_status()
        out: list[dict] = []
        stack = [(folder_id, resp.json().get("name", ""))]
        while stack and len(out) < max_photos:
            current, folder_name = stack.pop()
            token = None
            while len(out) < max_photos:
                params = {
                    "q": f"'{current}' in parents and trashed = false",
                    "fields": "nextPageToken,files(id,name,mimeType,modifiedTime,"
                    "imageMediaMetadata(time))",
                    "pageSize": 1000,
                    "supportsAllDrives": "true",
                    "includeItemsFromAllDrives": "true",
                }
                if token:
                    params["pageToken"] = token
                resp = sess.get(f"{DRIVE_API}/files", params=params, timeout=30)
                resp.raise_for_status()
                body = resp.json()
                for item in body.get("files", []):
                    mime = item.get("mimeType", "")
                    if mime == FOLDER_MIME:
                        stack.append((item["id"], item.get("name", "")))
                    elif mime.startswith("image/") and len(out) < max_photos:
                        out.append(
                            {
                                "id": item["id"],
                                "name": item.get("name", ""),
                                "modified": item.get("modifiedTime", ""),
                                "taken": (item.get("imageMediaMetadata") or {}).get(
                                    "time"
                                ),
                                "folder": folder_name,
                            }
                        )
                token = body.get("nextPageToken")
                if not token:
                    break
        return out

    def _drive_download(
        self, sess, file_id: str, master: Path, master_max: int
    ) -> None:
        tmp = master.with_suffix(".download")
        try:
            with sess.get(
                f"{DRIVE_API}/files/{file_id}",
                params={"alt": "media", "supportsAllDrives": "true"},
                stream=True,
                timeout=120,
            ) as resp:
                resp.raise_for_status()
                with open(tmp, "wb") as handle:
                    handle.writelines(resp.iter_content(1 << 16))
            with Image.open(tmp) as image:
                image = ImageOps.exif_transpose(image).convert("RGB")
                image.thumbnail((master_max, master_max), Image.LANCZOS)
                ready = master.with_suffix(".tmp")
                image.save(ready, "JPEG", quality=90)
                ready.replace(master)
        finally:
            tmp.unlink(missing_ok=True)


def taken_label(now: datetime) -> str:  # pragma: no cover - convenience for future use
    return f"{MONTHS[now.month - 1]} {now.year}"
