# Desk companion server

A small Docker service that gathers everything the desk screen shows and hands it over in
one request: today's Google Calendar agenda, photos from a Google Drive folder, news
headlines, weather, sunrise and sunset, and the moon phase.

The screen (ESP32 or a browser) stays simple: it fetches `/api/dashboard` once a minute and
`/api/photo/next` whenever it wants a new picture.

## Run it

```bash
cp config/config.example.yaml config/config.yaml   # then set your timezone and location
docker compose up -d --build
```

Open <http://localhost:8080/setup>. Weather and news work immediately. Calendar and photos
need the Google sign-in below.

The container runs as user id 1000 and writes to `./data`. On Linux, if `./data` is owned
by a different user, run `sudo chown -R 1000:1000 data`.

## Connect Google (once)

Sign-in happens on Google's own pages in your browser, including two-step verification.
The server never sees your password. It receives a token limited to **read-only Calendar**
and **read-only Drive**, stored in `data/google_token.json`, which you can revoke at any
time at <https://myaccount.google.com/permissions>.

Google requires each self-hosted app to register its own credentials. This is free:

1. In [Google Cloud Console](https://console.cloud.google.com/), create a project.
2. Enable the **Google Calendar API** and the **Google Drive API**.
3. Configure the OAuth consent screen as **External** and add yourself as a user. Then
   **publish it** so its status is **In production**. While it is in **Testing**, Google
   expires the sign-in every 7 days. You will see an "unverified app" warning when signing
   in; that is expected for a personal app, and you can continue past it.
4. Create credentials: **OAuth client ID**, type **Web application**, with the authorized
   redirect URI `http://localhost:8080/auth/callback`.
5. Download the JSON, open the setup page, and paste it into step 1.
6. Click **Connect Google account**, approve, then choose calendars and paste your Drive
   photo folder link.

### Signing in when the server is another machine

Google only redirects to `http://localhost` or a real `https://` address, never to a LAN
address like `http://192.168.1.20:8080`. Pick whichever of these suits you:

- **Paste the address (no extra setup).** Open the setup page at the server's LAN address
  and click Connect. After you approve, the browser says it cannot reach `localhost`. Copy
  the full address from the address bar and paste it into the box on the setup page.
- **SSH tunnel (fully automatic).** Run `ssh -L 8080:localhost:8080 you@server`, then open
  <http://localhost:8080/setup> on your own computer.
- **Your own https address.** If the container sits behind a reverse proxy with a real
  certificate, set `public_url: https://your.domain` in `config.yaml` and register
  `https://your.domain/auth/callback` as the redirect URI.

## What the screen reads

| Address | Returns |
|---|---|
| `GET /api/dashboard` | Time, agenda, weather, moon, news as one JSON document |
| `GET /api/photo/next?w=1024&h=600` | The next photo as a baseline JPEG at exactly that size |
| `GET /api/photo/{id}?w=&h=` | A specific photo |
| `GET /api/photos` | Photo ids and captions |
| `GET /api/status` | Whether each source is healthy and when it last updated |
| `GET /setup` | The setup page |

Agenda events carry a `status` the screen can style directly: `past`, `now` (in
progress), `soon` (starts within `agenda.soon_minutes`) or `upcoming`. Photo responses
include the caption in the `X-Photo-Caption` header (the Drive subfolder name and the month
the photo was taken).

## Notes

- **Photos** are downloaded once, shrunk to `photos.master_max_px`, and the originals are
  discarded, so 2,000 photos take roughly 1 GB. Removing a photo from the Drive folder
  removes it here at the next sync. HEIC files from iPhones are converted.
- **If a source fails**, the last good data keeps being served and `/api/status` says why.
- **The setup page has no login.** Anyone on your network can open it, so do not expose
  port 8080 to the internet.
- **Secrets** live in `data/` and `config/`, both ignored by git.

## Develop

```bash
uv venv .venv && uv pip install --python .venv/bin/python -r requirements.txt pytest httpx
```

```bash
.venv/bin/python -m pytest -q
```
