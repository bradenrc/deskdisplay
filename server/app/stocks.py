"""Market quotes for the markets page: price, change on the day, and the day's shape.

Quotes come from Yahoo Finance's public chart feed. It needs no account but is unofficial,
so it could change without notice, and quotes can be delayed. This is a glanceable view,
not something to trade on.
"""

from __future__ import annotations

import logging
import time
from urllib.parse import quote

import requests

log = logging.getLogger("deskpal.stocks")

URL = "https://query1.finance.yahoo.com/v8/finance/chart/"
HEADERS = {"User-Agent": "Mozilla/5.0 (compatible; desk-companion/1.0)"}
SPARK_POINTS = 40
CLOSED_REFRESH = 30 * 60  # when every market is shut there is little point asking often


def fetch_one(entry: dict) -> dict:
    symbol = entry["symbol"]
    resp = requests.get(
        URL + quote(symbol, safe=""),
        params={"range": "1d", "interval": "5m"},
        headers=HEADERS,
        timeout=15,
    )
    resp.raise_for_status()
    result = resp.json()["chart"]["result"][0]
    meta = result["meta"]
    closes = [
        c for c in result["indicators"]["quote"][0].get("close") or [] if c is not None
    ]
    session = (meta.get("currentTradingPeriod") or {}).get("regular") or {}
    return {
        "symbol": symbol,
        "name": entry.get("name") or meta.get("shortName") or symbol,
        "price": meta.get("regularMarketPrice"),
        "previous": meta.get("chartPreviousClose") or meta.get("previousClose"),
        "closes": closes,
        "session_start": session.get("start"),
        "session_end": session.get("end"),
    }


def is_open(raw: dict, now: float) -> bool:
    start, end = raw.get("session_start"), raw.get("session_end")
    return bool(start and end and start <= now < end)


class Watchlist:
    def __init__(self, symbols: list[dict]):
        self.symbols = symbols
        self.quotes: list[dict] = []
        self.fetched_at = 0.0

    def refresh(self, now: float | None = None) -> int:
        """Returns how many symbols could not be read. Asks less often when markets are shut."""
        now = time.time() if now is None else now
        anything_open = any(is_open(q, now) for q in self.quotes)
        # after the close, one more fetch captures the final prices; then ease off
        have_close = all(
            self.fetched_at >= (q.get("session_end") or 0) for q in self.quotes
        )
        if (
            self.quotes
            and not anything_open
            and have_close
            and now - self.fetched_at < CLOSED_REFRESH
        ):
            return 0
        fresh, failures = [], 0
        previous = {q["symbol"]: q for q in self.quotes}
        for entry in self.symbols:
            try:
                fresh.append(fetch_one(entry))
            except Exception as exc:
                failures += 1
                log.warning("%s: %s", entry.get("symbol"), type(exc).__name__)
                if (
                    entry.get("symbol") in previous
                ):  # keep the last good quote on screen
                    fresh.append(previous[entry["symbol"]])
        self.quotes = fresh
        self.fetched_at = now
        return failures


def _spark(closes: list[float], previous: float | None) -> tuple[list[int], int | None]:
    """The day's prices squeezed to 0..100, plus where yesterday's close sits on that scale."""
    if len(closes) < 2:
        return [], None
    step = max(
        1, -(-len(closes) // SPARK_POINTS)
    )  # round up, so at most SPARK_POINTS remain
    series = closes[::step]
    if series[-1] != closes[-1]:
        series.append(closes[-1])
    low = min([*series, previous] if previous else series)
    high = max([*series, previous] if previous else series)
    span = (high - low) or 1
    points = [round((c - low) / span * 100) for c in series]
    return points, round((previous - low) / span * 100) if previous else None


def view(watch: Watchlist | None, now: float) -> dict | None:
    if not watch:
        return None
    items = []
    for raw in watch.quotes:
        price, previous = raw.get("price"), raw.get("previous")
        if price is None:
            continue
        change = (price - previous) / previous * 100 if previous else 0.0
        points, baseline = _spark(raw.get("closes", []), previous)
        items.append(
            {
                "symbol": raw["symbol"],
                "name": raw["name"],
                "price": f"{price:,.2f}",
                "change": f"{change:+.2f}%",
                "direction": (
                    "up" if change > 0.005 else "down" if change < -0.005 else "flat"
                ),
                "points": points,
                "baseline": baseline,
            }
        )
    return {"open": any(is_open(q, now) for q in watch.quotes), "items": items}
