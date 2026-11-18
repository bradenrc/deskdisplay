#include "app.h"

#include <cstdio>
#include <cstring>

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

// Night clock: a black layer over everything, with the time in dark grey.
constexpr uint32_t SNOOZE_MS = 10 * 60 * 1000;
lv_obj_t *night_box, *night_time, *night_date;
uint32_t snoozed_at = 0;
bool snoozed = false, night_forced = false;

void night_refresh() {
    if (snoozed && plat::millis() - snoozed_at > SNOOZE_MS) snoozed = false;
    if (!(model.night || night_forced)) snoozed = false;  // a new night starts un-snoozed
    bool show = (model.night || night_forced) && !snoozed;
    if (!show) {
        lv_obj_add_flag(night_box, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_remove_flag(night_box, LV_OBJ_FLAG_HIDDEN);
    char text[16];
    app::clock_text(text, sizeof text);
    if (strcmp(lv_label_get_text(night_time), text) != 0) lv_label_set_text(night_time, text);
}

void night_build() {
    night_box = make_box(lv_layer_top());
    lv_obj_set_size(night_box, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(night_box, col(0x000000), 0);
    lv_obj_set_style_bg_opa(night_box, LV_OPA_COVER, 0);
    lv_obj_add_flag(night_box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(night_box, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(
        night_box,
        [](lv_event_t*) {  // any touch: back to the dashboard for a while
            snoozed = true;
            snoozed_at = plat::millis();
            lv_obj_add_flag(night_box, LV_OBJ_FLAG_HIDDEN);
        },
        LV_EVENT_PRESSED, nullptr);
    night_time = make_label(night_box, &font_clock_140, 0x4A4945);
    lv_obj_align(night_time, LV_ALIGN_CENTER, 0, -10);
    night_date = make_label(night_box, &font_text_20, 0x3A3936, "Touch to wake");
    lv_obj_align(night_date, LV_ALIGN_CENTER, 0, 90);
}

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
        night_refresh();
    } else if (!synced) {
        dashboard::set_status("Can't reach the server");
    }
}

void every_second(lv_timer_t*) {
    dashboard::tick();
    photo::tick(app::active_page() == 1);
    night_refresh();
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
    night_build();

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

void force_night(bool on) {
    night_forced = on;
    night_refresh();
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
