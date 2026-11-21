"""The start page: its saved layout, the feeds its feed widgets show, and site icons.

Layout, feed lists and bookmarks live in data/start.json, so every browser on the home
network sees the same page.
"""

from __future__ import annotations

import calendar
import io
import json
import logging
import re
import threading
import time
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import urljoin, urlparse

import feedparser
from fastapi import Body, HTTPException, Query, Request
from fastapi.responses import FileResponse, HTMLResponse
from fastapi.staticfiles import StaticFiles
from PIL import Image

from . import fetch
from .news import _clean, _link, age_label

log = logging.getLogger("deskpal.start")

LAYOUTS = (
    "1",
    "2",
    "2-left",
    "2-right",
    "3",
)  # 2-left: narrow left column; 2-right: narrow right
TYPES = (
    "agenda",
    "weather",
    "games",
    "snow",
    "markets",
    "photo",
    "tasks",
    "feeds",
    "links",
)
# Widgets with a "show on the desk display" setting. A feeds widget supplies the display's
# news tile; the others can be switched off there. The rest of the display is not optional.
DESK_TYPES = ("agenda", "games", "snow", "feeds")
MAX_WIDGETS, MAX_FEEDS, MAX_LINKS = 40, 20, 150
FEED_TTL = 600
FEED_ITEMS = 20
DOMAIN = re.compile(r"^(?=.{1,253}$)([a-z0-9]([a-z0-9-]{0,61}[a-z0-9])?\.)+[a-z]{2,}$")


def _feed(name: str, url: str, site: str) -> dict:
    return {"name": name, "url": url, "site": site}


def _site(name: str, url: str) -> dict:
    return {"name": name, "url": url}


DEFAULT = {
    "layout": "3",
    "columns": [
        ["agenda", "links"],
        ["news", "misc"],
        ["weather", "games", "snow", "markets", "photo"],
    ],
    "widgets": {
        "agenda": {"type": "agenda", "title": "Agenda"},
        "weather": {"type": "weather", "title": "Weather"},
        "games": {"type": "games", "title": "Games"},
        "snow": {"type": "snow", "title": "Snow, 48 hours"},
        "markets": {"type": "markets", "title": "Markets"},
        "photo": {"type": "photo", "title": "Photo"},
        "news": {
            "type": "feeds",
            "title": "News",
            "desk": True,
            "feeds": [
                _feed("Top stories", "https://news.google.com/rss", "news.google.com"),
                _feed("FOX", "http://feeds.foxnews.com/foxnews/latest", "foxnews.com"),
                _feed(
                    "NY Times",
                    "https://rss.nytimes.com/services/xml/rss/nyt/HomePage.xml",
                    "nytimes.com",
                ),
                _feed(
                    "DR",
                    "http://feeds.feedburner.com/DrudgeReportFeed",
                    "drudgereport.com",
                ),
                _feed(
                    "Politico", "https://rss.politico.com/magazine.xml", "politico.com"
                ),
            ],
        },
        "misc": {
            "type": "feeds",
            "title": "Misc",
            "feeds": [
                _feed("TC", "https://techcrunch.com/feed/", "techcrunch.com"),
                _feed("GC", "https://golf.com/feed/", "golf.com"),
                _feed("Giz", "https://gizmodo.com/rss", "gizmodo.com"),
                _feed("Engadget", "https://www.engadget.com/rss.xml", "engadget.com"),
                _feed(
                    "PB",
                    "https://www.pinkbike.com/pinkbike_xml_feed.php",
                    "pinkbike.com",
                ),
            ],
        },
        "links": {
            "type": "links",
            "title": "Links",
            "links": [
                _site("TechCrunch", "https://techcrunch.com/"),
                _site("Amazon", "https://www.amazon.com/"),
                _site("News", "https://news.google.com/"),
                _site("Maps", "https://maps.google.com/"),
                _site("Wikipedia", "https://www.wikipedia.org/"),
                _site("eBay", "https://www.ebay.com/"),
                _site("YouTube", "https://www.youtube.com/"),
                _site("Facebook", "https://www.facebook.com/"),
                _site("Zappos", "https://www.zappos.com/"),
                _site("LinkedIn", "https://www.linkedin.com/"),
                _site("Outlook", "https://outlook.office.com/"),
                _site("Google Calendar", "https://calendar.google.com/"),
                _site("AWS", "https://console.aws.amazon.com/"),
                _site("Google Drive", "https://drive.google.com/"),
                _site("Google Photos", "https://photos.google.com/"),
                _site("golfwrx", "https://www.golfwrx.com/"),
                _site("MyGolfSpy", "https://mygolfspy.com/"),
                _site("Lucid Chart", "https://lucid.app/"),
                _site("Bronco6G", "https://www.bronco6g.com/"),
                _site("Bronco Nation", "https://thebronconation.com/"),
                _site("ChatGPT", "https://chatgpt.com/"),
            ],
        },
    },
}


class Invalid(ValueError):
    """The page layout sent by the browser is not usable."""


def _text(value, limit: int, what: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise Invalid(f"{what} is missing")
    return value.strip()[:limit]


def _web(value, what: str) -> str:
    url = _text(value, 600, what)
    if not _link(url):
        raise Invalid(f"{what} must start with http:// or https://")
    return url


def clean(board) -> dict:
    """Checks a layout from the browser and returns only the parts this server understands."""
    if not isinstance(board, dict) or not isinstance(board.get("widgets"), dict):
        raise Invalid("Not a page layout")
    if board.get("layout") not in LAYOUTS:
        raise Invalid("Unknown column layout")
    if len(board["widgets"]) > MAX_WIDGETS:
        raise Invalid("Too many widgets")
    widgets: dict[str, dict] = {}
    for wid, raw in board["widgets"].items():
        if not re.fullmatch(r"[a-z0-9-]{1,40}", str(wid)) or not isinstance(raw, dict):
            raise Invalid("Bad widget")
        if raw.get("type") not in TYPES:
            raise Invalid("Unknown kind of widget")
        widget = {"type": raw["type"], "title": _text(raw.get("title"), 60, "A title")}
        if widget["type"] in DESK_TYPES:
            # Unset means: a panel stays on the display, a feeds widget stays off it.
            widget["desk"] = bool(raw.get("desk", widget["type"] != "feeds"))
        if widget["type"] == "feeds":
            feeds = raw.get("feeds") or []
            if not isinstance(feeds, list) or len(feeds) > MAX_FEEDS:
                raise Invalid("Too many feeds in one widget")
            widget["feeds"] = [
                {
                    "name": _text(f.get("name"), 40, "A feed name"),
                    "url": _web(f.get("url"), "A feed address"),
                    "site": (
                        str(f.get("site") or "").lower()
                        if DOMAIN.match(str(f.get("site") or "").lower())
                        else ""
                    ),
                }
                for f in feeds
                if isinstance(f, dict)
            ]
        if widget["type"] == "links":
            links = raw.get("links") or []
            if not isinstance(links, list) or len(links) > MAX_LINKS:
                raise Invalid("Too many links in one widget")
            widget["links"] = [
                {
                    "name": _text(link.get("name"), 40, "A link name"),
                    "url": _web(link.get("url"), "A link address"),
                }
                for link in links
                if isinstance(link, dict)
            ]
        widgets[wid] = widget
    # A page saved before the desk setting existed: its first feeds widget keeps the display's
    # news tile supplied.
    feed_widgets = [w for w in board["widgets"].values() if w.get("type") == "feeds"]
    if feed_widgets and not any("desk" in w for w in feed_widgets):
        next(w for w in widgets.values() if w["type"] == "feeds")["desk"] = True
    columns: list[list[str]] = [[], [], []]
    placed: set[str] = set()
    raw_columns = board.get("columns")
    if isinstance(raw_columns, list):
        for index, column in enumerate(raw_columns[:3]):
            for wid in column if isinstance(column, list) else []:
                if wid in widgets and wid not in placed:
                    columns[index].append(wid)
                    placed.add(wid)
    columns[0].extend(wid for wid in widgets if wid not in placed)
    return {"layout": board["layout"], "columns": columns, "widgets": widgets}


class Board:
    def __init__(self, path: Path):
        self.path = path
        self._lock = threading.Lock()

    def get(self) -> dict:
        with self._lock:
            try:
                return clean(json.loads(self.path.read_text()))
            except (OSError, ValueError):
                return clean(DEFAULT)

    def save(self, board) -> dict:
        board = clean(board)
        with self._lock:
            tmp = self.path.with_suffix(".tmp")
            tmp.write_text(json.dumps(board, indent=1))
            tmp.replace(self.path)
        return board

    def desk_feeds(self) -> list[dict]:
        """The feeds behind the desk display's news tile."""
        return [
            feed
            for widget in self.get()["widgets"].values()
            if widget["type"] == "feeds" and widget["desk"]
            for feed in widget["feeds"]
        ]

    def desk_hidden(self) -> set[str]:
        """Panels switched off for the desk display."""
        return {
            w["type"]
            for w in self.get()["widgets"].values()
            if w["type"] != "feeds" and w.get("desk") is False
        }

    def feed_urls(self) -> set[str]:
        return {
            feed["url"]
            for widget in self.get()["widgets"].values()
            for feed in widget.get("feeds", [])
        }


# ---- feeds -----------------------------------------------------------------------


def _image(entry) -> str:
    for key in ("media_thumbnail", "media_content"):
        for media in entry.get(key) or []:
            kind = media.get("type", "") or media.get("medium", "")
            if media.get("url") and (
                key == "media_thumbnail" or "image" in kind or not kind
            ):
                return _link(media["url"])
    for item in entry.get("enclosures") or entry.get("links") or []:
        if str(item.get("type", "")).startswith("image/") and item.get("href"):
            return _link(item["href"])
    html = entry.get("summary", "") + "".join(
        c.get("value", "") for c in entry.get("content") or []
    )
    found = re.search(r'<img[^>]+src=["\']([^"\']+)', html)
    return _link(found.group(1).replace("&amp;", "&")) if found else ""


def _summary(entry, title: str) -> str:
    """Some feeds (Google News) only repeat the headline as the summary; show nothing then."""
    text = _clean(entry.get("summary", ""))
    return "" if text.startswith(title[:40]) else text[:300]


def parse_feed(content: bytes) -> dict:
    feed = feedparser.parse(content)
    items = []
    for entry in feed.entries[:FEED_ITEMS]:
        title = _clean(entry.get("title", ""))
        if not title:
            continue
        stamp = entry.get("published_parsed") or entry.get("updated_parsed")
        items.append(
            {
                "title": title,
                "link": _link(entry.get("link", "")),
                "summary": _summary(entry, title),
                "image": _image(entry),
                "published": calendar.timegm(stamp) if stamp else None,
            }
        )
    site = urlparse(feed.feed.get("link", "")).hostname or ""
    return {
        "title": _clean(feed.feed.get("title", "")),
        "site": site.removeprefix("www."),
        "items": items,
    }


class Feeds:
    """Reads feeds on request and remembers each for ten minutes."""

    def __init__(self):
        self._lock = threading.Lock()
        self._cache: dict[str, tuple[float, dict]] = {}

    def read(self, url: str) -> dict:
        now = time.time()
        with self._lock:
            kept = self._cache.get(url)
        if not kept or now - kept[0] > FEED_TTL:
            try:
                _, _, body = fetch.safe_get(url)
                fresh = parse_feed(body)
                if not fresh["items"]:
                    raise ValueError("no items in the feed")
                kept = (now, fresh)
                with self._lock:
                    self._cache[url] = kept
            except Exception as exc:
                log.warning("feed %s failed: %s", urlparse(url).hostname, exc)
                if not kept:  # nothing older to fall back on
                    raise
        items = [
            {**i, "age": age_label(now - i["published"]) if i["published"] else ""}
            for i in kept[1]["items"]
        ]
        return {**kept[1], "items": items, "stale": now - kept[0] > FEED_TTL}


class _FeedLinks(HTMLParser):
    """Collects <link rel="alternate" type="application/rss+xml" href="..."> from a page."""

    def __init__(self):
        super().__init__()
        self.found: list[str] = []

    def handle_starttag(self, tag, attrs):
        if tag != "link":
            return
        a = {k: (v or "") for k, v in attrs}
        if "alternate" in a.get("rel", "").lower() and re.search(
            r"rss|atom", a.get("type", ""), re.IGNORECASE
        ):
            if a.get("href"):
                self.found.append(a["href"])


def feed_links(html: str, base: str) -> list[str]:
    parser = _FeedLinks()
    try:
        parser.feed(html)
    except Exception:  # broken markup: use whatever was found before it broke
        pass
    return [urljoin(base, href) for href in parser.found]


def find_feed(address: str) -> dict:
    """A site or feed address typed by the user -> {url, name, site} of a working feed."""
    address = address.strip()
    if not re.match(r"^https?://", address, re.IGNORECASE):
        address = "https://" + address
    final, _, body = fetch.safe_get(address)

    def result(url: str, parsed: dict) -> dict:
        host = (urlparse(url).hostname or "").removeprefix("www.")
        return {
            "url": url,
            "name": (parsed["title"] or host)[:40],
            "site": parsed["site"] or host,
        }

    parsed = parse_feed(body)
    if parsed["items"]:
        return result(final, parsed)
    guesses = feed_links(body.decode("utf-8", "replace"), final)
    guesses += [urljoin(final, p) for p in ("/feed/", "/rss", "/rss.xml", "/feed.xml")]
    for guess in guesses[:8]:
        try:
            url, _, body = fetch.safe_get(guess, timeout=6)
            parsed = parse_feed(body)
            if parsed["items"]:
                return result(url, parsed)
        except Exception:
            continue
    raise LookupError("No feed found at that address")


# ---- site icons ------------------------------------------------------------------


class _IconLinks(HTMLParser):
    def __init__(self):
        super().__init__()
        self.found: list[tuple[int, str]] = []

    def handle_starttag(self, tag, attrs):
        if tag != "link":
            return
        a = {k: (v or "") for k, v in attrs}
        rel = a.get("rel", "").lower()
        if "icon" not in rel or not a.get("href") or a["href"].startswith("data:"):
            return
        size = re.match(r"(\d+)", a.get("sizes", ""))
        pixels = int(size.group(1)) if size else (180 if "apple" in rel else 32)
        self.found.append((pixels, a["href"]))


def icon_links(html: str, base: str) -> list[str]:
    """Icon addresses named by a page, the ones nearest 64 pixels or larger first."""
    parser = _IconLinks()
    try:
        parser.feed(html)
    except Exception:
        pass
    ranked = sorted(parser.found, key=lambda f: (f[0] < 64, abs(f[0] - 64)))
    return [urljoin(base, href) for _, href in ranked]


def to_png(data: bytes, size: int = 64) -> bytes:
    image = Image.open(io.BytesIO(data))
    image = image.convert("RGBA")
    if max(image.size) > size:
        image.thumbnail((size, size), Image.LANCZOS)
    out = io.BytesIO()
    image.save(out, "PNG")
    return out.getvalue()


class Icons:
    """One small picture per site, fetched once and kept in data/icons."""

    RETRY_AFTER = 86400  # a site with no usable icon is asked again after a day

    def __init__(self, folder: Path):
        self.folder = folder
        folder.mkdir(parents=True, exist_ok=True)

    def _fetch(self, domain: str) -> bytes | None:
        candidates: list[str] = []
        try:
            final, _, body = fetch.safe_get(f"https://{domain}/", timeout=6)
            candidates = icon_links(body.decode("utf-8", "replace"), final)
            candidates.append(urljoin(final, "/favicon.ico"))
        except Exception:
            pass
        candidates.append(f"https://{domain}/favicon.ico")
        for url in dict.fromkeys(candidates):
            try:
                _, _, data = fetch.safe_get(url, max_bytes=1_000_000, timeout=6)
                return to_png(data)
            except Exception:
                continue
        return None

    def get(self, domain: str) -> Path | None:
        domain = domain.lower().removeprefix("www.")
        if not DOMAIN.match(domain):
            return None
        path = self.folder / f"{domain}.png"
        if path.exists():
            return path
        missing = self.folder / f"{domain}.none"
        if (
            missing.exists()
            and time.time() - missing.stat().st_mtime < self.RETRY_AFTER
        ):
            return None
        # feeds.example.com often has no icon of its own; example.com does.
        labels = domain.split(".")
        for start in range(max(1, len(labels) - 1)):
            data = self._fetch(".".join(labels[start:]))
            if data:
                path.write_bytes(data)
                return path
        missing.touch()
        return None


# ---- routes ----------------------------------------------------------------------


# The desk display's screens, in the order they sit side by side.
SCREENS = ("dashboard", "tasks", "markets", "photo", "aquarium")
ROTATE_MIN, ROTATE_MAX = 10, 3600


def clean_rotation(raw) -> dict:
    """Which screens the desk display moves through by itself, and how long each stays up.
    seconds 0 means it stays where it is put."""
    raw = raw if isinstance(raw, dict) else {}
    try:
        seconds = int(raw.get("seconds") or 0)
    except (TypeError, ValueError):
        seconds = 0
    if seconds:
        seconds = max(ROTATE_MIN, min(ROTATE_MAX, seconds))
    wanted = raw.get("screens") if isinstance(raw.get("screens"), list) else []
    screens = [name for name in SCREENS if name in wanted]
    return {"seconds": seconds, "screens": screens or ["dashboard"]}


def register(app, core, web_dir: Path) -> None:
    from .web import _same_site

    board = core.board
    feeds = Feeds()
    icons = Icons(core.data_dir / "icons")

    if (web_dir / "assets").is_dir():
        app.mount("/assets", StaticFiles(directory=web_dir / "assets"), name="assets")

    @app.get("/", response_class=HTMLResponse, include_in_schema=False)
    def home():
        index = web_dir / "index.html"
        if index.exists():
            return FileResponse(index, headers={"Cache-Control": "no-cache"})
        return HTMLResponse(
            "<!doctype html><title>Desk companion</title><p>The start page has not been built."
            " Run <code>npm install && npm run build</code> in <code>server/web</code>, or use"
            ' the Docker image.</p><p><a href="/setup">Setup</a></p>'
        )

    @app.get("/api/start")
    def get_start() -> dict:
        return board.get()

    @app.get("/api/display")
    def get_display() -> dict:
        return clean_rotation(core.settings.get("rotation"))

    @app.put("/api/display")
    def put_display(request: Request, body: dict = Body(...)) -> dict:
        _same_site(request)
        rotation = clean_rotation(body)
        core.settings.set("rotation", rotation)
        return rotation

    @app.put("/api/start")
    def put_start(request: Request, layout: dict = Body(...)) -> dict:
        _same_site(request)
        try:
            saved = board.save(layout)
        except Invalid as exc:
            raise HTTPException(400, str(exc)) from exc
        core.runner.trigger("news")  # the display's headlines may have a new source
        return saved

    @app.get("/api/feed")
    def read_feed(url: str = Query(..., max_length=600)) -> dict:
        if url not in board.feed_urls():  # only feeds saved on the page are fetched
            raise HTTPException(403, "That feed is not on the start page")
        try:
            return feeds.read(url)
        except Exception as exc:
            raise HTTPException(502, "That feed could not be read") from exc

    @app.post("/api/feed/find")
    def find(
        request: Request, address: str = Body(..., embed=True, max_length=600)
    ) -> dict:
        _same_site(request)
        try:
            return find_feed(address)
        except LookupError as exc:
            raise HTTPException(404, str(exc)) from exc
        except fetch.Refused as exc:
            raise HTTPException(400, str(exc)) from exc
        except Exception as exc:
            raise HTTPException(502, "That address could not be reached") from exc

    @app.get("/api/favicon")
    def favicon(domain: str = Query(..., max_length=253)):
        path = icons.get(domain)
        if not path:
            raise HTTPException(
                404, "No icon", headers={"Cache-Control": "max-age=3600"}
            )
        return FileResponse(
            path, media_type="image/png", headers={"Cache-Control": "max-age=604800"}
        )
