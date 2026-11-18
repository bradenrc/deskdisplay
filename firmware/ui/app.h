// The whole user interface. Both the simulator and the ESP32 call app::init() once after
// LVGL and a display exist, then keep calling lv_timer_handler().
#pragma once
#include <ctime>

#include "lvgl.h"
#include "model.h"

namespace app {
void init();
void show_page(int index);  // 0 dashboard, 1 photo frame, 2 aquarium
int active_page();
bool local_time(struct tm& out);            // false until the first reply from the server
void clock_text(char* out, size_t n, const char** ampm = nullptr);  // "10:52"
}  // namespace app

namespace dashboard {
void build(lv_obj_t* tile);
void update(const Model& model);
void set_status(const char* text);  // shown in place of the date until data arrives
void tick();                        // once a second
void next_headline();
}  // namespace dashboard

namespace photo {
constexpr int SECONDS_PER_PHOTO = 30;
void build(lv_obj_t* tile);
void tick(bool active);  // once a second
}  // namespace photo

namespace aquarium {
void build(lv_obj_t* tile);
void tick(bool active);  // ten times a second
}  // namespace aquarium
