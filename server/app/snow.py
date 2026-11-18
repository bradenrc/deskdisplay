"""Snow outlook for ski areas, shown as a card that rotates with the news headlines.

The numbers come from Open-Meteo's weather model at the mountain's coordinates and
elevation. They are a forecast-model estimate, not the resort's own measured snow report,
and there is no lift status: no free, openly accessible source for those was found.
"""

from __future__ import annotations

from datetime import datetime

import requests

URL = "https://api.open-meteo.com/v1/forecast"
DAY = 24 * 3600


def fetch(resort: dict, timezone: str, units: str = "fahrenheit") -> dict:
    metric = units == "celsius"
    params = {
        "latitude": resort["latitude"],
        "longitude": resort["longitude"],
        "hourly": "snowfall,temperature_2m",
        "past_days": 1,
        "forecast_days": 3,
        "precipitation_unit": "mm" if metric else "inch",
        "temperature_unit": "celsius" if metric else "fahrenheit",
        "timeformat": "unixtime",
        "timezone": timezone,
    }
    if resort.get("elevation_m"):
        params["elevation"] = resort["elevation_m"]
    resp = requests.get(URL, params=params, timeout=20)
    resp.raise_for_status()
    hourly = resp.json()["hourly"]
    return {
        "name": resort["name"],
        "unit": "cm" if metric else '"',
        "hours": [
            {
                "time": hourly["time"][i],
                "snow": hourly["snowfall"][i] or 0,
                "temp": hourly["temperature_2m"][i],
            }
            for i in range(len(hourly["time"]))
        ],
    }


def _amount(value: float, unit: str) -> str:
    shown = f"{value:.0f}" if value >= 1 else "under 1"
    return f"{shown}{unit}" if unit == '"' else f"{shown} {unit}"


def summary(raw: dict, now_ts: float) -> dict:
    """Snowfall in the last and next 24 hours, and the current temperature."""
    hours = raw["hours"]
    # each hourly value covers the hour that ends at its timestamp
    recent = sum(h["snow"] for h in hours if now_ts - DAY < h["time"] <= now_ts)
    coming = sum(h["snow"] for h in hours if now_ts < h["time"] <= now_ts + DAY)
    current = min(hours, key=lambda h: abs(h["time"] - now_ts), default=None)
    return {
        "recent": recent,
        "coming": coming,
        "temp": (
            None
            if current is None or current["temp"] is None
            else round(current["temp"])
        ),
    }


def card(
    raw: dict, now: datetime, months: list[int], threshold: float = 0.2
) -> dict | None:
    """A headline-style card, or None when there is nothing worth showing (summer)."""
    s = summary(raw, now.timestamp())
    snowing = s["recent"] >= threshold or s["coming"] >= threshold
    if not snowing and now.month not in months:
        return None
    unit = raw["unit"]
    if s["recent"] >= threshold:
        text = f"{_amount(s['recent'], unit)} of new snow in the last 24 hours"
    else:
        text = "No new snow in the last 24 hours"
    if s["coming"] >= threshold:
        more = " more" if s["recent"] >= threshold else ""
        text += f", {_amount(s['coming'], unit)}{more} expected by this time tomorrow."
    else:
        text += ", none expected by tomorrow."
    if s["temp"] is not None:
        text += f" {s['temp']}° on the mountain."
    return {
        "title": text,
        "source": raw["name"],
        "age": "Snow forecast",
        "published": None,
        "summary": "",
        "kind": "snow",
    }
