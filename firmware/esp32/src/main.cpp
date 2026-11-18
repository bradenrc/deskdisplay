// ESP32-S3 entry point for the desk display.
//
// Brings up the panel, touch and Wi-Fi, then hands over to the shared UI in ../ui (the same
// code the desktop simulator runs). Everything specific to the board lives in this file and
// in platformio.ini.
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <esp_display_panel.hpp>
#include <lvgl.h>

#include "app.h"
#include "platform.h"
#include "theme.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#warning "src/secrets.h not found: copy secrets.example.h to secrets.h and fill it in"
#include "secrets.example.h"
#endif

using namespace esp_panel::drivers;
using namespace esp_panel::board;

namespace {

constexpr int DRAW_LINES = 60;  // LVGL renders the screen in strips this tall

// Set to 1 if the picture shifts or flickers while Wi-Fi is busy (a known trait of RGB panels
// on the ESP32-S3). It costs some internal RAM.
#define USE_BOUNCE_BUFFER 0

Board* board = nullptr;
LCD* lcd = nullptr;
Touch* touch = nullptr;
uint32_t last_wifi_check = 0;

void flush(lv_display_t* display, const lv_area_t* area, uint8_t* pixels) {
    lcd->drawBitmap(area->x1, area->y1, lv_area_get_width(area), lv_area_get_height(area), pixels);
    lv_display_flush_ready(display);
}

void read_touch(lv_indev_t*, lv_indev_data_t* data) {
    TouchPoint point[1];
    if (touch && touch->readPoints(point, 1, 0) > 0) {
        data->point.x = point[0].x;
        data->point.y = point[0].y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void start_display() {
    board = new Board();
    board->init();
    lcd = board->getLCD();
    touch = board->getTouch();
#if USE_BOUNCE_BUFFER
    static_cast<BusRGB*>(lcd->getBus())->configRGB_BounceBufferSize(SCREEN_W * 10);
#endif
    if (!board->begin()) Serial.println("display: board->begin() failed");

    lv_init();
    lv_tick_set_cb([]() -> uint32_t { return millis(); });
    lv_display_t* display = lv_display_create(SCREEN_W, SCREEN_H);
    size_t bytes = SCREEN_W * DRAW_LINES * 2;
    void* strip = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    lv_display_set_buffers(display, strip, nullptr, bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);

    lv_indev_t* pointer = lv_indev_create();
    lv_indev_set_type(pointer, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(pointer, read_touch);
}

// Shows progress on the panel itself, since there is no other way to see it on a desk.
void connect_wifi() {
    lv_obj_t* screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, col(C_BG), 0);
    lv_obj_t* note = make_label(screen, &font_text_24, C_MUTED);
    lv_label_set_text_fmt(note, "Connecting to Wi-Fi \"%s\"…", WIFI_SSID);
    lv_obj_center(note);

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    uint32_t started = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - started < 20000) {
        lv_timer_handler();
        delay(20);
    }
    Serial.printf("wifi: %s\n", WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str()
                                                               : "not connected, will keep trying");
    lv_obj_delete(note);
}

}  // namespace

namespace plat {

bool http_get(const std::string& path, std::vector<uint8_t>& body, std::string* caption) {
    body.clear();
    if (WiFi.status() != WL_CONNECTED) return false;
    HTTPClient http;
    http.setConnectTimeout(4000);
    http.setTimeout(8000);
    if (!http.begin(String(SERVER_URL) + path.c_str())) return false;
    const char* wanted[] = {"X-Photo-Caption"};
    http.collectHeaders(wanted, 1);
    bool ok = http.GET() == HTTP_CODE_OK;
    if (ok) {
        int remaining = http.getSize();  // -1 when the server does not say
        if (remaining > 0) body.reserve(remaining);
        WiFiClient* stream = http.getStreamPtr();
        uint8_t chunk[2048];
        uint32_t last_data = millis();
        while (http.connected() && remaining != 0 && millis() - last_data < 8000) {
            size_t ready = stream->available();
            if (!ready) {
                delay(2);
                continue;
            }
            int got = stream->readBytes(chunk, ready < sizeof chunk ? ready : sizeof chunk);
            body.insert(body.end(), chunk, chunk + got);
            if (remaining > 0) remaining -= got;
            last_data = millis();
        }
        ok = remaining <= 0 && !body.empty();
        if (caption) *caption = http.header("X-Photo-Caption").c_str();
    }
    http.end();
    return ok;
}

uint32_t millis() { return ::millis(); }

void* big_alloc(size_t bytes) { return heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM); }

}  // namespace plat

void setup() {
    Serial.begin(115200);
    start_display();
    connect_wifi();
    app::init();
}

void loop() {
    lv_timer_handler();
    delay(5);
    if (millis() - last_wifi_check > 15000) {  // come back by itself after a router restart
        last_wifi_check = millis();
        if (WiFi.status() != WL_CONNECTED) WiFi.reconnect();
    }
}
