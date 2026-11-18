"""Driving time to meetings that have a street address, for a "leave by" hint.

Uses OpenStreetMap services, which are free and need no account: Nominatim to turn an
address into coordinates and the public OSRM server for the route. Neither knows about
live traffic, so the caller pads the result.

Privacy: the location text of an event is sent to those services. Only locations that look
like street addresses are sent, and each one is looked up once and then cached on disk.
"""

from __future__ import annotations

import json
import logging
import math
import re
import threading
import time
from pathlib import Path

import requests

log = logging.getLogger("deskpal.travel")

HEADERS = {"User-Agent": "desk-companion/1.0 (personal dashboard)"}
GEOCODE = "https://nominatim.openstreetmap.org/search"
ROUTE = "https://router.project-osrm.org/route/v1/driving"
ONLINE = re.compile(
    r"https?:|zoom|teams|meet\.google|webex|skype|slack|discord|online|virtual|remote|phone|dial",
    re.IGNORECASE,
)
STREET = re.compile(
    r"\b(st|street|ave|avenue|rd|road|blvd|boulevard|dr|drive|way|ln|lane|hwy|highway|pkwy|"
    r"parkway|ct|court|pl|place|cir|circle|sq|square)\b\.?",
    re.IGNORECASE,
)
FOUND_DAYS, MISSING_DAYS = 30, 7
MAX_LOOKUPS_PER_RUN = 5


def looks_like_address(text: str) -> bool:
    """True for "800 W Main St, Boise"; false for "Zoom", "Room 4" or a meeting link."""
    text = (text or "").strip()
    if len(text) < 8 or ONLINE.search(text):
        return False
    return bool(re.search(r"\d", text)) and bool("," in text or STREET.search(text))


def miles_between(a: tuple[float, float], b: tuple[float, float]) -> float:
    lat1, lon1, lat2, lon2 = map(math.radians, (*a, *b))
    h = (
        math.sin((lat2 - lat1) / 2) ** 2
        + math.cos(lat1) * math.cos(lat2) * math.sin((lon2 - lon1) / 2) ** 2
    )
    return 3958.8 * 2 * math.asin(math.sqrt(h))


class Travel:
    def __init__(
        self, cache_path: Path, origin: tuple[float, float], max_miles: float = 150
    ):
        self.cache_path = cache_path
        self.origin = origin
        self.max_miles = max_miles
        self._lock = threading.Lock()
        try:
            self._cache: dict[str, dict] = json.loads(cache_path.read_text())
        except (OSError, ValueError):
            self._cache = {}

    def _lookup(self, location: str) -> float | None:
        """Minutes to drive from the origin, or None if the place cannot be resolved."""
        lat, lon = self.origin
        resp = requests.get(
            GEOCODE,
            params={
                "q": location,
                "format": "jsonv2",
                "limit": 1,
                # prefer matches near home without excluding others
                "viewbox": f"{lon - 1.5},{lat + 1.5},{lon + 1.5},{lat - 1.5}",
            },
            headers=HEADERS,
            timeout=15,
        )
        resp.raise_for_status()
        time.sleep(1.1)  # Nominatim asks for at most one request a second
        found = resp.json()
        if not found:
            return None
        dest = (float(found[0]["lat"]), float(found[0]["lon"]))
        if miles_between(self.origin, dest) > self.max_miles:
            return None
        resp = requests.get(
            f"{ROUTE}/{lon},{lat};{dest[1]},{dest[0]}",
            params={"overview": "false"},
            headers=HEADERS,
            timeout=15,
        )
        resp.raise_for_status()
        routes = resp.json().get("routes") or []
        return routes[0]["duration"] / 60 if routes else None

    def drive_minutes(
        self, location: str, now: float, allow_lookup: bool = True
    ) -> float | None:
        key = " ".join(location.lower().split())
        with self._lock:
            hit = self._cache.get(key)
        if hit:
            days = FOUND_DAYS if hit["minutes"] is not None else MISSING_DAYS
            if now - hit["at"] < days * 86400:
                return hit["minutes"]
        if not allow_lookup:
            return None
        try:
            minutes = self._lookup(location)
        except Exception as exc:  # leave it uncached so a later run retries
            log.warning("travel lookup failed: %s", type(exc).__name__)
            return None
        with self._lock:
            self._cache[key] = {"minutes": minutes, "at": now}
            tmp = self.cache_path.with_suffix(".tmp")
            tmp.write_text(json.dumps(self._cache))
            tmp.replace(self.cache_path)
        return minutes

    def annotate(self, events: list[dict], now: float | None = None) -> None:
        """Add `drive_minutes` to upcoming events whose location is a street address."""
        now = time.time() if now is None else now
        lookups = 0
        for event in events:
            if event.get("all_day") or event["start"] <= now:
                continue
            location = event.get("location", "")
            if not looks_like_address(location):
                continue
            key = " ".join(location.lower().split())
            cached = key in self._cache
            minutes = self.drive_minutes(
                location, now, allow_lookup=lookups < MAX_LOOKUPS_PER_RUN
            )
            if not cached:
                lookups += 1
            if (
                minutes is not None and minutes >= 3
            ):  # under 3 minutes: effectively here
                event["drive_minutes"] = minutes
