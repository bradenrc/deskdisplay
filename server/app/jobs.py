"""Background refresh: each source runs in its own thread and keeps the last good result."""

from __future__ import annotations

import logging
import threading
import time
from collections.abc import Callable
from typing import Any

log = logging.getLogger("deskpal.jobs")


class Skip(Exception):
    """Raised by a job that has nothing to do yet (for example, not signed in)."""


class Store:
    def __init__(self) -> None:
        self._lock = threading.Lock()
        self._data: dict[str, Any] = {}
        self._meta: dict[str, dict[str, Any]] = {}

    def set(self, name: str, value: Any) -> None:
        with self._lock:
            self._data[name] = value
            self._meta[name] = {"state": "ok", "updated": time.time(), "detail": ""}

    def note(self, name: str, state: str, detail: str) -> None:
        with self._lock:
            previous = self._meta.get(name, {})
            self._meta[name] = {
                "state": state,
                "updated": previous.get("updated"),
                "detail": detail,
            }

    def get(self, name: str, default: Any = None) -> Any:
        with self._lock:
            return self._data.get(name, default)

    def drop(self, name: str) -> None:
        with self._lock:
            self._data.pop(name, None)

    def meta(self) -> dict[str, dict[str, Any]]:
        with self._lock:
            return {k: dict(v) for k, v in self._meta.items()}


class Runner:
    def __init__(self, store: Store, jobs: list[tuple[str, Callable[[], None], float]]):
        self.store = store
        self.jobs = jobs
        self._wake = {name: threading.Event() for name, _, _ in jobs}
        self._stop = threading.Event()

    def start(self) -> None:
        for name, fn, minutes in self.jobs:
            threading.Thread(
                target=self._loop,
                args=(name, fn, minutes),
                name=f"job-{name}",
                daemon=True,
            ).start()

    def stop(self) -> None:
        self._stop.set()
        for event in self._wake.values():
            event.set()

    def trigger(self, *names: str) -> None:
        for name in names:
            if name in self._wake:
                self._wake[name].set()

    def run_once(self, name: str, fn: Callable[[], None]) -> None:
        try:
            fn()
        except Skip as exc:
            self.store.note(name, "idle", str(exc))
        except Exception as exc:  # keep the last good data, record why
            log.warning("%s refresh failed: %s", name, exc)
            self.store.note(name, "error", str(exc)[:300])

    def _loop(self, name: str, fn: Callable[[], None], minutes: float) -> None:
        while not self._stop.is_set():
            self.run_once(name, fn)
            self._wake[name].wait(timeout=max(60.0, minutes * 60))
            self._wake[name].clear()
