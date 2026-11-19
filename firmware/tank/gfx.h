// Small drawing engine for a 1024x600 RGB565 frame buffer.
// - Text is drawn from pre-rendered anti-aliased glyphs (font_data.h).
// - Only the parts of the screen that changed are redrawn ("dirty rectangles"),
//   which is what lets a microcontroller animate a 1024x600 screen smoothly.
#pragma once
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "font_data.h"

#define SCR_W 1024
#define SCR_H 600

struct Col { uint8_t r, g, b; };
static inline Col hexCol(uint32_t h) { return { uint8_t(h >> 16), uint8_t(h >> 8), uint8_t(h) }; }
static inline Col scaleCol(Col c, float k) {
  if (k > 1) k = 1;
  if (k < 0) k = 0;
  return { uint8_t(c.r * k), uint8_t(c.g * k), uint8_t(c.b * k) };
}
static inline uint16_t pack565(int r, int g, int b) {
  return uint16_t(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
static inline uint16_t blend565(uint16_t d, Col c, int a) {  // a: 0..255
  int dr = (d >> 8) & 0xF8, dg = (d >> 3) & 0xFC, db = (d << 3) & 0xF8;
  dr += ((c.r - dr) * a) >> 8;
  dg += ((c.g - dg) * a) >> 8;
  db += ((c.b - db) * a) >> 8;
  return pack565(dr, dg, db);
}

// Map a character byte to a glyph index (-1 = nothing to draw)
static inline int glyphIndex(uint8_t ch) {
  if (ch > 32 && ch < 127) return ch - 32;
  if (ch == 0xB0) return 95;  // degree sign, used for fish eyes
  return -1;
}

#ifdef GFX_COUNT_WRITES
#define COUNT_WRITE() statWritten++
#else
#define COUNT_WRITE()
#endif

struct Rect { int16_t x0, y0, x1, y1; };

// Where the ink is inside one glyph image: for each row the first and last column that
// isn't empty, plus the box around all of it. Most of a glyph's cell is empty (think of
// '.' or '_'), so knowing this lets us skip looking at, and later erasing, the empty part.
struct Ink {
  uint8_t first[FONT_GLOW_H], last[FONT_GLOW_H];  // per row; first > last = empty row
  uint8_t u0, u1, v0, v1;                         // box around the ink; v0 > v1 = no ink at all
};

class Gfx {
 public:
  static const int MAX_DIRTY = 1400;
  static const int MAX_BUFFERS = 3;
  // Glow pixels fainter than this (out of 255) are skipped: you can't see them, and
  // they're the bulk of the work in a glow.
  static const int GLOW_MIN_ALPHA = 4;
  uint16_t* fb = nullptr;  // buffer being drawn this frame
  uint16_t* bg = nullptr;  // pre-rendered background (water, sand, castle...)
  // Optional: the bottom strip of the background WITHOUT the castle, rocks and coral
  // (rows cleanY0..bottom). Solid creatures clear to this, so they hide the scenery they
  // pass in front of. Leave null to clear to the normal background instead.
  uint16_t* clean = nullptr;
  int cleanY0 = SCR_H;
  bool marking = true;     // false = don't record dirty areas (used for one-off drawing)
  // Pixel counters, for measuring how much work a frame is (reset them yourself):
  //   statBlend   = pixels looked at while drawing glyphs and rectangles
  //   statWritten = pixels actually blended (only counted when GFX_COUNT_WRITES is defined,
  //                 because counting in the inner loop costs time on the board)
  //   statRestore = pixels copied back from the background (erasing + solid creatures)
  uint32_t statBlend = 0, statRestore = 0, statWritten = 0, statFull = 0;

  // Start a frame. bufIdx (0..MAX_BUFFERS-1) says which screen buffer we draw into;
  // we erase whatever that buffer showed last time by copying the background back.
  void beginFrame(int bufIdx, uint16_t* target, uint16_t* background, bool forceFull) {
    if (!inkReady) buildInk();
    cur = bufIdx % MAX_BUFFERS;
    fb = target;
    bg = background;
    List& L = lists[cur];
    if (forceFull)  // the other buffers need a full redraw too
      for (int i = 0; i < MAX_BUFFERS; i++) lists[i].full = true;
    if (forceFull || L.full) {
      memcpy(fb, bg, SCR_W * SCR_H * 2);
      statFull++;
    } else {
      for (int i = 0; i < L.n; i++) restore(L.r[i]);
    }
    L.n = 0;
    L.full = false;
  }

  // Remember that this area was drawn on, so it gets erased later.
  void mark(int x0, int y0, int x1, int y1) {
    if (!marking) return;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > SCR_W) x1 = SCR_W;
    if (y1 > SCR_H) y1 = SCR_H;
    if (x0 >= x1 || y0 >= y1) return;
    addRect(lists[cur], x0, y0, x1, y1);
  }

  // The background itself changed here (for example the clock ticked over): every screen
  // buffer must refresh this area from the background the next time it's drawn.
  void invalidate(int x0, int y0, int x1, int y1) {
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > SCR_W) x1 = SCR_W;
    if (y1 > SCR_H) y1 = SCR_H;
    if (x0 >= x1 || y0 >= y1) return;
    for (int i = 0; i < MAX_BUFFERS; i++) addRect(lists[i], x0, y0, x1, y1);
  }

  // Draw one glyph centered at (cx, cy), scaled by (sx, sy). Negative sx mirrors it.
  // alpha 0..255 multiplies the glyph's own anti-aliasing. Optional soft glow behind it.
  void glyph(uint8_t ch, float cx, float cy, float sx, float sy, Col c, int alpha,
             bool glow = false, Col gc = {0, 0, 0}, int glowAlpha = 0) {
    int gi = glyphIndex(ch);
    if (gi < 0 || alpha <= 0) return;
    if (!inkReady) buildInk();
    if (glow && glowAlpha > 0)
      blit(FONT_GLOWS + gi * FONT_GLOW_W * FONT_GLOW_H, glowInk[gi], FONT_GLOW_W, FONT_GLOW_H, cx, cy, sx, sy, gc,
           glowAlpha, GLOW_MIN_ALPHA);
    blit(FONT_GLYPHS + gi * FONT_CELL_W * FONT_CELL_H, glyphInk[gi], FONT_CELL_W, FONT_CELL_H, cx, cy, sx, sy, c, alpha, 1);
  }

  // Unscaled text with its top-left corner at (x, y).
  void text(const char* s, float x, float y, Col c, int alpha = 255) {
    for (int i = 0; s[i]; i++)
      glyph((uint8_t)s[i], x + i * FONT_CELL_W + FONT_CELL_W * 0.5f, y + FONT_CELL_H * 0.5f, 1, 1, c, alpha);
  }

  void fillRect(int x0, int y0, int x1, int y1, Col c, int alpha) {
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > SCR_W) x1 = SCR_W;
    if (y1 > SCR_H) y1 = SCR_H;
    if (x0 >= x1 || y0 >= y1 || alpha <= 0) return;
    for (int y = y0; y < y1; y++) {
      uint16_t* p = fb + y * SCR_W;
      for (int x = x0; x < x1; x++) p[x] = blend565(p[x], c, alpha);
    }
#ifdef GFX_COUNT_WRITES
    statWritten += (x1 - x0) * (y1 - y0);
#endif
    statBlend += (x1 - x0) * (y1 - y0);
    mark(x0, y0, x1, y1);
  }

  // Draw a w x h grey-scale picture (0 = see-through .. 255 = solid) in color c, top-left at (x, y).
  void alphaMap(const uint8_t* a, int w, int h, int x, int y, Col c, int alpha) {
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y, x1 = x + w > SCR_W ? SCR_W : x + w, y1 = y + h > SCR_H ? SCR_H : y + h;
    if (x0 >= x1 || y0 >= y1 || alpha <= 0) return;
    for (int yy = y0; yy < y1; yy++) {
      const uint8_t* src = a + (yy - y) * w - x;
      uint16_t* p = fb + yy * SCR_W;
      for (int xx = x0; xx < x1; xx++) {
        int v = (src[xx] * alpha) >> 8;
        if (v) { p[xx] = blend565(p[xx], c, v); COUNT_WRITE(); }
      }
    }
    statBlend += (x1 - x0) * (y1 - y0);
    mark(x0, y0, x1, y1);
  }

  // Blend one pixel (no dirty-area bookkeeping: call mark() once for the whole shape).
  void blendPixel(int x, int y, Col c, int alpha) {
    if (x < 0 || y < 0 || x >= SCR_W || y >= SCR_H || alpha <= 0) return;
    uint16_t* p = fb + y * SCR_W + x;
    *p = blend565(*p, c, alpha > 255 ? 255 : alpha);
  }

  // Blank out an area, hiding anything drawn there so far (used to make creatures solid).
  // Above the scenery strip this copies the plain background, and needs no erasing later
  // because those pixels then already match it. Inside the strip it copies the
  // scenery-free version, which does have to be erased later, so that part is marked.
  void clearToBg(int x0, int y0, int x1, int y1) {
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > SCR_W) x1 = SCR_W;
    if (y1 > SCR_H) y1 = SCR_H;
    if (x0 >= x1 || y0 >= y1) return;
    int split = !clean ? y1 : y1 < cleanY0 ? y1 : y0 > cleanY0 ? y0 : cleanY0;
    if (y0 < split) restore({ int16_t(x0), int16_t(y0), int16_t(x1), int16_t(split) });
    if (split < y1) {
      int w = (x1 - x0) * 2;
      for (int y = split; y < y1; y++)
        memcpy(fb + y * SCR_W + x0, clean + (y - cleanY0) * SCR_W + x0, w);
      statRestore += (x1 - x0) * (y1 - split);
      mark(x0, split, x1, y1);
    }
  }

 private:
  struct List { Rect r[MAX_DIRTY]; int n = 0; bool full = true; };
  List lists[MAX_BUFFERS];
  int cur = 0;
  Ink glyphInk[FONT_NUM_GLYPHS], glowInk[FONT_NUM_GLYPHS];
  bool inkReady = false;

  // Work out where the ink is in every glyph and glow image (once, at start-up).
  static void findInk(const uint8_t* img, int w, int h, int threshold, Ink& k) {
    k.u0 = 255; k.u1 = 0; k.v0 = 255; k.v1 = 0;
    for (int v = 0; v < h; v++) {
      int f = w, l = -1;
      for (int u = 0; u < w; u++) if (img[v * w + u] >= threshold) { if (u < f) f = u; l = u; }
      if (l < 0) { k.first[v] = 1; k.last[v] = 0; continue; }
      k.first[v] = f; k.last[v] = l;
      if (f < k.u0) k.u0 = f;
      if (l > k.u1) k.u1 = l;
      if (v < k.v0) k.v0 = v;
      k.v1 = v;
    }
  }
  void buildInk() {
    for (int i = 0; i < FONT_NUM_GLYPHS; i++) {
      findInk(FONT_GLYPHS + i * FONT_CELL_W * FONT_CELL_H, FONT_CELL_W, FONT_CELL_H, 1, glyphInk[i]);
      // a glow pixel below ~10 can't reach GLOW_MIN_ALPHA at the glow strengths the tank uses
      findInk(FONT_GLOWS + i * FONT_GLOW_W * FONT_GLOW_H, FONT_GLOW_W, FONT_GLOW_H, 10, glowInk[i]);
    }
    inkReady = true;
  }

  void addRect(List& L, int x0, int y0, int x1, int y1) {
    // Things are usually drawn left to right (letters of a fish, a row of waves), so if this
    // area touches or overlaps the previous one, grow that one instead. Overlapping glow
    // boxes then get erased once instead of several times, and a whole row of letters
    // becomes one wide copy. Only merge when that adds little extra (bare water) area.
    if (L.n > 0) {
      Rect& p = L.r[L.n - 1];
      int ux0 = p.x0 < x0 ? p.x0 : x0, uy0 = p.y0 < y0 ? p.y0 : y0;
      int ux1 = p.x1 > x1 ? p.x1 : x1, uy1 = p.y1 > y1 ? p.y1 : y1;
      int32_t ua = (ux1 - ux0) * (uy1 - uy0), pa = (p.x1 - p.x0) * (p.y1 - p.y0), na = (x1 - x0) * (y1 - y0);
      if (ua - pa - na <= na / 2) { p = { int16_t(ux0), int16_t(uy0), int16_t(ux1), int16_t(uy1) }; return; }
    }
    if (L.n >= MAX_DIRTY) { L.full = true; return; }
    L.r[L.n++] = { int16_t(x0), int16_t(y0), int16_t(x1), int16_t(y1) };
  }

  void restore(const Rect& r) {
    int w = (r.x1 - r.x0) * 2;
    statRestore += (r.x1 - r.x0) * (r.y1 - r.y0);
    for (int y = r.y0; y < r.y1; y++) {
      int o = y * SCR_W + r.x0;
      memcpy(fb + o, bg + o, w);
    }
  }

  // Draw one glyph image. Pixels whose final alpha is below minAlpha are skipped.
  void blit(const uint8_t* src, const Ink& ink, int sw, int sh, float cx, float cy, float sx, float sy,
            Col c, int alpha, int minAlpha) {
    if (ink.v0 > ink.v1) return;  // nothing to draw (blank glyph)
    float dw = fabsf(sw * sx), dh = sh * sy;
    if (dw < 0.5f || dh < 0.5f) return;
    bool flip = sx < 0;
    float left = cx - dw * 0.5f, top = cy - dh * 0.5f;
    int mx0, my0, mx1, my1;  // the part of the screen we touch (and must erase later)
    if (!flip && fabsf(sx - 1.f) < 0.001f && fabsf(sy - 1.f) < 0.001f) {
      // fast path: unscaled, not mirrored
      int ox = (int)lrintf(left), oy = (int)lrintf(top);
      mx0 = ox + ink.u0; mx1 = ox + ink.u1 + 1; my0 = oy + ink.v0; my1 = oy + ink.v1 + 1;
      if (mx0 < 0) mx0 = 0;
      if (my0 < 0) my0 = 0;
      if (mx1 > SCR_W) mx1 = SCR_W;
      if (my1 > SCR_H) my1 = SCR_H;
      if (mx0 >= mx1 || my0 >= my1) return;
      for (int y = my0; y < my1; y++) {
        int v = y - oy;
        int xs = ox + ink.first[v], xe = ox + ink.last[v] + 1;
        if (xs < mx0) xs = mx0;
        if (xe > mx1) xe = mx1;
        if (xs >= xe) continue;
        const uint8_t* row = src + v * sw - ox;
        uint16_t* p = fb + y * SCR_W;
        for (int x = xs; x < xe; x++) {
          int a = (row[x] * alpha) >> 8; if (a < minAlpha) continue;
          p[x] = blend565(p[x], c, a);
          COUNT_WRITE();
        }
        statBlend += xe - xs;
      }
    } else {
      // scaled and/or mirrored: step through the source in whole-number (16.16) steps
      float ku = sw / dw, kv = sh / dh;
      // the ink box, in source columns as seen after mirroring
      int iu0 = flip ? sw - 1 - ink.u1 : ink.u0, iu1 = flip ? sw - 1 - ink.u0 : ink.u1;
      mx0 = (int)floorf(left + iu0 / ku) - 1; mx1 = (int)ceilf(left + (iu1 + 1) / ku) + 1;
      my0 = (int)floorf(top + ink.v0 / kv) - 1; my1 = (int)ceilf(top + (ink.v1 + 1) / kv) + 1;
      if (mx0 < 0) mx0 = 0;
      if (my0 < 0) my0 = 0;
      if (mx1 > SCR_W) mx1 = SCR_W;
      if (my1 > SCR_H) my1 = SCR_H;
      if (mx0 >= mx1 || my0 >= my1) return;
      int32_t stepU = (int32_t)(ku * 65536.f);
      int32_t startU = (int32_t)((mx0 + 0.5f - left) * ku * 65536.f);
      for (int y = my0; y < my1; y++) {
        int v = (int)((y + 0.5f - top) * kv); if (v < 0 || v >= sh) continue;
        if (ink.first[v] > ink.last[v]) continue;
        const uint8_t* row = src + v * sw;
        uint16_t* p = fb + y * SCR_W;
        int32_t uf = startU;
        for (int x = mx0; x < mx1; x++, uf += stepU) {
          int u = uf >> 16; if (u < 0 || u >= sw) continue;
          if (flip) u = sw - 1 - u;
          int a = (row[u] * alpha) >> 8; if (a < minAlpha) continue;
          p[x] = blend565(p[x], c, a);
          COUNT_WRITE();
        }
        statBlend += mx1 - mx0;
      }
    }
    mark(mx0, my0, mx1, my1);
  }
};
