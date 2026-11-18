# Desk display

A small always-on desk screen: clock, meeting agenda, news and weather by day; photo frame
and aquarium off-hours.

- [`server/`](server/README.md): Docker service that supplies calendar, photos, news and
  weather to the screen. Start here.
- [`firmware/`](firmware/README.md): the screen's user interface and a desktop simulator for it.
  The ESP32-S3 build (Waveshare 7" 1024×600 touch) is not started yet.
