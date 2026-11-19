# Desk display

A small always-on desk screen: clock, meeting agenda, news and weather by day; photo frame
and aquarium off-hours.

- [`server/`](server/README.md): Docker service that supplies calendar, photos, news and
  weather to the screen. Start here.
- [`firmware/`](firmware/README.md): the screen's user interface, a desktop simulator for it,
  and the ESP32-S3 build (Waveshare 7" 1024×600 touch), which compiles but is untested on the board.

The aquarium page and the board driver come from
[Desk_Aquarium](https://github.com/npceight/Desk_Aquarium) by npceight; see
[firmware/tank/README.md](firmware/tank/README.md).
