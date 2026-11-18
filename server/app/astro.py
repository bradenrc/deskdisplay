"""Moon phase from the date alone (mean synodic month; good to about half a day)."""

from __future__ import annotations

import math
from datetime import UTC, datetime

SYNODIC_DAYS = 29.530588853
REFERENCE_NEW_MOON = datetime(2000, 1, 6, 18, 14, tzinfo=UTC)

NAMES = [
    "New moon",
    "Waxing crescent",
    "First quarter",
    "Waxing gibbous",
    "Full moon",
    "Waning gibbous",
    "Last quarter",
    "Waning crescent",
]


def moon(now: datetime) -> dict:
    days = (now.astimezone(UTC) - REFERENCE_NEW_MOON).total_seconds() / 86400
    phase = (days / SYNODIC_DAYS) % 1.0
    illumination = (1 - math.cos(2 * math.pi * phase)) / 2
    index = int((phase * 8) + 0.5) % 8
    return {
        "phase": round(phase, 3),
        "name": NAMES[index],
        "illumination": round(illumination * 100),
    }
