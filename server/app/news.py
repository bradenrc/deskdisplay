"""Headlines from RSS/Atom feeds."""

from __future__ import annotations

import calendar
import html
import logging
import re
import time

import feedparser
from .fetch import safe_get

log = logging.getLogger("deskpal.news")


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


def _link(url: str) -> str:
    """Only plain web links are passed on; the browser page turns them into clickable links."""
    url = (url or "").strip()
    return url if url.lower().startswith(("http://", "https://")) else ""


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
                "link": _link(entry.get("link", "")),
            }
        )
    return items


def mix(per_feed: list[list[dict]], max_items: int = 12) -> list[dict]:
    """Takes headlines from each feed in turn, so one busy feed cannot fill the whole list."""
    seen: set[str] = set()
    mixed: list[dict] = []
    for rank in range(max((len(items) for items in per_feed), default=0)):
        for items in per_feed:
            if rank < len(items) and items[rank]["title"].lower() not in seen:
                seen.add(items[rank]["title"].lower())
                mixed.append(items[rank])
    return mixed[:max_items]


def fetch(feeds: list[dict], max_items: int = 12) -> list[dict]:
    per_feed: list[list[dict]] = []
    for feed in feeds:
        try:
            _, _, body = safe_get(feed["url"], timeout=15)
            per_feed.append(parse(body, feed.get("name")))  # in the feed's own order
        except Exception as exc:
            log.warning("feed %s failed: %s", feed.get("url"), exc)
    if feeds and not per_feed:
        raise RuntimeError("no news feed could be read")
    return mix(per_feed, max_items)


def view(items: list[dict] | None, now_ts: float | None = None) -> list[dict]:
    now_ts = time.time() if now_ts is None else now_ts
    out = []
    for item in items or []:
        age = age_label(now_ts - item["published"]) if item["published"] else ""
        out.append({**item, "age": age, "kind": "news"})
    return out
