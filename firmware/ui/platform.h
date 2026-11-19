// The few things that differ between the desktop simulator and the ESP32.
// Each target provides its own implementation of these functions.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace plat {

// GET <server><path>. True on HTTP 200. `caption` receives the X-Photo-Caption header.
bool http_get(const std::string& path, std::vector<uint8_t>& body, std::string* caption = nullptr);

// Milliseconds since start; also drives LVGL's clock.
uint32_t millis();

// A large buffer (photo frame, aquarium background). On the ESP32 this comes from PSRAM.
void* big_alloc(size_t bytes);

// Screen brightness, 0 to 100 percent.
void backlight(int percent);

// The aquarium paints whole-screen buffers itself. On the board it draws straight into the
// panel's own buffers and the rest of the interface stops drawing while it does; the
// simulator has no such buffers, so there the aquarium keeps one of its own.
bool panel_direct();               // true on the board
int panel_buffers(uint16_t** out); // fills up to 3 pointers, returns how many
void panel_wait(int index);        // block until that buffer is off screen
void panel_show(int index);        // put that buffer on screen
void panel_hold(bool on);          // true: the aquarium owns the panel

}  // namespace plat
