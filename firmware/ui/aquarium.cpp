// Page 4: the aquarium.
//
// The tank itself is npceight's Desk_Aquarium (see ../tank/README.md). It has its own drawing
// engine that paints whole-screen buffers and only touches what moved. This file hosts it as
// a page of the desk display:
//   - on the board, the tank takes over the panel while its page is showing, for full speed;
//   - in the simulator, it paints into one buffer that the page shows as a picture.
// It is fed the time, weather and sunrise/sunset the rest of the screen already has, so it
// needs no Wi-Fi or clock of its own.
//
// Keep this file free of the other ui/ headers: the tank defines short names of its own.
#include <cmath>
#include <cstring>

#include "app.h"
#include "lvgl.h"
#include "platform.h"

#include "settings.h"
#include "tank.h"
#include "controls.h"

namespace {

Gfx gfx;
Tank tank;
Controls controls;
DayNight lights;

uint16_t *bgDay, *bgNight, *cleanDay, *cleanNight;
uint16_t *clockSaveDay, *clockSaveNight, *wxSaveDay, *wxSaveNight;
uint16_t* screens[3] = {nullptr, nullptr, nullptr};
int screenCount = 1;

lv_obj_t* tile = nullptr;
lv_obj_t* view = nullptr;  // simulator only: the picture of the tank
lv_image_dsc_t viewDsc;

bool ready = false, failed = false, running = false, needFull = true, firstFrame = true;
bool wantNight = false, showClock = SHOW_CLOCK;
bool nightOverride = false, overrideNight = false, overrideBase = false;
int back = 0, brightStep = 0, lastLevel = -1;
float t = 0;
uint32_t lastMs = 0, lastNightCheck = 0, lastFeed = 0, skyUntil = 0, statAt = 0;
int frames = 0;

// From the server, by way of aquarium::update().
int weatherCode = -1;
float weatherTemp = NAN;
int sunriseMin = 7 * 60, sunsetMin = 18 * 60;

// Day or night by the real sun, with the tank's own margin either side.
bool nightBySun() {
    if (NIGHT_MODE == 1) return false;
    if (NIGHT_MODE == 2) return true;
    struct tm now;
    if (!app::local_time(now)) return false;
    int minute = now.tm_hour * 60 + now.tm_min;
    return minute < sunriseMin - SUN_MARGIN_MINUTES || minute >= sunsetMin + SUN_MARGIN_MINUTES;
}

bool prepare() {
    if (ready || failed) return ready;
    const size_t whole = (size_t)SCR_W * SCR_H * 2;
    bgDay = (uint16_t*)plat::big_alloc(whole);
    bgNight = (uint16_t*)plat::big_alloc(whole);
    if (plat::panel_direct()) {
        screenCount = plat::panel_buffers(screens);
    } else {
        screens[0] = (uint16_t*)plat::big_alloc(whole);
        screenCount = 1;
    }
    if (!bgDay || !bgNight || !screens[0]) {
        failed = true;
        return false;
    }
    tank.begin(plat::millis() | 1);
    // The rest are optional: without them fish show through the castle and the corners are
    // simply redrawn every frame.
    size_t strip = (size_t)(SCR_H - sceneryTop()) * SCR_W * 2;
    cleanDay = (uint16_t*)plat::big_alloc(strip);
    cleanNight = (uint16_t*)plat::big_alloc(strip);
    if (!cleanDay || !cleanNight) cleanDay = cleanNight = nullptr;
    clockSaveDay = (uint16_t*)plat::big_alloc((size_t)Tank::CLOCK_W * Tank::CLOCK_H * 2);
    clockSaveNight = (uint16_t*)plat::big_alloc((size_t)Tank::CLOCK_W * Tank::CLOCK_H * 2);
    wxSaveDay = (uint16_t*)plat::big_alloc((size_t)Tank::WX_W * Tank::WX_H * 2);
    wxSaveNight = (uint16_t*)plat::big_alloc((size_t)Tank::WX_W * Tank::WX_H * 2);
    tank.renderBackground(gfx, bgDay, false, cleanDay);
    tank.renderBackground(gfx, bgNight, true, cleanNight);
    gfx.cleanY0 = sceneryTop();

    if (!plat::panel_direct()) {
        memset(&viewDsc, 0, sizeof viewDsc);
        viewDsc.header.magic = LV_IMAGE_HEADER_MAGIC;
        viewDsc.header.cf = LV_COLOR_FORMAT_RGB565;
        viewDsc.header.w = SCR_W;
        viewDsc.header.h = SCR_H;
        viewDsc.header.stride = SCR_W * 2;
        viewDsc.data_size = (uint32_t)whole;
        viewDsc.data = (const uint8_t*)screens[0];
        view = lv_image_create(tile);
        lv_obj_set_pos(view, 0, 0);
    }
    ready = true;
    return true;
}

void tap(int x, int y) {
    uint32_t now = plat::millis();
    switch (controls.tap(x, y)) {
        case CTL_FEED:
            tank.dropFood(controls.feedX >= 0 ? controls.feedX : rnd(W * .2f, W * .8f));
            break;
        case CTL_NIGHT: {  // until the sun next changes its mind
            bool bySun = nightBySun();
            overrideNight = !wantNight;
            nightOverride = overrideNight != bySun;
            overrideBase = bySun;
            wantNight = overrideNight;
            break;
        }
        case CTL_WEATHER:  // a sky picked by hand lasts an hour
            tank.weather = (tank.weather + 1) % WEATHER_COUNT;
            skyUntil = now + 3600UL * 1000;
            break;
        case CTL_BRIGHT: brightStep = (brightStep + 1) % 4; break;
        case CTL_CLOCK: showClock = !showClock; break;
        case CTL_VISITOR:
            if (!tank.whaleOn) tank.startWhale(rnd01() < .5f ? SP_WHALE : SP_TURTLE);
            break;
        default: break;
    }
}

// If the board cannot keep up, turn down the most expensive effect still on.
void autoQuality(float fps) {
    if (!AUTO_QUALITY || MIN_FPS <= 0 || fps >= MIN_FPS) return;
    if (tank.glow && !tank.glowLight) tank.glowLight = true;
    else if (tank.glow) tank.glow = false;
    else if (tank.solid) tank.solid = false;
    else if (tank.shadows) tank.shadows = false;
}

void frame(lv_timer_t*) {
    if (!running) return;
    uint32_t now = plat::millis();
    float dt = (now - lastMs) / 1000.0f;
    if (dt > 0.1f) dt = 0.1f;
    lastMs = now;
    t += dt;
    if (t > 3600) t -= 3600;

    bool full = needFull;
    needFull = false;
    if (firstFrame || now - lastNightCheck > 1000) {
        lastNightCheck = now;
        bool bySun = nightBySun();
        if (nightOverride && bySun != overrideBase) nightOverride = false;
        wantNight = nightOverride ? overrideNight : bySun;
        if ((int32_t)(now - skyUntil) > 0) tank.weather = weatherFromCode(weatherCode);
    }
    controls.update(dt);
    if (firstFrame) lights.set(wantNight);
    if (lights.step(wantNight, dt) || firstFrame) full = true;  // the background swapped
    tank.night = lights.night;

    int level = lights.brightness();
    if (tank.weather != WEATHER_CLEAR && !tank.night) level = level * 85 / 100;  // overcast
    if (tank.flash > 0) level += (int)((100 - level) * tank.flash);              // lightning
    level = (int)lrintf(level * brightnessStep(brightStep));
    if (level != lastLevel) {
        plat::backlight(level);
        lastLevel = level;
    }

    if (FEED_EVERY_MINUTES > 0) {  // first meal about a minute after the tank first appears
        uint32_t every = FEED_EVERY_MINUTES * 60000UL;
        if (firstFrame) lastFeed = now - every + 60000;
        if (now - lastFeed >= every && (!tank.night || FEED_AT_NIGHT)) {
            lastFeed = now;
            tank.dropFoodRandom();
        }
    }

    char clockText[72] = "";
    struct tm ti;
    if (showClock && app::local_time(ti)) {
        formatClock(clockText, sizeof clockText, ti.tm_hour, ti.tm_min, ti.tm_sec, ti.tm_wday,
                    ti.tm_mon, ti.tm_mday);
    }

    tank.update(dt, t);
    plat::panel_wait(back);  // never draw into the buffer that is on screen
    tank.bakeClock(gfx, bgDay, bgNight, clockSaveDay, clockSaveNight, clockText);
    if (USE_WEATHER && SHOW_WEATHER) {
        tank.bakeWeather(gfx, bgDay, bgNight, wxSaveDay, wxSaveNight, weatherCode, weatherTemp);
    }
    gfx.clean = tank.night ? cleanNight : cleanDay;
    gfx.beginFrame(back, screens[back], tank.night ? bgNight : bgDay, full);
    tank.draw(gfx, t, clockText);
    controls.setLabels(wantNight, tank.weather, (int)(brightnessStep(brightStep) * 100), showClock,
                       tank.whaleOn);
    controls.draw(gfx);
    plat::panel_show(back);
    if (view) {
        if (firstFrame) lv_image_set_src(view, &viewDsc);
        lv_obj_invalidate(view);
    }
    back = (back + 1) % screenCount;
    firstFrame = false;

    frames++;
    if (now - statAt > 10000) {
        if (statAt) autoQuality(frames * 1000.0f / (now - statAt));  // skip the start-up window
        statAt = now;
        frames = 0;
    }
}

}  // namespace

namespace aquarium {

void build(lv_obj_t* t) {
    tile = t;
    lv_obj_set_style_bg_color(t, lv_color_hex(0x04263F), 0);
    lv_obj_add_event_cb(
        t,
        [](lv_event_t*) {  // a tap, not a swipe: feed, or bring out the buttons
            lv_point_t p;
            lv_indev_get_point(lv_indev_active(), &p);
            if (running) tap(p.x, p.y);
        },
        LV_EVENT_SHORT_CLICKED, nullptr);
    lv_timer_create(frame, 1000 / TARGET_FPS, nullptr);
}

void update(const Model& model) {
    weatherCode = model.has_weather ? model.weather_code : -1;
    weatherTemp = model.has_weather ? (float)model.temp : NAN;
    sunriseMin = model.sunrise_min;
    sunsetMin = model.sunset_min;
}

void tick(bool active) {
    if (active && !running) {
        if (!prepare()) return;
        running = true;
        needFull = true;  // the rest of the screen has drawn over the tank's buffers
        lastMs = plat::millis();
        lastLevel = -1;
        plat::panel_hold(true);
    } else if (!active && running) {
        running = false;
        plat::panel_hold(false);
        lv_obj_invalidate(lv_screen_active());
    }
}

bool running_now() { return running; }

}  // namespace aquarium
