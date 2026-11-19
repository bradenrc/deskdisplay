// Screen test picture, for checking a new board. Turn it on with SCREEN_TEST in settings.h.
// Every part checks one thing about the screen setup; README.md says what to look for.
#pragma once
#include "gfx.h"

// Draw the whole test picture into fb (a full screen buffer). backlight = the percent to
// print on screen, step = which backlight step this is (shown so you can match it to Serial).
static void drawScreenTest(Gfx& g, uint16_t* fb, int backlight) {
  uint16_t* keepFb = g.fb; bool keepMark = g.marking;
  g.fb = fb; g.marking = false;
  auto px = [&](int x, int y, int r, int gr, int b) { fb[y * SCR_W + x] = pack565(r, gr, b); };
  auto box = [&](int x0, int y0, int x1, int y1, int r, int gr, int b) {
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) px(x, y, r, gr, b);
  };
  Col white = { 255, 255, 255 }, grey = { 150, 150, 150 };

  // black, with a dim grid every 64 pixels (the picture is lined up if the grid looks square)
  box(0, 0, SCR_W, SCR_H, 0, 0, 0);
  for (int y = 0; y < SCR_H; y++)
    for (int x = 0; x < SCR_W; x++)
      if (x % 64 == 0 || y % 64 == 0) px(x, y, 40, 40, 40);

  // 1) eight color bars, in this order, left to right
  static const struct { uint8_t r, g, b; const char* name; } BARS[8] = {
    { 255, 255, 255, "white" }, { 255, 255, 0, "yellow" }, { 0, 255, 255, "cyan" }, { 0, 255, 0, "green" },
    { 255, 0, 255, "magenta" }, { 255, 0, 0, "red" }, { 0, 0, 255, "blue" }, { 0, 0, 0, "black" } };
  for (int i = 0; i < 8; i++) {
    box(i * 128 + 2, 34, i * 128 + 126, 186, BARS[i].r, BARS[i].g, BARS[i].b);
    if (i == 7) for (int x = i * 128 + 2; x < i * 128 + 126; x++) { px(x, 34, 90, 90, 90); px(x, 185, 90, 90, 90); }
    int len = strlen(BARS[i].name);
    g.text(BARS[i].name, i * 128 + 64 - len * FONT_CELL_W / 2.f, 190, white);
  }

  // 2) smooth ramps, dark to bright: red, green, blue, grey. Each should fade evenly with
  //    no sudden jumps; a jump means one of the color wires isn't working.
  static const char* RAMPS[4] = { "R", "G", "B", "W" };
  for (int i = 0; i < 4; i++) {
    int y0 = 226 + i * 40;
    g.text(RAMPS[i], 24, y0 + 6, white);
    for (int x = 64; x < 960; x++) {
      int v = (x - 64) * 255 / 895;
      for (int y = y0; y < y0 + 34; y++) px(x, y, i == 0 || i == 3 ? v : 0, i == 1 || i == 3 ? v : 0, i == 2 || i == 3 ? v : 0);
    }
  }

  // 3) fine patterns: a one-pixel checkerboard, thin vertical lines, thin horizontal lines.
  //    They should look like even grey patches, without shimmering or crawling.
  for (int y = 394; y < 474; y++) {
    for (int x = 64; x < 320; x++) { int on = ((x + y) & 1) * 255; px(x, y, on, on, on); }
    for (int x = 384; x < 640; x++) { int on = ((x >> 1) & 1) * 255; px(x, y, on, on, on); }
    for (int x = 704; x < 960; x++) { int on = ((y >> 1) & 1) * 255; px(x, y, on, on, on); }
  }
  g.text("checker", 136, 476, grey);
  g.text("lines |", 470, 476, grey);
  g.text("lines -", 790, 476, grey);

  // 4) a white frame along the very edge, with a different color just inside it on each
  //    side, and a label in each corner. All four should be fully visible.
  for (int x = 0; x < SCR_W; x++) { px(x, 0, 255, 255, 255); px(x, SCR_H - 1, 255, 255, 255); px(x, 1, 255, 0, 0); px(x, SCR_H - 2, 0, 0, 255); }
  for (int y = 0; y < SCR_H; y++) { px(0, y, 255, 255, 255); px(SCR_W - 1, y, 255, 255, 255); px(1, y, 0, 255, 0); px(SCR_W - 2, y, 255, 255, 0); }
  g.text("TOP LEFT", 6, 6, white);
  g.text("TOP RIGHT", SCR_W - 6 - 9 * FONT_CELL_W, 6, white);
  g.text("BOTTOM LEFT", 6, SCR_H - 6 - FONT_CELL_H, white);
  g.text("BOTTOM RIGHT", SCR_W - 6 - 12 * FONT_CELL_W, SCR_H - 6 - FONT_CELL_H, white);

  // 5) what this is, and the backlight level right now
  char line[80];
  snprintf(line, sizeof line, "SCREEN TEST   backlight %3d%%", backlight);
  g.text(line, SCR_W / 2 - strlen(line) * FONT_CELL_W / 2.f, 512, white);
  g.text("Set SCREEN_TEST 0 in settings.h to go back to the aquarium", SCR_W / 2 - 59 * FONT_CELL_W / 2.f, 540, grey);

  g.fb = keepFb; g.marking = keepMark;
}
