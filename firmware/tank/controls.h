// On-screen controls for touchscreen boards (and mouse clicks in the PC viewer).
//  - Tap the water near the top: food drops where you tapped.
//  - Tap the left or right edge: a column of buttons slides in on that side. It slides away
//    after BUTTONS_HIDE_SECONDS without a tap, or when you tap somewhere else.
// This file only works out what a tap means and draws the buttons; the sketch (or viewer)
// does the actual switching, so the same code works on the board and on the PC.
#pragma once
#include "tank.h"

enum CtlAction { CTL_NONE, CTL_FEED, CTL_NIGHT, CTL_WEATHER, CTL_BRIGHT, CTL_CLOCK, CTL_VISITOR };

struct Controls {
  static const int N_BTN = 6, PANEL_W = 176, BTN_H = 52, BTN_GAP = 10, TOP = 92, EDGE = 80;
  float show = 0;      // 0 = hidden .. 1 = all the way out (it slides)
  bool open = false;
  bool right = true;   // which side the buttons are on
  float idle = 0;      // seconds since the last tap while open
  float feedX = -1;    // where to drop food for CTL_FEED (-1 = anywhere)

  // What the buttons do, top to bottom.
  CtlAction order(int i) const {
    static const CtlAction ORDER[N_BTN] = { CTL_NIGHT, CTL_WEATHER, CTL_FEED, CTL_BRIGHT, CTL_CLOCK, CTL_VISITOR };
    return ORDER[i];
  }

  // Button text, from the current state. Call before draw().
  void setLabels(bool night, int weather, int brightPercent, bool clockOn, bool visitorHere) {
    static const char* SKY[6] = { "Sky: sunny", "Sky: cloudy", "Sky: rain", "Sky: snow", "Sky: storm", "Sky: fog" };
    snprintf(text[0], sizeof text[0], "%s", night ? "Day mode" : "Night mode");
    snprintf(text[1], sizeof text[1], "%s", SKY[weather >= 0 && weather < 6 ? weather : 0]);
    snprintf(text[2], sizeof text[2], "Feed");
    snprintf(text[3], sizeof text[3], "Light %d%%", brightPercent);
    snprintf(text[4], sizeof text[4], "%s", clockOn ? "Clock: on" : "Clock: off");
    snprintf(text[5], sizeof text[5], "%s", visitorHere ? "Visitor..." : "Visitor");
  }

  // A tap at screen position (x, y). Returns what should happen.
  CtlAction tap(int x, int y) {
    if (show > .6f) {
      int px = panelX();
      if (x >= px && x < px + PANEL_W) {  // on the buttons
        idle = 0;
        for (int i = 0; i < N_BTN; i++) {
          int by = TOP + i * (BTN_H + BTN_GAP);
          if (y >= by && y < by + BTN_H) { feedX = -1; return order(i); }
        }
        return CTL_NONE;
      }
    }
    if (y < SURFACE_Y + LH * 2) { feedX = (float)x; return CTL_FEED; }  // the water at the top
    bool edgeR = x >= SCR_W - EDGE, edgeL = x < EDGE;
    if (edgeR || edgeL) {                 // an edge: bring the buttons out (or put them away)
      if (open && right == edgeR) open = false;
      else { open = true; right = edgeR; idle = 0; }
      return CTL_NONE;
    }
    open = false;                         // anywhere else: put them away
    return CTL_NONE;
  }

  void update(float dt) {
    if (open && (idle += dt) > BUTTONS_HIDE_SECONDS) open = false;
    float target = open ? 1.f : 0.f, step = dt * 5;
    show += clampf(target - show, -step, step);
  }

  void draw(Gfx& g) {
    if (show <= 0) return;
    int px = panelX();
    Col fill = rgb(8, 30, 52), edge = rgb(120, 190, 230), ink = rgb(235, 248, 255);
    for (int i = 0; i < N_BTN; i++) {
      int by = TOP + i * (BTN_H + BTN_GAP);
      g.fillRect(px, by, px + PANEL_W, by + BTN_H, fill, 205);
      g.fillRect(px, by, px + PANEL_W, by + 2, edge, 170);
      g.fillRect(px, by + BTN_H - 2, px + PANEL_W, by + BTN_H, edge, 170);
      g.fillRect(px, by + 2, px + 2, by + BTN_H - 2, edge, 170);
      g.fillRect(px + PANEL_W - 2, by + 2, px + PANEL_W, by + BTN_H - 2, edge, 170);
      int len = strlen(text[i]);
      g.text(text[i], px + (PANEL_W - len * FONT_CELL_W) / 2.f, by + (BTN_H - FONT_CELL_H) / 2.f, ink);
    }
  }

  // Left edge of the button column right now (slides in from its side).
  int panelX() const {
    float k = show * show * (3 - 2 * show), travel = PANEL_W + 14;
    return right ? (int)(SCR_W - travel * k) : (int)(-PANEL_W + travel * k);
  }

 private:
  char text[N_BTN][16] = {};
};

// Turn a brightness step (0, 1, 2, 3) into a fraction of the normal backlight.
static inline float brightnessStep(int step) {
  static const float STEPS[4] = { 1.f, .7f, .45f, .25f };
  return STEPS[step & 3];
}
