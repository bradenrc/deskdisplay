# Desk companion server

A small Docker service that gathers everything the desk screen shows and hands it over in
one request: today's calendar agenda, photos from a Google Drive folder, news headlines,
weather, sunrise and sunset, and the moon phase.

The screen (ESP32 or a browser) stays simple: it fetches `/api/dashboard` once a minute and
`/api/photo/next` whenever it wants a new picture.

## Run it

```bash
cp config/config.example.yaml config/config.yaml   # then set your timezone and location
docker compose up -d --build
```

Open <http://localhost:8080/setup>. Weather and news work immediately. The setup page has
three steps for the rest. No Google Cloud project is needed.

The container runs as user id 1000 and writes to `./data`. On Linux, if `./data` is owned
by a different user, run `sudo chown -R 1000:1000 data`.

## 1. Calendar: a private iCal link

In Google Calendar open **Settings**, choose the calendar under **Settings for my
calendars**, go to **Integrate calendar** and copy the **Secret address in iCal format**.
Paste it into step 1 of the setup page. You can add several calendars.

There is no sign-in and nothing expires. Anyone who has the link can read that calendar,
so the server stores it in `data/settings.json`, never logs it, and shows only a masked
form on the setup page. Google lets you reset the link in the same settings screen.

## 2. Google Drive: sign in once

Click **Sign in with Google** in step 2. A Google tab opens; sign in there (including
two-step verification) and approve. Google names the app **rclone**: that is the
open-source tool this server uses to read Drive, and it comes with its own registered
Google app. Access is **read-only Drive**, stored in `data/rclone/rclone.conf`, and you can
revoke it at <https://myaccount.google.com/permissions>.

Google finishes by sending the browser to `http://127.0.0.1:53682/`. What happens next
depends on where your browser is:

- **Browser on the same machine as Docker:** it completes by itself, because
  `docker-compose.yml` publishes that port on the local machine.
- **Server is another machine:** the Google tab ends on "This site can't be reached".
  That is expected. Copy the whole address from the address bar and paste it into the box
  on the setup page.
- **Or use an SSH tunnel** to make the remote case automatic:
  `ssh -L 8080:localhost:8080 -L 53682:localhost:53682 you@server`, then open
  <http://localhost:8080/setup>.

## 3. Photos: a folder of any size

Paste a Drive folder link in step 3 (or the My Drive link for everything). Subfolders are
included, and each subfolder's name becomes the photo caption.

The folder is **not mirrored**. The server reads only the list of file names, then keeps a
small rotating pool on disk:

- `photos.pool_size` (default 300) photos are downloaded, shrunk to screen size, and the
  originals are discarded. That is roughly 100 MB however large the Drive folder is.
- `photos.daily_swap` (default 30) of them are replaced with new random picks each day.
- The list of names is re-read every `photos.index_hours` (default 12).

A local folder can be used as well or instead: mount it in `docker-compose.yml` and set
`photos.local_dir`. Local folders are shown in full.

## Extras

All of these are configured in `config/config.yaml`; the example file documents each one.

- **Air quality.** The US air quality index for your location appears beside the weather,
  coloured by category. No setup.
- **Night mode.** Between `night.start` and `night.end` the screen shows a dim, plain
  clock. Touching it brings the dashboard back for 10 minutes.
- **Ski areas.** When any mountain under `ski.resorts` has had `ski.min_new` or more of new
  snow in the last 48 hours, a small snow report appears beside the news: the 48-hour
  total, what is expected by tomorrow, and the temperature. With no fresh snow it is not
  shown at all. The figures are a weather-model estimate at the coordinates and elevation
  you give, not the resort's own report, and there is no lift status.
- **Sports.** In a week a team under `sports.teams` plays, a small card beside the clock
  shows the next game ("IU @ WASH", "Sat 1:30 pm"), then the live score and game clock,
  then the final. Up to three show at once; more than that rotate. A live score is
  refreshed every minute. Scores come from ESPN's public feeds, which are unofficial and
  could change without notice.
- **Markets.** Symbols under `stocks.symbols` appear on a markets page: price, change on the
  day, and the day's shape against yesterday's close. Up to six are shown. Quotes come from
  Yahoo Finance's public feed, which is unofficial and can be delayed, so treat it as a
  glance and not as something to trade on.
- **Clock colour.** `display.clock_color` is `sun` by default, which shifts the clock from
  peach at sunrise through amber at sunset to blue-white at night. Set it to `white` for a
  plain clock, or to any `"#rrggbb"`.
- **Leave-by times.** With `travel.enabled: true`, a meeting whose location is a street
  address gets a line such as "Leave by 12:06 pm · 19 min drive", worked out from your
  `location`. It turns amber when it is nearly time and red when you should already have
  left. Read the privacy note below before enabling it.

**Travel time and privacy.** Enabling it sends the location text of those meetings to
OpenStreetMap's public address lookup and routing services. Only locations that look like
street addresses are sent (never meeting links or room names), each is looked up once and
cached in `data/travel_cache.json`, and places more than `travel.max_miles` away are
ignored. Those services have no live traffic, so `travel.padding_percent` is added to
every drive and `travel.buffer_minutes` on top.

## What the screen reads

| Address | Returns |
|---|---|
| `GET /api/dashboard` | Time, agenda, weather, moon, news as one JSON document |
| `GET /api/photo/next?w=1024&h=600` | The next photo as a baseline JPEG at exactly that size |
| `GET /api/photo/{id}?w=&h=` | A specific photo |
| `GET /api/photos` | Photo ids and captions |
| `GET /api/status` | Whether each source is healthy and when it last updated |
| `GET /` | The start page (see below) |
| `GET /setup` | The setup page |

Agenda events carry a `status` the screen can style directly: `past`, `now` (in
progress), `soon` (starts within `agenda.soon_minutes`) or `upcoming`. Photo responses
include the caption in the `X-Photo-Caption` header (the subfolder name and, when the
photo records it, the month it was taken).

## Notes

- **If a source fails**, the last good data keeps being served and `/api/status` says why.
- **The setup page has no login.** Anyone on your network can open it, so do not expose
  port 8080 to the internet.
- **Secrets** live in `data/` and `config/`, both ignored by git.
- **HEIC** photos from iPhones are converted automatically.

## Develop

```bash
uv venv .venv && uv pip install --python .venv/bin/python -r requirements.txt pytest httpx
```

```bash
.venv/bin/python -m pytest -q
```

## The start page

`http://<server>:8080/` is a personal start page for a browser: a search box, the desk
screen's panels (agenda, weather, games, snow, markets, photo), and two kinds of widget of
your own:

- **News feeds**: tabs of RSS feeds. Paste a site or feed address and the server finds the feed.
- **Links**: bookmarks shown with each site's icon.

**Customize** (top right) turns on editing: drag widgets by their handle, pick one, two
(even, or one third and two thirds either way) or three columns, add and remove widgets, and
edit the feeds and links inside them. The page is saved on the server in `data/start.json`,
so every browser on the network sees the same one. Delete that file to go back to the
starting layout.

**What the desk display shows** is set from the same widgets. Editing a widget offers "Show
on the desk display" where the display has a matching tile:

- A **news feeds** widget with it ticked supplies the display's news tile. Headlines are taken
  from its feeds in turn, so one busy feed does not crowd out the rest.
- **Agenda**, **Games** and **Snow** can be unticked to keep them on the start page only.

Site icons are fetched once by the server and kept in `data/icons`. Feeds are read by the
server, only for feeds saved on the page, and only from public web addresses.

It is a React app in `web/`, built when the Docker image is built; the running image has no
Node in it. To work on it with live reload: `cd web && npm install && npm run dev`, with the
server running on port 8080.
