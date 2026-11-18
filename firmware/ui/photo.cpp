// Page 2: full-screen photo from the server, with a caption and a small clock.
#include <cstring>

#include "app.h"
#include "platform.h"
#include "theme.h"

extern "C" {
#include "src/libs/tjpgd/tjpgd.h"
}

namespace {

lv_obj_t *image, *caption, *clock_lbl, *hint;
uint16_t* pixels = nullptr;  // SCREEN_W x SCREEN_H, RGB565
lv_image_dsc_t dsc;
uint32_t last_fetch = 0;
bool loaded = false;

struct Source {
    const uint8_t* data;
    size_t len, pos;
};

size_t read_bytes(JDEC* jd, uint8_t* buff, size_t n) {
    Source* src = (Source*)jd->device;
    if (n > src->len - src->pos) n = src->len - src->pos;
    if (buff) memcpy(buff, src->data + src->pos, n);
    src->pos += n;
    return n;
}

int write_block(JDEC*, void* bitmap, JRECT* rect) {
    const uint8_t* p = (const uint8_t*)bitmap;  // LVGL's decoder emits blue, green, red
    for (int y = rect->top; y <= rect->bottom; y++) {
        for (int x = rect->left; x <= rect->right; x++, p += 3) {
            if (x < SCREEN_W && y < SCREEN_H) {
                pixels[y * SCREEN_W + x] = (uint16_t)(((p[2] & 0xF8) << 8) | ((p[1] & 0xFC) << 3) | (p[0] >> 3));
            }
        }
    }
    return 1;
}

bool decode(const std::vector<uint8_t>& jpeg) {
    static uint8_t work[8192];
    Source src = {jpeg.data(), jpeg.size(), 0};
    JDEC jd;
    if (jd_prepare(&jd, read_bytes, work, sizeof work, &src) != JDR_OK) return false;
    if (jd.width != SCREEN_W || jd.height != SCREEN_H) memset(pixels, 0, SCREEN_W * SCREEN_H * 2);
    return jd_decomp(&jd, write_block, 0) == JDR_OK;
}

void load() {
    last_fetch = plat::millis();
    std::vector<uint8_t> jpeg;
    std::string text;
    char path[64];
    lv_snprintf(path, sizeof path, "/api/photo/next?w=%d&h=%d", SCREEN_W, SCREEN_H);
    if (!plat::http_get(path, jpeg, &text) || !decode(jpeg)) {
        if (!loaded) lv_label_set_text(hint, "No photos yet.\nChoose a folder on the server's setup page.");
        return;
    }
    loaded = true;
    lv_obj_add_flag(hint, LV_OBJ_FLAG_HIDDEN);
    lv_image_cache_drop(&dsc);
    lv_image_set_src(image, &dsc);
    lv_obj_invalidate(image);
    lv_label_set_text(caption, text.c_str());
    if (text.empty()) {
        lv_obj_add_flag(caption, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(caption, LV_OBJ_FLAG_HIDDEN);
    }
}

// Text over a photo needs a backing to stay readable on any picture.
void backing(lv_obj_t* label) {
    lv_obj_set_style_bg_color(label, col(0x000000), 0);
    lv_obj_set_style_bg_opa(label, LV_OPA_50, 0);
    lv_obj_set_style_radius(label, 10, 0);
    lv_obj_set_style_pad_hor(label, 14, 0);
    lv_obj_set_style_pad_ver(label, 8, 0);
}

}  // namespace

namespace photo {

void build(lv_obj_t* t) {
    lv_obj_set_style_bg_color(t, col(0x000000), 0);
    pixels = (uint16_t*)plat::big_alloc(SCREEN_W * SCREEN_H * 2);
    memset(pixels, 0, SCREEN_W * SCREEN_H * 2);
    memset(&dsc, 0, sizeof dsc);
    dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    dsc.header.w = SCREEN_W;
    dsc.header.h = SCREEN_H;
    dsc.header.stride = SCREEN_W * 2;
    dsc.data_size = SCREEN_W * SCREEN_H * 2;
    dsc.data = (const uint8_t*)pixels;

    image = lv_image_create(t);
    lv_obj_set_pos(image, 0, 0);
    hint = make_label(t, &font_text_20, C_MUTED, "Loading photos…");
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(hint);

    caption = make_label(t, &font_text_20, C_TEXT);
    backing(caption);
    lv_obj_align(caption, LV_ALIGN_BOTTOM_LEFT, 20, -18);
    lv_obj_add_flag(caption, LV_OBJ_FLAG_HIDDEN);

    clock_lbl = make_label(t, &font_clock_56, C_TEXT);
    backing(clock_lbl);
    lv_obj_align(clock_lbl, LV_ALIGN_BOTTOM_RIGHT, -20, -18);
}

void tick(bool active) {
    if (!active) return;
    char text[16];
    app::clock_text(text, sizeof text);
    if (strcmp(lv_label_get_text(clock_lbl), text) != 0) lv_label_set_text(clock_lbl, text);
    if (!loaded || plat::millis() - last_fetch > SECONDS_PER_PHOTO * 1000u) load();
}

}  // namespace photo
