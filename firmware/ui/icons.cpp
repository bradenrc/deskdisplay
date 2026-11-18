#include "icons.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "theme.h"

namespace {

struct State {
    char key[16];
    float phase;
};

struct Pen {
    lv_layer_t* layer;
    int x, y, s;  // top-left corner and size of the icon in pixels

    int px(float f) const { return (int)lroundf(f * s); }

    void circle(float cx, float cy, float r, uint32_t hex) const {
        lv_draw_rect_dsc_t d;
        lv_draw_rect_dsc_init(&d);
        d.bg_color = col(hex);
        d.radius = LV_RADIUS_CIRCLE;
        lv_area_t a = {x + px(cx - r), y + px(cy - r), x + px(cx + r), y + px(cy + r)};
        lv_draw_rect(layer, &d, &a);
    }

    void bar(float x1, float y1, float x2, float y2, uint32_t hex) const {
        lv_draw_rect_dsc_t d;
        lv_draw_rect_dsc_init(&d);
        d.bg_color = col(hex);
        d.radius = LV_RADIUS_CIRCLE;
        lv_area_t a = {x + px(x1), y + px(y1), x + px(x2), y + px(y2)};
        lv_draw_rect(layer, &d, &a);
    }

    void line(float x1, float y1, float x2, float y2, float w, uint32_t hex) const {
        lv_draw_line_dsc_t d;
        lv_draw_line_dsc_init(&d);
        d.color = col(hex);
        d.width = LV_MAX(2, px(w));
        d.round_start = 1;
        d.round_end = 1;
        d.p1.x = x + px(x1);
        d.p1.y = y + px(y1);
        d.p2.x = x + px(x2);
        d.p2.y = y + px(y2);
        lv_draw_line(layer, &d);
    }

    void triangle(float x1, float y1, float x2, float y2, float x3, float y3, uint32_t hex) const {
        lv_draw_triangle_dsc_t d;
        lv_draw_triangle_dsc_init(&d);
        d.bg_color = col(hex);
        d.bg_opa = LV_OPA_COVER;
        d.p[0].x = x + px(x1);
        d.p[0].y = y + px(y1);
        d.p[1].x = x + px(x2);
        d.p[1].y = y + px(y2);
        d.p[2].x = x + px(x3);
        d.p[2].y = y + px(y3);
        lv_draw_triangle(layer, &d);
    }

    void sun(float cx, float cy, float r) const {
        for (int i = 0; i < 8; i++) {
            float a = i * 3.14159f / 4;
            line(cx + cosf(a) * r * 1.45f, cy + sinf(a) * r * 1.45f, cx + cosf(a) * r * 1.95f,
                 cy + sinf(a) * r * 1.95f, 0.06f, C_AMBER);
        }
        circle(cx, cy, r, C_AMBER);
    }

    // Drawn row by row as "disc minus an offset disc", so it works on any background colour.
    void crescent(float cx, float cy, float r) const {
        lv_draw_rect_dsc_t d;
        lv_draw_rect_dsc_init(&d);
        d.bg_color = col(0xCECBF6);
        float R = r * s, bite = R * 0.85f;
        float ox = x + cx * s, oy = y + cy * s;       // centre of the moon
        float bx = ox + R * 0.45f, by = oy - R * 0.25f;  // centre of the part cut away
        for (int row = (int)floorf(oy - R); row <= (int)ceilf(oy + R); row++) {
            float dy = row + 0.5f - oy;
            if (fabsf(dy) >= R) continue;
            float half = sqrtf(R * R - dy * dy);
            float right = ox + half;
            float cut = row + 0.5f - by;
            if (fabsf(cut) < bite) right = LV_MIN(right, bx - sqrtf(bite * bite - cut * cut));
            int x1 = (int)lroundf(ox - half), x2 = (int)lroundf(right) - 1;
            if (x2 < x1) continue;
            lv_area_t a = {x1, row, x2, row};
            lv_draw_rect(layer, &d, &a);
        }
    }

    // A cloud about 0.8 wide, centred on (cx, cy).
    void cloud(float cx, float cy, uint32_t hex = C_CLOUD) const {
        circle(cx - 0.20f, cy + 0.04f, 0.16f, hex);
        circle(cx + 0.02f, cy - 0.08f, 0.22f, hex);
        circle(cx + 0.24f, cy + 0.05f, 0.15f, hex);
        bar(cx - 0.36f, cy + 0.02f, cx + 0.39f, cy + 0.20f, hex);
    }
};

void draw_icon(const Pen& p, const char* key) {
    if (!strcmp(key, "clear")) {
        p.sun(0.5f, 0.5f, 0.2f);
    } else if (!strcmp(key, "clear-night")) {
        p.crescent(0.5f, 0.5f, 0.32f);
    } else if (!strcmp(key, "partly")) {
        p.sun(0.66f, 0.34f, 0.15f);
        p.cloud(0.44f, 0.58f);
    } else if (!strcmp(key, "partly-night")) {
        p.crescent(0.68f, 0.32f, 0.2f);
        p.cloud(0.44f, 0.58f);
    } else if (!strcmp(key, "fog")) {
        p.line(0.14f, 0.34f, 0.86f, 0.34f, 0.08f, C_MUTED);
        p.line(0.24f, 0.52f, 0.76f, 0.52f, 0.08f, C_MUTED);
        p.line(0.14f, 0.70f, 0.86f, 0.70f, 0.08f, C_MUTED);
    } else if (!strcmp(key, "rain") || !strcmp(key, "drizzle")) {
        p.cloud(0.5f, 0.36f);
        int drops = !strcmp(key, "rain") ? 3 : 2;
        for (int i = 0; i < drops; i++) {
            float dx = (drops == 3 ? 0.30f : 0.40f) + i * 0.2f;
            p.line(dx + 0.04f, 0.68f, dx - 0.04f, 0.88f, 0.07f, 0x378ADD);
        }
    } else if (!strcmp(key, "snow")) {
        p.cloud(0.5f, 0.36f);
        for (int i = 0; i < 3; i++) p.circle(0.30f + i * 0.2f, 0.78f + (i % 2) * 0.08f, 0.05f, C_TEXT);
    } else if (!strcmp(key, "storm")) {
        p.cloud(0.5f, 0.36f);
        p.triangle(0.56f, 0.58f, 0.36f, 0.80f, 0.52f, 0.80f, C_AMBER);
        p.triangle(0.46f, 0.76f, 0.62f, 0.76f, 0.42f, 0.98f, C_AMBER);
    } else if (!strcmp(key, "sunrise") || !strcmp(key, "sunset")) {
        bool rise = !strcmp(key, "sunrise");
        uint32_t hex = rise ? C_AMBER : C_ORANGE;
        lv_draw_arc_dsc_t arc;  // half a sun sitting on the horizon
        lv_draw_arc_dsc_init(&arc);
        arc.color = col(hex);
        arc.center.x = p.x + p.px(0.5f);
        arc.center.y = p.y + p.px(0.74f);
        arc.radius = p.px(0.24f);
        arc.width = p.px(0.24f);
        arc.start_angle = 180;
        arc.end_angle = 360;
        lv_draw_arc(p.layer, &arc);
        p.line(0.10f, 0.80f, 0.90f, 0.80f, 0.07f, hex);
        float tip = rise ? 0.12f : 0.36f, tail = rise ? 0.36f : 0.12f, wing = rise ? 0.24f : 0.24f;
        p.line(0.5f, tail, 0.5f, tip, 0.07f, hex);
        p.line(0.5f, tip, 0.38f, wing, 0.07f, hex);
        p.line(0.5f, tip, 0.62f, wing, 0.07f, hex);
    } else {  // cloudy, and anything unknown
        p.cloud(0.5f, 0.48f);
    }
}

void draw_moon(const Pen& p, float phase) {
    p.circle(0.5f, 0.5f, 0.5f, C_LINE);
    int r = p.s / 2;
    float k = cosf(2 * 3.14159f * phase);
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = col(C_TEXT);
    for (int row = -r; row < r; row++) {
        float half = sqrtf((float)(r * r) - (row + 0.5f) * (row + 0.5f));
        float left = phase <= 0.5f ? half * k : -half;
        float right = phase <= 0.5f ? half : -half * k;
        int x1 = p.x + r + (int)lroundf(left), x2 = p.x + r + (int)lroundf(right) - 1;
        if (x2 < x1) continue;
        lv_area_t a = {x1, p.y + r + row, x2, p.y + r + row};
        lv_draw_rect(p.layer, &d, &a);
    }
}

void on_draw(lv_event_t* e) {
    lv_obj_t* obj = (lv_obj_t*)lv_event_get_target(e);
    State* st = (State*)lv_obj_get_user_data(obj);
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    Pen p = {lv_event_get_layer(e), a.x1, a.y1, (int)lv_area_get_width(&a)};
    if (st->key[0]) {
        draw_icon(p, st->key);
    } else {
        draw_moon(p, st->phase);
    }
}

void on_delete(lv_event_t* e) { free(lv_obj_get_user_data((lv_obj_t*)lv_event_get_target(e))); }

lv_obj_t* create(lv_obj_t* parent, int size) {
    lv_obj_t* obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, size, size);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_user_data(obj, calloc(1, sizeof(State)));
    lv_obj_add_event_cb(obj, on_draw, LV_EVENT_DRAW_MAIN, nullptr);
    lv_obj_add_event_cb(obj, on_delete, LV_EVENT_DELETE, nullptr);
    return obj;
}

}  // namespace

lv_obj_t* icon_create(lv_obj_t* parent, int size, const char* key) {
    lv_obj_t* obj = create(parent, size);
    icon_set(obj, key);
    return obj;
}

void icon_set(lv_obj_t* icon, const char* key) {
    State* st = (State*)lv_obj_get_user_data(icon);
    if (strncmp(st->key, key, sizeof(st->key)) == 0) return;
    strncpy(st->key, key, sizeof(st->key) - 1);
    lv_obj_invalidate(icon);
}

lv_obj_t* moon_create(lv_obj_t* parent, int size) { return create(parent, size); }

void moon_set(lv_obj_t* moon, float phase) {
    State* st = (State*)lv_obj_get_user_data(moon);
    st->key[0] = 0;
    st->phase = phase;
    lv_obj_invalidate(moon);
}
