"""Setup page: add a calendar link, connect Google Drive in the browser, pick a photo folder."""

from __future__ import annotations

from html import escape
from urllib.parse import quote, urlparse

from fastapi import FastAPI, Form, HTTPException, Request
from fastapi.responses import HTMLResponse, RedirectResponse

from . import ical
from .photos import parse_folder_id
from .rclone import AUTH_PORT, RcloneError

STYLE = """
:root{--bg:#f6f5f1;--card:#fff;--text:#1f1e1c;--muted:#6b6a65;--line:#dddbd3;--accent:#185fa5;
--ok:#0f6e56;--okbg:#e1f5ee;--bad:#a32d2d;--badbg:#fcebeb;--warn:#854f0b;--warnbg:#faeeda}
@media (prefers-color-scheme:dark){:root{--bg:#161615;--card:#222220;--text:#f1efe8;--muted:#a3a199;
--line:#3a3936;--accent:#85b7eb;--ok:#5dcaa5;--okbg:#04342c;--bad:#f09595;--badbg:#501313;
--warn:#fac775;--warnbg:#412402}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);
font:16px/1.55 system-ui,-apple-system,Segoe UI,sans-serif}
main{max-width:720px;margin:0 auto;padding:24px 16px 64px}
h1{font-size:24px;margin:8px 0 4px}h2{font-size:17px;margin:0 0 8px}
p{margin:8px 0}.muted{color:var(--muted);font-size:14px}
section{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:16px 18px;margin:16px 0}
.pill{display:inline-block;font-size:13px;padding:2px 10px;border-radius:999px;margin-left:8px;vertical-align:2px}
.ok{background:var(--okbg);color:var(--ok)}.bad{background:var(--badbg);color:var(--bad)}
.warn{background:var(--warnbg);color:var(--warn)}
.note{border-radius:8px;padding:10px 12px;margin:12px 0;font-size:14px}
input[type=text],textarea{width:100%;padding:9px 10px;border:1px solid var(--line);border-radius:8px;
background:var(--bg);color:var(--text);font:14px ui-monospace,Menlo,monospace}
textarea{min-height:80px}form{margin:0}form.inline{display:inline}
button{display:inline-block;margin:10px 8px 0 0;padding:9px 16px;border-radius:8px;border:1px solid var(--line);
background:var(--card);color:var(--text);font:inherit;cursor:pointer}
button.primary{background:var(--accent);border-color:var(--accent);color:var(--card)}
code{font:13px ui-monospace,Menlo,monospace;background:var(--bg);padding:1px 5px;border-radius:4px;word-break:break-all}
table{width:100%;border-collapse:collapse;font-size:14px}
td{padding:6px 4px;border-top:1px solid var(--line);vertical-align:top}ol{padding-left:20px;margin:8px 0}
details{margin-top:10px}summary{cursor:pointer;color:var(--accent)}
"""

# While a sign-in is in progress, reload once Drive reports connected.
POLL = (
    "<script>setInterval(async()=>{try{const r=await fetch('/api/status');"
    "if((await r.json()).drive_connected)location.replace('/setup?msg='+"
    "encodeURIComponent('Google Drive connected.'))}catch(e){}},3000)</script>"
)


def _pill(ok: bool, yes: str, no: str) -> str:
    return f'<span class="pill {"ok" if ok else "warn"}">{escape(yes if ok else no)}</span>'


def _age(seconds: int | None) -> str:
    if seconds is None:
        return "never"
    if seconds < 90:
        return "just now"
    if seconds < 5400:
        return f"{seconds // 60} min ago"
    return f"{seconds // 3600} hr ago"


def render(core, msg: str = "", err: str = "") -> str:
    status = core.status()
    parts = [
        "<!doctype html><html lang=en><meta charset=utf-8>"
        "<meta name=viewport content='width=device-width,initial-scale=1'>"
        f"<title>Desk companion setup</title><style>{STYLE}</style><main>"
        "<h1>Desk companion setup</h1>"
        "<p class=muted>This server feeds the desk screen its calendar, photos, news and weather.</p>"
    ]
    if msg:
        parts.append(f'<div class="note ok">{escape(msg)}</div>')
    if err:
        parts.append(f'<div class="note bad">{escape(err)}</div>')

    # 1. Calendar
    links = core.ical_urls()
    parts.append(
        f"<section><h2>1. Calendar {_pill(bool(links), f'{len(links)} linked', 'Not linked')}</h2>"
    )
    if links:
        parts.append(
            "<p>Linked calendars (the secret part of each link is hidden):</p><p>"
        )
        parts.append("<br>".join(f"<code>{escape(ical.mask(u))}</code>" for u in links))
        parts.append("</p>")
    parts.append(
        "<p>In Google Calendar open <b>Settings</b>, choose the calendar under "
        "<b>Settings for my calendars</b>, go to <b>Integrate calendar</b> and copy the "
        "<b>Secret address in iCal format</b>. No sign-in is needed here; treat the link "
        "like a password.</p>"
        "<form method=post action='/setup/calendar'><textarea name=links required "
        "placeholder='https://calendar.google.com/calendar/ical/.../basic.ics (one per line)'>"
        f"</textarea><button class=primary>{'Add link' if links else 'Save link'}</button></form>"
    )
    if links:
        parts.append(
            "<form method=post action='/setup/calendar/clear'>"
            "<button>Remove all calendar links</button></form>"
        )
    parts.append("</section>")

    # 2. Google Drive
    connected = status["drive_connected"]
    signing_in = core.drive.signing_in() and not connected
    parts.append(
        f"<section><h2>2. Google Drive {_pill(connected, 'Connected', 'Not connected')}</h2>"
    )
    if connected:
        parts.append(
            "<p>Read-only access to Google Drive, used only to fetch photos. You can revoke it "
            "any time at <a href='https://myaccount.google.com/permissions' target=_blank "
            "rel=noopener>myaccount.google.com/permissions</a> (it is listed as rclone).</p>"
            "<form method=post action='/drive/disconnect'><button>Disconnect</button></form>"
        )
    else:
        if signing_in:
            parts.append(
                "<div class='note warn'>Waiting for you to finish signing in on the Google tab. "
                "This page updates by itself when it is done.</div>"
                "<p><b>If the Google tab ended on &ldquo;This site can&rsquo;t be reached&rdquo;</b> "
                f"at an address starting with <code>http://127.0.0.1:{AUTH_PORT}/</code>, that is "
                "expected when this server is another machine. Copy that whole address from the "
                "address bar and paste it here:</p>"
                "<form method=post action='/drive/paste'><input type=text name=url required "
                f"placeholder='http://127.0.0.1:{AUTH_PORT}/?state=...&code=...'>"
                "<button class=primary>Finish sign-in</button></form>" + POLL
            )
        else:
            parts.append(
                "<p>Sign-in opens in a new tab on Google's own page, including two-step "
                "verification. This server never sees your password, and the access is "
                "read-only. Google will name the app <b>rclone</b>, the open-source tool this "
                "server uses to read Drive.</p>"
            )
        parts.append(
            "<form method=post action='/drive/connect' target=_blank "
            "onsubmit=\"setTimeout(()=>location.replace('/setup'),1500)\">"
            f"<button class='{'' if signing_in else 'primary'}'>"
            f"{'Start sign-in again' if signing_in else 'Sign in with Google'}</button></form>"
        )
    parts.append("</section>")

    # 3. Photos
    folder = core.drive_folder_id()
    in_folder = status["photos_in_drive_folder"]
    found = f" The folder holds {in_folder:,} photos." if in_folder else ""
    pool = core.cfg["photos"]["pool_size"]
    swap = core.cfg["photos"]["daily_swap"]
    shown = "My Drive (everything)" if folder == "root" else folder
    parts.append(
        f"<section><h2>3. Photos {_pill(status['photos'] > 0, str(status['photos']) + ' ready', 'None yet')}</h2>"
        f"<p>Paste the link to a Google Drive folder. Subfolders are included.{found}</p>"
        f"<p class=muted>Only {pool} photos are kept on this server at a time, shrunk to screen "
        f"size, and {swap} are swapped for new random picks each day. The folder can be as "
        "large as you like.</p>"
        "<form method=post action='/setup/photos'><input type=text name=folder "
        f"value='{escape(shown, quote=True)}' "
        "placeholder='https://drive.google.com/drive/folders/...'>"
        "<button class=primary>Save folder</button></form></section>"
    )

    # Status
    parts.append("<section><h2>Status</h2><table>")
    labels = {
        "agenda": "Calendar",
        "photos": "Photos",
        "weather": "Weather",
        "news": "News",
    }
    for name, title in (
        ("air", "Air quality"),
        ("ski", "Ski areas"),
        ("sports", "Sports"),
        ("stocks", "Markets"),
    ):
        if name in status["sources"]:
            labels[name] = title
    for name, title in labels.items():
        meta = status["sources"].get(name)
        if not meta:
            parts.append(f"<tr><td>{title}</td><td class=muted>Starting…</td></tr>")
            continue
        css = {"ok": "ok", "error": "bad"}.get(meta["state"], "warn")
        label = {
            "ok": "OK",
            "error": "Problem",
            "idle": "Waiting",
            "working": "Working",
        }.get(meta["state"], meta["state"])
        detail = f" · {escape(meta['detail'])}" if meta["detail"] else ""
        parts.append(
            f"<tr><td>{title}</td><td><span class='pill {css}' style='margin:0'>{label}</span>"
            f" <span class=muted>updated {_age(meta['age_seconds'])}{detail}</span></td></tr>"
        )
    parts.append(
        "</table><p class=muted>What the screen reads: <a href='/api/dashboard'>/api/dashboard</a>"
        " · <a href='/api/photo/next?w=1024&h=600'>/api/photo/next</a></p></section></main></html>"
    )
    return "".join(parts)


def _back(msg: str = "", err: str = "") -> RedirectResponse:
    query = f"?msg={quote(msg)}" if msg else f"?err={quote(err)}" if err else ""
    return RedirectResponse(f"/setup{query}", status_code=303)


def _same_site(request: Request) -> None:
    """Refuse form posts that another website tries to send on the user's behalf."""
    origin = request.headers.get("origin")
    if origin and urlparse(origin).netloc != request.url.netloc:
        raise HTTPException(403, "Cross-site request refused")


def register(app: FastAPI, core) -> None:
    @app.get("/", include_in_schema=False)
    def root() -> RedirectResponse:
        return RedirectResponse("/setup")

    @app.get("/setup", response_class=HTMLResponse, include_in_schema=False)
    def setup(msg: str = "", err: str = "") -> str:
        return render(core, msg[:300], err[:300])

    @app.post("/setup/calendar", include_in_schema=False)
    def add_calendar(request: Request, links: str = Form(...)) -> RedirectResponse:
        _same_site(request)
        wanted = [line.strip() for line in links.splitlines() if line.strip()]
        cleaned = [ical.normalize_url(line) for line in wanted]
        if not cleaned or not all(cleaned):
            return _back(
                err="That does not look like a calendar link. It should start with https://"
            )
        merged = list(dict.fromkeys([*core.ical_urls(), *cleaned]))[:20]
        core.settings.set("ical_urls", merged)
        core.runner.trigger("agenda")
        return _back(msg="Calendar link saved. Events appear within a few seconds.")

    @app.post("/setup/calendar/clear", include_in_schema=False)
    def clear_calendar(request: Request) -> RedirectResponse:
        _same_site(request)
        core.settings.set("ical_urls", [])
        core.runner.trigger("agenda")
        return _back(msg="Calendar links removed.")

    @app.post("/drive/connect", include_in_schema=False)
    def drive_connect(request: Request) -> RedirectResponse:
        _same_site(request)
        try:
            return RedirectResponse(core.drive.start_auth(), status_code=303)
        except RcloneError as exc:
            return _back(err=str(exc))

    @app.post("/drive/paste", include_in_schema=False)
    def drive_paste(request: Request, url: str = Form(...)) -> RedirectResponse:
        _same_site(request)
        try:
            core.drive.complete_from_pasted(url)
        except RcloneError as exc:
            return _back(err=str(exc))
        return _back(msg="Google Drive connected.")

    @app.post("/drive/disconnect", include_in_schema=False)
    def drive_disconnect(request: Request) -> RedirectResponse:
        _same_site(request)
        core.drive.disconnect()
        core.runner.trigger("photos")
        return _back(
            msg="Disconnected. Photos already downloaded stay until you clear the folder."
        )

    @app.post("/setup/photos", include_in_schema=False)
    def save_photos(
        request: Request, folder: str = Form(default="")
    ) -> RedirectResponse:
        _same_site(request)
        folder = folder.strip()
        if folder.lower().startswith("my drive"):
            folder = "root"
        folder_id = parse_folder_id(folder)
        if folder and not folder_id:
            return _back(err="That does not look like a Drive folder link.")
        core.settings.set("drive_folder_id", folder_id)
        core.runner.trigger("photos")
        if not folder_id:
            return _back(msg="Photo folder cleared.")
        return _back(
            msg="Photo folder saved. The first photos appear within a few minutes."
        )
