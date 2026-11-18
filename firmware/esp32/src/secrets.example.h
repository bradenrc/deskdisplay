// Copy this file to secrets.h (which git ignores) and fill in your own values.
#pragma once

#define WIFI_SSID "your-wifi-name"
#define WIFI_PASSWORD "your-wifi-password"

// The desk companion server. Use the machine's address on your network, not "localhost":
// to the ESP32, localhost is the ESP32 itself. No trailing slash.
#define SERVER_URL "http://192.168.1.10:8080"
