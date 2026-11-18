# Desk display firmware

The screen's user interface, written once in C++ with [LVGL](https://lvgl.io) and run in two
places: a desktop simulator for designing, and the ESP32-S3 board.

| Dashboard | Photo frame | Aquarium |
|---|---|---|
| ![Dashboard](docs/dashboard.png) | ![Photo frame](docs/photo.png) | ![Aquarium](docs/aquarium.png) |

These are rendered by the simulator at the panel's real size, 1024×600, from the sample
data in `fixtures/`.

## Simulator

It shows exactly what the device will draw, using the same code. Needs a C++ compiler,
libcurl, and SDL2 (`brew install sdl2` on macOS).

```bash
make deps
```

```bash
make run
```

`make run` opens a window fed by the server at `http://localhost:8080`. Drag left or right
with the mouse to change pages; the mouse acts as the finger.

| Command | What it does |
|---|---|
| `make run SERVER=http://host:8080` | Live data from another server |
| `make demo` | Canned data from `fixtures/`, no server needed |
| `make shots` | Render every page to `build/shots/*.png` (canned data) |
| `make shots LIVE=1` | The same, with live data |

## Layout

| Folder | Contents |
|---|---|
| `ui/` | Everything on screen. Shared by the simulator and the device. |
| `ui/fonts/` | Generated bitmap fonts (Montserrat). Regenerate with `tools/make_fonts.sh`. |
| `sim/` | Simulator: window, networking through libcurl, screenshots. |
| `fixtures/` | Sample server reply and photo for `make demo` and `make shots`. |
| `lv_conf.h` | LVGL settings shared by both targets. |

`ui/platform.h` lists the three things each target must provide: an HTTP GET, a
millisecond clock, and a large-buffer allocator. The simulator's versions are in
`sim/main.cpp`.

## How the screens work

- **Dashboard** (`ui/dashboard.cpp`) polls `/api/dashboard` every 30 seconds. The clock is
  set from the server's reply, so the device needs no timezone settings of its own.
- **Photo frame** (`ui/photo.cpp`) fetches `/api/photo/next` at the panel size every 30
  seconds while it is the visible page, and decodes the JPEG into one full-screen buffer.
- **Aquarium** (`ui/aquarium.cpp`) paints the scenery once and moves small fish and bubble
  objects over it, so only the areas around them are redrawn.
- **Icons** (`ui/icons.cpp`) are drawn from circles, lines and arcs. There is no icon font.

## ESP32 target

Not started. It will add an `esp32/` folder with the board's display and touch setup and
its own `platform` functions (Wi-Fi and HTTP), and compile the same `ui/` code.
