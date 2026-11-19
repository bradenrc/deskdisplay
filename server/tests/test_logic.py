from datetime import UTC, datetime, timedelta
from zoneinfo import ZoneInfo

from app import (
    agenda,
    astro,
    fetch,
    ical,
    news,
    night,
    snow,
    sports,
    start,
    stocks,
    travel,
    weather,
)
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
    <item><title>First &amp; <b>best</b></title><link>https://example.com/a?b=1</link>
    <pubDate>Mon, 16 Nov 2026 17:00:00 GMT</pubDate></item>
    <item><title>Odd link</title><link>javascript:alert(1)</link></item>
    <item><title></title></item></channel></rss>"""
    items = news.parse(rss, "Example News")
    assert len(items) == 2 and items[0]["title"] == "First & best"
    assert items[0]["link"] == "https://example.com/a?b=1"
    assert items[1]["link"] == ""  # only web links are passed on
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


def test_air_quality_bands():
    assert weather.air_view({"aqi": 42}) == {"value": 42, "label": "Good", "level": 0}
    assert weather.air_view({"aqi": 51})["label"] == "Moderate"
    assert weather.air_view({"aqi": 135}) == {
        "value": 135,
        "label": "Sensitive groups",
        "level": 2,
    }
    assert weather.air_view({"aqi": 420})["level"] == 5
    assert weather.air_view(None) is None and weather.air_view({"aqi": None}) is None


def test_night_hours():
    cfg = {"enabled": True, "start": "22:30", "end": "06:30"}
    assert (
        night.active(at(23), cfg)
        and night.active(at(2), cfg)
        and night.active(at(22, 30), cfg)
    )
    assert not night.active(at(6, 30), cfg) and not night.active(at(12), cfg)
    assert not night.active(at(22, 29), cfg)
    assert not night.active(at(23), {**cfg, "enabled": False})
    daytime = {"enabled": True, "start": "13:00", "end": "14:00"}
    assert night.active(at(13, 30), daytime) and not night.active(at(14), daytime)


def test_snow_reports_need_fresh_snow():
    now = at(10)

    def mountain(name, recent_per_hour, coming_per_hour):
        return {
            "name": name,
            "unit": '"',
            "hours": [
                {
                    "time": (now + timedelta(hours=h)).timestamp(),
                    "snow": (
                        recent_per_hour
                        if -8 < h <= 0
                        else coming_per_hour if 0 < h <= 8 else 0
                    ),
                    "temp": 24.4,
                }
                for h in range(-48, 48)
            ],
        }

    bogus, brundage = mountain("Bogus Basin", 0.5, 0.25), mountain("Brundage", 0, 0)
    assert snow.summary(bogus, now.timestamp()) == {
        "recent": 4.0,
        "coming": 2.0,
        "temp": 24,
    }
    both = snow.reports([bogus, brundage], now.timestamp())
    assert both == [
        {"name": "Bogus Basin", "new": '4"', "detail": '+2" tomorrow', "temp": 24},
        {"name": "Brundage", "new": '0"', "detail": "None expected", "temp": 24},
    ]
    assert (
        snow.reports([brundage], now.timestamp()) == []
    )  # nothing new anywhere: no report
    two_days_on = (now + timedelta(hours=60)).timestamp()
    assert (
        snow.reports([bogus, brundage], two_days_on) == []
    )  # the snow is now older than 48 hours
    assert snow.reports([], now.timestamp()) == []
    dusting = snow.reports([mountain("Tamarack", 0.5, 0.05)], now.timestamp())
    assert dusting[0]["detail"] == '<1" tomorrow'


def test_travel_address_filter():
    yes = [
        "800 W Main St, Boise, ID 83702",
        "1910 University Dr",
        "2600 Bogus Basin Rd, Boise",
    ]
    no = [
        "",
        "Zoom",
        "https://teams.microsoft.com/l/meetup-join/19",
        "Room 4",
        "Conference Room B",
        "Microsoft Teams Meeting",
        "Phone call 555-1234",
        "Boise",
    ]
    assert all(travel.looks_like_address(t) for t in yes)
    assert not any(travel.looks_like_address(t) for t in no)
    assert (
        300 < travel.miles_between((43.615, -116.2023), (47.6062, -122.3321)) < 420
    )  # Boise to Seattle


def test_travel_annotate_uses_cache_only_for_addresses(tmp_path):
    planner = travel.Travel(tmp_path / "cache.json", (43.615, -116.2023))
    calls = []
    planner._lookup = lambda location: calls.append(location) or 18.0
    now = at(9).timestamp()
    events = [
        {
            "title": "Dentist",
            "start": at(14).timestamp(),
            "end": at(15).timestamp(),
            "all_day": False,
            "location": "800 W Main St, Boise",
        },
        {
            "title": "Standup",
            "start": at(10).timestamp(),
            "end": at(10, 30).timestamp(),
            "all_day": False,
            "location": "Zoom",
        },
        {
            "title": "Earlier",
            "start": at(8).timestamp(),
            "end": at(8, 30).timestamp(),
            "all_day": False,
            "location": "1 Old Rd, Boise",
        },
    ]
    planner.annotate(events, now)
    planner.annotate(events, now)
    assert calls == [
        "800 W Main St, Boise"
    ]  # looked up once, then cached; others never sent
    assert events[0]["drive_minutes"] == 18.0 and "drive_minutes" not in events[1]

    opts = {"buffer_minutes": 5, "padding_percent": 15}
    row = agenda.build(events, at(9), 10, travel=opts)["events"][-1]
    assert row["travel"]["drive_minutes"] == 21  # 18 min padded 15% and rounded up
    assert (
        row["travel"]["leave_label"] == "1:34" and row["travel"]["urgency"] == "later"
    )
    assert (
        agenda.build(events, at(13, 28), 10, travel=opts)["events"][-1]["travel"][
            "urgency"
        ]
        == "soon"
    )
    assert (
        agenda.build(events, at(13, 40), 10, travel=opts)["events"][-1]["travel"][
            "urgency"
        ]
        == "now"
    )
    assert (
        agenda.build(events, at(9), 10)["events"][-1]["travel"] is None
    )  # feature off


def _competition(state, detail, home, away, tv="Fox"):
    side = lambda where, abbr, name, score: {
        "homeAway": where,
        "score": score,
        "team": {"abbreviation": abbr, "shortDisplayName": name},
    }
    return {
        "status": {"type": {"state": state, "shortDetail": detail}},
        "competitors": [side("home", *home), side("away", *away)],
        "broadcasts": [{"media": {"shortName": tv}}] if tv else [],
    }


def test_sports_pick_and_chips():
    now = at(10)  # Monday Nov 16, 10:00

    def iso(dt):
        return dt.astimezone(ZoneInfo("UTC")).strftime("%Y-%m-%dT%H:%MZ")

    last = sports.parse_game(
        "1",
        iso(at(14, day=15)),
        _competition(
            "post",
            "Final",
            ("LV", "Raiders", {"displayValue": "13"}),
            ("SEA", "Seahawks", {"displayValue": "21"}),
            "CBS",
        ),
    )
    nxt = sports.parse_game(
        "2",
        iso(at(15, day=21)),
        _competition(
            "pre",
            "11/21 - 5:00 PM EST",
            ("SEA", "Seahawks", None),
            ("LAR", "Rams", None),
        ),
    )
    far = sports.parse_game(
        "3",
        iso(at(15, day=30)),
        _competition(
            "pre", "TBD", ("PHI", "Eagles", None), ("SEA", "Seahawks", None), ""
        ),
    )
    last["mine"], nxt["mine"], far["mine"] = "away", "home", "away"
    assert (
        last["home"]["score"] == 13
        and last["away"]["score"] == 21
        and not far["time_set"]
    )
    games = [last, nxt, far]

    final = sports.pick(games, now.timestamp(), preview_days=6, final_hours=36)
    assert final["id"] == "1"  # yesterday's final is still within 36 hours
    assert sports.chip(final, now) == {
        "state": "post",
        "line1": "SEA @ LV",
        "line2": "21–13 · Final",
        "tv": "CBS",
        "colors": [],
    }

    later = at(
        10, day=17
    )  # Tuesday: the final has aged out, next game is within 6 days
    upcoming = sports.pick(games, later.timestamp(), 6, 36)
    assert upcoming["id"] == "2"
    assert sports.chip(upcoming, later) == {
        "state": "pre",
        "line1": "LAR @ SEA",
        "line2": "Sat 3:00 pm",
        "tv": "Fox",
        "colors": [],
    }
    assert sports.chip(upcoming, at(9, day=21))["line2"] == "Today 3:00 pm"
    assert sports.chip(far, at(9, day=29))["line2"] == "Mon · TBD"
    assert (
        sports.pick([far], later.timestamp(), 6, 36) is None
    )  # quiet week: nothing shown

    live = dict(
        nxt,
        state="in",
        detail="8:42 - 3rd",
        home={**nxt["home"], "score": 17},
        away={**nxt["away"], "score": 10},
    )
    assert (
        sports.pick([last, live, far], now.timestamp(), 6, 36)["id"] == "2"
    )  # live beats a recent final
    assert sports.chip(live, at(16, day=21)) == {
        "state": "in",
        "line1": "LAR @ SEA",
        "line2": "10–17 · 3rd 8:42",
        "tv": "Fox",
        "colors": [],
    }
    assert (
        sports.chip(dict(live, detail="Halftime"), at(16, day=21))["line2"]
        == "10–17 · Halftime"
    )

    assert sports.may_be_live(
        nxt, at(14, 55, day=21).timestamp()
    )  # five minutes before kickoff
    assert sports.may_be_live(
        nxt, at(17, day=21).timestamp()
    )  # feed still says "pre" after kickoff
    assert not sports.may_be_live(nxt, at(10, day=21).timestamp())
    assert not sports.may_be_live(far, at(15, day=30).timestamp())  # time not set
    assert not sports.may_be_live(last, now.timestamp()) and not sports.may_be_live(
        None, 0
    )

    assert sports.team_colors({"color": "002244", "alternateColor": "69BE28"}) == [
        "#002244",
        "#69be28",
    ]
    assert sports.team_colors({"color": "nope"}) == [] and sports.team_colors({}) == []
    striped = sports.chip(dict(final, colors=["#002244", "#69be28"]), now)
    assert striped["colors"] == ["#002244", "#69be28"]


def test_sports_tracker_polls_live_only_when_needed(monkeypatch):
    kickoff = at(15, day=21)
    game = sports.parse_game(
        "2",
        kickoff.astimezone(ZoneInfo("UTC")).strftime("%Y-%m-%dT%H:%MZ"),
        _competition(
            "pre",
            "11/21 - 5:00 PM EST",
            ("SEA", "Seahawks", None),
            ("LAR", "Rams", None),
        ),
    )
    game["mine"] = "home"
    calls = {"schedule": 0, "live": 0}

    def schedule(league, team):
        calls["schedule"] += 1
        return [dict(game)]

    def live(league, current):
        calls["live"] += 1
        return dict(
            current,
            state="in",
            detail="12:00 - 1st",
            home={**current["home"], "score": 7},
            away={**current["away"], "score": 0},
        )

    monkeypatch.setattr(sports, "fetch_schedule", schedule)
    monkeypatch.setattr(sports, "fetch_live", live)
    tracker = sports.Tracker(
        [{"league": "nfl", "team": "sea", "name": "Seahawks"}], 60, 6, 36
    )

    quiet = at(10, day=18).timestamp()
    assert tracker.refresh(quiet) == 0 and tracker.refresh(quiet + 60) == 0
    assert calls == {
        "schedule": 1,
        "live": 0,
    }  # schedule cached for an hour, no live polling midweek
    assert sports.chips(tracker, at(10, day=18))[0]["line2"] == "Sat 3:00 pm"

    tracker.refresh(kickoff.timestamp() + 300)
    assert calls["live"] == 1
    assert sports.chips(tracker, kickoff)[0]["line2"] == "0–7 · 1st 12:00"
    assert sports.chips(None, kickoff) == []


def test_stocks_view_and_refresh_pacing(monkeypatch):
    calls = []

    def fake(entry):
        calls.append(entry["symbol"])
        if entry["symbol"] == "BAD":
            raise RuntimeError("no such symbol")
        return {
            "symbol": entry["symbol"],
            "name": entry.get("name") or entry["symbol"],
            "price": 7340.53,
            "previous": 7416.59,
            "closes": [7400 - i for i in range(79)],
            "session_start": 1000,
            "session_end": 2000,
        }

    monkeypatch.setattr(stocks, "fetch_one", fake)
    watch = stocks.Watchlist(
        [{"symbol": "^GSPC", "name": "S&P 500"}, {"symbol": "BAD"}]
    )
    assert watch.refresh(now=1500) == 1  # one failure, the good quote still shown
    out = stocks.view(watch, 1500)
    assert out["open"] is True and len(out["items"]) == 1
    item = out["items"][0]
    assert (
        item["price"] == "7,340.53"
        and item["change"] == "-1.03%"
        and item["direction"] == "down"
    )
    assert (
        len(item["points"]) <= 42
        and min(item["points"]) == 0
        and item["baseline"] == 100
    )
    assert item["points"][0] > item["points"][-1]  # the day sloped down

    watch.refresh(now=1800)  # open: asks again
    assert calls.count("^GSPC") == 2
    watch.refresh(now=2100)  # just closed: one more to capture the close
    watch.refresh(now=2200)  # closed and recently fetched: skipped
    assert calls.count("^GSPC") == 3
    watch.refresh(now=2100 + 31 * 60)
    assert calls.count("^GSPC") == 4
    assert stocks.view(watch, 5000)["open"] is False
    assert stocks.view(None, 0) is None


def test_start_feed_parse_finds_pictures_and_site():
    rss = b"""<?xml version="1.0"?><rss version="2.0" xmlns:media="http://search.yahoo.com/mrss/">
    <channel><title>Golf</title><link>https://www.golf.com/</link>
    <item><title>One</title><link>https://golf.com/1</link>
      <media:thumbnail url="https://golf.com/1.jpg"/><description>First &lt;b&gt;story&lt;/b&gt;</description></item>
    <item><title>Two</title><link>https://golf.com/2</link>
      <description>&lt;img src="https://golf.com/2.jpg?a=1&amp;amp;b=2"&gt; Second</description></item>
    <item><title>Three</title><link>javascript:alert(1)</link></item></channel></rss>"""
    feed = start.parse_feed(rss)
    assert feed["title"] == "Golf" and feed["site"] == "golf.com"
    one, two, three = feed["items"]
    assert one["image"] == "https://golf.com/1.jpg" and one["summary"] == "First story"
    assert (
        two["image"] == "https://golf.com/2.jpg?a=1&b=2" and two["summary"] == "Second"
    )
    assert three["link"] == "" and three["image"] == ""


def test_start_finds_feed_and_icon_links_in_a_page():
    page = """<html><head>
    <link rel="alternate" type="application/rss+xml" href="/feed/">
    <link rel="icon" sizes="16x16" href="/small.png">
    <link rel="apple-touch-icon" href="https://cdn.example.com/touch.png">
    <link rel="icon" sizes="96x96" href="big.png"><link rel="stylesheet" href="x.css">
    </head></html>"""
    assert start.feed_links(page, "https://example.com/a/") == [
        "https://example.com/feed/"
    ]
    assert start.icon_links(page, "https://example.com/a/") == [
        "https://example.com/a/big.png",
        "https://cdn.example.com/touch.png",
        "https://example.com/small.png",
    ]


def test_fetch_refuses_addresses_inside_the_home_network():
    for host in (
        "127.0.0.1",
        "localhost",
        "192.168.1.10",
        "10.0.0.5",
        "169.254.169.254",
    ):
        assert not fetch.is_public(host)
    for url in ("file:///etc/passwd", "ftp://example.com/x", "http://127.0.0.1/"):
        try:
            fetch.safe_get(url)
            raise AssertionError("should have been refused")
        except fetch.Refused:
            pass
