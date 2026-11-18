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

}  // namespace plat
