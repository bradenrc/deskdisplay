// Tasks page: open Google Tasks as one full-width list, soonest due first, that scrolls.
#include <cstdio>
#include <cstring>
#include <string>

#include "app.h"
#include "theme.h"

namespace {

constexpr int LEFT = 34, TOP = 92, ROW_H = 52, RING = 24, DUE_W = 150, BAR = 18;
constexpr int GRID_W = SCREEN_W - 2 * LEFT, GRID_H = SCREEN_H - TOP - 20;
constexpr int LIST_W = GRID_W - BAR;  // room on the right for the scrollbar

lv_obj_t *grid, *count_lbl, *clock_lbl, *hint;
std::string shown;  // what is on screen, so an unchanged list is left alone (and stays scrolled)

void place(lv_obj_t* label, int x, int y, int w, const lv_font_t* font) {
    lv_obj_set_pos(label, x, y + (ROW_H - lv_font_get_line_height(font)) / 2);
    lv_obj_set_size(label, w, lv_font_get_line_height(font));
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
}

void add_row(const TaskRow& row, int y) {
    if (row.header) {
        place(make_label(grid, &font_text_20, C_BLUE, row.title.c_str()), 0, y + 4, LIST_W, &font_text_20);
        return;
    }
    int at = row.indent * 36;
    lv_obj_t* ring = make_box(grid);  // the empty circle of a task not yet done
    lv_obj_set_size(ring, RING, RING);
    lv_obj_set_pos(ring, at, y + (ROW_H - RING) / 2);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(ring, 2, 0);
    lv_obj_set_style_border_color(ring, col(C_FAINT), 0);

    int due_w = row.due.empty() ? 0 : DUE_W;
    lv_obj_t* title = make_label(grid, &font_text_24, C_TEXT, row.title.c_str());
    place(title, at + RING + 16, y, LIST_W - (at + RING + 16) - due_w, &font_text_24);
    if (due_w) {
        uint32_t tone = row.tone == "late" ? C_RED_TEXT : row.tone == "today" ? C_AMBER_TEXT : C_DIM;
        lv_obj_t* due = make_label(grid, &font_text_20, tone, row.due.c_str());
        place(due, LIST_W - DUE_W + 8, y, DUE_W - 8, &font_text_20);
        lv_obj_set_style_text_align(due, LV_TEXT_ALIGN_RIGHT, 0);
    }
}

}  // namespace

namespace tasks {

void build(lv_obj_t* t) {
    lv_obj_set_pos(make_label(t, &font_text_28, C_TEXT, "Tasks"), LEFT, 28);
    count_lbl = make_label(t, &font_text_18, C_DIM);
    lv_obj_set_pos(count_lbl, LEFT + 110, 38);
    clock_lbl = make_label(t, &font_clock_56, C_MUTED);
    lv_obj_align(clock_lbl, LV_ALIGN_TOP_RIGHT, -LEFT, 18);

    // One full-width list. It scrolls up and down when there are more tasks than fit;
    // swiping sideways still changes page.
    grid = make_box(t);
    lv_obj_set_pos(grid, LEFT, TOP);
    lv_obj_set_size(grid, GRID_W, GRID_H);
    lv_obj_add_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(grid, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(grid, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_bg_color(grid, col(C_FAINT), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(grid, LV_OPA_COVER, LV_PART_SCROLLBAR);
    lv_obj_set_style_width(grid, 5, LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(grid, 3, LV_PART_SCROLLBAR);
    hint = make_label(t, &font_text_20, C_MUTED);
    lv_obj_center(hint);
}

void update(const Model& model) {
    std::string now = model.has_tasks ? "y" : "n";
    for (const TaskRow& row : model.tasks) {
        now += '\n' + row.title + '\t' + row.due + '\t' + row.tone + (row.header ? "h" : row.indent ? "s" : "");
    }
    now += '\n' + std::to_string(model.tasks_total);
    if (now == shown) return;
    shown = now;

    lv_obj_clean(grid);
    lv_obj_scroll_to_y(grid, 0, LV_ANIM_OFF);
    lv_label_set_text(count_lbl, "");
    if (model.tasks.empty()) {
        lv_label_set_text(hint, model.has_tasks ? "No open tasks."
                                                : "Connect Google Tasks on the server's setup page.");
        lv_obj_remove_flag(hint, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_add_flag(hint, LV_OBJ_FLAG_HIDDEN);
    char text[32];
    snprintf(text, sizeof text, "%d open", model.tasks_total);
    lv_label_set_text(count_lbl, text);

    int y = 0, drawn = 0;
    for (const TaskRow& row : model.tasks) {
        add_row(row, y);
        if (!row.header) drawn++;
        y += ROW_H;
    }
    if (drawn < model.tasks_total) {  // the server sends only so many
        snprintf(text, sizeof text, "and %d more", model.tasks_total - drawn);
        place(make_label(grid, &font_text_20, C_DIM, text), RING + 16, y, LIST_W - RING - 16, &font_text_20);
    }
}

void tick(bool active) {
    if (!active) return;
    char text[16];
    app::clock_text(text, sizeof text);
    if (strcmp(lv_label_get_text(clock_lbl), text) != 0) lv_label_set_text(clock_lbl, text);
}

}  // namespace tasks
