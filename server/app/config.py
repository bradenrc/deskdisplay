"""Static configuration: defaults deep-merged with an optional YAML file."""

from __future__ import annotations

import copy
import json
import threading
from pathlib import Path
from typing import Any

import yaml

DEFAULTS: dict[str, Any] = {
    "timezone": "America/Denver",
    "location": {"latitude": 39.74, "longitude": -104.99},
    "units": "fahrenheit",
    # The address Google sends the browser back to after sign-in. Must be
    # http://localhost:<port> or a real https:// address (Google's rule).
    "public_url": "http://localhost:8080",
    "agenda": {
        "calendar_ids": ["primary"],
        "soon_minutes": 10,
        "refresh_minutes": 5,
        "hide_declined": True,
    },
    "weather": {"refresh_minutes": 15, "hourly_step": 2, "hourly_count": 5},
    "news": {
        "refresh_minutes": 15,
        "max_items": 12,
        "feeds": [
            {"name": "NPR", "url": "https://feeds.npr.org/1001/rss.xml"},
            {"name": "BBC", "url": "https://feeds.bbci.co.uk/news/rss.xml"},
        ],
    },
    "photos": {
        "drive_folder_id": "",
        "local_dir": "",
        "refresh_minutes": 15,
        "fit": "cover",
        "master_max_px": 1920,
        "max_photos": 2000,
    },
}


def _merge(base: dict, extra: dict) -> dict:
    for key, value in extra.items():
        if isinstance(value, dict) and isinstance(base.get(key), dict):
            _merge(base[key], value)
        else:
            base[key] = value
    return base


def load(path: Path | None) -> dict[str, Any]:
    cfg = copy.deepcopy(DEFAULTS)
    if path and path.exists():
        loaded = yaml.safe_load(path.read_text()) or {}
        if not isinstance(loaded, dict):
            raise ValueError(f"{path} must contain a YAML mapping")
        _merge(cfg, loaded)
    return cfg


class Settings:
    """Small JSON file for choices made on the setup page (overrides config)."""

    def __init__(self, path: Path):
        self.path = path
        self._lock = threading.Lock()
        self._data: dict[str, Any] = {}
        if path.exists():
            try:
                self._data = json.loads(path.read_text())
            except (OSError, ValueError):
                self._data = {}

    def get(self, key: str, default: Any = None) -> Any:
        with self._lock:
            return self._data.get(key, default)

    def set(self, key: str, value: Any) -> None:
        with self._lock:
            if value in (None, "", []):
                self._data.pop(key, None)
            else:
                self._data[key] = value
            tmp = self.path.with_suffix(".tmp")
            tmp.write_text(json.dumps(self._data, indent=2))
            tmp.replace(self.path)
