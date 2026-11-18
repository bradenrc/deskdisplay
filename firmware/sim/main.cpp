// Desktop simulator: runs the exact UI code the ESP32 will run, in a 1024x600 window.
//
//   deskdisplay-sim                          live data from http://localhost:8080
//   deskdisplay-sim --server http://host:8080
//   deskdisplay-sim --fixtures fixtures      canned data, no server needed
//   deskdisplay-sim --screenshot out         render each page to out/page-N.bmp and exit
//
// In the window, drag left or right with the mouse to change pages (the mouse is the finger).
#include <curl/curl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "app.h"
#include "lvgl.h"
#include "platform.h"
#include "theme.h"

namespace {

std::string server = "http://localhost:8080";
std::string fixtures;

size_t on_body(char* data, size_t size, size_t count, void* out) {
    auto* body = (std::vector<uint8_t>*)out;
    body->insert(body->end(), data, data + size * count);
    return size * count;
}

size_t on_header(char* data, size_t size, size_t count, void* out) {
    static const char name[] = "x-photo-caption:";
    std::string line(data, size * count);
    if (out && line.size() > sizeof name && strncasecmp(line.c_str(), name, sizeof name - 1) == 0) {
        std::string value = line.substr(sizeof name - 1);
        size_t a = value.find_first_not_of(" \t"), b = value.find_last_not_of(" \t\r\n");
        *(std::string*)out = a == std::string::npos ? "" : value.substr(a, b - a + 1);
    }
    return size * count;
}

bool read_file(const std::string& path, std::vector<uint8_t>& body) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    body.assign(std::istreambuf_iterator<char>(in), {});
    return true;
}

}  // namespace

namespace plat {

bool http_get(const std::string& path, std::vector<uint8_t>& body, std::string* caption) {
    body.clear();
    if (!fixtures.empty()) {
        if (path.rfind("/api/dashboard", 0) == 0) return read_file(fixtures + "/dashboard.json", body);
        if (path.rfind("/api/photo/", 0) == 0) {
            if (caption) *caption = "Lake trip - August 2024";
            return read_file(fixtures + "/photo.jpg", body);
        }
        return false;
    }
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    curl_easy_setopt(curl, CURLOPT_URL, (server + path).c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, on_body);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, on_header);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, caption);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    long status = 0;
    bool ok = curl_easy_perform(curl) == CURLE_OK;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);
    return ok && status == 200;
}

uint32_t millis() {
    using namespace std::chrono;
    static const auto start = steady_clock::now();
    return (uint32_t)duration_cast<milliseconds>(steady_clock::now() - start).count();
}

void* big_alloc(size_t bytes) { return malloc(bytes); }

}  // namespace plat

namespace {

void pump(uint32_t ms) {
    uint32_t until = plat::millis() + ms;
    while (plat::millis() < until) {
        lv_timer_handler();
        usleep(2000);
    }
}

bool write_bmp(const std::string& path, const uint16_t* frame) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    const uint32_t row = SCREEN_W * 3, size = 54 + row * SCREEN_H;
    uint8_t head[54] = {'B', 'M'};
    auto put32 = [&](int at, uint32_t v) { memcpy(head + at, &v, 4); };
    put32(2, size);
    put32(10, 54);
    put32(14, 40);
    put32(18, SCREEN_W);
    put32(22, SCREEN_H);
    head[26] = 1;
    head[28] = 24;
    put32(34, row * SCREEN_H);
    fwrite(head, 1, 54, f);
    std::vector<uint8_t> line(row);
    for (int y = SCREEN_H - 1; y >= 0; y--) {
        for (int x = 0; x < SCREEN_W; x++) {
            uint16_t p = frame[y * SCREEN_W + x];
            line[x * 3 + 0] = (uint8_t)((p & 0x1F) << 3);
            line[x * 3 + 1] = (uint8_t)(((p >> 5) & 0x3F) << 2);
            line[x * 3 + 2] = (uint8_t)(((p >> 11) & 0x1F) << 3);
        }
        fwrite(line.data(), 1, row, f);
    }
    fclose(f);
    return true;
}

int screenshots(const std::string& dir) {
    static uint16_t frame[SCREEN_W * SCREEN_H];
    lv_display_t* display = lv_display_create(SCREEN_W, SCREEN_H);
    lv_display_set_buffers(display, frame, nullptr, sizeof frame, LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(display, [](lv_display_t* d, const lv_area_t*, uint8_t*) {
        lv_display_flush_ready(d);
    });
    app::init();
    mkdir(dir.c_str(), 0755);
    const char* const names[] = {"dashboard", "photo", "aquarium"};
    for (int page = 0; page < 3; page++) {
        app::show_page(page);
        pump(page == 1 ? 2500 : 1300);
        lv_refr_now(display);
        std::string path = dir + "/" + names[page] + ".bmp";
        if (!write_bmp(path, frame)) return 1;
        printf("wrote %s\n", path.c_str());
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    std::string shot_dir;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--server" && i + 1 < argc) server = argv[++i];
        else if (arg == "--fixtures" && i + 1 < argc) fixtures = argv[++i];
        else if (arg == "--screenshot" && i + 1 < argc) shot_dir = argv[++i];
        else {
            fprintf(stderr, "usage: %s [--server URL] [--fixtures DIR] [--screenshot DIR]\n", argv[0]);
            return 2;
        }
    }
    while (!server.empty() && server.back() == '/') server.pop_back();

    curl_global_init(CURL_GLOBAL_DEFAULT);
    lv_init();
    lv_tick_set_cb(plat::millis);

    if (!shot_dir.empty()) return screenshots(shot_dir);

    lv_sdl_window_create(SCREEN_W, SCREEN_H);
    lv_sdl_mouse_create();
    app::init();
    printf("Showing data from %s. Drag left or right to change pages.\n",
           fixtures.empty() ? server.c_str() : fixtures.c_str());
    while (true) {
        uint32_t wait = lv_timer_handler();
        usleep((wait > 20 ? 20 : wait < 1 ? 1 : wait) * 1000);
    }
}
