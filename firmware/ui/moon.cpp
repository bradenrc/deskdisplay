// The moon, lit for any phase from one small texture (ui/moon_texture.c).
//
// Each pixel is treated as a point on a sphere and lit from the sun's direction for the
// phase, with a soft edge between day and night and a faint glow on the dark side.
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "icons.h"

extern "C" {
extern const uint8_t moon_texture[64 * 64];
}

namespace {

constexpr int TEX = 64;

struct Moon {
    lv_image_dsc_t dsc;
    uint32_t* pixels;
    int size;
    float phase;
};

float sample(float u, float v) {  // bilinear lookup, u and v in 0..TEX-1
    u = LV_CLAMP(0.0f, u, TEX - 1.001f);
    v = LV_CLAMP(0.0f, v, TEX - 1.001f);
    int x = (int)u, y = (int)v;
    float fx = u - x, fy = v - y;
    const uint8_t* t = moon_texture + y * TEX + x;
    float top = t[0] + (t[1] - t[0]) * fx, bottom = t[TEX] + (t[TEX + 1] - t[TEX]) * fx;
    return (top + (bottom - top) * fy) / 255.0f;
}

float smooth(float lo, float hi, float value) {
    float t = LV_CLAMP(0.0f, (value - lo) / (hi - lo), 1.0f);
    return t * t * (3 - 2 * t);
}

void render(Moon* m) {
    float angle = 2 * 3.14159265f * m->phase;
    float sun_x = sinf(angle), sun_z = -cosf(angle);  // waxing lights the right-hand side
    float radius = m->size / 2.0f;
    for (int y = 0; y < m->size; y++) {
        for (int x = 0; x < m->size; x++) {
            float nx = (x + 0.5f - radius) / radius, ny = (y + 0.5f - radius) / radius;
            float distance = sqrtf(nx * nx + ny * ny);
            float cover = LV_CLAMP(0.0f, (1 - distance) * radius + 0.5f, 1.0f);  // smooth rim
            if (cover <= 0) {
                m->pixels[y * m->size + x] = 0;
                continue;
            }
            float nz = sqrtf(LV_MAX(0.0f, 1 - nx * nx - ny * ny));
            float lit = smooth(-0.04f, 0.14f, nx * sun_x + nz * sun_z);
            float face = sample((nx + 1) / 2 * (TEX - 1), (ny + 1) / 2 * (TEX - 1));
            float value = face * (0.09f + 0.91f * lit) * (0.80f + 0.20f * nz);
            uint32_t r = (uint32_t)(value * 255), g = (uint32_t)(value * 250), b = (uint32_t)(value * 236);
            m->pixels[y * m->size + x] = ((uint32_t)(cover * 255) << 24) | (r << 16) | (g << 8) | b;
        }
    }
}

void on_delete(lv_event_t* e) {
    Moon* m = (Moon*)lv_obj_get_user_data((lv_obj_t*)lv_event_get_target(e));
    lv_image_cache_drop(&m->dsc);
    free(m->pixels);
    free(m);
}

}  // namespace

lv_obj_t* moon_create(lv_obj_t* parent, int size) {
    Moon* m = (Moon*)calloc(1, sizeof(Moon));
    m->size = size;
    m->phase = -1;
    m->pixels = (uint32_t*)calloc((size_t)size * size, 4);
    m->dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    m->dsc.header.cf = LV_COLOR_FORMAT_ARGB8888;
    m->dsc.header.w = size;
    m->dsc.header.h = size;
    m->dsc.header.stride = size * 4;
    m->dsc.data_size = (uint32_t)size * size * 4;
    m->dsc.data = (const uint8_t*)m->pixels;
    lv_obj_t* obj = lv_image_create(parent);
    lv_obj_set_user_data(obj, m);
    lv_obj_add_event_cb(obj, on_delete, LV_EVENT_DELETE, nullptr);
    return obj;
}

void moon_set(lv_obj_t* moon, float phase) {
    Moon* m = (Moon*)lv_obj_get_user_data(moon);
    if (fabsf(phase - m->phase) < 0.004f) return;  // under three hours of change: not visible
    m->phase = phase;
    render(m);
    lv_image_cache_drop(&m->dsc);
    lv_image_set_src(moon, &m->dsc);
    lv_obj_invalidate(moon);
}
