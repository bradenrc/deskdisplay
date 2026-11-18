// Small icons drawn from circles, lines and arcs, so no icon font or image files are needed.
#pragma once
#include "lvgl.h"

// Weather icon. Keys match the server: clear, clear-night, partly, partly-night, cloudy,
// fog, drizzle, rain, snow, storm. Also "sunrise" and "sunset".
lv_obj_t* icon_create(lv_obj_t* parent, int size, const char* key = "cloudy");
void icon_set(lv_obj_t* icon, const char* key);

// A shaded moon for any phase (0 new, 0.5 full, 1 new again). Implemented in moon.cpp.
lv_obj_t* moon_create(lv_obj_t* parent, int size);
void moon_set(lv_obj_t* moon, float phase);
