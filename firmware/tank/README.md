# The aquarium

Everything in this folder comes from **Desk_Aquarium** by npceight
(<https://github.com/npceight/Desk_Aquarium>, commit `310fb75`), used here with the author's
permission. It is the ASCII-art tank, its drawing engine, and the driver for the Waveshare
ESP32-S3-Touch-LCD-7B.

| File | What it is |
|---|---|
| `tank.h` | Fish behaviour and how everything is drawn |
| `art.h` | All the drawings |
| `gfx.h` | Drawing engine that only redraws what moved |
| `controls.h` | Touch buttons and tap-to-feed |
| `settings.h` | Fish counts, colours, effects, panel settings |
| `board_7b.h` | Screen, backlight and touch driver for the 7B |
| `screentest.h` | Test picture for a new board |
| `font_data.h`, `weather_icons.h` | Generated font and weather pictures |

The files are unchanged except for two lines in `settings.h`:

- `SCREEN_BUFFERS` is 2 (was 3). The desk display also keeps a full-screen photo in memory,
  and three screen buffers plus the tank's backgrounds do not fit in the board's 8 MB.
- `MIN_FPS` is 12 (was 18), because two buffers cap the tank at about 16 frames a second and
  18 would make it switch its glow and shadows off.

The tank's own Wi-Fi, clock and weather lookup (in the original `DeskAquarium.ino`) are not
used. `../ui/aquarium.cpp` hosts the tank as a page of the desk display and feeds it the
time and weather the rest of the screen already has.

The weather pictures in `weather_icons.h` are built from Weather Icons by Erik Flowers,
under the SIL Open Font License 1.1 (`WEATHER_ICONS_OFL.txt`).
