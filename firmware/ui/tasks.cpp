// Tasks page: open Google Tasks in two columns, grouped under the name of each list.
#include <cstdio>
#include <cstring>

#include "app.h"
#include "theme.h"

namespace {

constexpr int LEFT = 34, TOP = 92, GAP = 36, ROW_H = 40;
constexpr int GRID_W = SCREEN_W - 2 * LEFT, GRID_H = SCREEN_H - TOP - 26;
constexpr int COL_W = (GRID_W - GAP) / 2, ROWS = GRID_H / ROW_H, DUE_W = 112;

lv_obj_t *grid, *count_lbl, *clock_lbl, *hint;

void place(lv_obj_t* label, int x, int y, int w, const lv_font_t* font) {
    lv_obj_set_pos(label, x, y + (ROW_H - lv_font_get_line_height(font)) / 2);
    lv_obj_set_size(label, w, lv_font_get_line_height(font));
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
}

void add_row(const TaskRow& row, int x, int y) {
    if (row.header) {
        place(make_label(grid, &font_text_18, C_BLUE, row.title.c_str()), x, y + 4, COL_W, &font_text_18);
        return;
    }
    int at = x + row.indent * 28;
    lv_obj_t* ring = make_box(grid);  // the empty circle of a task not yet done
    lv_obj_set_size(ring, 20, 20);
    lv_obj_set_pos(ring, at, y + (ROW_H - 20) / 2);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(ring, 2, 0);
    lv_obj_set_style_border_color(ring, col(C_FAINT), 0);

    int due_w = row.due.empty() ? 0 : DUE_W;
    lv_obj_t* title = make_label(grid, &font_text_20, C_TEXT, row.title.c_str());
    place(title, at + 32, y, x + COL_W - (at + 32) - due_w, &font_text_20);
    if (due_w) {
        uint32_t tone = row.tone == "late" ? C_RED_TEXT : row.tone == "today" ? C_AMBER_TEXT : C_DIM;
        lv_obj_t* due = make_label(grid, &font_text_18, tone, row.due.c_str());
        place(due, x + COL_W - DUE_W + 8, y, DUE_W - 8, &font_text_18);
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

    grid = make_box(t);
    lv_obj_set_pos(grid, LEFT, TOP);
    lv_obj_set_size(grid, GRID_W, GRID_H);
    hint = make_label(t, &font_text_20, C_MUTED);
    lv_obj_center(hint);
}

void update(const Model& model) {
    lv_obj_clean(grid);
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

    // Fill the left column, then the right. A list's name never sits alone at the foot of a
    // column, and the last slot is kept for "and N more" when not everything fits.
    int slot = 0, drawn = 0;
    const int capacity = ROWS * 2;
    for (size_t i = 0; i < model.tasks.size(); i++) {
        const TaskRow& row = model.tasks[i];
        if (row.header && slot % ROWS == ROWS - 1) slot++;
        bool last_slot = slot >= capacity - 1;
        bool more_after = i + 1 < model.tasks.size() || model.tasks_total > drawn + 1;
        if (slot >= capacity || (last_slot && (more_after || row.header))) break;
        add_row(row, (slot / ROWS) * (COL_W + GAP), (slot % ROWS) * ROW_H);
        if (!row.header) drawn++;
        slot++;
    }
    if (drawn < model.tasks_total && slot < capacity) {
        snprintf(text, sizeof text, "and %d more", model.tasks_total - drawn);
        lv_obj_t* more = make_label(grid, &font_text_18, C_DIM, text);
        place(more, (slot / ROWS) * (COL_W + GAP) + 32, (slot % ROWS) * ROW_H, COL_W - 32, &font_text_18);
    }
}

void tick(bool active) {
    if (!active) return;
    char text[16];
    app::clock_text(text, sizeof text);
    if (strcmp(lv_label_get_text(clock_lbl), text) != 0) lv_label_set_text(clock_lbl, text);
}

}  // namespace tasks
