// Markets page: up to six cards, each with a price, the change on the day and the day's shape.
#include <cstring>
#include <vector>

#include "app.h"
#include "theme.h"

namespace {

constexpr int LEFT = 34, TOP = 92, GAP = 16, MAX_CARDS = 6;
constexpr int GRID_W = SCREEN_W - 2 * LEFT, GRID_H = SCREEN_H - TOP - 26;
constexpr uint32_t C_CARD = 0x161A20;

lv_obj_t *grid, *status_lbl, *clock_lbl, *hint;
// Line objects keep a pointer to their points, so the points must outlive them.
std::vector<std::vector<lv_point_precise_t>> shapes;

void add_line(lv_obj_t* card, const std::vector<lv_point_precise_t>& points, uint32_t hex, int width,
              bool dashed) {
    lv_obj_t* line = lv_line_create(card);
    lv_line_set_points(line, points.data(), (uint32_t)points.size());
    lv_obj_set_style_line_color(line, col(hex), 0);
    lv_obj_set_style_line_width(line, width, 0);
    lv_obj_set_style_line_rounded(line, !dashed, 0);
    if (dashed) {
        lv_obj_set_style_line_dash_width(line, 4, 0);
        lv_obj_set_style_line_dash_gap(line, 5, 0);
    }
}

void add_card(const Stock& stock, int x, int y, int w, int h, size_t slot) {
    uint32_t tone = stock.direction == "up" ? C_GREEN : stock.direction == "down" ? C_RED_TEXT : C_MUTED;

    lv_obj_t* card = make_box(grid);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, w, h);
    tint(card, C_CARD);
    lv_obj_set_style_radius(card, 14, 0);

    lv_obj_t* name = make_label(card, &font_text_20, C_MUTED, stock.name.c_str());
    lv_obj_set_pos(name, 20, 16);
    lv_obj_set_width(name, w - 40);
    lv_obj_set_height(name, lv_font_get_line_height(&font_text_20));
    lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(make_label(card, &font_text_40, C_TEXT, stock.price.c_str()), 20, 46);
    lv_obj_set_pos(make_label(card, &font_text_24, tone, stock.change.c_str()), 20, 98);

    // The day's shape, with a dashed line at yesterday's close.
    const int cx = 20, cy = 142, cw = w - 40, ch = h - cy - 20;
    if (stock.points.size() < 2 || ch < 30) return;
    auto level = [&](int value) { return (lv_value_precise_t)(cy + ch - value * ch / 100); };
    if (stock.baseline >= 0) {
        std::vector<lv_point_precise_t>& base = shapes[slot * 2];
        base = {{(lv_value_precise_t)cx, level(stock.baseline)},
                {(lv_value_precise_t)(cx + cw), level(stock.baseline)}};
        add_line(card, base, C_FAINT, 1, true);
    }
    std::vector<lv_point_precise_t>& shape = shapes[slot * 2 + 1];
    size_t last = stock.points.size() - 1;
    for (size_t i = 0; i <= last; i++) {
        shape.push_back({(lv_value_precise_t)(cx + (int)(i * cw / last)), level(stock.points[i])});
    }
    add_line(card, shape, tone, 3, false);
}

}  // namespace

namespace markets {

void build(lv_obj_t* t) {
    lv_obj_set_pos(make_label(t, &font_text_28, C_TEXT, "Markets"), LEFT, 28);
    status_lbl = make_label(t, &font_text_18, C_DIM);
    lv_obj_set_pos(status_lbl, LEFT + 150, 38);
    clock_lbl = make_label(t, &font_clock_56, C_MUTED);
    lv_obj_align(clock_lbl, LV_ALIGN_TOP_RIGHT, -LEFT, 18);

    grid = make_box(t);
    lv_obj_set_pos(grid, LEFT, TOP);
    lv_obj_set_size(grid, GRID_W, GRID_H);
    hint = make_label(t, &font_text_20, C_MUTED,
                      "Add symbols under \"stocks\" in the server's config.yaml.");
    lv_obj_center(hint);
}

void update(const Model& model) {
    lv_obj_clean(grid);  // removes the lines before their points are replaced
    shapes.clear();
    int count = LV_MIN((int)model.stocks.size(), MAX_CARDS);
    if (count == 0) {
        lv_obj_remove_flag(hint, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(status_lbl, "");
        return;
    }
    lv_obj_add_flag(hint, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(status_lbl, model.market_open ? "Open" : "Closed");
    lv_obj_set_style_text_color(status_lbl, col(model.market_open ? C_GREEN : C_DIM), 0);

    // One row for up to three, two rows beyond that; cards fill the space they have.
    int cols = LV_MIN(count, 3), rows = (count + 2) / 3;
    int w = (GRID_W - (cols - 1) * GAP) / cols, h = (GRID_H - (rows - 1) * GAP) / rows;
    shapes.resize((size_t)count * 2);
    for (int i = 0; i < count; i++) {
        add_card(model.stocks[i], (i % cols) * (w + GAP), (i / cols) * (h + GAP), w, h, (size_t)i);
    }
}

void tick(bool active) {
    if (!active) return;
    char text[16];
    app::clock_text(text, sizeof text);
    if (strcmp(lv_label_get_text(clock_lbl), text) != 0) lv_label_set_text(clock_lbl, text);
}

}  // namespace markets
