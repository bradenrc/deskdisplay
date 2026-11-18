"""Snow report for ski areas, shown beside the news when there is fresh snow.

The numbers come from Open-Meteo's weather model at the mountain's coordinates and
elevation. They are a forecast-model estimate, not the resort's own measured snow report,
and there is no lift status: no free, openly accessible source for those was found.
"""

from __future__ import annotations

import requests

URL = "https://api.open-meteo.com/v1/forecast"
DAY = 24 * 3600


def fetch(resort: dict, timezone: str, units: str = "fahrenheit") -> dict:
    metric = units == "celsius"
    params = {
        "latitude": resort["latitude"],
        "longitude": resort["longitude"],
        "hourly": "snowfall,temperature_2m",
        "past_days": 2,
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
    shown = f"{value:.0f}" if value >= 1 else "<1" if value >= 0.1 else "0"
    return f"{shown}{unit}" if unit == '"' else f"{shown} {unit}"


def summary(raw: dict, now_ts: float) -> dict:
    """Snowfall over the last 48 hours and the next 24, and the current temperature."""
    hours = raw["hours"]
    # each hourly value covers the hour that ends at its timestamp
    recent = sum(h["snow"] for h in hours if now_ts - 2 * DAY < h["time"] <= now_ts)
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


def reports(
    raws: list[dict], now_ts: float, min_new: float = 0.5, limit: int = 2
) -> list[dict]:
    """One entry per mountain, or [] unless at least one has fresh snow in the last 48 hours.

    Every configured mountain is listed once any qualifies, so they can be compared.
    """
    summaries = [(raw, summary(raw, now_ts)) for raw in raws]
    if not any(s["recent"] >= min_new for _raw, s in summaries):
        return []
    out = []
    for raw, s in summaries[:limit]:
        unit = raw["unit"]
        # kept short: this sits in a narrow tile
        amount = _amount(s["coming"], unit)
        if s["coming"] >= 1:
            more = f"+{amount} tomorrow"
        elif s["coming"] >= 0.1:
            more = f"{amount} tomorrow"  # under an inch reads oddly with a plus sign
        else:
            more = "None expected"
        out.append(
            {
                "name": raw["name"],
                "new": _amount(s["recent"], unit),  # e.g. 4"
                "detail": more,
                "temp": s["temp"],
            }
        )
    return out
