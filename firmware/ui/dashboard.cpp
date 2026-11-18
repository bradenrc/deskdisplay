// Page 1: clock, one news headline, today's agenda and the weather strip.
#include <cstdio>
#include <cstring>
#include <string>

#include "app.h"
#include "icons.h"
#include "theme.h"

namespace {

constexpr int MAX_TITLE_LINES = 3;
constexpr size_t MAX_CHIPS = 3;
// The game cards and the snow tile share one column, so their edges line up.
constexpr int SIDE_X = 400, SIDE_W = 148;

const char* const DAYS[] = {"Sunday",   "Monday", "Tuesday", "Wednesday",
                            "Thursday", "Friday", "Saturday"};
const char* const MONTHS[] = {"January", "February", "March",     "April",   "May",      "June",
                              "July",    "August",   "September", "October", "November", "December"};

lv_obj_t *date_lbl, *time_lbl, *ampm_lbl;
lv_obj_t *news_box, *news_src, *news_head, *news_pager;
lv_obj_t *chips_box, *snow_box;
lv_obj_t *today_hdr, *list;
lv_obj_t *cur_icon, *cur_temp, *cur_cond, *cur_hl, *cur_air, *hour_box;
lv_obj_t *sunrise_lbl, *sunset_lbl, *moon, *moon_lbl;

Model model;
size_t news_index = 0, game_index = 0;
std::string status_text = "Connecting to the server…";

void show_headline() {
    if (model.news.empty()) {
        lv_label_set_text(news_src, "News");
        lv_label_set_text(news_head, model.valid ? "No headlines right now" : "");
        lv_label_set_text(news_pager, "");
        return;
    }
    news_index %= model.news.size();
    const NewsItem& item = model.news[news_index];
    std::string src = item.source;
    if (!item.age.empty()) src += "  ·  " + item.age;
    lv_label_set_text(news_src, src.c_str());
    lv_label_set_text(news_head, item.title.c_str());
    lv_label_set_text_fmt(news_pager, "%d / %d", (int)news_index + 1, (int)model.news.size());
}

// Up to MAX_CHIPS games in the space beside the clock. With more, the set rotates.
void show_games() {
    lv_obj_clean(chips_box);
    size_t count = model.games.size();
    if (count <= MAX_CHIPS) game_index = 0;
    for (size_t i = 0; i < count && i < MAX_CHIPS; i++) {
        const Game& game = model.games[(game_index + i) % count];
        bool live = game.state == "in";
        lv_obj_t* chip = make_box(chips_box);
        lv_obj_set_size(chip, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(chip, LV_FLEX_FLOW_COLUMN);
        tint(chip, live ? C_LIVE_TOP : C_GAME_TOP, live ? C_LIVE_BOTTOM : C_GAME_BOTTOM);
        lv_obj_set_style_radius(chip, 8, 0);
        lv_obj_set_style_pad_hor(chip, 10, 0);
        lv_obj_set_style_pad_ver(chip, 5, 0);
        if (live) {  // a game in progress gets a red edge
            lv_obj_set_style_border_side(chip, LV_BORDER_SIDE_LEFT, 0);
            lv_obj_set_style_border_width(chip, 3, 0);
            lv_obj_set_style_border_color(chip, col(C_RED), 0);
        }
        make_label(chip, &font_text_16, C_MUTED, game.line1.c_str());
        make_label(chip, &font_text_16, live ? C_RED_TEXT : game.state == "post" ? C_TEXT : C_DIM,
                   game.line2.c_str());
    }
}

// Fresh snow sits to the right of the news, which narrows to make room.
void show_snow() {
    constexpr int FULL = SIDE_X + SIDE_W - 28, GAP = 10;
    lv_obj_clean(snow_box);
    bool any = !model.snow.empty();
    lv_obj_set_width(news_box, any ? FULL - SIDE_W - GAP : FULL);
    const lv_font_t* font = any ? &font_text_24 : &font_text_28;
    lv_obj_set_style_text_font(news_head, font, 0);
    lv_obj_set_height(news_head, lv_font_get_line_height(font) * (any ? 5 : 4));
    if (!any) {
        lv_obj_add_flag(snow_box, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_remove_flag(snow_box, LV_OBJ_FLAG_HIDDEN);
    make_label(snow_box, &font_text_16, C_CLOUD, "Snow, 48 hours");
    for (const Snow& spot : model.snow) {
        lv_obj_t* block = make_box(snow_box);
        lv_obj_set_size(block, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(block, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_top(block, 12, 0);
        make_label(block, &font_text_16, C_MUTED, spot.name.c_str());
        lv_obj_t* line = make_box(block);  // amount, with the temperature beside it
        lv_obj_set_size(line, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(line, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(line, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
        make_label(line, &font_text_28, C_TEXT, spot.amount.c_str());
        lv_obj_set_style_pad_bottom(make_label(line, &font_text_16, C_DIM, spot.temp.c_str()), 4, 0);
        lv_obj_t* detail = make_label(block, &font_text_16, C_DIM, spot.detail.c_str());
        lv_obj_set_width(detail, lv_pct(100));
        lv_obj_set_height(detail, lv_font_get_line_height(&font_text_16));
        lv_label_set_long_mode(detail, LV_LABEL_LONG_DOT);
    }
}

void add_note(const char* text, uint32_t hex, const lv_font_t* font = &font_text_18) {
    lv_obj_t* note = make_label(list, font, hex, text);
    lv_obj_set_width(note, lv_pct(100));
    lv_obj_set_style_pad_left(note, 14, 0);
    lv_obj_set_style_pad_top(note, 8, 0);
    lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
}

// One agenda line. Returns the row so the caller can check that it fitted.
lv_obj_t* add_row(const Event& e) {
    bool now = e.status == "now", soon = e.status == "soon";
    const int line = lv_font_get_line_height(&font_text_20);

    lv_obj_t* row = make_box(list);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_hor(row, 12, 0);
    lv_obj_set_style_pad_ver(row, 9, 0);
    lv_obj_set_style_pad_column(row, 10, 0);
    lv_obj_set_style_radius(row, 10, 0);
    lv_obj_set_style_border_width(row, 2, 0);
    lv_obj_set_style_border_color(row, col(C_AMBER), 0);
    lv_obj_set_style_border_opa(row, soon ? LV_OPA_COVER : LV_OPA_TRANSP, 0);  // starts soon: outline

    uint32_t time_hex = now ? C_RED_TEXT : soon ? C_AMBER_TEXT : C_MUTED;
    lv_obj_t* when = make_label(row, &font_text_20, time_hex);
    lv_label_set_text_fmt(when, "%s %s", e.start_label.c_str(), e.start_ampm.c_str());
    lv_obj_set_width(when, 98);

    // Title, with an optional "leave by" line under it.
    lv_obj_t* middle = make_box(row);
    lv_obj_set_flex_grow(middle, 1);
    lv_obj_set_height(middle, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(middle, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(middle, 3, 0);

    // The title wraps instead of being cut off, up to MAX_TITLE_LINES.
    lv_obj_t* title = make_label(middle, &font_text_20, C_TEXT, e.title.c_str());
    lv_obj_set_width(title, lv_pct(100));
    lv_label_set_long_mode(title, LV_LABEL_LONG_WRAP);

    if (!e.leave.empty()) {
        uint32_t hex = e.leave_urgency == "now" ? C_RED_TEXT : e.leave_urgency == "soon" ? C_AMBER_TEXT : C_DIM;
        lv_obj_t* leave = make_label(middle, &font_text_16, hex, e.leave.c_str());
        lv_obj_set_width(leave, lv_pct(100));
        lv_label_set_long_mode(leave, LV_LABEL_LONG_DOT);
        lv_obj_set_height(leave, lv_font_get_line_height(&font_text_16));
    }

    if (now) {  // in progress: "Now" and a red dot
        lv_obj_t* tag = make_label(row, &font_text_16, C_RED_TEXT, "Now");
        lv_obj_set_style_translate_y(tag, 3, 0);
        lv_obj_t* dot = make_box(row);
        lv_obj_set_size(dot, 12, 12);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, col(C_RED), 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_set_style_translate_y(dot, (line - 12) / 2, 0);
    } else if (soon && e.minutes_until >= 0) {
        lv_obj_t* in = make_label(row, &font_text_16, C_AMBER_TEXT);
        lv_label_set_text_fmt(in, "in %d min", e.minutes_until);
        lv_obj_set_style_translate_y(in, 3, 0);
    }

    lv_obj_update_layout(row);
    if (lv_obj_get_height(title) > line * MAX_TITLE_LINES) {  // very long: end with "…"
        lv_obj_set_height(title, line * MAX_TITLE_LINES);
        lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
        lv_obj_update_layout(row);
    }
    return row;
}

// Adds rows until the list is full. Returns how many were shown.
int add_rows(const std::vector<const Event*>& events, int reserve) {
    int shown = 0;
    for (const Event* e : events) {
        lv_obj_t* row = add_row(*e);
        lv_obj_update_layout(list);
        int bottom = lv_obj_get_y(row) + lv_obj_get_height(row);
        bool last = shown + 1 == (int)events.size();
        if (bottom > lv_obj_get_height(list) - (last ? 0 : reserve) && shown > 0) {
            lv_obj_delete(row);
            break;
        }
        shown++;
    }
    return shown;
}

void show_agenda() {
    lv_obj_clean(list);
    std::string header = "Today";
    for (const std::string& name : model.all_day) header += "  ·  " + name;
    lv_label_set_text(today_hdr, header.c_str());

    if (!model.has_agenda) {
        add_note("Add a calendar link on the server's setup page.", C_MUTED);
        return;
    }
    std::vector<const Event*> left;
    for (const Event& e : model.events) {
        if (e.status != "past") left.push_back(&e);
    }
    const int NOTE = 34;  // room kept for one line of small text under the rows
    if (!left.empty()) {
        int shown = add_rows(left, NOTE);
        if ((int)left.size() > shown) {
            char more[32];
            snprintf(more, sizeof more, "+ %d more", (int)left.size() - shown);
            add_note(more, C_DIM, &font_text_16);
        } else if (!model.free_after.empty()) {
            lv_obj_update_layout(list);
            lv_obj_t* last = lv_obj_get_child(list, -1);
            if (lv_obj_get_y(last) + lv_obj_get_height(last) + NOTE <= lv_obj_get_height(list)) {
                add_note(("Free after " + model.free_after).c_str(), C_GREEN);
            }
        }
        return;
    }
    add_note(model.events.empty() ? "No meetings today" : "Nothing else today", C_GREEN);
    if (!model.tomorrow.empty()) {
        add_note("Tomorrow", C_DIM);
        std::vector<Event> plain(model.tomorrow);
        std::vector<const Event*> next;
        for (Event& e : plain) {
            e.status = "upcoming";
            next.push_back(&e);
        }
        add_rows(next, 0);
    }
}

void show_weather() {
    if (!model.has_weather) {
        lv_label_set_text(cur_temp, "");
        lv_label_set_text(cur_cond, "Weather unavailable");
        lv_label_set_text(cur_hl, "");
        lv_obj_clean(hour_box);
        return;
    }
    icon_set(cur_icon, model.icon.c_str());
    lv_label_set_text_fmt(cur_temp, "%d°", model.temp);
    lv_label_set_text(cur_cond, model.condition.c_str());
    if (model.has_today) {
        lv_label_set_text_fmt(cur_hl, "H %d°  ·  L %d°", model.high, model.low);
        lv_label_set_text(sunrise_lbl, model.sunrise.c_str());
        lv_label_set_text(sunset_lbl, model.sunset.c_str());
    }
    static const uint32_t AIR_HEX[] = {C_GREEN, C_AMBER_TEXT, C_AMBER, C_RED_TEXT, 0xAFA9EC, 0xD4537E};
    if (model.has_air) {
        lv_label_set_text_fmt(cur_air, "AQI %d  ·  %s", model.aqi, model.aqi_label.c_str());
        lv_obj_set_style_text_color(cur_air, col(AIR_HEX[LV_CLAMP(0, model.aqi_level, 5)]), 0);
    } else {
        lv_label_set_text(cur_air, "");
    }
    // Two or three lines of text beside the temperature.
    lv_obj_update_layout(cur_temp);
    int x = lv_obj_get_x(cur_temp) + lv_obj_get_width(cur_temp) + 16;
    int top = model.has_air ? 27 : 39, step = 25;
    lv_obj_set_pos(cur_cond, x, top);
    lv_obj_set_pos(cur_hl, x, top + step);
    lv_obj_set_pos(cur_air, x, top + step * 2);

    lv_obj_clean(hour_box);
    for (const Hour& h : model.hourly) {
        lv_obj_t* column = make_box(hour_box);
        lv_obj_set_size(column, 72, lv_pct(100));
        lv_obj_set_flex_flow(column, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(column, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        make_label(column, &font_text_16, C_MUTED, h.label.c_str());
        icon_create(column, 40, h.icon.c_str());
        lv_label_set_text_fmt(make_label(column, &font_text_20, C_TEXT), "%d°", h.temp);
    }
    moon_set(moon, model.moon_phase);
    lv_label_set_text(moon_lbl, model.moon_name.c_str());
}

lv_obj_t* rule(lv_obj_t* parent, int x, int y, int w, int h) {
    lv_obj_t* line = make_box(parent);
    lv_obj_set_pos(line, x, y);
    lv_obj_set_size(line, w, h);
    lv_obj_set_style_bg_color(line, col(C_LINE), 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    return line;
}

}  // namespace

namespace dashboard {

void build(lv_obj_t* t) {
    constexpr int SPLIT = 566, STRIP_Y = 470;

    // Left: date, clock, game chips, news, snow
    date_lbl = make_label(t, &font_text_20, C_MUTED, status_text.c_str());
    lv_obj_set_pos(date_lbl, 34, 20);
    time_lbl = make_label(t, &font_clock_120, C_TEXT);
    lv_obj_set_pos(time_lbl, 28, 78);
    ampm_lbl = make_label(t, &font_text_28, C_MUTED);

    chips_box = make_box(t);
    lv_obj_set_pos(chips_box, SIDE_X, 22);
    lv_obj_set_size(chips_box, SIDE_W, 174);
    lv_obj_set_flex_flow(chips_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(chips_box, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(chips_box, 6, 0);

    constexpr int NEWS_Y = 204, NEWS_H = STRIP_Y - NEWS_Y - 18;
    news_box = make_box(t);
    lv_obj_set_pos(news_box, 28, NEWS_Y);
    lv_obj_set_size(news_box, SIDE_X + SIDE_W - 28, NEWS_H);
    tint(news_box, C_NEWS_TOP, C_NEWS_BOTTOM);
    lv_obj_set_style_radius(news_box, 14, 0);
    lv_obj_set_style_pad_all(news_box, 18, 0);
    news_src = make_label(news_box, &font_text_18, C_BLUE, "News");
    lv_obj_set_width(news_src, lv_pct(100));
    lv_obj_set_height(news_src, lv_font_get_line_height(&font_text_18));
    lv_label_set_long_mode(news_src, LV_LABEL_LONG_DOT);
    news_head = make_label(news_box, &font_text_28, C_TEXT);
    lv_obj_set_pos(news_head, 0, 34);
    lv_obj_set_size(news_head, lv_pct(100), lv_font_get_line_height(&font_text_28) * 4);
    lv_label_set_long_mode(news_head, LV_LABEL_LONG_DOT);
    news_pager = make_label(news_box, &font_text_16, C_FAINT);
    lv_obj_align(news_pager, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    snow_box = make_box(t);
    lv_obj_set_size(snow_box, SIDE_W, NEWS_H);
    lv_obj_set_pos(snow_box, SIDE_X, NEWS_Y);
    lv_obj_set_flex_flow(snow_box, LV_FLEX_FLOW_COLUMN);
    tint(snow_box, C_SNOW_TOP, C_SNOW_BOTTOM);
    lv_obj_set_style_radius(snow_box, 14, 0);
    lv_obj_set_style_pad_all(snow_box, 12, 0);
    lv_obj_add_flag(snow_box, LV_OBJ_FLAG_HIDDEN);

    // Right: agenda
    rule(t, SPLIT, 0, 1, STRIP_Y);
    today_hdr = make_label(t, &font_text_18, C_DIM, "Today");
    lv_obj_set_pos(today_hdr, SPLIT + 26, 24);
    lv_obj_set_width(today_hdr, SCREEN_W - SPLIT - 46);
    lv_label_set_long_mode(today_hdr, LV_LABEL_LONG_DOT);
    lv_obj_set_height(today_hdr, lv_font_get_line_height(&font_text_18));
    list = make_box(t);
    lv_obj_set_pos(list, SPLIT + 12, 56);
    lv_obj_set_size(list, SCREEN_W - SPLIT - 24, STRIP_Y - 60);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, 4, 0);

    // Bottom: weather strip
    rule(t, 0, STRIP_Y, SCREEN_W, 1);
    lv_obj_t* strip = make_box(t);
    lv_obj_set_pos(strip, 0, STRIP_Y + 1);
    lv_obj_set_size(strip, SCREEN_W, SCREEN_H - STRIP_Y - 1);

    cur_icon = icon_create(strip, 64);
    lv_obj_set_pos(cur_icon, 30, 32);
    cur_temp = make_label(strip, &font_text_40, C_TEXT);
    lv_obj_set_pos(cur_temp, 108, 40);
    cur_cond = make_label(strip, &font_text_18, C_MUTED);
    cur_hl = make_label(strip, &font_text_18, C_MUTED);
    cur_air = make_label(strip, &font_text_18, C_GREEN);

    hour_box = make_box(strip);
    lv_obj_set_pos(hour_box, 376, 12);
    lv_obj_set_size(hour_box, 396, 106);
    lv_obj_set_flex_flow(hour_box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hour_box, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    rule(strip, 796, 18, 1, 94);
    lv_obj_set_pos(icon_create(strip, 32, "sunrise"), 816, 24);
    sunrise_lbl = make_label(strip, &font_text_18, C_TEXT);
    lv_obj_set_pos(sunrise_lbl, 856, 30);
    lv_obj_set_pos(icon_create(strip, 32, "sunset"), 816, 70);
    sunset_lbl = make_label(strip, &font_text_18, C_TEXT);
    lv_obj_set_pos(sunset_lbl, 856, 76);
    moon = moon_create(strip, 36);
    lv_obj_set_pos(moon, 942, 24);
    moon_lbl = make_label(strip, &font_text_16, C_MUTED);
    lv_obj_set_pos(moon_lbl, 908, 68);
    lv_obj_set_width(moon_lbl, 104);
    lv_obj_set_style_text_align(moon_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(moon_lbl, LV_LABEL_LONG_WRAP);
}

void update(const Model& fresh) {
    model = fresh;
    show_snow();
    show_headline();
    show_games();
    show_agenda();
    show_weather();
    tick();
}

void set_status(const char* text) {
    status_text = text;
    if (!model.valid) lv_label_set_text(date_lbl, text);
}

void tick() {
    struct tm now;
    if (!app::local_time(now)) return;
    char text[48];
    const char* ampm;
    app::clock_text(text, sizeof text, &ampm);
    if (strcmp(lv_label_get_text(time_lbl), text) != 0) {
        lv_label_set_text(time_lbl, text);
        lv_label_set_text(ampm_lbl, ampm);
        lv_obj_update_layout(time_lbl);
        lv_obj_align_to(ampm_lbl, time_lbl, LV_ALIGN_OUT_RIGHT_BOTTOM, 12, -2);
    }
    snprintf(text, sizeof text, "%s, %s %d", DAYS[now.tm_wday], MONTHS[now.tm_mon], now.tm_mday);
    if (strcmp(lv_label_get_text(date_lbl), text) != 0) lv_label_set_text(date_lbl, text);
}

void next_headline() {
    if (model.news.size() > 1) {
        news_index++;
        show_headline();
    }
    if (model.games.size() > MAX_CHIPS) {
        game_index = (game_index + 1) % model.games.size();
        show_games();
    }
}

}  // namespace dashboard
