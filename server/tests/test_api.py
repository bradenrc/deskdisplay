import json

import pytest
from app.main import create_app
from fastapi.testclient import TestClient
from PIL import Image


@pytest.fixture
def setup(tmp_path):
    photos = tmp_path / "pics" / "Lake trip"
    photos.mkdir(parents=True)
    Image.new("RGB", (3000, 2000), (200, 120, 40)).save(photos / "a.jpg")
    Image.new("RGB", (1000, 1500), (40, 120, 200)).save(photos / "b.png")
    (photos / "notes.txt").write_text("not a photo")
    config = tmp_path / "config.yaml"
    config.write_text(f"photos:\n  local_dir: {tmp_path / 'pics'}\n")
    app = create_app(data_dir=tmp_path / "data", config_path=config, start_jobs=False)
    return app, TestClient(app, follow_redirects=False), tmp_path


def test_dashboard_before_any_data(setup):
    _, client, _ = setup
    body = client.get("/api/dashboard").json()
    assert body["agenda"] is None and body["weather"] is None and body["news"] == []
    assert body["google_connected"] is False
    assert body["moon"]["name"] and body["time"]["epoch"] > 0
    assert client.get("/healthz").json() == {"ok": True}


def test_dashboard_with_cached_data(setup):
    app, client, _ = setup
    core = app.state.core
    core.store.set("agenda", [])
    core.store.set(
        "news", [{"title": "T", "source": "S", "published": None, "summary": ""}]
    )
    body = client.get("/api/dashboard").json()
    assert body["agenda"]["events"] == [] and body["news"][0]["age"] == ""


def test_local_photos_render_baseline_jpeg(setup):
    app, client, _ = setup
    core = app.state.core
    assert client.get("/api/photo/next").status_code == 404
    core.runner.run_once("photos", core.job_photos)
    assert client.get("/api/photos").json()["count"] == 2
    seen = set()
    for _ in range(2):
        resp = client.get("/api/photo/next?w=1024&h=600")
        assert resp.status_code == 200 and resp.headers["content-type"] == "image/jpeg"
        assert resp.headers["x-photo-caption"] == "Lake trip"
        seen.add(resp.headers["x-photo-id"])
        path = core.library.render(resp.headers["x-photo-id"], 1024, 600)
        with Image.open(path) as image:
            assert image.size == (1024, 600)
            assert not image.info.get("progressive")
    assert len(seen) == 2  # no repeats until every photo has been shown
    assert client.get("/api/photo/nope").status_code == 404
    assert client.get("/api/photo/next?w=10&h=10").status_code == 422


def test_photos_removed_when_file_deleted(setup):
    app, _, tmp_path = setup
    core = app.state.core
    core.runner.run_once("photos", core.job_photos)
    (tmp_path / "pics" / "Lake trip" / "a.jpg").unlink()
    core.runner.run_once("photos", core.job_photos)
    assert core.library.count() == 1


def test_setup_page_and_forms(setup):
    app, client, tmp_path = setup
    page = client.get("/setup")
    assert page.status_code == 200 and "Google app credentials" in page.text
    assert "http://localhost:8080/auth/callback" in page.text

    bad = client.post("/setup/client", data={"client_json": "{}"})
    assert bad.status_code == 303 and "err=" in bad.headers["location"]
    assert client.get("/auth/start").headers["location"].startswith("/setup?err=")

    client_json = json.dumps(
        {
            "web": {
                "client_id": "id.apps.googleusercontent.com",
                "client_secret": "secret",
                "auth_uri": "https://accounts.google.com/o/oauth2/auth",
                "token_uri": "https://oauth2.googleapis.com/token",
            }
        }
    )
    ok = client.post("/setup/client", data={"client_json": client_json})
    assert "msg=" in ok.headers["location"]
    start = client.get("/auth/start").headers["location"]
    assert start.startswith("https://accounts.google.com/")
    assert "redirect_uri=http%3A%2F%2Flocalhost%3A8080%2Fauth%2Fcallback" in start
    assert "calendar.readonly" in start and "drive.readonly" in start
    assert "access_type=offline" in start

    expired = client.post(
        "/auth/paste",
        data={"url": "http://localhost:8080/auth/callback?state=x&code=y"},
    )
    assert "expired" in expired.headers["location"]
    assert (
        "err=" in client.get("/auth/callback?error=access_denied").headers["location"]
    )

    fid = "1AbCdEfGhIjKlMnOpQrStUvWxYz012345"
    client.post(
        "/setup/photos",
        data={"folder": f"https://drive.google.com/drive/folders/{fid}"},
    )
    assert app.state.core.drive_folder_id() == fid
    assert (
        "err="
        in client.post("/setup/photos", data={"folder": "nonsense"}).headers["location"]
    )

    client.post(
        "/setup/calendars", data={"calendar_ids": ["a@example.com", "b@example.com"]}
    )
    assert app.state.core.calendar_ids() == ["a@example.com", "b@example.com"]

    blocked = client.post(
        "/auth/disconnect", headers={"origin": "https://evil.example"}
    )
    assert blocked.status_code == 403


def test_agenda_job_skips_without_google(setup):
    app, _, _ = setup
    core = app.state.core
    core.runner.run_once("agenda", core.job_agenda)
    assert core.status()["sources"]["agenda"]["state"] == "idle"
