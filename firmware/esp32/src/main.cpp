// ESP32-S3 entry point for the desk display on the Waveshare ESP32-S3-Touch-LCD-7B.
//
// Brings up the panel, touch and Wi-Fi, then hands over to the shared UI in ../ui (the same
// code the desktop simulator runs). The panel, backlight and touch are driven by
// ../tank/board_7b.h, which was written for this exact board.
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <Wire.h>  // named here so the build finds it for board_7b.h
#include <lvgl.h>

#include "settings.h"  // from ../tank: panel clock, buffer count, touch orientation
#include "board_7b.h"

#include "app.h"
#include "platform.h"
#include "theme.h"

#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef WIFI_SSID
#warning "src/secrets.h not found: copy secrets.example.h to secrets.h and fill it in"
#include "secrets.example.h"
#endif

namespace {

constexpr int DRAW_LINES = 60;  // LVGL renders the screen in strips this tall

bool has_touch = false;
bool tank_has_panel = false;  // the aquarium is drawing; LVGL must not
uint32_t last_wifi_check = 0;

// LVGL hands over a finished strip; the panel driver copies it into the buffer on screen.
void flush(lv_display_t* display, const lv_area_t* area, uint8_t* pixels) {
    if (!tank_has_panel) {
        esp_lcd_panel_draw_bitmap(board::panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, pixels);
    }
    lv_display_flush_ready(display);
}

void read_touch(lv_indev_t*, lv_indev_data_t* data) {
    int x = 0, y = 0;
    if (has_touch && board::touchRead(x, y)) {
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void start_display() {
    if (!board::begin()) {
        Serial.println("display: failed to start (is OPI PSRAM enabled?)");
        for (;;) delay(1000);
    }
    board::setBacklight(100);
    has_touch = board::touchBegin();

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

void* big_alloc(size_t bytes) { return heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }

void backlight(int percent) { board::setBacklight(percent); }

bool panel_direct() { return true; }

int panel_buffers(uint16_t** out) {
    for (int i = 0; i < board::NUM_FB; i++) out[i] = board::fb[i];
    return board::NUM_FB;
}

void panel_wait(int index) { board::waitUntilFree(index); }

void panel_show(int index) { board::present(index); }

void panel_hold(bool on) { tank_has_panel = on; }

}  // namespace plat

void setup() {
    Serial.begin(115200);
    delay(200);
    start_display();
    connect_wifi();
    app::init();
    Serial.printf("desk display: running, %u KB PSRAM free\n",
                  (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
}

void loop() {
    lv_timer_handler();
    delay(5);
    if (millis() - last_wifi_check > 15000) {  // come back by itself after a router restart
        last_wifi_check = millis();
        if (WiFi.status() != WL_CONNECTED) WiFi.reconnect();
    }
}
