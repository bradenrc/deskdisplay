"""Headlines from RSS/Atom feeds."""

from __future__ import annotations

import calendar
import html
import logging
import re
import time

import feedparser
import requests

log = logging.getLogger("deskpal.news")
HEADERS = {"User-Agent": "desk-companion/1.0 (RSS reader)"}


def age_label(seconds: float) -> str:
    minutes = int(seconds // 60)
    if minutes < 1:
        return "just now"
    if minutes < 60:
        return f"{minutes} min ago"
    hours = minutes // 60
    if hours < 24:
        return f"{hours} hr ago"
    days = hours // 24
    return f"{days} day ago" if days == 1 else f"{days} days ago"


def _clean(text: str) -> str:
    return re.sub(
        r"\s+", " ", html.unescape(re.sub(r"<[^>]+>", "", text or ""))
    ).strip()


def parse(content: bytes, name: str | None = None) -> list[dict]:
    feed = feedparser.parse(content)
    source = name or _clean(feed.feed.get("title", "")) or "News"
    items = []
    for entry in feed.entries:
        title = _clean(entry.get("title", ""))
        if not title:
            continue
        stamp = entry.get("published_parsed") or entry.get("updated_parsed")
        items.append(
            {
                "title": title,
                "source": source,
                "published": calendar.timegm(stamp) if stamp else None,
                "summary": _clean(entry.get("summary", ""))[:280],
            }
        )
    return items


def fetch(feeds: list[dict], max_items: int = 12) -> list[dict]:
    items: list[dict] = []
    failures = 0
    for feed in feeds:
        try:
            resp = requests.get(feed["url"], headers=HEADERS, timeout=15)
            resp.raise_for_status()
            items.extend(parse(resp.content, feed.get("name")))
        except Exception as exc:
            failures += 1
            log.warning("feed %s failed: %s", feed.get("url"), exc)
    if feeds and failures == len(feeds):
        raise RuntimeError("no news feed could be read")
    seen: set[str] = set()
    unique = []
    for item in sorted(items, key=lambda i: i["published"] or 0, reverse=True):
        key = item["title"].lower()
        if key not in seen:
            seen.add(key)
            unique.append(item)
    return unique[:max_items]


def view(items: list[dict] | None, now_ts: float | None = None) -> list[dict]:
    now_ts = time.time() if now_ts is None else now_ts
    out = []
    for item in items or []:
        age = age_label(now_ts - item["published"]) if item["published"] else ""
        out.append({**item, "age": age})
    return out
