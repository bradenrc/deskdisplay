"""Regenerates fixtures/dashboard.json and fixtures/photo.jpg using the server's own code,
so the sample data always has the shape the real server produces.

Run from the firmware folder with the server's Python:
    ../server/.venv/bin/python tools/make_fixtures.py
"""

import json
import sys
from datetime import datetime, timedelta
from pathlib import Path
from zoneinfo import ZoneInfo

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "server"))
from app import agenda, astro, news, night, snow, weather  # noqa: E402
from PIL import Image, ImageDraw  # noqa: E402

OUT = Path(__file__).resolve().parents[1] / "fixtures"
TZ = ZoneInfo("America/Boise")


def at(hour, minute=0, day=16):
    return datetime(2026, 11, day, hour, minute, tzinfo=TZ)


def event(title, start, end, location="", drive=None):
    item = {"title": title, "start": start.timestamp(), "end": end.timestamp(), "all_day": False, "location": location}
    if drive:
        item["drive_minutes"] = drive
    return item


now = at(10, 52)
events = [
    event("Team standup", at(9), at(9, 30)),
    event("Weekly sync", at(10, 30), at(11)),
    event("Design review with the platform team", at(11), at(11, 45)),
    event("Lunch with Sam", at(12, 30), at(13, 30), "800 W Main St, Boise", drive=16),
    event("1:1 with manager", at(14), at(14, 30)),
    event("Sprint planning", at(16, 30), at(17, 30)),
    event("Dentist", at(9, day=17), at(10, day=17)),
]
base = at(0)
temps = {11: 50, 13: 51, 15: 49, 17: 45, 19: 42}
codes = {11: 3, 13: 61, 15: 61, 17: 3, 19: 0}
forecast = {
    "units": "F",
    "current": {"temp": 48.2, "code": 3, "is_day": True},
    "hourly": [
        {"time": (base + timedelta(hours=h)).timestamp(), "temp": temps.get(h, 44), "code": codes.get(h, 3), "precip": 20, "is_day": 7 <= h <= 16}
        for h in range(48)
    ],
    "daily": [{"high": 52, "low": 39, "sunrise": at(7, 38).timestamp(), "sunset": at(17, 18).timestamp()}],
}
headlines = [
    ("Example News", 12, "City council approves new transit plan after months of debate"),
    ("Example Tech", 40, "Chip maker announces new low-power radio for small devices"),
    ("Example Weather", 65, "Storm system expected to arrive midweek, bringing the season’s first snow"),
    ("Example Sports", 130, "Local team wins in overtime"),
]
feed = [{"title": t, "source": s, "published": now.timestamp() - m * 60, "summary": ""} for s, m, t in headlines]
def mountain(name, recent_per_hour, coming_per_hour):
    return {
        "name": name,
        "unit": '"',
        "hours": [
            {"time": (now + timedelta(hours=h)).timestamp(), "snow": recent_per_hour if -8 < h <= 0 else coming_per_hour if 0 < h <= 8 else 0, "temp": 24}
            for h in range(-48, 48)
        ],
    }


# Three games in the three states a card can be in.
games = [
    {"state": "in", "line1": "SDSU @ BOIS", "line2": "17–24 · 3rd 8:42", "tv": "USA Net"},
    {"state": "pre", "line1": "IU @ WASH", "line2": "Sat 1:30 pm", "tv": "Peacock"},
    {"state": "post", "line1": "SEA @ LV", "line2": "21–13 · Final", "tv": "CBS"},
]
view = weather.view(forecast, now, 2, 5)
view["air"] = weather.air_view({"aqi": 42})
doc = {
    "time": {"epoch": int(now.timestamp()), "iso": now.isoformat(timespec="seconds"), "timezone": "America/Boise", "utc_offset_seconds": int(now.utcoffset().total_seconds())},
    "agenda": agenda.build(events, now, 10, travel={"buffer_minutes": 5, "padding_percent": 15}),
    "weather": view,
    "moon": astro.moon(now),
    "news": news.view(feed, now.timestamp()),
    "games": games,
    "snow": snow.reports([mountain("Bogus Basin", 0.5, 0.25), mountain("Brundage", 0.75, 0)], now.timestamp()),
    "night": night.active(now, {"enabled": True, "start": "22:30", "end": "06:30"}),
    "photos": {"count": 1},
}
OUT.mkdir(exist_ok=True)
(OUT / "dashboard.json").write_text(json.dumps(doc, indent=2, ensure_ascii=False) + "\n")

# A stand-in photo: a flat sunset over a lake, plus pure colour swatches to check the decoder.
W, H = 1024, 600
image = Image.new("RGB", (W, H), (250, 199, 117))
draw = ImageDraw.Draw(image)
draw.rectangle([0, 0, W, 240], fill=(240, 153, 123))
draw.ellipse([670, 230, 810, 370], fill=(250, 238, 218))
draw.polygon([(0, 380), (184, 220), (348, 360), (532, 180), (737, 370), (880, 260), (W, 360), (W, H), (0, H)], fill=(60, 52, 137))
draw.polygon([(0, 440), (245, 320), (491, 430), (696, 340), (W, 450), (W, H), (0, H)], fill=(38, 33, 92))
draw.rectangle([0, 470, W, H], fill=(12, 68, 124))
draw.rectangle([676, 480, 800, 486], fill=(250, 199, 117))
draw.rectangle([696, 504, 780, 510], fill=(250, 199, 117))
for i, colour in enumerate([(255, 0, 0), (0, 255, 0), (0, 0, 255)]):
    draw.rectangle([20 + i * 44, 20, 56 + i * 44, 56], fill=colour)
image.save(OUT / "photo.jpg", "JPEG", quality=88, progressive=False, subsampling="4:2:0")
print("wrote", OUT / "dashboard.json", "and photo.jpg")
