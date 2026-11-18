"""Photo library.

Two sources, usable together:
  * a local folder, shown in full;
  * a Google Drive folder of any size, shown through a small rotating pool. The whole
    folder is indexed by name only; a random sample is downloaded, shrunk to screen size
    and the originals are discarded. Each day part of the pool is swapped for new picks,
    so disk use stays fixed however large the Drive is.

Renders are baseline JPEGs because the small JPEG decoders used on microcontrollers
cannot read progressive files.
"""

from __future__ import annotations

import hashlib
import json
import logging
import random
import re
import shutil
import threading
import time
from pathlib import Path

from PIL import Image, ImageOps

try:  # HEIC/HEIF (iPhone photos)
    from pillow_heif import register_heif_opener

    register_heif_opener()
except Exception:  # pragma: no cover - optional
    pass

log = logging.getLogger("deskpal.photos")

IMAGE_EXT = {".jpg", ".jpeg", ".png", ".webp", ".heic", ".heif", ".bmp", ".gif"}
ID_RE = re.compile(r"^[A-Za-z0-9_-]{10,}$")
MONTHS = [
    "January",
    "February",
    "March",
    "April",
    "May",
    "June",
    "July",
    "August",
    "September",
    "October",
    "November",
    "December",
]
BATCH = 20
EXIF_IFD, DATE_ORIGINAL, DATE_TIME = 0x8769, 36867, 306


def parse_folder_id(text: str) -> str:
    """Accept a Drive folder link, a bare folder id, or My Drive; return the id or ''."""
    text = (text or "").strip()
    match = re.search(r"/folders/([A-Za-z0-9_-]{10,})", text) or re.search(
        r"[?&]id=([A-Za-z0-9_-]{10,})", text
    )
    if match:
        return match.group(1)
    if text.lower() == "root" or text.rstrip("/").lower().endswith("/my-drive"):
        return "root"
    return text if ID_RE.match(text) else ""


def _month_year(stamp: str | None) -> str:
    match = re.match(r"(\d{4})[:-](\d{2})", stamp or "")
    if match and 1 <= int(match.group(2)) <= 12 and int(match.group(1)) > 1970:
        return f"{MONTHS[int(match.group(2)) - 1]} {match.group(1)}"
    return ""


def _taken(image: Image.Image) -> str:
    try:
        exif = image.getexif()
        return _month_year(
            str(exif.get_ifd(EXIF_IFD).get(DATE_ORIGINAL) or exif.get(DATE_TIME) or "")
        )
    except Exception:
        return ""


def _caption(folder: str, when: str) -> str:
    return " · ".join(part for part in (folder, when) if part)


def _pool_id(item_id: str) -> str:
    return "r_" + hashlib.sha1(item_id.encode()).hexdigest()[:16]


class Library:
    def __init__(self, root: Path):
        self.root = root
        self.masters = root / "masters"
        self.cache = root / "cache"
        self.incoming = root / "incoming"
        self.index_path = root / "index.json"
        self.catalog_path = root / "catalog.json"
        self.pool_path = root / "pool.json"
        self._lock = threading.Lock()
        self._index: dict[str, dict] = {}
        self._bag: list[str] = []
        self.catalog_size = 0
        for folder in (self.masters, self.cache):
            folder.mkdir(parents=True, exist_ok=True)
        self._index = self._read(self.index_path, {})
        self.catalog_size = len(self._read(self.catalog_path, {}).get("items", []))

    @staticmethod
    def _read(path: Path, default):
        try:
            return json.loads(path.read_text())
        except (OSError, ValueError):
            return default

    @staticmethod
    def _write(path: Path, data) -> None:
        tmp = path.with_suffix(".tmp")
        tmp.write_text(json.dumps(data))
        tmp.replace(path)

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

    def _replace_source(self, prefix: str, fresh: dict[str, dict]) -> None:
        """Swap in the entries for one source ('r_' Drive pool, 'l_' local); drop stale files."""
        with self._lock:
            stale = [
                pid
                for pid in self._index
                if pid.startswith(prefix) and pid not in fresh
            ]
            for pid in stale:
                del self._index[pid]
            self._index.update(fresh)
            self._write(self.index_path, self._index)
        for pid in stale:
            for path in [*self.cache.glob(f"{pid}_*"), self.masters / f"{pid}.jpg"]:
                path.unlink(missing_ok=True)

    def sync_local(self, folder: str | Path | None) -> int:
        fresh: dict[str, dict] = {}
        base = Path(folder) if folder else None
        if base and base.is_dir():
            for path in sorted(base.rglob("*")):
                if path.is_file() and path.suffix.lower() in IMAGE_EXT:
                    rel = path.relative_to(base)
                    pid = "l_" + hashlib.sha1(str(rel).encode()).hexdigest()[:16]
                    fresh[pid] = {
                        "path": str(path),
                        "caption": rel.parent.name,
                        "name": path.name,
                    }
        self._replace_source("l_", fresh)
        return len(fresh)

    def clear_pool(self) -> None:
        self._replace_source("r_", {})
        for path in (self.catalog_path, self.pool_path):
            path.unlink(missing_ok=True)
        self.catalog_size = 0

    def _catalog(
        self, source, folder_id: str, index_hours: float, now: float
    ) -> list[dict]:
        cached = self._read(self.catalog_path, {})
        fresh_enough = now - cached.get("listed", 0) < index_hours * 3600
        if cached.get("folder") == folder_id and fresh_enough and cached.get("items"):
            return cached["items"]
        try:
            items = source.list_images(folder_id)
        except Exception:
            if cached.get("folder") == folder_id and cached.get("items"):
                log.warning("could not re-list the Drive folder; using the saved list")
                return cached["items"]
            raise
        self._write(
            self.catalog_path, {"folder": folder_id, "listed": now, "items": items}
        )
        log.info("indexed %d photos in the Drive folder", len(items))
        return items

    def sync_pool(
        self,
        source,
        folder_id: str,
        pool_size: int = 300,
        daily_swap: int = 30,
        index_hours: float = 12,
        master_max: int = 1920,
        today: str | None = None,
        now: float | None = None,
        progress=None,
    ) -> int:
        """Keep `pool_size` photos from the Drive folder on disk, rotating some each day.

        `source` provides list_images(folder_id) and copy(folder_id, paths, dest).
        `progress(text)` is called with a short human-readable note as work proceeds.
        """
        progress = progress or (lambda text: None)
        progress("Reading the list of photos in the Drive folder")
        now = time.time() if now is None else now
        today = today or time.strftime("%Y-%m-%d", time.localtime(now))
        catalog = {
            _pool_id(item["id"]): item
            for item in self._catalog(source, folder_id, index_hours, now)
            # macOS leaves "._name.jpg" companion files beside photos; they are not images
            if not Path(item["path"]).name.startswith(".")
        }
        self.catalog_size = len(catalog)
        state = self._read(self.pool_path, {})
        if state.get("folder") != folder_id:
            state = {"folder": folder_id, "last_swap": today, "bad": []}
        bad = set(state.get("bad", []))

        with self._lock:
            pool = {pid: e for pid, e in self._index.items() if pid.startswith("r_")}
        keep = {
            pid: e
            for pid, e in pool.items()
            if pid in catalog and Path(e["path"]).exists()
        }
        evicted: set[str] = set()
        if state.get("last_swap") != today:
            state["last_swap"] = today
            if len(keep) >= pool_size and len(catalog) > pool_size:
                oldest = sorted(keep, key=lambda pid: keep[pid].get("added", 0))
                evicted.update(oldest[: max(0, daily_swap)])
        by_age = sorted(keep, key=lambda pid: keep[pid].get("added", 0))
        evicted.update(by_age[: max(0, len(keep) - pool_size)])  # pool size was lowered
        for pid in evicted:
            del keep[pid]
        self._replace_source("r_", keep)

        skip = bad | evicted  # do not hand back a photo that was just rotated out
        candidates = [pid for pid in catalog if pid not in keep and pid not in skip]
        random.shuffle(candidates)
        ready = len(keep)
        try:
            # Take picks in small batches until the pool is full; a file that turns out
            # to be unreadable is skipped and another pick takes its place.
            while ready < pool_size and candidates:
                count = min(BATCH, pool_size - ready)
                batch, candidates = candidates[:count], candidates[count:]
                shutil.rmtree(self.incoming, ignore_errors=True)
                self.incoming.mkdir(parents=True)
                source.copy(
                    folder_id, [catalog[pid]["path"] for pid in batch], self.incoming
                )
                added: dict[str, dict] = {}
                for pid in batch:
                    rel = Path(catalog[pid]["path"])
                    try:
                        added[pid] = self._make_master(
                            pid, self.incoming / rel, rel, master_max, now
                        )
                    except Exception as exc:
                        bad.add(pid)
                        log.warning("skipping %s: %s", rel.name, exc)
                with self._lock:
                    self._index.update(added)
                    self._write(self.index_path, self._index)
                    ready = sum(1 for pid in self._index if pid.startswith("r_"))
                target = min(pool_size, len(catalog) - len(bad))
                if ready < target:
                    progress(f"Downloading photos: {ready} of {target} ready")
        finally:
            shutil.rmtree(self.incoming, ignore_errors=True)
            state["bad"] = sorted(bad)[-5000:]
            self._write(self.pool_path, state)
        with self._lock:
            return sum(1 for pid in self._index if pid.startswith("r_"))

    def _make_master(
        self, pid: str, original: Path, rel: Path, master_max: int, now: float
    ) -> dict:
        master = self.masters / f"{pid}.jpg"
        with Image.open(original) as image:
            when = _taken(image)
            image = ImageOps.exif_transpose(image).convert("RGB")
            image.thumbnail((master_max, master_max), Image.LANCZOS)
            tmp = master.with_suffix(".tmp")
            image.save(tmp, "JPEG", quality=90)
            tmp.replace(master)
        return {
            "path": str(master),
            "caption": _caption(rel.parent.name, when),
            "name": rel.name,
            "added": now,
        }
