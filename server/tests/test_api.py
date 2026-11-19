import shutil
from pathlib import Path

import pytest
from fastapi.testclient import TestClient
from PIL import Image

from app.main import create_app
from app.photos import Library

SECRET_LINK = "https://calendar.google.com/calendar/ical/me%40example.com/private-abcdef123456/basic.ics"
FOLDER = "1AbCdEfGhIjKlMnOpQrStUvWxYz012345"


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


class FakeDrive:
    """Stands in for rclone: a 'Drive folder' that is really a local directory."""

    def __init__(self, root: Path, count: int):
        self.root = root
        self.lists = 0
        self.copied: list[str] = []
        (root / "Trip").mkdir(parents=True)
        for n in range(count):
            Image.new("RGB", (400, 300), (n * 7 % 255, 90, 160)).save(
                root / "Trip" / f"p{n:03d}.jpg"
            )
        (root / "Trip" / "broken.jpg").write_text("not really a jpeg")
        (root / "Trip" / "._p000.jpg").write_text("macOS companion file")

    def list_images(self, folder_id):
        self.lists += 1
        return [
            {"id": f"id-{p.name}", "path": str(p.relative_to(self.root))}
            for p in sorted(self.root.rglob("*.jpg"))
        ]

    def copy(self, folder_id, paths, dest):
        for rel in paths:
            self.copied.append(rel)
            target = Path(dest) / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy(self.root / rel, target)


def test_dashboard_before_any_data(setup):
    _, client, _ = setup
    body = client.get("/api/dashboard").json()
    assert body["agenda"] is None and body["weather"] is None and body["news"] == []
    assert body["moon"]["name"] and body["time"]["epoch"] > 0
    assert client.get("/healthz").json() == {"ok": True}
    status = client.get("/api/status").json()
    assert status["drive_connected"] is False and status["calendar_links"] == 0


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


def test_pool_keeps_a_fixed_sample_and_rotates_daily(tmp_path):
    drive = FakeDrive(tmp_path / "drive", count=60)
    library = Library(tmp_path / "lib")
    day = 86400.0

    assert (
        library.sync_pool(
            drive, FOLDER, pool_size=25, daily_swap=5, today="d1", now=day
        )
        == 25
    )
    assert library.catalog_size == 61 and drive.lists == 1
    assert "Trip/._p000.jpg" not in drive.copied  # hidden companion files are ignored
    first = {e["id"] for e in library.entries()}
    assert len(list(library.masters.glob("*.jpg"))) == 25
    assert not library.incoming.exists()  # originals are not kept
    entry = library.get(next(iter(first)))
    assert entry["caption"] == "Trip"
    with Image.open(entry["path"]) as image:
        assert max(image.size) <= 1920

    # same day: nothing changes, and the folder is not listed again
    copied = len(drive.copied)
    assert (
        library.sync_pool(
            drive, FOLDER, pool_size=25, daily_swap=5, today="d1", now=day + 60
        )
        == 25
    )
    assert {e["id"] for e in library.entries()} == first
    assert len(drive.copied) == copied and drive.lists == 1

    # next day: five of the oldest are swapped for new picks
    assert (
        library.sync_pool(
            drive, FOLDER, pool_size=25, daily_swap=5, today="d2", now=2 * day
        )
        == 25
    )
    second = {e["id"] for e in library.entries()}
    assert len(first - second) == 5 and len(second - first) == 5
    assert len(list(library.masters.glob("*.jpg"))) == 25
    assert drive.lists == 2  # the list is older than index_hours by now

    # a photo deleted from Drive leaves the pool once the folder is listed again
    victim = library.get(next(iter(second)))["name"]
    (drive.root / "Trip" / victim).unlink()
    library.sync_pool(
        drive, FOLDER, pool_size=25, daily_swap=5, today="d2", now=3 * day
    )
    assert victim not in {library.get(e["id"])["name"] for e in library.entries()}
    assert library.count() == 25  # and the gap is refilled

    # a different folder starts the pool over; clearing empties it
    library.sync_pool(drive, "another-folder-id", pool_size=10, today="d2", now=3 * day)
    assert library.count() == 10
    library.clear_pool()
    assert library.count() == 0 and not list(library.masters.glob("*.jpg"))


def test_pool_skips_unreadable_files_and_small_folders(tmp_path):
    drive = FakeDrive(tmp_path / "drive", count=4)
    library = Library(tmp_path / "lib")
    notes = []
    big = FakeDrive(tmp_path / "big", count=45)
    Library(tmp_path / "lib2").sync_pool(
        big, FOLDER, pool_size=45, now=1.0, progress=notes.append
    )
    assert notes[0].startswith("Reading the list") and len(notes) >= 3
    assert all(
        n.startswith("Downloading photos: ") and n.endswith(" of 45 ready")
        for n in notes[1:]
    )
    assert library.sync_pool(drive, FOLDER, pool_size=25, today="d1", now=1.0) == 4
    tries = drive.copied.count("Trip/broken.jpg")
    library.sync_pool(drive, FOLDER, pool_size=25, today="d2", now=2.0)
    assert library.count() == 4  # too few to rotate, so nothing is evicted
    assert drive.copied.count("Trip/broken.jpg") == tries  # the bad file is not retried


def test_start_page_layout_is_saved_and_checked(setup):
    _, client, tmp_path = setup
    assert client.get("/").status_code == 200
    board = client.get("/api/start").json()
    assert board["layout"] == "3" and board["widgets"]["news"]["type"] == "feeds"

    board["layout"] = "2-left"
    board["columns"] = [["links", "nope"], ["news"], []]
    board["widgets"]["links"]["links"] = [
        {"name": " Router ", "url": "http://192.168.1.1/"}
    ]
    saved = client.put("/api/start", json=board).json()
    assert saved["columns"][0][:1] == ["links"] and "nope" not in saved["columns"][0]
    assert "agenda" in saved["columns"][0]  # widgets left out of the columns are kept
    assert saved["widgets"]["links"]["links"] == [
        {"name": "Router", "url": "http://192.168.1.1/"}
    ]
    assert client.get("/api/start").json() == saved
    assert (tmp_path / "data" / "start.json").exists()

    board["widgets"]["links"]["links"] = [{"name": "Bad", "url": "javascript:alert(1)"}]
    assert client.put("/api/start", json=board).status_code == 400
    board["layout"] = "9"
    assert client.put("/api/start", json=board).status_code == 400
    cross = client.put(
        "/api/start", json=saved, headers={"origin": "https://evil.example"}
    )
    assert cross.status_code == 403


def test_feeds_and_icons_refuse_what_is_not_on_the_page(setup):
    _, client, _ = setup
    assert (
        client.get("/api/feed", params={"url": "http://169.254.169.254/"}).status_code
        == 403
    )
    assert client.get("/api/favicon", params={"domain": "localhost"}).status_code == 404
    assert (
        client.get("/api/favicon", params={"domain": "../etc/passwd"}).status_code
        == 404
    )
    local = client.post("/api/feed/find", json={"address": "http://127.0.0.1:8080/"})
    assert local.status_code == 400


def test_setup_page_and_forms(setup):
    app, client, _ = setup
    core = app.state.core
    page = client.get("/setup")
    assert page.status_code == 200
    assert (
        "Secret address in iCal format" in page.text
        and "Sign in with Google" in page.text
    )

    bad = client.post("/setup/calendar", data={"links": "not a link"})
    assert bad.status_code == 303 and "err=" in bad.headers["location"]
    ok = client.post(
        "/setup/calendar", data={"links": f"{SECRET_LINK}\nwebcal://example.com/b.ics"}
    )
    assert "msg=" in ok.headers["location"]
    assert core.ical_urls() == [SECRET_LINK, "https://example.com/b.ics"]
    client.post(
        "/setup/calendar", data={"links": SECRET_LINK}
    )  # duplicates are ignored
    assert len(core.ical_urls()) == 2
    page = client.get("/setup").text
    assert (
        "2 linked" in page and "private-abcdef" not in page
    )  # the secret is never shown
    client.post("/setup/calendar/clear")
    assert core.ical_urls() == []

    client.post(
        "/setup/photos",
        data={"folder": f"https://drive.google.com/drive/folders/{FOLDER}"},
    )
    assert core.drive_folder_id() == FOLDER
    assert (
        "err="
        in client.post("/setup/photos", data={"folder": "nonsense"}).headers["location"]
    )
    client.post(
        "/setup/photos", data={"folder": "https://drive.google.com/drive/my-drive"}
    )
    assert core.drive_folder_id() == "root"
    assert "My Drive (everything)" in client.get("/setup").text
    client.post("/setup/photos", data={"folder": ""})
    assert core.drive_folder_id() == ""

    expired = client.post(
        "/drive/paste", data={"url": "http://127.0.0.1:53682/?state=x&code=y"}
    )
    assert "expired" in expired.headers["location"]
    blocked = client.post(
        "/drive/disconnect", headers={"origin": "https://evil.example"}
    )
    assert blocked.status_code == 403


def test_jobs_wait_until_configured(setup):
    app, client, _ = setup
    core = app.state.core
    core.runner.run_once("agenda", core.job_agenda)
    assert core.status()["sources"]["agenda"]["state"] == "idle"
    client.post("/setup/photos", data={"folder": FOLDER})
    core.runner.run_once("photos", core.job_photos)
    photos = core.status()["sources"]["photos"]
    assert photos["state"] == "idle" and "not connected" in photos["detail"]
    assert core.library.count() == 2  # local photos still work


def test_widget_settings_drive_the_desk_display(setup):
    app, client, _ = setup
    core = app.state.core
    core.store.set("agenda", [])
    board = client.get("/api/start").json()
    assert board["widgets"]["news"]["desk"] and not board["widgets"]["misc"]["desk"]
    assert [f["name"] for f in core.board.desk_feeds()][:2] == ["Top stories", "FOX"]
    shown = client.get("/api/dashboard").json()
    assert shown["agenda"] is not None and shown["desk"]["agenda"]

    board["widgets"]["agenda"]["desk"] = False
    board["widgets"]["news"]["desk"] = False
    board["widgets"]["misc"]["desk"] = True
    client.put("/api/start", json=board)
    assert [f["name"] for f in core.board.desk_feeds()][:2] == ["TC", "GC"]
    device = client.get("/api/dashboard").json()
    assert device["agenda"] is None and device["desk"]["agenda"] is False
    web = client.get("/api/dashboard", params={"view": "web"}).json()
    assert web["agenda"] is not None  # the start page still gets everything

    # A page saved before this setting existed: panels stay on, and the first feeds widget
    # supplies the display's headlines.
    for widget in board["widgets"].values():
        widget.pop("desk", None)
    saved = client.put("/api/start", json=board).json()
    assert saved["widgets"]["agenda"]["desk"] and saved["widgets"]["news"]["desk"]
    assert not saved["widgets"]["misc"]["desk"]
    assert "desk" not in saved["widgets"]["links"]
