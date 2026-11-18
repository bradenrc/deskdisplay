"""Setup page: connect a Google account in the browser, pick calendars and a photo folder."""

from __future__ import annotations

from html import escape
from urllib.parse import quote, urlparse

from fastapi import FastAPI, Form, HTTPException, Request
from fastapi.responses import HTMLResponse, RedirectResponse

from . import gcal
from .google_auth import AuthError
from .photos import parse_folder_id

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
textarea{min-height:110px}
button,a.btn{display:inline-block;margin-top:10px;padding:9px 16px;border-radius:8px;border:1px solid var(--line);
background:var(--card);color:var(--text);font:inherit;cursor:pointer;text-decoration:none}
button.primary,a.btn.primary{background:var(--accent);border-color:var(--accent);color:var(--card)}
code{font:13px ui-monospace,Menlo,monospace;background:var(--bg);padding:1px 5px;border-radius:4px;word-break:break-all}
label.row{display:block;padding:3px 0}table{width:100%;border-collapse:collapse;font-size:14px}
td{padding:6px 4px;border-top:1px solid var(--line);vertical-align:top}ol{padding-left:20px;margin:8px 0}
details{margin-top:10px}summary{cursor:pointer;color:var(--accent)}
"""


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


def render(core, request: Request, msg: str = "", err: str = "") -> str:
    status = core.status()
    has_client = status["google_client"]
    connected = status["google_connected"]
    redirect_uri = core.redirect_uri()
    here = request.url.netloc
    same_origin = urlparse(core.cfg["public_url"]).netloc == here

    parts = [
        f"<!doctype html><html lang=en><meta charset=utf-8>"
        f"<meta name=viewport content='width=device-width,initial-scale=1'>"
        f"<title>Desk companion setup</title><style>{STYLE}</style><main>"
        f"<h1>Desk companion setup</h1>"
        f"<p class=muted>This server feeds the desk screen its calendar, photos, news and weather.</p>"
    ]
    if msg:
        parts.append(f'<div class="note ok">{escape(msg)}</div>')
    if err:
        parts.append(f'<div class="note bad">{escape(err)}</div>')

    # 1. Google client
    parts.append(
        f"<section><h2>1. Google app credentials {_pill(has_client, 'Added', 'Needed')}</h2>"
    )
    if not has_client:
        parts.append(
            "<p>Google requires you to register your own small &ldquo;app&rdquo; once. It is free "
            "and takes about ten minutes.</p><ol>"
            "<li>Open <a href='https://console.cloud.google.com/' target=_blank rel=noopener>"
            "Google Cloud Console</a> and create a project.</li>"
            "<li>Enable the <b>Google Calendar API</b> and the <b>Google Drive API</b>.</li>"
            "<li>Set up the consent screen as <b>External</b>, then publish it "
            "(<b>In production</b>). In <b>Testing</b> mode the sign-in expires every 7 days.</li>"
            "<li>Create credentials: <b>OAuth client ID</b>, type <b>Web application</b>, with this "
            f"authorized redirect URI:<br><code>{escape(redirect_uri)}</code></li>"
            "<li>Download the JSON file and paste its contents below.</li></ol>"
        )
    parts.append(
        f"<details {'open' if not has_client else ''}><summary>"
        f"{'Paste the client JSON' if not has_client else 'Replace the client JSON'}</summary>"
        "<form method=post action='/setup/client'><textarea name=client_json "
        'placeholder=\'{"web":{"client_id":"..."}}\' required></textarea>'
        "<button class=primary>Save</button></form></details></section>"
    )

    # 2. Connect
    parts.append(
        f"<section><h2>2. Google account {_pill(connected, 'Connected', 'Not connected')}</h2>"
    )
    if connected:
        account = status["google_account"]
        who = f" as <b>{escape(account)}</b>" if account else ""
        parts.append(
            f"<p>Signed in{who}. Access is read-only for Calendar and Drive, and you can revoke it "
            "any time at <a href='https://myaccount.google.com/permissions' target=_blank "
            "rel=noopener>myaccount.google.com/permissions</a>.</p>"
            "<form method=post action='/auth/disconnect'><button>Disconnect</button></form>"
        )
    elif has_client:
        parts.append(
            "<p>You sign in on Google's own page, including two-step verification. This server "
            "never sees your password.</p>"
            "<a class='btn primary' href='/auth/start'>Connect Google account</a>"
        )
        if not same_origin:
            parts.append(
                "<div class='note warn'>You opened this page at "
                f"<code>{escape(here)}</code>, but Google will send the browser back to "
                f"<code>{escape(redirect_uri)}</code>. After you approve access the browser will "
                "say it cannot reach the page. That is expected: copy the full address from the "
                "address bar and paste it below.</div>"
            )
        parts.append(
            f"<details {'open' if not same_origin else ''}><summary>Finish sign-in by pasting "
            "the address</summary><form method=post action='/auth/paste'>"
            "<input type=text name=url placeholder='http://localhost:8080/auth/callback?state=...&code=...' required>"
            "<button class=primary>Finish sign-in</button></form></details>"
        )
    else:
        parts.append("<p class=muted>Add the app credentials in step 1 first.</p>")
    parts.append("</section>")

    # 3. Calendars
    parts.append("<section><h2>3. Calendars</h2>")
    if connected:
        try:
            calendars = gcal.list_calendars(core.auth.session())
            chosen = set(core.calendar_ids())
            parts.append("<form method=post action='/setup/calendars'>")
            for cal in calendars:
                picked = cal["id"] in chosen or (cal["primary"] and "primary" in chosen)
                parts.append(
                    f"<label class=row><input type=checkbox name=calendar_ids "
                    f"value='{escape(cal['id'], quote=True)}' {'checked' if picked else ''}> "
                    f"{escape(cal['name'])}{' (primary)' if cal['primary'] else ''}</label>"
                )
            parts.append("<button class=primary>Save calendars</button></form>")
        except Exception as exc:
            parts.append(
                f"<p class='note bad'>Could not list calendars: {escape(str(exc)[:200])}</p>"
            )
    else:
        parts.append("<p class=muted>Connect a Google account to choose calendars.</p>")
    parts.append("</section>")

    # 4. Photos
    folder = core.drive_folder_id()
    parts.append(
        f"<section><h2>4. Photos {_pill(status['photos'] > 0, str(status['photos']) + ' ready', 'None yet')}</h2>"
        "<p>Paste the link to a Google Drive folder. Subfolders are included, and each "
        "subfolder's name becomes the photo caption.</p>"
        "<form method=post action='/setup/photos'><input type=text name=folder "
        f"value='{escape(folder, quote=True)}' "
        "placeholder='https://drive.google.com/drive/folders/...'>"
        "<button class=primary>Save folder</button></form></section>"
    )

    # 5. Status
    parts.append("<section><h2>Status</h2><table>")
    for name in ("agenda", "photos", "weather", "news"):
        meta = status["sources"].get(name)
        if not meta:
            parts.append(
                f"<tr><td>{name.title()}</td><td class=muted>Starting…</td></tr>"
            )
            continue
        css = {"ok": "ok", "error": "bad"}.get(meta["state"], "warn")
        label = {"ok": "OK", "error": "Problem", "idle": "Waiting"}.get(
            meta["state"], meta["state"]
        )
        detail = f" · {escape(meta['detail'])}" if meta["detail"] else ""
        parts.append(
            f"<tr><td>{name.title()}</td><td><span class='pill {css}' style='margin:0'>{label}</span>"
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
    def _connected() -> RedirectResponse:
        try:
            core.settings.set(
                "google_account", gcal.primary_account(core.auth.session())
            )
        except Exception:
            core.settings.set("google_account", "")
        core.runner.trigger("agenda", "photos")
        return _back(msg="Google account connected.")

    @app.get("/", include_in_schema=False)
    def root() -> RedirectResponse:
        return RedirectResponse("/setup")

    @app.get("/setup", response_class=HTMLResponse, include_in_schema=False)
    def setup(request: Request, msg: str = "", err: str = "") -> str:
        return render(core, request, msg[:300], err[:300])

    @app.post("/setup/client", include_in_schema=False)
    def save_client(request: Request, client_json: str = Form(...)) -> RedirectResponse:
        _same_site(request)
        try:
            core.auth.save_client(client_json)
        except AuthError as exc:
            return _back(err=str(exc))
        return _back(msg="Credentials saved. Now connect your Google account.")

    @app.get("/auth/start", include_in_schema=False)
    def auth_start() -> RedirectResponse:
        try:
            return RedirectResponse(core.auth.start(core.redirect_uri()))
        except AuthError as exc:
            return _back(err=str(exc))

    @app.get("/auth/callback", include_in_schema=False)
    def auth_callback(
        state: str = "", code: str = "", error: str = ""
    ) -> RedirectResponse:
        if error:
            return _back(err=f"Google reported: {error}")
        try:
            core.auth.finish(state, code)
        except AuthError as exc:
            return _back(err=str(exc))
        return _connected()

    @app.post("/auth/paste", include_in_schema=False)
    def auth_paste(request: Request, url: str = Form(...)) -> RedirectResponse:
        _same_site(request)
        try:
            core.auth.finish_from_pasted(url)
        except AuthError as exc:
            return _back(err=str(exc))
        return _connected()

    @app.post("/auth/disconnect", include_in_schema=False)
    def auth_disconnect(request: Request) -> RedirectResponse:
        _same_site(request)
        core.auth.disconnect()
        core.settings.set("google_account", "")
        core.runner.trigger("agenda", "photos")
        return _back(
            msg="Disconnected. Photos already downloaded are kept until the next sync."
        )

    @app.post("/setup/calendars", include_in_schema=False)
    def save_calendars(
        request: Request, calendar_ids: list[str] = Form(default=[])
    ) -> RedirectResponse:
        _same_site(request)
        core.settings.set("calendar_ids", [c for c in calendar_ids if c][:50])
        core.runner.trigger("agenda")
        return _back(msg="Calendars saved.")

    @app.post("/setup/photos", include_in_schema=False)
    def save_photos(
        request: Request, folder: str = Form(default="")
    ) -> RedirectResponse:
        _same_site(request)
        folder = folder.strip()
        folder_id = parse_folder_id(folder)
        if folder and not folder_id:
            return _back(err="That does not look like a Drive folder link.")
        core.settings.set("drive_folder_id", folder_id)
        core.runner.trigger("photos")
        return _back(msg="Photo folder saved. The first sync can take a few minutes.")
