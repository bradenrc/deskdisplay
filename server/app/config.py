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
    "agenda": {
        # Private iCal links; normally added on the setup page instead.
        "ical_urls": [],
        "soon_minutes": 10,
        "refresh_minutes": 5,
    },
    "weather": {"refresh_minutes": 15, "hourly_step": 2, "hourly_count": 5},
    # clock_color: "sun" shifts the clock's colour through the day, "white" keeps it plain,
    # or give any colour as "#rrggbb".
    "display": {"clock_color": "sun"},
    # Markets page: [{symbol, name}] using Yahoo Finance symbols (^GSPC is the S&P 500).
    "stocks": {"symbols": [], "refresh_minutes": 5},
    # A dim, plain clock overnight. Touching the screen brings the dashboard back for a while.
    "night": {"enabled": True, "start": "22:30", "end": "06:30"},
    # "Leave by" hints for meetings with a street address. Off by default because it sends
    # those addresses to OpenStreetMap's public lookup services.
    "travel": {
        "enabled": False,
        "buffer_minutes": 5,
        "padding_percent": 15,
        "max_miles": 150,
    },
    # Ski areas: [{name, latitude, longitude, elevation_m}]. Shown only after fresh snow.
    "ski": {"resorts": [], "refresh_minutes": 60, "min_new": 0.5},
    # Teams to follow: [{league: nfl | college-football, team: ESPN id or abbreviation, name}].
    "sports": {
        "teams": [],
        "schedule_minutes": 60,
        "preview_days": 6,
        "final_hours": 36,
    },
    "news": {
        "refresh_minutes": 15,
        "max_items": 150,
    },
    # Google Tasks, connected on the setup page. display_max is how many the desk display is sent.
    "tasks": {"refresh_minutes": 5, "display_max": 40},
    "photos": {
        "drive_folder_id": "",
        "local_dir": "",
        "refresh_minutes": 15,
        "fit": "cover",
        "master_max_px": 1920,
        "pool_size": 300,
        "daily_swap": 30,
        "index_hours": 12,
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
