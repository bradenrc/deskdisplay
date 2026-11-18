"""Calendar from private iCal links (for example Google Calendar's "secret address").

No sign-in is involved: the link itself is the secret, so it is stored in the data
folder and never shown in full on the setup page.
"""

from __future__ import annotations

import logging
from datetime import date, datetime, time, timedelta
from urllib.parse import urlparse
from zoneinfo import ZoneInfo

import icalendar
import recurring_ical_events
import requests

log = logging.getLogger("deskpal.ical")
HEADERS = {"User-Agent": "desk-companion/1.0 (calendar reader)"}


def normalize_url(text: str) -> str:
    """Return a fetchable https/http address, or '' if it is not one."""
    url = (text or "").strip()
    if url.lower().startswith("webcal://"):
        url = "https://" + url[len("webcal://") :]
    parsed = urlparse(url)
    if parsed.scheme not in ("http", "https") or not parsed.netloc:
        return ""
    return url


def mask(url: str) -> str:
    """Enough of a link to recognise it, without revealing the secret part."""
    parsed = urlparse(url)
    return f"{parsed.netloc}/…{url[-6:]}"


def _stamp(value, tz: ZoneInfo) -> float:
    if isinstance(value, datetime):
        if value.tzinfo is None:  # "floating" time: means local wall-clock time
            value = value.replace(tzinfo=tz)
        return value.timestamp()
    return datetime.combine(value, time.min, tz).timestamp()


def parse(content: bytes, tz: ZoneInfo, start: datetime, end: datetime) -> list[dict]:
    """Expand repeating events and return those overlapping [start, end)."""
    calendar = icalendar.Calendar.from_ical(content)
    events = []
    for item in recurring_ical_events.of(calendar).between(start, end):
        if str(item.get("STATUS", "")).upper() == "CANCELLED":
            continue
        begins = item["DTSTART"].dt
        all_day = isinstance(begins, date) and not isinstance(begins, datetime)
        if item.get("DTEND") is not None:
            ends = item["DTEND"].dt
        elif item.get("DURATION") is not None:
            ends = begins + item["DURATION"].dt
        else:
            ends = begins + (timedelta(days=1) if all_day else timedelta(0))
        events.append(
            {
                "title": str(item.get("SUMMARY", "")).strip() or "(No title)",
                "start": _stamp(begins, tz),
                "end": _stamp(ends, tz),
                "all_day": all_day,
                "location": str(item.get("LOCATION", "")).strip(),
            }
        )
    return events


def fetch(urls: list[str], tz: ZoneInfo, days: int = 2) -> list[dict]:
    midnight = datetime.now(tz).replace(hour=0, minute=0, second=0, microsecond=0)
    end = midnight + timedelta(days=days)
    events: list[dict] = []
    seen: set[tuple] = set()
    failures = []
    for url in urls:
        try:
            resp = requests.get(url, headers=HEADERS, timeout=30)
            resp.raise_for_status()
            found = parse(resp.content, tz, midnight, end)
        except Exception as exc:
            # Never log the link itself: it is the credential.
            reason = (
                f"HTTP {exc.response.status_code}"
                if isinstance(exc, requests.HTTPError) and exc.response is not None
                else type(exc).__name__
            )
            failures.append(f"{mask(url)}: {reason}")
            log.warning("calendar link %s failed: %s", mask(url), reason)
            continue
        for event in found:
            key = (event["title"], event["start"], event["end"])
            if key not in seen:
                seen.add(key)
                events.append(event)
    if urls and len(failures) == len(urls):
        raise RuntimeError(
            "Could not read the calendar link (" + "; ".join(failures) + ")"
        )
    events.sort(key=lambda e: (e["start"], e["end"]))
    return events
