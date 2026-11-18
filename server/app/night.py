"""Overnight quiet hours for the screen."""

from __future__ import annotations

from datetime import datetime


def _minutes(text: str) -> int:
    hours, minutes = str(text).split(":")
    return int(hours) * 60 + int(minutes)


def active(now: datetime, cfg: dict) -> bool:
    """True between `start` and `end`, which may span midnight (22:30 to 06:30)."""
    if not cfg.get("enabled"):
        return False
    start, end, current = (
        _minutes(cfg["start"]),
        _minutes(cfg["end"]),
        now.hour * 60 + now.minute,
    )
    if start == end:
        return False
    if start < end:
        return start <= current < end
    return current >= start or current < end
