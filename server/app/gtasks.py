"""Google Tasks, read-only.

Google has no "secret link" for tasks the way it does for a calendar, so this needs a sign-in.
The sign-in uses Google app credentials the owner creates once (a client ID and secret), kept
with the resulting token in one file in the data folder. Nothing here can change a task.
"""

from __future__ import annotations

import base64
import hashlib
import json
import logging
import os
import secrets
import threading
import time
from datetime import date
from pathlib import Path
from urllib.parse import parse_qs, urlencode, urlparse

import requests

log = logging.getLogger("deskpal.tasks")

SCOPE = "https://www.googleapis.com/auth/tasks.readonly"
AUTH_URL = "https://accounts.google.com/o/oauth2/v2/auth"
TOKEN_URL = "https://oauth2.googleapis.com/token"
REVOKE_URL = "https://oauth2.googleapis.com/revoke"
API = "https://tasks.googleapis.com/tasks/v1"
CALLBACK_PATH = "/google/callback"
PENDING_SECONDS = 15 * 60
MAX_PER_LIST = 300


class TasksError(Exception):
    """Something the person at the setup page can read and act on."""


def _problem(resp: requests.Response) -> str:
    """Google's own words for what went wrong, without echoing anything we sent."""
    try:
        body = resp.json()
    except ValueError:
        return f"Google answered with error {resp.status_code}"
    error = body.get("error")
    if isinstance(error, dict):
        return str(error.get("message") or error.get("status") or resp.status_code)[
            :200
        ]
    return str(body.get("error_description") or error or resp.status_code)[:200]


class Account:
    def __init__(self, path: Path):
        self.path = path
        self._lock = threading.Lock()
        self._data: dict = {}
        self._pending: dict | None = None  # a sign-in that has been started
        self._access: tuple[str, float] | None = None  # token, good until
        if path.exists():
            try:
                self._data = json.loads(path.read_text())
            except (OSError, ValueError):
                self._data = {}

    # ---- what is set up ----------------------------------------------------------

    def configured(self) -> bool:
        return bool(self._data.get("client_id") and self._data.get("client_secret"))

    def connected(self) -> bool:
        return self.configured() and bool(self._data.get("refresh_token"))

    def signing_in(self) -> bool:
        pending = self._pending
        return bool(pending and time.time() - pending["at"] < PENDING_SECONDS)

    def client_id(self) -> str:
        return self._data.get("client_id", "")

    def _save(self) -> None:
        tmp = self.path.with_suffix(".tmp")
        tmp.write_text(json.dumps(self._data, indent=2))
        os.chmod(tmp, 0o600)  # holds a secret and a token: owner only
        tmp.replace(self.path)

    def save_client(self, client_id: str, client_secret: str) -> None:
        client_id, client_secret = client_id.strip(), client_secret.strip()
        if not client_id.endswith(".apps.googleusercontent.com"):
            raise TasksError(
                "That does not look like a client ID. It ends with .apps.googleusercontent.com"
            )
        if not 10 <= len(client_secret) <= 200 or " " in client_secret:
            raise TasksError("That does not look like a client secret.")
        with self._lock:
            self._data = {"client_id": client_id, "client_secret": client_secret}
            self._access = self._pending = None
            self._save()

    def forget(self) -> None:
        """Signs out and removes the app credentials too."""
        self.disconnect()
        with self._lock:
            self._data = {}
            self.path.unlink(missing_ok=True)

    def disconnect(self) -> None:
        with self._lock:
            token = self._data.pop("refresh_token", None)
            self._access = self._pending = None
            if self._data:
                self._save()
        if token:
            try:  # tell Google too, so the access is gone at their end
                requests.post(REVOKE_URL, data={"token": token}, timeout=10)
            except requests.RequestException:
                pass

    # ---- sign-in -----------------------------------------------------------------

    def start(self, redirect_uri: str) -> str:
        """Returns the Google page to send the browser to."""
        if not self.configured():
            raise TasksError("Save the client ID and secret first.")
        verifier = secrets.token_urlsafe(48)
        challenge = (
            base64.urlsafe_b64encode(hashlib.sha256(verifier.encode()).digest())
            .rstrip(b"=")
            .decode()
        )
        state = secrets.token_urlsafe(24)
        self._pending = {
            "state": state,
            "verifier": verifier,
            "redirect": redirect_uri,
            "at": time.time(),
        }
        return (
            AUTH_URL
            + "?"
            + urlencode(
                {
                    "client_id": self._data["client_id"],
                    "redirect_uri": redirect_uri,
                    "response_type": "code",
                    "scope": SCOPE,
                    "access_type": "offline",
                    "prompt": "consent",  # so Google always hands back a long-lived token
                    "state": state,
                    "code_challenge": challenge,
                    "code_challenge_method": "S256",
                }
            )
        )

    def finish(self, state: str, code: str) -> None:
        pending = self._pending
        if not pending or time.time() - pending["at"] > PENDING_SECONDS:
            raise TasksError("That sign-in has expired. Start it again.")
        if not state or not secrets.compare_digest(state, pending["state"]):
            raise TasksError("That reply does not belong to the sign-in in progress.")
        if not code:
            raise TasksError("Google did not grant access.")
        try:
            resp = requests.post(
                TOKEN_URL,
                data={
                    "client_id": self._data["client_id"],
                    "client_secret": self._data["client_secret"],
                    "code": code,
                    "code_verifier": pending["verifier"],
                    "grant_type": "authorization_code",
                    "redirect_uri": pending["redirect"],
                },
                timeout=20,
            )
        except requests.RequestException as exc:
            raise TasksError("Could not reach Google to finish signing in.") from exc
        if resp.status_code != 200:
            raise TasksError(f"Google refused the sign-in: {_problem(resp)}")
        body = resp.json()
        if not body.get("refresh_token"):
            raise TasksError("Google did not return a long-lived token. Try again.")
        with self._lock:
            self._data["refresh_token"] = body["refresh_token"]
            self._pending = None
            if body.get("access_token"):
                self._access = (
                    body["access_token"],
                    time.time() + int(body.get("expires_in", 3600)) - 60,
                )
            self._save()

    def finish_from_pasted(self, url: str) -> None:
        """For when the browser could not reach this server at 127.0.0.1: the address it
        ended on still carries what is needed."""
        query = parse_qs(urlparse(url.strip()).query)
        if query.get("error"):
            raise TasksError("Google did not grant access.")
        if not query.get("state") or not query.get("code"):
            raise TasksError(
                "That address has no sign-in reply in it. Copy the whole address."
            )
        self.finish(query["state"][0], query["code"][0])

    # ---- reading tasks -----------------------------------------------------------

    def _token(self) -> str:
        with self._lock:
            if self._access and self._access[1] > time.time():
                return self._access[0]
            data = dict(self._data)
        if not data.get("refresh_token"):
            raise TasksError("Google Tasks is not connected")
        resp = requests.post(
            TOKEN_URL,
            data={
                "client_id": data["client_id"],
                "client_secret": data["client_secret"],
                "refresh_token": data["refresh_token"],
                "grant_type": "refresh_token",
            },
            timeout=20,
        )
        if resp.status_code in (400, 401):
            # The access was revoked or has lapsed: only a new sign-in fixes it.
            with self._lock:
                self._data.pop("refresh_token", None)
                self._access = None
                self._save()
            raise TasksError(
                f"Google ended the sign-in ({_problem(resp)}). Sign in again on the setup page."
            )
        resp.raise_for_status()
        body = resp.json()
        with self._lock:
            self._access = (
                body["access_token"],
                time.time() + int(body.get("expires_in", 3600)) - 60,
            )
            return self._access[0]

    def _pages(self, url: str, params: dict, cap: int) -> list[dict]:
        headers = {"Authorization": f"Bearer {self._token()}"}
        items: list[dict] = []
        params = dict(params)
        while len(items) < cap:
            resp = requests.get(url, params=params, headers=headers, timeout=20)
            if resp.status_code != 200:
                raise TasksError(f"Google Tasks: {_problem(resp)}")
            body = resp.json()
            items.extend(body.get("items") or [])
            if not body.get("nextPageToken"):
                break
            params["pageToken"] = body["nextPageToken"]
        return items[:cap]

    def fetch(self) -> list[dict]:
        """Every list with its open tasks, as Google returns them (trimmed to what is used)."""
        out = []
        for entry in self._pages(f"{API}/users/@me/lists", {"maxResults": 100}, 100):
            raw = self._pages(
                f"{API}/lists/{entry['id']}/tasks",
                {"maxResults": 100, "showCompleted": "false", "showHidden": "false"},
                MAX_PER_LIST,
            )
            tasks = [
                {
                    "id": t.get("id", ""),
                    "title": (t.get("title") or "").strip(),
                    "notes": (t.get("notes") or "").strip()[:300],
                    "due": (t.get("due") or "")[:10],  # Google keeps the date only
                    "parent": t.get("parent", ""),
                    "position": t.get("position", ""),
                    "link": t.get("webViewLink", ""),
                }
                for t in raw
                if (t.get("title") or "").strip() and t.get("status") != "completed"
            ]
            out.append(
                {"title": (entry.get("title") or "Tasks").strip(), "tasks": tasks}
            )
        return out


def due_label(due: str, today: date) -> tuple[str, str]:
    """("Tomorrow", "soon") and the like. Tone is late, today, soon or empty."""
    try:
        day = date.fromisoformat(due)
    except ValueError:
        return "", ""
    gap = (day - today).days
    if gap == 0:
        return "Today", "today"
    if gap == 1:
        return "Tomorrow", "soon"
    if gap == -1:
        return "Yesterday", "late"
    if 1 < gap < 7:
        return day.strftime("%A"), ""
    label = f"{day.strftime('%b')} {day.day}"
    if day.year != today.year:
        label += f", {day.year}"
    return label, "late" if gap < 0 else ""


def _ordered(tasks: list[dict]) -> list[tuple[dict, int]]:
    """Google's own order, each task followed by its subtasks."""
    ids = {t["id"] for t in tasks}
    children: dict[str, list[dict]] = {}
    for task in tasks:
        parent = task["parent"] if task["parent"] in ids else ""
        children.setdefault(parent, []).append(task)
    rows: list[tuple[dict, int]] = []
    for top in sorted(children.get("", []), key=lambda t: t["position"]):
        rows.append((top, 0))
        for sub in sorted(children.get(top["id"], []), key=lambda t: t["position"]):
            rows.append((sub, 1))
    return rows


def view(lists: list[dict] | None, today: date, limit: int, full: bool) -> dict | None:
    """What a screen shows. `full` adds each task's notes and link, for the browser page."""
    if lists is None:
        return None
    out, shown, total = [], 0, 0
    for entry in lists:
        rows = _ordered(entry["tasks"])
        total += len(rows)
        items = []
        for task, indent in rows[: max(0, limit - shown)]:
            label, tone = due_label(task["due"], today)
            item = {
                "title": task["title"][:160],
                "due": label,
                "tone": tone,
                "indent": indent,
            }
            if full:
                item["notes"] = task["notes"]
                link = task["link"]
                item["link"] = link if link.startswith("https://") else ""
            items.append(item)
        shown += len(items)
        if items:
            out.append({"title": entry["title"][:60], "items": items})
    return {"lists": out, "total": total, "more": total - shown}
