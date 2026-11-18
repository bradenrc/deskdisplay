// Colours, fonts and screen size shared by every page.
#pragma once
#include "lvgl.h"

#define SCREEN_W 1024
#define SCREEN_H 600

#define C_BG 0x0B0B0B
#define C_PANEL 0x1A1A19
#define C_LINE 0x2C2C2A
#define C_TEXT 0xF1EFE8
#define C_MUTED 0xB4B2A9
#define C_DIM 0x888780
#define C_FAINT 0x5F5E5A
#define C_AMBER 0xEF9F27
#define C_AMBER_TEXT 0xFAC775
#define C_RED 0xE24B4A
#define C_RED_TEXT 0xF09595
#define C_GREEN 0x5DCAA5
#define C_BLUE 0x85B7EB
#define C_ORANGE 0xD85A30
#define C_CLOUD 0xB5D4F4

LV_FONT_DECLARE(font_text_16)
LV_FONT_DECLARE(font_text_18)
LV_FONT_DECLARE(font_text_20)
LV_FONT_DECLARE(font_text_24)
LV_FONT_DECLARE(font_text_28)
LV_FONT_DECLARE(font_text_40)
LV_FONT_DECLARE(font_clock_56)
LV_FONT_DECLARE(font_clock_120)
LV_FONT_DECLARE(font_clock_140)

static inline lv_color_t col(uint32_t hex) { return lv_color_hex(hex); }

// A label with font and colour set in one call.
static inline lv_obj_t* make_label(lv_obj_t* parent, const lv_font_t* font, uint32_t hex,
                                   const char* text = "") {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, col(hex), 0);
    lv_label_set_text(label, text);
    return label;
}

// Tile colours: each kind of tile has its own flat dark tint, so the screen is not one grey.
// Flat on purpose: on a 16-bit panel a dark gradient shows visible bands.
#define C_NEWS_TILE 0x18222E
#define C_SNOW_TILE 0x122A36
#define C_GAME_TILE 0x23201B
#define C_LIVE_TILE 0x331A1A
#define C_ROW_ALT 0x151514  // every other agenda row, barely lighter than the background

static inline void tint(lv_obj_t* obj, uint32_t hex) {
    lv_obj_set_style_bg_color(obj, lv_color_hex(hex), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
}

// A plain container: no background, border, padding or scrolling.
static inline lv_obj_t* make_box(lv_obj_t* parent) {
    lv_obj_t* box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    return box;
}
