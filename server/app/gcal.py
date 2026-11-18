"""Google Calendar: read today's and tomorrow's events (read-only scope)."""

from __future__ import annotations

from datetime import datetime, timedelta
from urllib.parse import quote
from zoneinfo import ZoneInfo

API = "https://www.googleapis.com/calendar/v3"
TIMEOUT = 20


def list_calendars(sess) -> list[dict]:
    out, token = [], None
    while True:
        params = {
            "maxResults": 250,
            "fields": "nextPageToken,items(id,summary,primary)",
        }
        if token:
            params["pageToken"] = token
        resp = sess.get(f"{API}/users/me/calendarList", params=params, timeout=TIMEOUT)
        resp.raise_for_status()
        body = resp.json()
        for item in body.get("items", []):
            out.append(
                {
                    "id": item["id"],
                    "name": item.get("summary", item["id"]),
                    "primary": bool(item.get("primary")),
                }
            )
        token = body.get("nextPageToken")
        if not token:
            break
    out.sort(key=lambda c: (not c["primary"], c["name"].lower()))
    return out


def primary_account(sess) -> str:
    resp = sess.get(
        f"{API}/calendars/primary", params={"fields": "id"}, timeout=TIMEOUT
    )
    resp.raise_for_status()
    return resp.json().get("id", "")


def _declined(item: dict) -> bool:
    for attendee in item.get("attendees", []):
        if attendee.get("self") and attendee.get("responseStatus") == "declined":
            return True
    return False


def normalize(item: dict, tz: ZoneInfo) -> dict | None:
    """One API event -> {title, start, end, all_day, location}, or None to skip it."""
    if item.get("status") == "cancelled" or item.get("eventType") == "workingLocation":
        return None
    start, end = item.get("start", {}), item.get("end", {})
    if "dateTime" in start and "dateTime" in end:
        all_day = False
        start_ts = datetime.fromisoformat(start["dateTime"]).timestamp()
        end_ts = datetime.fromisoformat(end["dateTime"]).timestamp()
    elif "date" in start and "date" in end:
        all_day = True
        start_ts = datetime.fromisoformat(start["date"]).replace(tzinfo=tz).timestamp()
        end_ts = datetime.fromisoformat(end["date"]).replace(tzinfo=tz).timestamp()
    else:
        return None
    return {
        "title": item.get("summary") or "(No title)",
        "start": start_ts,
        "end": end_ts,
        "all_day": all_day,
        "location": item.get("location", ""),
    }


def fetch(
    sess, calendar_ids: list[str], tz: ZoneInfo, hide_declined: bool = True
) -> list[dict]:
    midnight = datetime.now(tz).replace(hour=0, minute=0, second=0, microsecond=0)
    params_base = {
        "timeMin": midnight.isoformat(),
        "timeMax": (midnight + timedelta(days=2)).isoformat(),
        "singleEvents": "true",
        "orderBy": "startTime",
        "maxResults": 250,
        "fields": "nextPageToken,items(status,summary,location,start,end,eventType,"
        "attendees(self,responseStatus))",
    }
    events: list[dict] = []
    seen: set[tuple] = set()
    for calendar_id in calendar_ids:
        token = None
        while True:
            params = dict(params_base)
            if token:
                params["pageToken"] = token
            resp = sess.get(
                f"{API}/calendars/{quote(calendar_id, safe='')}/events",
                params=params,
                timeout=TIMEOUT,
            )
            resp.raise_for_status()
            body = resp.json()
            for item in body.get("items", []):
                if hide_declined and _declined(item):
                    continue
                event = normalize(item, tz)
                if not event:
                    continue
                key = (event["title"], event["start"], event["end"])
                if key not in seen:
                    seen.add(key)
                    events.append(event)
            token = body.get("nextPageToken")
            if not token:
                break
    events.sort(key=lambda e: (e["start"], e["end"]))
    return events
