// Page 3: a pixel-art aquarium.
//
// The scenery is painted once into a full-screen image. Fish and bubbles are small objects
// that move a few pixels at a time, so only the areas around them are redrawn. That keeps
// it smooth on the ESP32, where redrawing the whole 1024x600 screen every frame is slow.
#include <cmath>
#include <cstring>

#include "app.h"
#include "platform.h"
#include "theme.h"

namespace {

constexpr int PX = 4;  // one art pixel = 4 screen pixels
constexpr int ART_W = SCREEN_W / PX, ART_H = SCREEN_H / PX;
constexpr int SAND = 12;  // art pixels of sand at the bottom

constexpr int FISH_W = 12, FISH_H = 7;
const char* const FISH_ART[FISH_H] = {
    "....XXXX....",
    "X..XXXXXXX..",
    "XXXXXSXXoXX.",
    "XXXXXSXXXXXX",
    "XXXXXSXXXXX.",
    "X..XXXXXXX..",
    "....XXXX....",
};

struct Fish {
    uint32_t body, stripe;
    int block;  // screen pixels per sprite pixel: bigger = closer fish
    int speed;  // art pixels per tick
    lv_obj_t* obj;
    lv_image_dsc_t facing[2];  // right, left
    int x, y, dir;
};

Fish fishes[] = {
    {0xF0997B, 0xFAECE7, 12, 1},
    {0xFAC775, 0xEF9F27, 8, 1},
    {0xAFA9EC, 0x7F77DD, 8, 2},
    {0x5DCAA5, 0x1D9E75, 4, 1},
    {0xED93B1, 0xD4537E, 4, 2},
    {0x85B7EB, 0x378ADD, 4, 1},
};

struct Bubble {
    lv_obj_t* obj;
    int home_x, y, rate;
};
Bubble bubbles[] = {{nullptr, 500, 0, 2}, {nullptr, 516, 0, 3}, {nullptr, 150, 0, 2}, {nullptr, 880, 0, 3}};

uint16_t* scene = nullptr;
lv_image_dsc_t scene_dsc;
lv_obj_t* clock_lbl;
uint32_t ticks = 0;

uint16_t rgb565(uint32_t hex) {
    return (uint16_t)(((hex >> 8) & 0xF800) | ((hex >> 5) & 0x07E0) | ((hex >> 3) & 0x001F));
}

void dot(int ax, int ay, uint32_t hex) {  // one art pixel
    if (ax < 0 || ay < 0 || ax >= ART_W || ay >= ART_H) return;
    uint16_t c = rgb565(hex);
    for (int y = ay * PX; y < ay * PX + PX; y++) {
        for (int x = ax * PX; x < ax * PX + PX; x++) scene[y * SCREEN_W + x] = c;
    }
}

void block(int ax, int ay, int w, int h, uint32_t hex) {
    for (int y = ay; y < ay + h; y++) {
        for (int x = ax; x < ax + w; x++) dot(x, y, hex);
    }
}

void plant(int ax, int height, uint32_t dark, uint32_t light) {
    int floor = ART_H - SAND;
    for (int i = 0; i < height; i++) {
        int sway = (int)lroundf(sinf(i / 5.0f + ax) * 2);
        block(ax + sway, floor - 1 - i, 3, 1, (i / 4) % 2 ? light : dark);
    }
}

void paint_scene() {
    const uint32_t water[] = {0x0C447C, 0x083A6B, 0x06325C, 0x042C53};
    for (int y = 0; y < ART_H; y++) {
        int band = y < 14 ? 0 : y < 40 ? 1 : y < 80 ? 2 : 3;
        // dither the band edges so they read as light fading with depth
        if ((y == 14 || y == 40 || y == 80)) band--;
        for (int x = 0; x < ART_W; x++) {
            bool edge = (y == 14 || y == 40 || y == 80) && (x % 2);
            dot(x, y, water[edge ? band + 1 : band]);
        }
    }
    int floor = ART_H - SAND;
    block(0, floor, ART_W, SAND, 0x633806);
    for (int x = 0; x < ART_W; x++) {
        if (x % 3 == 0) dot(x, floor, 0x854F0B);
        if ((x * 7) % 11 == 0) dot(x, floor + 3 + (x % 5), 0x854F0B);
        if ((x * 5) % 13 == 0) dot(x, floor + 2 + (x % 7), 0x412402);
    }
    plant(18, 46, 0x0F6E56, 0x1D9E75);
    plant(27, 30, 0x1D9E75, 0x5DCAA5);
    plant(36, 18, 0x0F6E56, 0x1D9E75);
    plant(206, 58, 0x1D9E75, 0x5DCAA5);
    plant(216, 40, 0x0F6E56, 0x1D9E75);
    plant(228, 24, 0x1D9E75, 0x5DCAA5);
    // rocks
    block(112, floor - 8, 30, 8, 0x5F5E5A);
    block(117, floor - 13, 20, 5, 0x5F5E5A);
    block(117, floor - 13, 20, 1, 0x888780);
    block(112, floor - 8, 5, 1, 0x888780);
    block(137, floor - 8, 5, 1, 0x888780);
    block(148, floor - 5, 14, 5, 0x444441);
    block(148, floor - 5, 14, 1, 0x5F5E5A);
}

void make_sprite(Fish& f, bool left) {
    int w = FISH_W * f.block, h = FISH_H * f.block;
    uint32_t* data = (uint32_t*)plat::big_alloc((size_t)w * h * 4);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int column = x / f.block;
            char c = FISH_ART[y / f.block][left ? FISH_W - 1 - column : column];
            uint32_t argb = 0;  // transparent
            if (c == 'X') argb = 0xFF000000 | f.body;
            if (c == 'S') argb = 0xFF000000 | f.stripe;
            if (c == 'o') argb = 0xFF0B0B0B;
            data[y * w + x] = argb;
        }
    }
    lv_image_dsc_t& d = f.facing[left ? 1 : 0];
    memset(&d, 0, sizeof d);
    d.header.magic = LV_IMAGE_HEADER_MAGIC;
    d.header.cf = LV_COLOR_FORMAT_ARGB8888;
    d.header.w = w;
    d.header.h = h;
    d.header.stride = w * 4;
    d.data_size = (uint32_t)w * h * 4;
    d.data = (const uint8_t*)data;
}

int swim_y(const Fish& f) {
    int top = 6, bottom = ART_H - SAND - 4 - FISH_H * f.block / PX;
    return (int)lv_rand(top, bottom) * PX;
}

void place(Fish& f) { lv_obj_set_pos(f.obj, f.x, f.y); }

}  // namespace

namespace aquarium {

void build(lv_obj_t* t) {
    scene = (uint16_t*)plat::big_alloc(SCREEN_W * SCREEN_H * 2);
    paint_scene();
    memset(&scene_dsc, 0, sizeof scene_dsc);
    scene_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    scene_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    scene_dsc.header.w = SCREEN_W;
    scene_dsc.header.h = SCREEN_H;
    scene_dsc.header.stride = SCREEN_W * 2;
    scene_dsc.data_size = SCREEN_W * SCREEN_H * 2;
    scene_dsc.data = (const uint8_t*)scene;
    lv_obj_t* background = lv_image_create(t);
    lv_image_set_src(background, &scene_dsc);
    lv_obj_set_pos(background, 0, 0);

    for (Bubble& b : bubbles) {
        b.obj = make_box(t);
        lv_obj_set_size(b.obj, PX * 2, PX * 2);
        lv_obj_set_style_border_width(b.obj, PX / 2, 0);
        lv_obj_set_style_border_color(b.obj, col(C_BLUE), 0);
        b.y = (int)lv_rand(60, SCREEN_H - SAND * PX - 20);
        lv_obj_set_pos(b.obj, b.home_x, b.y);
    }

    int i = 0;
    for (Fish& f : fishes) {
        make_sprite(f, false);
        make_sprite(f, true);
        f.dir = (i++ % 2) ? -1 : 1;
        f.x = (int)lv_rand(0, SCREEN_W - FISH_W * f.block) / PX * PX;
        f.y = swim_y(f);
        f.obj = lv_image_create(t);
        lv_image_set_src(f.obj, &f.facing[f.dir < 0 ? 1 : 0]);
        place(f);
    }

    clock_lbl = make_label(t, &font_clock_56, C_CLOUD);
    lv_obj_align(clock_lbl, LV_ALIGN_TOP_RIGHT, -24, 18);
}

void tick(bool active) {
    if (!active) return;
    ticks++;
    for (Fish& f : fishes) {
        f.x += f.dir * f.speed * PX;
        int w = FISH_W * f.block;
        if (f.x > SCREEN_W + PX * 6 || f.x < -w - PX * 6) {  // out of sight: turn around
            f.dir = -f.dir;
            f.y = swim_y(f);
            lv_image_set_src(f.obj, &f.facing[f.dir < 0 ? 1 : 0]);
        }
        place(f);
    }
    for (Bubble& b : bubbles) {
        b.y -= b.rate * PX / 2;
        int wobble = ((b.y / 24) % 2) * PX;
        if (b.y < -PX * 2) b.y = SCREEN_H - SAND * PX - PX * (10 + (int)lv_rand(0, 6));
        lv_obj_set_pos(b.obj, b.home_x + wobble, b.y);
    }
    if (ticks % 10 == 0) {
        char text[16];
        app::clock_text(text, sizeof text);
        if (strcmp(lv_label_get_text(clock_lbl), text) != 0) lv_label_set_text(clock_lbl, text);
    }
}

}  // namespace aquarium
