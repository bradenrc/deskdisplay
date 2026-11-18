from datetime import UTC, datetime, timedelta
from zoneinfo import ZoneInfo

from app import agenda, astro, ical, news, weather
from app.photos import parse_folder_id
from app.rclone import extract_token

TZ = ZoneInfo("America/Denver")


def at(hour, minute=0, day=16):
    return datetime(2026, 11, day, hour, minute, tzinfo=TZ)


def event(title, start, end, all_day=False):
    return {
        "title": title,
        "start": start.timestamp(),
        "end": end.timestamp(),
        "all_day": all_day,
        "location": "",
    }


EVENTS = [
    event("Standup", at(9), at(9, 30)),
    event("Weekly sync", at(10, 30), at(11)),
    event("Design review", at(11), at(11, 45)),
    event("Sprint planning", at(16, 30), at(17, 30)),
    event("Holiday", at(0), at(0, day=17), all_day=True),
    event("Tomorrow thing", at(9, day=17), at(10, day=17)),
]


def test_agenda_statuses_at_1052():
    out = agenda.build(EVENTS, at(10, 52), soon_minutes=10)
    by_title = {e["title"]: e for e in out["events"]}
    assert by_title["Standup"]["status"] == "past"
    assert by_title["Weekly sync"]["status"] == "now"
    assert by_title["Weekly sync"]["minutes_left"] == 8
    assert by_title["Design review"]["status"] == "soon"
    assert by_title["Design review"]["minutes_until"] == 8
    assert by_title["Sprint planning"]["status"] == "upcoming"
    assert out["all_day"] == ["Holiday"]
    assert out["next"]["title"] == "Design review"
    assert [e["title"] for e in out["current"]] == ["Weekly sync"]
    assert out["free_after"]["label"] == "5:30" and out["free_after"]["ampm"] == "pm"
    assert [e["title"] for e in out["tomorrow"]] == ["Tomorrow thing"]
    assert "Tomorrow thing" not in by_title


def test_agenda_soon_boundary_and_labels():
    out = agenda.build(EVENTS, at(10, 49), soon_minutes=10)
    review = next(e for e in out["events"] if e["title"] == "Design review")
    assert review["status"] == "upcoming" and review["minutes_until"] == 11
    assert review["start_label"] == "11:00" and review["start_ampm"] == "am"
    out = agenda.build(EVENTS, at(10, 50), soon_minutes=10)
    assert (
        next(e for e in out["events"] if e["title"] == "Design review")["status"]
        == "soon"
    )


def test_agenda_after_last_event():
    out = agenda.build(EVENTS, at(18), soon_minutes=10)
    assert out["remaining"] == 0 and out["free_after"] is None and out["next"] is None


def test_agenda_empty():
    out = agenda.build([], at(8))
    assert out["events"] == [] and out["free_after"] is None


ICS = b"""BEGIN:VCALENDAR
VERSION:2.0
PRODID:-//test//EN
BEGIN:VEVENT
UID:weekly
SUMMARY:Weekly sync
DTSTART;TZID=America/Denver:20261102T103000
DTEND;TZID=America/Denver:20261102T110000
RRULE:FREQ=WEEKLY;BYDAY=MO
LOCATION:Room 4
END:VEVENT
BEGIN:VEVENT
UID:utc
SUMMARY:Call in UTC
DTSTART:20261116T180000Z
DTEND:20261116T183000Z
END:VEVENT
BEGIN:VEVENT
UID:allday
SUMMARY:Holiday
DTSTART;VALUE=DATE:20261116
DTEND;VALUE=DATE:20261117
END:VEVENT
BEGIN:VEVENT
UID:gone
SUMMARY:Cancelled thing
STATUS:CANCELLED
DTSTART:20261116T200000Z
DTEND:20261116T210000Z
END:VEVENT
BEGIN:VEVENT
UID:floating
DTSTART:20261117T090000
DTEND:20261117T093000
END:VEVENT
BEGIN:VEVENT
UID:old
SUMMARY:Last month
DTSTART:20261016T180000Z
DTEND:20261016T183000Z
END:VEVENT
END:VCALENDAR
"""


def test_ical_parse_expands_and_normalizes():
    events = ical.parse(ICS, TZ, at(0), at(0, day=18))
    by_title = {e["title"]: e for e in events}
    assert set(by_title) == {"Weekly sync", "Call in UTC", "Holiday", "(No title)"}
    weekly = by_title["Weekly sync"]  # third repeat of a weekly event lands on the 16th
    assert (
        weekly["start"] == at(10, 30).timestamp()
        and weekly["end"] == at(11).timestamp()
    )
    assert weekly["location"] == "Room 4" and not weekly["all_day"]
    assert (
        by_title["Call in UTC"]["start"] == at(11).timestamp()
    )  # 18:00Z is 11:00 in Denver
    holiday = by_title["Holiday"]
    assert holiday["all_day"] and holiday["start"] == at(0).timestamp()
    assert holiday["end"] == at(0, day=17).timestamp()
    assert (
        by_title["(No title)"]["start"] == at(9, day=17).timestamp()
    )  # floating = local time
    built = agenda.build(events, at(10, 52))
    assert [e["title"] for e in built["current"]] == ["Weekly sync"]
    assert built["next"]["title"] == "Call in UTC" and built["all_day"] == ["Holiday"]


def test_ical_urls():
    assert (
        ical.normalize_url("webcal://example.com/a.ics") == "https://example.com/a.ics"
    )
    assert (
        ical.normalize_url(" https://example.com/a.ics ") == "https://example.com/a.ics"
    )
    assert (
        ical.normalize_url("file:///etc/passwd") == ""
        and ical.normalize_url("nope") == ""
    )
    secret = "https://calendar.google.com/calendar/ical/me%40example.com/private-abcdef123456/basic.ics"
    masked = ical.mask(secret)
    assert "private-abcdef" not in masked and masked.startswith("calendar.google.com/")


def test_extract_token():
    token = '{"access_token":"a","token_type":"Bearer","refresh_token":"r","expiry":"2026-01-01T00:00:00Z"}'
    plain = f"NOTICE: Got code\nPaste the following into your remote machine --->\n{token}\n<---End paste\n"
    assert extract_token(plain) == token
    import base64
    import json

    wrapped = (
        base64.b64encode(json.dumps({"token": token}).encode()).decode().rstrip("=")
    )
    assert extract_token(f"--->\n{wrapped}\n<---End paste") == token
    assert extract_token("NOTICE: Waiting for code...") is None
    assert extract_token("--->\nnot a token\n<---") is None


def test_moon_known_dates():
    new = astro.moon(datetime(2024, 1, 11, 11, 57, tzinfo=UTC))
    assert min(new["phase"], 1 - new["phase"]) < 0.03 and new["name"] == "New moon"
    full = astro.moon(datetime(2024, 1, 25, 17, 54, tzinfo=UTC))
    assert abs(full["phase"] - 0.5) < 0.03 and full["illumination"] >= 98
    assert full["name"] == "Full moon"


def test_weather_view():
    now = at(10, 52)
    base = at(0)
    raw = {
        "units": "F",
        "current": {"temp": 47.6, "code": 3, "is_day": True},
        "hourly": [
            {
                "time": (base + timedelta(hours=h)).timestamp(),
                "temp": 40 + h,
                "code": 61 if h == 13 else 0,
                "precip": 10,
                "is_day": 7 <= h <= 16,
            }
            for h in range(48)
        ],
        "daily": [
            {
                "high": 52.2,
                "low": 38.9,
                "sunrise": at(6, 52).timestamp(),
                "sunset": at(16, 38).timestamp(),
            }
        ],
    }
    out = weather.view(raw, now, step=2, count=5)
    assert (
        out["temp"] == 48 and out["condition"] == "Cloudy" and out["icon"] == "cloudy"
    )
    assert [h["label"] for h in out["hourly"]] == [
        "11 am",
        "1 pm",
        "3 pm",
        "5 pm",
        "7 pm",
    ]
    assert out["hourly"][1]["icon"] == "rain"
    assert out["hourly"][4]["icon"] == "clear-night"
    assert out["high"] == 52 and out["low"] == 39
    assert out["sunrise_label"] == "6:52" and out["sunset_label"] == "4:38"
    assert weather.view(None, now) is None


def test_news_parse_and_age():
    rss = b"""<?xml version="1.0"?><rss version="2.0"><channel><title>Example</title>
    <item><title>First &amp; <b>best</b></title><pubDate>Mon, 16 Nov 2026 17:00:00 GMT</pubDate></item>
    <item><title></title></item></channel></rss>"""
    items = news.parse(rss, "Example News")
    assert len(items) == 1 and items[0]["title"] == "First & best"
    assert items[0]["source"] == "Example News"
    shown = news.view(items, items[0]["published"] + 12 * 60)
    assert shown[0]["age"] == "12 min ago"
    assert news.age_label(10) == "just now"
    assert news.age_label(3 * 3600) == "3 hr ago"
    assert news.age_label(86400) == "1 day ago"


def test_parse_folder_id():
    fid = "1AbCdEfGhIjKlMnOpQrStUvWxYz012345"
    assert (
        parse_folder_id(f"https://drive.google.com/drive/folders/{fid}?usp=sharing")
        == fid
    )
    assert parse_folder_id(f"https://drive.google.com/drive/u/0/folders/{fid}") == fid
    assert parse_folder_id(f"https://drive.google.com/open?id={fid}") == fid
    assert parse_folder_id(fid) == fid
    assert parse_folder_id("' or 1=1") == ""
    assert parse_folder_id("") == ""
    assert parse_folder_id("https://drive.google.com/drive/my-drive") == "root"
    assert parse_folder_id("root") == "root"
