"""Desk companion server: one small API for the screen, plus a setup page."""

from __future__ import annotations

import logging
import os
import time
from contextlib import asynccontextmanager
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo

from fastapi import FastAPI, HTTPException, Query
from fastapi.responses import FileResponse

from . import (
    agenda,
    astro,
    config,
    gtasks,
    ical,
    news,
    night,
    snow,
    sports,
    start,
    stocks,
    weather,
)
from .jobs import Runner, Skip, Store
from .photos import Library
from .rclone import Rclone
from .travel import Travel

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s %(levelname)s %(name)s: %(message)s"
)


class Core:
    def __init__(self, cfg: dict, data_dir: Path):
        self.cfg = cfg
        self.tz = ZoneInfo(cfg["timezone"])
        self.data_dir = data_dir
        self.store = Store()
        self.settings = config.Settings(data_dir / "settings.json")
        self.board = start.Board(data_dir / "start.json")
        self.library = Library(data_dir / "photos")
        self.tasks = gtasks.Account(data_dir / "google_tasks.json")
        loc = cfg["location"]
        self.travel = (
            Travel(
                data_dir / "travel_cache.json",
                (loc["latitude"], loc["longitude"]),
                cfg["travel"]["max_miles"],
            )
            if cfg["travel"]["enabled"]
            else None
        )
        teams = cfg["sports"]["teams"]
        self.sports = (
            sports.Tracker(
                teams,
                cfg["sports"]["schedule_minutes"],
                cfg["sports"]["preview_days"],
                cfg["sports"]["final_hours"],
            )
            if teams
            else None
        )
        self.stocks = (
            stocks.Watchlist(cfg["stocks"]["symbols"])
            if cfg["stocks"]["symbols"]
            else None
        )
        self.runner = Runner(
            self.store,
            [
                ("weather", self.job_weather, cfg["weather"]["refresh_minutes"]),
                ("news", self.job_news, cfg["news"]["refresh_minutes"]),
                ("agenda", self.job_agenda, cfg["agenda"]["refresh_minutes"]),
                ("photos", self.job_photos, cfg["photos"]["refresh_minutes"]),
                ("tasks", self.job_tasks, cfg["tasks"]["refresh_minutes"]),
                *(
                    [("ski", self.job_ski, cfg["ski"]["refresh_minutes"])]
                    if cfg["ski"]["resorts"]
                    else []
                ),
                *(
                    [("stocks", self.job_stocks, cfg["stocks"]["refresh_minutes"])]
                    if cfg["stocks"]["symbols"]
                    else []
                ),
                # checked every minute so a live score stays current; quiet otherwise
                *([("sports", self.job_sports, 1)] if cfg["sports"]["teams"] else []),
            ],
        )
        self.drive = Rclone(
            data_dir, on_connected=lambda: self.runner.trigger("photos")
        )

    # ---- effective settings (setup page overrides the config file) ---------------

    def ical_urls(self) -> list[str]:
        return self.settings.get("ical_urls") or self.cfg["agenda"]["ical_urls"]

    def drive_folder_id(self) -> str:
        return (
            self.settings.get("drive_folder_id")
            or self.cfg["photos"]["drive_folder_id"]
        )

    # ---- jobs --------------------------------------------------------------------

    def job_weather(self) -> None:
        loc = self.cfg["location"]
        self.store.set(
            "weather",
            weather.fetch(
                loc["latitude"],
                loc["longitude"],
                self.cfg["timezone"],
                self.cfg["units"],
            ),
        )
        try:  # air quality is a bonus: never let it take the weather down with it
            self.store.set(
                "air",
                weather.fetch_air(
                    loc["latitude"], loc["longitude"], self.cfg["timezone"]
                ),
            )
        except Exception as exc:
            self.store.note("air", "error", str(exc)[:200])

    def job_stocks(self) -> None:
        failures = self.stocks.refresh()
        self.store.set("stocks", True)
        if failures:
            raise RuntimeError(f"{failures} symbol(s) could not be read")

    def job_sports(self) -> None:
        failures = self.sports.refresh(time.time())
        self.store.set("sports", True)
        if failures:
            raise RuntimeError(f"{failures} team(s) could not be read")

    def job_ski(self) -> None:
        reports, failures = [], 0
        for resort in self.cfg["ski"]["resorts"]:
            try:
                reports.append(
                    snow.fetch(resort, self.cfg["timezone"], self.cfg["units"])
                )
            except Exception:
                failures += 1
        if reports:
            self.store.set("ski", reports)
        if failures:
            raise RuntimeError(f"{failures} ski area(s) could not be read")

    def job_news(self) -> None:
        feeds = self.board.desk_feeds()
        if not feeds:
            self.store.set("news", [])
            raise Skip(
                "No feed widget on the start page is set to show on the desk display"
            )
        self.store.set("news", news.fetch(feeds, self.cfg["news"]["max_items"]))

    def job_agenda(self) -> None:
        urls = self.ical_urls()
        if not urls:
            self.store.drop("agenda")
            raise Skip("No calendar link added yet")
        events = ical.fetch(urls, self.tz)
        if self.travel:
            self.travel.annotate(events)
        self.store.set("agenda", events)

    def job_tasks(self) -> None:
        if not self.tasks.connected():
            self.store.drop("tasks")
            raise Skip("Google Tasks not connected")
        lists, problems = self.tasks.fetch(self.store.get("tasks"))
        if self.tasks.connected():
            self.store.set("tasks", lists)
        else:  # Google ended the only sign-in: show nothing stale
            self.store.drop("tasks")
        if problems:
            raise RuntimeError("; ".join(problems))

    def job_photos(self) -> None:
        photos_cfg = self.cfg["photos"]
        self.library.sync_local(photos_cfg["local_dir"])
        folder = self.drive_folder_id()
        connected = self.drive.connected()
        if not folder:
            self.library.clear_pool()
        elif connected:
            self.library.sync_pool(
                self.drive,
                folder,
                pool_size=photos_cfg["pool_size"],
                daily_swap=photos_cfg["daily_swap"],
                index_hours=photos_cfg["index_hours"],
                master_max=photos_cfg["master_max_px"],
                progress=lambda text: self.store.note("photos", "working", text),
            )
        self.store.set("photos", self.library.count())
        if folder and not connected:
            raise Skip("Google Drive not connected")
        if not folder and not photos_cfg["local_dir"]:
            raise Skip("No photo folder chosen yet")

    # ---- views -------------------------------------------------------------------

    def dashboard(self, everything: bool = False) -> dict:
        """What the desk display shows. Panels switched off on the start page are left out,
        unless `everything` is set (the start page itself asks for it all)."""
        now = datetime.now(self.tz)
        hidden = set() if everything else self.board.desk_hidden()
        events = None if "agenda" in hidden else self.store.get("agenda")
        forecast = weather.view(
            self.store.get("weather"),
            now,
            self.cfg["weather"]["hourly_step"],
            self.cfg["weather"]["hourly_count"],
        )
        if forecast is not None:
            forecast["air"] = weather.air_view(self.store.get("air"))
        return {
            "time": {
                "epoch": int(now.timestamp()),
                "iso": now.isoformat(timespec="seconds"),
                "timezone": self.cfg["timezone"],
                "utc_offset_seconds": int(now.utcoffset().total_seconds()),
            },
            "agenda": (
                None
                if events is None
                else agenda.build(
                    events,
                    now,
                    self.cfg["agenda"]["soon_minutes"],
                    travel=self.cfg["travel"] if self.travel else None,
                )
            ),
            "weather": forecast,
            "moon": astro.moon(now),
            "news": news.view(self.store.get("news"), now.timestamp()),
            "games": [] if "games" in hidden else sports.chips(self.sports, now),
            "stocks": stocks.view(self.stocks, now.timestamp()),
            "display": {"clock": str(self.cfg["display"]["clock_color"]).lower()},
            "snow": (
                []
                if "snow" in hidden
                else snow.reports(
                    self.store.get("ski") or [],
                    now.timestamp(),
                    self.cfg["ski"]["min_new"],
                )
            ),
            "desk": {name: name not in hidden for name in ("agenda", "games", "snow")},
            "tasks": gtasks.view(
                self.store.get("tasks"),
                now.date(),
                200 if everything else self.cfg["tasks"]["display_max"],
                full=everything,
            ),
            "night": night.active(now, self.cfg["night"]),
            "photos": {"count": self.library.count()},
        }

    def status(self) -> dict:
        now = time.time()
        sources = {}
        for name, meta in self.store.meta().items():
            updated = meta.get("updated")
            sources[name] = {
                "state": meta["state"],
                "detail": meta["detail"],
                "age_seconds": None if updated is None else int(now - updated),
            }
        return {
            "calendar_links": len(self.ical_urls()),
            "drive_connected": self.drive.connected(),
            "drive_folder": bool(self.drive_folder_id()),
            "tasks_connected": self.tasks.connected(),
            "tasks_accounts": len(self.tasks.accounts()),
            "photos": self.library.count(),
            "photos_in_drive_folder": self.library.catalog_size,
            "sources": sources,
        }


def create_app(
    data_dir: str | Path | None = None,
    config_path: str | Path | None = None,
    start_jobs: bool = True,
) -> FastAPI:
    data_dir = Path(data_dir or os.environ.get("DATA_DIR", "/data"))
    config_path = Path(
        config_path or os.environ.get("CONFIG_PATH", "/config/config.yaml")
    )
    data_dir.mkdir(parents=True, exist_ok=True)
    core = Core(config.load(config_path), data_dir)

    @asynccontextmanager
    async def lifespan(app: FastAPI):
        if start_jobs:
            core.runner.start()
        yield
        core.runner.stop()

    app = FastAPI(title="Desk companion server", lifespan=lifespan)
    app.state.core = core

    @app.get("/healthz")
    def healthz() -> dict:
        return {"ok": True}

    @app.get("/api/status")
    def status() -> dict:
        return core.status()

    @app.get("/api/dashboard")
    def dashboard(view: str = "") -> dict:
        return core.dashboard(everything=view == "web")

    @app.get("/api/photos")
    def photos() -> dict:
        entries = core.library.entries()
        return {"count": len(entries), "photos": entries}

    def _photo(pid: str | None, w: int, h: int, fit: str | None) -> FileResponse:
        if not pid:
            raise HTTPException(404, "No photos yet")
        path = core.library.render(pid, w, h, fit or core.cfg["photos"]["fit"])
        if not path:
            raise HTTPException(404, "Photo not found")
        entry = core.library.get(pid) or {}
        caption = (
            entry.get("caption", "")
            .replace("·", "-")
            .encode("ascii", "ignore")
            .decode()
        )
        return FileResponse(
            path,
            media_type="image/jpeg",
            headers={
                "Cache-Control": "no-store",
                "X-Photo-Id": pid,
                "X-Photo-Caption": caption,
            },
        )

    size = dict(ge=64, le=4096)

    @app.get("/api/photo/next")
    def photo_next(
        w: int = Query(800, **size), h: int = Query(480, **size), fit: str | None = None
    ) -> FileResponse:
        return _photo(core.library.next_id(), w, h, fit)

    @app.get("/api/photo/{pid}")
    def photo(
        pid: str,
        w: int = Query(800, **size),
        h: int = Query(480, **size),
        fit: str | None = None,
    ) -> FileResponse:
        return _photo(pid, w, h, fit)

    from .web import register

    register(app, core)
    default_web = Path(__file__).resolve().parent.parent / "web" / "dist"
    start.register(app, core, Path(os.environ.get("WEB_DIR", default_web)))
    return app
