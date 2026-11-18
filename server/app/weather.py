"""Weather from Open-Meteo (free, no API key)."""

from __future__ import annotations

from datetime import datetime

import requests

from . import agenda

URL = "https://api.open-meteo.com/v1/forecast"

# WMO weather code -> (text, icon key). The device maps icon keys to its own artwork.
CODES = {
    0: ("Clear", "clear"),
    1: ("Mostly clear", "clear"),
    2: ("Partly cloudy", "partly"),
    3: ("Cloudy", "cloudy"),
    45: ("Fog", "fog"),
    48: ("Fog", "fog"),
    51: ("Light drizzle", "drizzle"),
    53: ("Drizzle", "drizzle"),
    55: ("Drizzle", "drizzle"),
    56: ("Freezing drizzle", "drizzle"),
    57: ("Freezing drizzle", "drizzle"),
    61: ("Light rain", "rain"),
    63: ("Rain", "rain"),
    65: ("Heavy rain", "rain"),
    66: ("Freezing rain", "rain"),
    67: ("Freezing rain", "rain"),
    71: ("Light snow", "snow"),
    73: ("Snow", "snow"),
    75: ("Heavy snow", "snow"),
    77: ("Snow", "snow"),
    80: ("Showers", "rain"),
    81: ("Showers", "rain"),
    82: ("Heavy showers", "rain"),
    85: ("Snow showers", "snow"),
    86: ("Snow showers", "snow"),
    95: ("Thunderstorm", "storm"),
    96: ("Thunderstorm", "storm"),
    99: ("Thunderstorm", "storm"),
}


def describe(code: int | None, is_day: bool = True) -> tuple[str, str]:
    text, icon = CODES.get(int(code) if code is not None else -1, ("Unknown", "cloudy"))
    if not is_day and icon in ("clear", "partly"):
        icon += "-night"
    return text, icon


def fetch(
    latitude: float, longitude: float, timezone: str, units: str = "fahrenheit"
) -> dict:
    resp = requests.get(
        URL,
        params={
            "latitude": latitude,
            "longitude": longitude,
            "timezone": timezone,
            "timeformat": "unixtime",
            "temperature_unit": "celsius" if units == "celsius" else "fahrenheit",
            "forecast_days": 3,
            "current": "temperature_2m,weather_code,is_day",
            "hourly": "temperature_2m,weather_code,precipitation_probability,is_day",
            "daily": "temperature_2m_max,temperature_2m_min,sunrise,sunset",
        },
        timeout=15,
    )
    resp.raise_for_status()
    body = resp.json()
    hourly = body["hourly"]
    daily = body["daily"]
    return {
        "units": "C" if units == "celsius" else "F",
        "current": {
            "temp": body["current"]["temperature_2m"],
            "code": body["current"]["weather_code"],
            "is_day": bool(body["current"]["is_day"]),
        },
        "hourly": [
            {
                "time": hourly["time"][i],
                "temp": hourly["temperature_2m"][i],
                "code": hourly["weather_code"][i],
                "precip": hourly["precipitation_probability"][i],
                "is_day": bool(hourly["is_day"][i]),
            }
            for i in range(len(hourly["time"]))
        ],
        "daily": [
            {
                "high": daily["temperature_2m_max"][i],
                "low": daily["temperature_2m_min"][i],
                "sunrise": daily["sunrise"][i],
                "sunset": daily["sunset"][i],
            }
            for i in range(len(daily["sunrise"]))
        ],
    }


def pick_hourly(hourly: list[dict], now_ts: float, step: int, count: int) -> list[dict]:
    """Every `step`-th hour after now, `count` of them."""
    future = [h for h in hourly if h["time"] > now_ts]
    return future[:: max(1, step)][:count]


def _round(value) -> int | None:
    return None if value is None else round(value)


def view(raw: dict | None, now: datetime, step: int = 2, count: int = 5) -> dict | None:
    """Shape the cached forecast for the screen as of `now`."""
    if not raw:
        return None
    tz = now.tzinfo
    current = raw["current"]
    text, icon = describe(current["code"], current["is_day"])
    today = None
    for day in raw["daily"]:
        if datetime.fromtimestamp(day["sunrise"], tz).date() == now.date():
            today = day
            break
    out = {
        "units": raw["units"],
        "temp": _round(current["temp"]),
        "condition": text,
        "icon": icon,
        "hourly": [],
    }
    for hour in pick_hourly(raw["hourly"], now.timestamp(), step, count):
        when = datetime.fromtimestamp(hour["time"], tz)
        out["hourly"].append(
            {
                "time": hour["time"],
                "label": f"{when.hour % 12 or 12} {agenda.ampm(when)}",
                "temp": _round(hour["temp"]),
                "icon": describe(hour["code"], hour["is_day"])[1],
                "precip": hour["precip"],
            }
        )
    if today:
        sunrise = datetime.fromtimestamp(today["sunrise"], tz)
        sunset = datetime.fromtimestamp(today["sunset"], tz)
        out.update(
            {
                "high": _round(today["high"]),
                "low": _round(today["low"]),
                "sunrise": today["sunrise"],
                "sunrise_label": agenda.clock_label(sunrise),
                "sunset": today["sunset"],
                "sunset_label": agenda.clock_label(sunset),
            }
        )
    return out


AIR_URL = "https://air-quality-api.open-meteo.com/v1/air-quality"
AQI_BANDS = [
    (50, "Good"),
    (100, "Moderate"),
    (150, "Sensitive groups"),
    (200, "Unhealthy"),
    (300, "Very unhealthy"),
    (10**6, "Hazardous"),
]


def fetch_air(latitude: float, longitude: float, timezone: str) -> dict:
    resp = requests.get(
        AIR_URL,
        params={
            "latitude": latitude,
            "longitude": longitude,
            "timezone": timezone,
            "current": "us_aqi,pm2_5",
        },
        timeout=15,
    )
    resp.raise_for_status()
    current = resp.json()["current"]
    return {"aqi": current.get("us_aqi"), "pm25": current.get("pm2_5")}


def air_view(raw: dict | None) -> dict | None:
    """US air quality index with its standard category. `level` runs 0 (good) to 5."""
    if not raw or raw.get("aqi") is None:
        return None
    value = round(raw["aqi"])
    for level, (limit, label) in enumerate(AQI_BANDS):
        if value <= limit:
            return {"value": value, "label": label, "level": level}
    return None
