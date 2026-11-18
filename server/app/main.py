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
from google.auth.exceptions import RefreshError

from . import agenda, astro, config, gcal, news, weather
from .google_auth import GoogleAuth
from .jobs import Runner, Skip, Store
from .photos import Library

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s %(levelname)s %(name)s: %(message)s"
)

EXPIRED = "Google sign-in has expired or was revoked. Reconnect on the setup page."


class Core:
    def __init__(self, cfg: dict, data_dir: Path, config_dir: Path | None):
        self.cfg = cfg
        self.tz = ZoneInfo(cfg["timezone"])
        self.data_dir = data_dir
        self.store = Store()
        self.settings = config.Settings(data_dir / "settings.json")
        self.auth = GoogleAuth(data_dir, config_dir)
        self.library = Library(data_dir / "photos")
        self.runner = Runner(
            self.store,
            [
                ("weather", self.job_weather, cfg["weather"]["refresh_minutes"]),
                ("news", self.job_news, cfg["news"]["refresh_minutes"]),
                ("agenda", self.job_agenda, cfg["agenda"]["refresh_minutes"]),
                ("photos", self.job_photos, cfg["photos"]["refresh_minutes"]),
            ],
        )

    # ---- effective settings (setup page overrides the config file) ---------------

    def calendar_ids(self) -> list[str]:
        return self.settings.get("calendar_ids") or self.cfg["agenda"]["calendar_ids"]

    def drive_folder_id(self) -> str:
        return (
            self.settings.get("drive_folder_id")
            or self.cfg["photos"]["drive_folder_id"]
        )

    def redirect_uri(self) -> str:
        return self.cfg["public_url"].rstrip("/") + "/auth/callback"

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

    def job_news(self) -> None:
        self.store.set(
            "news", news.fetch(self.cfg["news"]["feeds"], self.cfg["news"]["max_items"])
        )

    def job_agenda(self) -> None:
        sess = self.auth.session()
        if not sess:
            self.store.drop("agenda")
            raise Skip("Google account not connected")
        try:
            events = gcal.fetch(
                sess, self.calendar_ids(), self.tz, self.cfg["agenda"]["hide_declined"]
            )
        except RefreshError as exc:
            raise RuntimeError(EXPIRED) from exc
        self.store.set("agenda", events)

    def job_photos(self) -> None:
        photos_cfg = self.cfg["photos"]
        self.library.sync_local(photos_cfg["local_dir"])
        folder = self.drive_folder_id()
        sess = self.auth.session()
        if folder and sess:
            try:
                self.library.sync_drive(
                    sess, folder, photos_cfg["master_max_px"], photos_cfg["max_photos"]
                )
            except RefreshError as exc:
                raise RuntimeError(EXPIRED) from exc
        elif not folder:
            self.library.clear_drive()
        self.store.set("photos", self.library.count())
        if not folder and not photos_cfg["local_dir"]:
            raise Skip("No photo folder chosen yet")
        if folder and not sess:
            raise Skip("Google account not connected")

    # ---- views -------------------------------------------------------------------

    def dashboard(self) -> dict:
        now = datetime.now(self.tz)
        events = self.store.get("agenda")
        weather_view = weather.view(
            self.store.get("weather"),
            now,
            self.cfg["weather"]["hourly_step"],
            self.cfg["weather"]["hourly_count"],
        )
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
                else agenda.build(events, now, self.cfg["agenda"]["soon_minutes"])
            ),
            "weather": weather_view,
            "moon": astro.moon(now),
            "news": news.view(self.store.get("news"), now.timestamp()),
            "photos": {"count": self.library.count()},
            "google_connected": self.auth.connected(),
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
            "google_client": self.auth.client_path() is not None,
            "google_connected": self.auth.connected(),
            "google_account": self.settings.get("google_account", ""),
            "photos": self.library.count(),
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
    core = Core(config.load(config_path), data_dir, config_path.parent)

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
    def dashboard() -> dict:
        return core.dashboard()

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
        caption = entry.get("caption", "").encode("ascii", "ignore").decode()
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
    return app
