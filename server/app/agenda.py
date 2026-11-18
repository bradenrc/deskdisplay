"""Turn raw calendar events into the agenda the screen shows.

Pure functions: the status of each event depends only on the events and `now`.
"""

from __future__ import annotations

import math
from datetime import datetime, timedelta


def clock_label(dt: datetime) -> str:
    return f"{dt.hour % 12 or 12}:{dt.minute:02d}"


def ampm(dt: datetime) -> str:
    return "am" if dt.hour < 12 else "pm"


def _travel(
    event: dict, now: datetime, status: str, soon_minutes: int, opts: dict
) -> dict | None:
    """When to leave for an upcoming event that has a known driving time."""
    drive = event.get("drive_minutes")
    if not opts or drive is None or status not in ("soon", "upcoming"):
        return None
    minutes = math.ceil(drive * (1 + opts.get("padding_percent", 0) / 100))
    leave_ts = event["start"] - (minutes + opts.get("buffer_minutes", 0)) * 60
    leave = datetime.fromtimestamp(leave_ts, now.tzinfo)
    until = (leave_ts - now.timestamp()) / 60
    return {
        "drive_minutes": minutes,
        "leave": int(leave_ts),
        "leave_label": clock_label(leave),
        "leave_ampm": ampm(leave),
        "urgency": (
            "now" if until <= 0 else "soon" if until <= soon_minutes else "later"
        ),
    }


def _view(
    event: dict, now: datetime, soon_minutes: int, travel: dict | None = None
) -> dict:
    tz = now.tzinfo
    start = datetime.fromtimestamp(event["start"], tz)
    end = datetime.fromtimestamp(event["end"], tz)
    now_ts = now.timestamp()
    minutes_until = None
    minutes_left = None
    if event["end"] <= now_ts:
        status = "past"
    elif event["start"] <= now_ts:
        status = "now"
        minutes_left = math.ceil((event["end"] - now_ts) / 60)
    else:
        minutes_until = math.ceil((event["start"] - now_ts) / 60)
        status = "soon" if minutes_until <= soon_minutes else "upcoming"
    return {
        "title": event["title"],
        "location": event.get("location", ""),
        "start": int(event["start"]),
        "end": int(event["end"]),
        "start_label": clock_label(start),
        "start_ampm": ampm(start),
        "end_label": clock_label(end),
        "end_ampm": ampm(end),
        "status": status,
        "minutes_until": minutes_until,
        "minutes_left": minutes_left,
        "travel": _travel(event, now, status, soon_minutes, travel or {}),
    }


def build(
    events: list[dict],
    now: datetime,
    soon_minutes: int = 10,
    tomorrow_count: int = 3,
    travel: dict | None = None,
) -> dict:
    """`events` items: {title, start, end, all_day, location}; start/end are epoch seconds."""
    midnight = now.replace(hour=0, minute=0, second=0, microsecond=0)
    t0 = midnight.timestamp()
    t1 = (midnight + timedelta(days=1)).timestamp()
    t2 = (midnight + timedelta(days=2)).timestamp()

    ordered = sorted(events, key=lambda e: (e["start"], e["end"]))
    today = [e for e in ordered if e["start"] < t1 and e["end"] > t0]
    timed = [_view(e, now, soon_minutes, travel) for e in today if not e.get("all_day")]
    all_day = [e["title"] for e in today if e.get("all_day")]
    tomorrow = [
        _view(e, now, soon_minutes)
        for e in ordered
        if not e.get("all_day") and t1 <= e["start"] < t2
    ]

    remaining = [e for e in timed if e["status"] != "past"]
    upcoming = [e for e in timed if e["status"] in ("soon", "upcoming")]
    free_after = None
    if remaining:
        last_end = datetime.fromtimestamp(max(e["end"] for e in remaining), now.tzinfo)
        free_after = {
            "epoch": int(last_end.timestamp()),
            "label": clock_label(last_end),
            "ampm": ampm(last_end),
        }
    return {
        "events": timed,
        "all_day": all_day,
        "current": [e for e in timed if e["status"] == "now"],
        "next": upcoming[0] if upcoming else None,
        "remaining": len(remaining),
        "free_after": free_after,
        "tomorrow": tomorrow[:tomorrow_count],
        "tomorrow_total": len(tomorrow),
    }
