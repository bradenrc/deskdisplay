#include "app.h"

#include <cstdio>

#include "platform.h"
#include "theme.h"

namespace {

constexpr uint32_t POLL_MS = 30 * 1000;

lv_obj_t* tileview;
lv_obj_t* tiles[3];
Model model;
long long sync_epoch = 0;
uint32_t sync_ms = 0;
bool synced = false;

void poll(lv_timer_t*) {
    std::vector<uint8_t> body;
    Model fresh;
    if (plat::http_get("/api/dashboard", body) &&
        parse_model((const char*)body.data(), body.size(), fresh)) {
        model = fresh;
        sync_epoch = model.epoch;
        sync_ms = plat::millis();
        synced = true;
        dashboard::update(model);
    } else if (!synced) {
        dashboard::set_status("Can't reach the server");
    }
}

void every_second(lv_timer_t*) {
    dashboard::tick();
    photo::tick(app::active_page() == 1);
}

void every_tenth(lv_timer_t*) { aquarium::tick(app::active_page() == 2); }

void headline(lv_timer_t*) { dashboard::next_headline(); }

}  // namespace

namespace app {

void init() {
    lv_obj_t* screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, col(C_BG), 0);

    tileview = lv_tileview_create(screen);
    lv_obj_set_size(tileview, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(tileview, col(C_BG), 0);
    lv_obj_set_scrollbar_mode(tileview, LV_SCROLLBAR_MODE_OFF);
    for (int i = 0; i < 3; i++) {
        tiles[i] = lv_tileview_add_tile(tileview, i, 0, LV_DIR_HOR);
        lv_obj_remove_flag(tiles[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(tiles[i], col(C_BG), 0);
        lv_obj_set_style_bg_opa(tiles[i], LV_OPA_COVER, 0);
    }
    dashboard::build(tiles[0]);
    photo::build(tiles[1]);
    aquarium::build(tiles[2]);

    lv_timer_ready(lv_timer_create(poll, POLL_MS, nullptr));
    lv_timer_create(every_second, 1000, nullptr);
    lv_timer_create(every_tenth, 100, nullptr);
    lv_timer_create(headline, 8000, nullptr);
}

void show_page(int index) {
    if (index >= 0 && index < 3) lv_tileview_set_tile_by_index(tileview, index, 0, LV_ANIM_OFF);
}

int active_page() {
    lv_obj_t* active = lv_tileview_get_tile_active(tileview);
    for (int i = 0; i < 3; i++) {
        if (tiles[i] == active) return i;
    }
    return 0;
}

bool local_time(struct tm& out) {
    if (!synced) return false;
    time_t t = (time_t)(sync_epoch + (plat::millis() - sync_ms) / 1000 + model.utc_offset);
    gmtime_r(&t, &out);  // the offset is already applied, so "UTC" here is local time
    return true;
}

void clock_text(char* out, size_t n, const char** ampm) {
    struct tm now;
    if (!local_time(now)) {
        out[0] = 0;
        if (ampm) *ampm = "";
        return;
    }
    snprintf(out, n, "%d:%02d", now.tm_hour % 12 ? now.tm_hour % 12 : 12, now.tm_min);
    if (ampm) *ampm = now.tm_hour < 12 ? "am" : "pm";
}

}  // namespace app
