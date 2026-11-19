// The aquarium itself: fish behavior, scenery, and how everything is drawn.
// Ported from the web version. All art faces RIGHT; left-facing is a mirror,
// so fish can turn smoothly (they squash edge-on, then flip).
#pragma once
#include "gfx.h"
#include "settings.h"
#include "art.h"      // all the drawings
#include "weather_icons.h"  // weather pictures (generated, see tools/weather_icons)

// ---------------------------------------------------------------- helpers
static uint32_t rngState = 0x1234567u;
static inline uint32_t rngNext() { rngState ^= rngState << 13; rngState ^= rngState >> 17; rngState ^= rngState << 5; return rngState; }
static inline float rnd01() { return (rngNext() >> 8) * (1.0f / 16777216.0f); }
static inline float rnd(float a, float b) { return a + rnd01() * (b - a); }
static inline float clampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }
static inline float sgnf(float v) { return v > 0 ? 1.f : v < 0 ? -1.f : 0.f; }
static inline Col rgb(int r, int g, int b) { return { uint8_t(r), uint8_t(g), uint8_t(b) }; }

// ---------------------------------------------------------------- layout
static const float CW = FONT_CELL_W;      // character width
static const float LH = 22.4f;            // line height
static const float W = SCR_W, H = SCR_H;
static const float SURFACE_Y = LH * 1.4f;
static const float SAND_TOP = H - LH * 2.6f;
static const float CASTLE_X = W * 0.66f;
static const float CHEST_X = W * 0.257f;
static const float CAVE_X = 0;             // the rock with the eel's cave (at the far left)
static const float CAVE_BACK = 11.6f;
static const float CRITTER = FISH_SIZE;    // the snail and eel are drawn at the fish size too
static const float CRAB_SCALE = CRAB_SIZE; // the crab has its own size      // where the back wall of the cave is, in characters from CAVE_X
static const char DEG = (char)0xB0;       // the ° eye
// The castle, rocks and coral all fit in the strip of screen from this row down.
static inline int sceneryTop() { return (int)(SAND_TOP - 5 * LH) - 6; }


static inline Col mixCol(Col a, Col b, float k) {
  return { uint8_t(a.r + (b.r - a.r) * k), uint8_t(a.g + (b.g - a.g) * k), uint8_t(a.b + (b.b - a.b) * k) };
}

static const int MAX_ROWS = 8;
enum { PC_BODY, PC_LIGHT, PC_DARK, PC_FIN, PC_FIN_LIGHT, PC_WHITE, PC_MARK, PC_EYE, PC_COUNT };

struct Species {
  const char* art[MAX_ROWS]; const char* paint[MAX_ROWS]; int rows, w;
  const char* puff[MAX_ROWS]; const char* puffPaint[MAX_ROWS]; int puffRows, puffW;
  Col pal[PC_COUNT]; float speed, agility; int eyeRow;
  bool facesLeft;  // drawn with its head on the left instead of the right
};

enum { SP_DELUXE, SP_CLASSIC, SP_MINNOW, SP_CLOWN, SP_ANGEL, SP_GROUPER, SP_PUFFER, SP_LIONFISH, SP_BETTA, SP_WHALE, SP_SNAIL, SP_CRAB,
       SP_CASTLE, SP_CHEST, SP_CHEST_OPEN, SP_FAN_CORAL, SP_TUBE_CORAL, SP_CAVE,
       SP_EEL, SP_EEL_SHUT, SP_TURTLE, SP_COUNT };
static Species SPECIES[SP_COUNT];

static void setupSpecies(Species& s, const char** art, const char** paint, int rows, uint32_t body, uint32_t fin,
                         float speed, float agility) {
  s = Species{};
  s.rows = rows; s.w = 0; s.eyeRow = 0;
  for (int i = 0; i < rows; i++) {
    s.art[i] = art[i]; s.paint[i] = paint[i];
    int L = strlen(art[i]); if (L > s.w) s.w = L;
    if (strchr(paint[i], 'e')) s.eyeRow = i;
  }
  Col b = hexCol(body), f = hexCol(fin), white = rgb(255, 255, 255);
  s.pal[PC_BODY] = b;
  s.pal[PC_LIGHT] = mixCol(b, white, .38f);
  s.pal[PC_DARK] = mixCol(b, rgb(0, 0, 0), .38f);
  s.pal[PC_FIN] = f;
  s.pal[PC_FIN_LIGHT] = mixCol(f, white, .45f);
  s.pal[PC_WHITE] = mixCol(b, white, .82f);
  s.pal[PC_MARK] = mixCol(b, rgb(8, 14, 24), .55f);
  s.pal[PC_EYE] = white;
  s.speed = speed * SPEED_MULTIPLIER; s.agility = agility;
}

static void initSpecies() {
  setupSpecies(SPECIES[SP_DELUXE], ART_DELUXE, PAINT_DELUXE, 3, COLOR_DELUXE, COLOR_DELUXE_FINS, 2.6f, 1.4f);
  setupSpecies(SPECIES[SP_CLASSIC], ART_CLASSIC, PAINT_CLASSIC, 1, COLOR_CLASSIC, COLOR_CLASSIC_FINS, 3.3f, 1.8f);
  setupSpecies(SPECIES[SP_MINNOW], ART_MINNOW, PAINT_MINNOW, 1, COLOR_MINNOW, COLOR_MINNOW_FINS, 4.4f, 2.6f);
  setupSpecies(SPECIES[SP_CLOWN], ART_CLOWN, PAINT_CLOWN, 3, COLOR_CLOWN, COLOR_CLOWN_FINS, 2.4f, 1.6f);
  setupSpecies(SPECIES[SP_ANGEL], ART_ANGEL, PAINT_ANGEL, 5, COLOR_ANGEL, COLOR_ANGEL_FINS, 1.7f, 0.9f);
  setupSpecies(SPECIES[SP_GROUPER], ART_GROUPER, PAINT_GROUPER, 4, COLOR_GROUPER, COLOR_GROUPER_SPOTS, 1.3f, 0.7f);
  setupSpecies(SPECIES[SP_PUFFER], ART_PUFFER, PAINT_PUFFER, 3, COLOR_PUFFER, COLOR_PUFFER_FINS, 1.1f, 1.0f);
  setupSpecies(SPECIES[SP_WHALE], ART_WHALE, PAINT_WHALE, 5, COLOR_WHALE, COLOR_WHALE_FINS, 1.6f, 0.5f);
  setupSpecies(SPECIES[SP_SNAIL], ART_SNAIL, PAINT_SNAIL, 2, COLOR_SNAIL_SHELL, COLOR_SNAIL_BODY, 0.2f, 1.0f);
  setupSpecies(SPECIES[SP_CRAB], ART_CRAB, PAINT_CRAB, 4, COLOR_CRAB, COLOR_CRAB_CLAWS, 1.0f, 1.0f);
  setupSpecies(SPECIES[SP_CASTLE], ART_CASTLE, PAINT_CASTLE, 5, COLOR_CASTLE, COLOR_CASTLE_FLAG, 0, 0);
  setupSpecies(SPECIES[SP_CHEST], ART_CHEST, PAINT_CHEST, 3, COLOR_CHEST, COLOR_CHEST_GOLD, 0, 0);
  setupSpecies(SPECIES[SP_CHEST_OPEN], ART_CHEST_OPEN, PAINT_CHEST_OPEN, 4, COLOR_CHEST, COLOR_CHEST_GOLD, 0, 0);
  setupSpecies(SPECIES[SP_FAN_CORAL], ART_FAN_CORAL, PAINT_FAN_CORAL, 4, 0xFF7AA8, 0xFFC2D8, 0, 0);
  setupSpecies(SPECIES[SP_TUBE_CORAL], ART_TUBE_CORAL, PAINT_TUBE_CORAL, 4, 0xB48CFF, 0xE0CCFF, 0, 0);
  setupSpecies(SPECIES[SP_LIONFISH], ART_LIONFISH, PAINT_LIONFISH, 3, COLOR_LIONFISH, COLOR_LIONFISH_FINS, 1.4f, 0.9f);
  setupSpecies(SPECIES[SP_BETTA], ART_BETTA, PAINT_BETTA, 3, COLOR_BETTA, COLOR_BETTA_FINS, 2.0f, 1.3f);
  setupSpecies(SPECIES[SP_CAVE], ART_CAVE, PAINT_CAVE, 5, COLOR_ROCK, COLOR_ROCK, 0, 0);
  setupSpecies(SPECIES[SP_EEL], ART_EEL, PAINT_EEL, 4, COLOR_EEL, 0xC5E0A5, 0, 0);
  setupSpecies(SPECIES[SP_EEL_SHUT], ART_EEL_SHUT, PAINT_EEL_SHUT, 4, COLOR_EEL, 0xC5E0A5, 0, 0);
  setupSpecies(SPECIES[SP_TURTLE], ART_TURTLE, PAINT_TURTLE, 5, COLOR_TURTLE, COLOR_TURTLE_SHELL, 1.2f, 0.5f);
  SPECIES[SP_TURTLE].facesLeft = true;
  Species& p = SPECIES[SP_PUFFER];
  p.puffRows = 3; p.puffW = 0;
  for (int i = 0; i < p.puffRows; i++) {
    p.puff[i] = ART_PUFFED[i]; p.puffPaint[i] = PAINT_PUFFED[i];
    int L2 = strlen(ART_PUFFED[i]); if (L2 > p.puffW) p.puffW = L2;
  }
}

// Color for one character of a fish, from its paint letter.
static Col paintColor(const Species* sp, char p, float phase, int c) {
  switch (p) {
    case 'l': return sp->pal[PC_LIGHT];
    case 'd': return sp->pal[PC_DARK];
    case 'f': return sp->pal[PC_FIN];
    case 'F': return sp->pal[PC_FIN_LIGHT];
    case 'w': return sp->pal[PC_WHITE];
    case 'k': return sp->pal[PC_MARK];
    case 'e': return sp->pal[PC_EYE];
    case 's': return mixCol(sp->pal[PC_BODY], sp->pal[PC_LIGHT], .5f + .5f * sinf(phase * .5f + c * .9f));
    default: return sp->pal[PC_BODY];
  }
}

static inline const char* weatherWord(int code);  // (further down) a short word for a weather code

// ---------------------------------------------------------------- entities
struct School { float tx, ty, timer; };
struct Food { float x, y, phase, landed, spin; bool alive; };
struct Fish {
  const Species* sp; float d;
  float x, y, vx, vy, f, ft, phase, tx, ty, tTimer, burst, blink, blinkT, puff, gulp;
  School* school; float offx, offy; int target;
  uint8_t mood;   // MOOD_NORMAL, MOOD_SHY (keeps near the castle), MOOD_CURIOUS (visits the snail and crab)
  int8_t buddy;   // index of the fish it swims with, or -1
  float size;     // how big it's drawn: FISH_SIZE (x 0.85 while young, if FISH_GROW is on)
};
// A fish's overall scale: its depth (distant fish are smaller) times its size.
static inline float fishScale(const Fish& f) { return f.d * f.size; }
enum { MOOD_NORMAL, MOOD_SHY, MOOD_CURIOUS };
enum { WEATHER_CLEAR, WEATHER_CLOUDY, WEATHER_RAIN, WEATHER_SNOW, WEATHER_STORM, WEATHER_FOG, WEATHER_COUNT };
struct Bubble { float x, y, age, phase, s; };
struct Kelp { float x; int h; float phase, amp; Col color; };
struct Grass { float x; int h; float phase; Col color; };
struct Plankton { float x, y, z, phase; };

class Tank {
 public:
  static const int MAX_FISH = 48, MAX_FOOD = 48, MAX_BUBBLES = 140, MAX_KELP = 16, MAX_GRASS = 60, N_PLANKTON = 70;
  Fish fish[MAX_FISH]; int nFish = 0;
  School school;
  Food food[MAX_FOOD]; int nFood = 0;
  Bubble bubbles[MAX_BUBBLES]; int nBubbles = 0;
  Kelp kelp[MAX_KELP]; int nKelp = 0;
  Grass grass[MAX_GRASS]; int nGrass = 0;
  Plankton plankton[N_PLANKTON];
  struct { float x, y, vx, pulse; } jelly;
  struct { float x, dir, pause, phase, snap, blink, blinkT, click; int clickSide; } crab;
  bool night = false;
  // Effects that the automatic quality control may switch off at run time.
  // glowLight = glow only every other letter (about half the work, nearly the same look).
  bool glow = FISH_GLOW != 0, glowLight = FISH_GLOW == 2, solid = FISH_SOLID, shadows = FISH_SHADOWS;
  // little extras
  static const int N_GLINTS = 5;
  struct { float x, life, dur; } glints[N_GLINTS];
  struct { float x, dir, pause; } snail;
  Fish whale; bool whaleOn = false; float whaleWait = 0;  // the rare visitor
  float chestOpen = 0, chestWait = 20;                    // treasure chest lid
  static const int N_POPS = 10, N_PUFFS = 14, N_RIPPLES = 14;
  struct { float x, life; } pops[N_POPS];                 // bubbles popping at the surface
  static const int N_FLAKES = 36;
  struct { float x, y, vy, phase, end, s; } flakes[N_FLAKES];  // snow; a flake is falling while y < end
  static const int N_WISPS = 10;
  struct { float x, y, vx, life, dur; int len; } wisps[N_WISPS];  // fog: mist drifting near the top
  float flash = 0;                                        // lightning: 0..1 (the board raises the backlight with it)
  float boltWait = 3, boltT = -1;                         // storm: seconds until the next bolt / into the current one
  struct { float x, y, vx, vy, life; } puffs[N_PUFFS];    // sand kicked up
  int crabStep = 0;
  float crabBlocked = 0;                                  // crab waiting at the snail
  bool snailTucked = false;
  float eelOut = 0, eelWait = 25, eelAge = 0;             // eel: seconds left outside / until next peek / since it came out
  float scatterCool = 0;                                  // minnows: time until they can be startled again
  float visitorAlpha = .28f;
  uint8_t weather = WEATHER_CLEAR;                        // set from outside (see USE_WEATHER)
  bool wasNight = false;

  void begin(uint32_t seed) {
    rngState = seed ? seed : 1;
    initSpecies();
    layout();
    populate();
  }

  // ------------------------------------------------ background (drawn once, kept in memory)
  // If sceneryFree is given, it receives a copy of rows sceneryTop()..bottom as they
  // look before the castle, rocks and coral are added (see Gfx::clean).
  void renderBackground(Gfx& g, uint16_t* dst, bool nightBg, uint16_t* sceneryFree = nullptr) {
    bgNight = nightBg;
    // 1) water gradient, light rays, surface glow, lights-out, vignette, glare — per pixel,
    //    with a little ordered dithering so the 16-bit colors don't show rings/bands.
    static const uint8_t bayer[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };
    const float rayCx[5] = { W * .1f, W * .3f, W * .5f, W * .7f, W * .9f };
    Col c0 = hexCol(nightBg ? 0x071D31 : 0x0F5A82), c1 = hexCol(nightBg ? 0x041426 : 0x08405F), c2 = hexCol(nightBg ? 0x020A14 : 0x03192B);
    for (int y = 0; y < SCR_H; y++) {
      float t = y / H, r, gg, b;
      if (t < .45f) { float k = t / .45f; r = c0.r + (c1.r - c0.r) * k; gg = c0.g + (c1.g - c0.g) * k; b = c0.b + (c1.b - c0.b) * k; }
      else { float k = (t - .45f) / .55f; r = c1.r + (c2.r - c1.r) * k; gg = c1.g + (c2.g - c1.g) * k; b = c1.b + (c2.b - c1.b) * k; }
      float sEnd = SURFACE_Y + LH;
      if (y < sEnd) {
        float a = (1 - y / sEnd) * (nightBg ? .12f : .22f);
        float sr = nightBg ? 90 : 170, sg = nightBg ? 140 : 230, sb = nightBg ? 190 : 255;
        r += (sr - r) * a; gg += (sg - gg) * a; b += (sb - b) * a;
      }
      for (int x = 0; x < SCR_W; x++) {
        float pr = r, pg = gg, pb = b;
        if (!nightBg && y < SAND_TOP) {
          float fr = y / SAND_TOP;
          for (int i = 0; i < 5; i++) {
            float L = rayCx[i] - W * .025f + (W * .01f) * fr, R = rayCx[i] + W * .025f + (W * .085f) * fr;
            float edge = fminf(x - L, R - x);
            if (edge <= -1) continue;
            float a = (.05f + .035f * sinf(i * 1.7f)) * (1 - fr) * clampf(edge + 1, 0, 1) * 1.3f;
            pr += (190 - pr) * a; pg += (235 - pg) * a; pb += (255 - pb) * a;
          }
        }
        if (nightBg && y < SAND_TOP) {
          // moonlight: one pale beam slanting down, and a soft glow where it enters the water
          float fr = y / SAND_TOP, mx = W * .62f + W * .06f * fr;
          float edge = W * (.022f + .05f * fr) - fabsf(x - mx);
          if (edge > -1) {
            float a = .11f * (1 - fr) * clampf(edge + 1, 0, 1);
            pr += (150 - pr) * a; pg += (185 - pg) * a; pb += (255 - pb) * a;
          }
          float gx = (x - W * .62f) / (W * .09f), gy = y / (LH * 2.2f);
          float gl = .22f * expf(-(gx * gx + gy * gy));
          pr += (170 - pr) * gl; pg += (200 - pg) * gl; pb += (255 - pb) * gl;
        }
        glass(rgb(0, 0, 0), x, y, pr, pg, pb);
        float d = bayer[y & 3][x & 3] / 16.f - .47f;
        int R8 = (int)clampf(gPr + d * 8, 0, 255), G8 = (int)clampf(gPg + d * 4, 0, 255), B8 = (int)clampf(gPb + d * 8, 0, 255);
        dst[y * SCR_W + x] = pack565(R8, G8, B8);
      }
    }
    // 2) sand, then rocks, coral and castle — drawn as text into the background
    uint16_t* saveFb = g.fb; bool saveMark = g.marking;
    g.fb = dst; g.marking = false;
    uint32_t saveRng = rngState; rngState = 777;  // same sand pattern for day and night
    const char* top = ".,:;'`_. ,."; const char* mid = ".:;,'. ,:.";
    const uint32_t shades[3][3] = { { 0xE3C98F, 0xD9BF86, 0xCDB07A }, { 0xB89A64, 0xA98E5C, 0x9C8153 }, { 0x86704A, 0x7A6643, 0x6E5B3B } };
    const uint32_t pebbleCols[3] = { 0x9AA3AD, 0xC9B9A0, 0x7F8A96 };
    int cols = SCR_W / FONT_CELL_W + 2;
    for (int r = 0; r < 3; r++)
      for (int c = 0; c < cols; c++) {
        char ch; Col col;
        if (r == 0 && rnd01() < .07f) { const char peb[3] = { 'o', DEG, 'O' }; ch = peb[rngNext() % 3]; col = hexCol(pebbleCols[rngNext() % 3]); }
        else { const char* set = r == 0 ? top : mid; ch = set[rngNext() % strlen(set)]; col = hexCol(shades[r][rngNext() % 3]); }
        bgChar(g, ch, c * CW, SAND_TOP + r * LH * .8f, col);
      }
    rngState = saveRng;
    if (sceneryFree) memcpy(sceneryFree, dst + sceneryTop() * SCR_W, (SCR_H - sceneryTop()) * SCR_W * 2);
    static const uint32_t brainC[] = { 0xFFA25C, 0xFF8A3D };
    gNow = &g;
    drawCave(dst);
    if (SHOW_CHEST) bgArt(g, SPECIES[SP_CHEST], CHEST_X);
    bgArt(g, SPECIES[SP_FAN_CORAL], W * .40f);
    drawArt(g, BRAIN, 2, W * .52f, brainC, 2);
    if (SHOW_CASTLE) bgArt(g, SPECIES[SP_CASTLE], CASTLE_X);
    bgArt(g, SPECIES[SP_TUBE_CORAL], W * .89f);
    g.fb = saveFb; g.marking = saveMark;
  }

  // ------------------------------------------------ clock, baked into the background
  // The clock only changes once a second, so instead of drawing it every frame it is
  // drawn into both backgrounds when its text changes. saveDay/saveNight (each CLOCK_W x
  // CLOCK_H pixels) keep the bare water under it so the old time can be wiped.
  // Call every frame before Gfx::beginFrame. Without the save buffers it does nothing and
  // draw() falls back to drawing the clock each frame.
  static const int CLOCK_W = 252, CLOCK_H = 136;
  void bakeClock(Gfx& g, uint16_t* bgDay, uint16_t* bgNight, uint16_t* saveDay, uint16_t* saveNight, const char* text) {
    if (!saveDay || !saveNight) return;
    clockBaked = true;
    if (!strcmp(text, clockShown)) return;
    int x0 = CLOCK_CORNER == 1 ? SCR_W - (int)(CW * 2) - CLOCK_W + 4 : (int)(CW * 2) - 4;
    int y0 = (int)(SURFACE_Y + LH * .9f) - 2;
    uint16_t* bgs[2] = { bgDay, bgNight };
    uint16_t* saves[2] = { saveDay, saveNight };
    uint16_t* keepFb = g.fb; bool keepMark = g.marking, keepNight = night;
    g.marking = false;
    for (int i = 0; i < 2; i++) {
      for (int y = 0; y < CLOCK_H; y++) {
        uint16_t* row = bgs[i] + (y0 + y) * SCR_W + x0;
        if (!clockSaved) memcpy(saves[i] + y * CLOCK_W, row, CLOCK_W * 2);  // remember the bare water
        else memcpy(row, saves[i] + y * CLOCK_W, CLOCK_W * 2);             // wipe the old time
      }
      g.fb = bgs[i]; night = i == 1;
      if (text[0]) drawClock(g, text);
    }
    clockSaved = true;
    g.fb = keepFb; g.marking = keepMark; night = keepNight;
    strncpy(clockShown, text, sizeof clockShown - 1);
    clockShown[sizeof clockShown - 1] = 0;
    g.invalidate(x0, y0, x0 + CLOCK_W, y0 + CLOCK_H);
  }

  // ------------------------------------------------ weather corner, baked into the background
  // A weather picture with the temperature under it, in the top corner opposite the clock.
  // Like the clock, it's drawn into both backgrounds only when it changes; saveDay/saveNight
  // (each WX_W x WX_H pixels) keep the bare water under it. code < 0 and temp NAN = show nothing.
  static const int WX_W = 184, WX_H = 196;  // roomy enough for the glow around the picture
  void bakeWeather(Gfx& g, uint16_t* bgDay, uint16_t* bgNight, uint16_t* saveDay, uint16_t* saveNight, int code, float temp) {
    wxCode = code; wxTemp = temp;
    if (!saveDay || !saveNight) return;
    wxBaked = true;
    char key[24];
    snprintf(key, sizeof key, "%d|%d", code, temp == temp ? (int)lrintf(temp) : -999);
    if (!strcmp(key, wxShown)) return;
    int x0, y0;
    wxBox(x0, y0);
    uint16_t* bgs[2] = { bgDay, bgNight };
    uint16_t* saves[2] = { saveDay, saveNight };
    uint16_t* keepFb = g.fb; bool keepMark = g.marking, keepNight = night;
    g.marking = false;
    for (int i = 0; i < 2; i++) {
      for (int y = 0; y < WX_H; y++) {
        uint16_t* row = bgs[i] + (y0 + y) * SCR_W + x0;
        if (!wxSaved) memcpy(saves[i] + y * WX_W, row, WX_W * 2);  // remember the bare water
        else memcpy(row, saves[i] + y * WX_W, WX_W * 2);            // wipe the old picture
      }
      g.fb = bgs[i]; night = i == 1;   // the night background gets the moon instead of the sun
      drawWeather(g, code, temp);
    }
    wxSaved = true;
    g.fb = keepFb; g.marking = keepMark; night = keepNight;
    strncpy(wxShown, key, sizeof wxShown - 1);
    wxShown[sizeof wxShown - 1] = 0;
    g.invalidate(x0, y0, x0 + WX_W, y0 + WX_H);
  }

  // ------------------------------------------------ feeding
  void dropFood(float x) {
    int n = 5 + (rngNext() % 3);
    for (int i = 0; i < n && nFood < MAX_FOOD; i++)
      food[nFood++] = { x + rnd(-2, 2) * CW, SURFACE_Y + rnd(0, .6f) * LH, rnd(0, 6.28f), 0, rnd(0, 6), true };
  }
  void dropFoodRandom() { dropFood(rnd(W * .2f, W * .8f)); }

  // ------------------------------------------------ simulation step
  void update(float dt, float t) {
    float tempo = night ? .45f : 1.f;
    if (night != wasNight) {  // lights changed: everyone picks a new spot (low down at night)
      wasNight = night;
      for (int i = 0; i < nFish; i++) if (!fish[i].school) pickTarget(fish[i]);
      school.timer = 0;
    }
    school.timer -= dt;
    if (school.timer <= 0) {
      // the school heads for whichever of a few spots has the most open water
      float sw = (NUM_MINNOWS * 2.2f + 3) * CW * FISH_SIZE, sh = LH * 4.9f * FISH_SIZE, bestS = -1;
      for (int n = 0; n < 6; n++) {
        float x = rnd(W * .15f, W * .85f), y = rnd(SURFACE_Y + LH * 3, SAND_TOP - LH * 3);
        if (SLEEPY_NIGHT && night) y = rnd((SURFACE_Y + SAND_TOP) / 2, SAND_TOP - LH * 3);
        float s = openness(x, y, sw, sh, 1, nullptr, &school);
        if (s > bestS) { bestS = s; school.tx = x; school.ty = y; }
      }
      school.timer = rnd(3, 7);
    }
    if (MINNOWS_SCATTER && scatterCool <= 0) {
      float cx = 0, cy = 0; int n = 0;
      for (int i = 0; i < nFish; i++) if (fish[i].school) { cx += fish[i].x; cy += fish[i].y; n++; }
      if (n) {
        cx /= n; cy /= n;
        for (int i = 0; i < nFish; i++) {
          const Fish& b = fish[i];
          if (b.school || b.d < 1 || b.sp->rows < 4) continue;  // only the big ones are scary
          if (fabsf(b.x - cx) < b.sp->w * CW * b.size / 2 + CW * 5 && fabsf(b.y - cy) < b.sp->rows * LH * b.size / 2 + LH * 1.5f) {
            school.timer = 0;                                    // dart off to open water...
            for (int k = 0; k < nFish; k++) if (fish[k].school) fish[k].burst = rnd(.6f, 1.1f);  // ...fast
            scatterCool = 5;
            break;
          }
        }
      }
    } else scatterCool -= dt;
    for (int i = 0; i < nFish; i++) updateFish(fish[i], dt, tempo);
    compactFood();
    if (FISH_GROW)  // about two weeks of running to grow from young to full size
      for (int i = 0; i < nFish; i++) fish[i].size = fminf(FISH_SIZE, fish[i].size + dt * (.15f * FISH_SIZE / (14 * 86400.f)));

    for (int i = 0; i < nFood; i++) {
      Food& p = food[i];
      if (p.y < SAND_TOP - LH * .2f) { p.y += LH * .65f * dt; p.x += sinf(t * 2.2f + p.phase) * CW * .5f * dt; }
      else {
        if (p.landed == 0) sandPuff(p.x, 2);
        p.landed += dt;
      }
      if (p.landed >= 14) p.alive = false;
    }
    compactFood();

    // bubbler beside the castle
    if (SHOW_CASTLE && rnd01() < dt * 3.5f)
      addBubble(CASTLE_X + CW * 13.5f + rnd(-.2f, .2f) * CW, SAND_TOP - LH * 4.3f, rnd(.7f, 1));

    // treasure chest: the lid lifts now and then and lets out a burst of bubbles
    if (SHOW_CHEST) {
      if (chestOpen > 0) {
        chestOpen -= dt;
        if (rnd01() < dt * 7) addBubble(CHEST_X + rnd(2, 7) * CW, SAND_TOP - LH * 2.6f, rnd(.6f, 1));
      } else if ((chestWait -= dt) <= 0) { chestOpen = rnd(3, 4.5f); chestWait = rnd(35, 80); }
    }
    int nb = 0;
    for (int i = 0; i < nBubbles; i++) {
      Bubble b = bubbles[i];
      b.age += dt;
      b.y -= LH * (1.3f + b.age * .35f) * dt;
      b.x += sinf(t * 3 + b.phase) * CW * .45f * dt;
      if (b.y > SURFACE_Y - LH * .3f) bubbles[nb++] = b;
      else if (BUBBLE_POPS && b.age > 1.5f)  // a grown bubble reaching the surface pops with a little ring
        for (int k = 0; k < N_POPS; k++) if (pops[k].life <= 0) { pops[k] = { b.x, .35f }; break; }
    }
    nBubbles = nb;
    for (int k = 0; k < N_POPS; k++) if (pops[k].life > 0) pops[k].life -= dt;
    for (int k = 0; k < N_PUFFS; k++) if (puffs[k].life > 0) {
      puffs[k].life -= dt;
      puffs[k].x += puffs[k].vx * dt; puffs[k].y += puffs[k].vy * dt;
      puffs[k].vy += LH * 1.2f * dt;  // settles back down
      puffs[k].vx *= 1 - dt * 2;
    }

    // jellyfish: contract to rise, relax to drift down
    jelly.pulse += dt * 1.5f * tempo;
    if (jelly.pulse > 62.831853f) jelly.pulse -= 62.831853f;
    float c = fmaxf(0, sinf(jelly.pulse));
    jelly.y += (-c * LH * 1.1f + LH * .22f) * dt;
    jelly.x += (jelly.vx + sinf(jelly.pulse * .3f) * .15f) * CW * dt;
    if (jelly.x < CW) jelly.vx = fabsf(jelly.vx);
    if (jelly.x > W - CW * 11) jelly.vx = -fabsf(jelly.vx);
    jelly.y = clampf(jelly.y, SURFACE_Y + LH, SAND_TOP - LH * 7);

    // crab patrols, and walks over to food that reaches the sand
    if (SHOW_CRAB) {
      float crabW = SPECIES[SP_CRAB].w * CW * CRAB_SCALE;
      int goal = -1; float gd = W * .45f;
      const float crabMinX = SHOW_EEL ? CAVE_X + 21.5f * CW : 0;  // stays out of the cave mouth
      for (int i = 0; i < nFood; i++)
        if (food[i].landed > 0 && food[i].x > crabMinX + crabW / 2) {
          float dd = fabsf(food[i].x - (crab.x + crabW / 2)); if (dd < gd) { gd = dd; goal = i; }
        }
      if (goal >= 0) {
        crab.pause = 0;
        float s = sgnf(food[goal].x - (crab.x + crabW / 2)); crab.dir = s != 0 ? s : 1;
        if (gd < CW * 1.2f) {
          food[goal].alive = false; crab.snap = .6f; crab.pause = .8f;
          addBubble(crab.x + crabW / 2, SAND_TOP - LH * 3.2f, .6f);
        }
      }
      compactFood();
      // the snail is in the way: stop, think about it, then turn back
      if (SHOW_SNAIL && goal < 0) {
        float snailMid = snail.x + SPECIES[SP_SNAIL].w * CW * CRITTER / 2, ahead = (snailMid - (crab.x + crabW / 2)) * crab.dir;
        if (ahead > 0 && ahead < crabW / 2 + CW * 5) {
          if (crabBlocked <= 0) { crabBlocked = 1.6f; crab.pause = 1.6f; crab.click = .3f; }
          else if ((crabBlocked -= dt) <= 0) { crab.dir = -crab.dir; crab.pause = 0; }
        } else crabBlocked = 0;
      }
      if (crab.snap > 0) crab.snap -= dt;
      if (crab.pause > 0) crab.pause -= dt;
      else {
        crab.x += crab.dir * CW * (goal >= 0 ? 3 : 1.6f) * tempo * dt;
        crab.phase += dt * 9 * tempo;
        if (crab.phase > 62.831853f) crab.phase -= 62.831853f;
        int step = sinf(crab.phase) > 0;
        if (step != crabStep) { crabStep = step; if (rnd01() < .5f) sandPuff(crab.x + crabW / 2 - crab.dir * crabW * .3f, 1); }
        if (goal < 0 && rnd01() < dt * .18f) crab.pause = rnd(1, 4);
        if (goal < 0 && rnd01() < dt * .05f) crab.dir *= -1;
      }
      if (crab.x < crabMinX) { crab.x = fmaxf(crab.x, crabMinX - CW); crab.dir = 1; }
      if (crab.x + crabW > W) crab.dir = -1;
      // little habits: blink, and click one claw now and then while idle
      crab.blink -= dt;
      if (crab.blink <= 0) { crab.blinkT = .15f; crab.blink = rnd(2, 6); }
      if (crab.blinkT > 0) crab.blinkT -= dt;
      if (crab.click > 0) crab.click -= dt;
      else if (goal < 0 && rnd01() < dt * (crab.pause > 0 ? .5f : .12f)) { crab.click = .3f; crab.clickSide = rngNext() & 1; }
    }

    for (int i = 0; i < N_PLANKTON; i++) {
      Plankton& p = plankton[i];
      p.x += (sinf(t * .2f + p.phase) * .3f + .15f) * CW * dt * (.3f + p.z);
      p.y += cosf(t * .25f + p.phase) * LH * .08f * dt;
      if (p.x > W + 5) p.x = -5;
    }

    // sparkles on the surface (daytime)
    for (int i = 0; i < N_GLINTS; i++) {
      if (glints[i].life > 0) glints[i].life -= dt;
      else if (SURFACE_SPARKLES && !night && weather == WEATHER_CLEAR && rnd01() < dt * .25f) {
        glints[i].x = rnd(CW, W - CW);
        glints[i].life = glints[i].dur = rnd(.6f, 1.4f);
      }
    }

    // rain outside: raindrop rings all along the surface (more of them in a thunderstorm)
    if ((weather == WEATHER_RAIN && rnd01() < dt * 9) || (weather == WEATHER_STORM && rnd01() < dt * 14))
      for (int k = 0; k < N_POPS; k++) if (pops[k].life <= 0) { pops[k] = { rnd(CW, W - CW), .35f }; break; }

    // snow outside: flakes drift down through the top of the water, swaying, and melt away.
    // When the snow stops, the flakes already falling finish their way down.
    for (int i = 0; i < N_FLAKES; i++) {
      auto& f = flakes[i];
      if (f.y < f.end) { f.y += f.vy * dt; f.x += sinf(t * .9f + f.phase) * CW * .4f * dt; }
      else if (weather == WEATHER_SNOW && rnd01() < dt * .5f)
        flakes[i] = { rnd(0, W), rnd(-LH * .5f, SURFACE_Y * .5f), rnd(LH * .5f, LH * 1.1f), rnd(0, 6.28f),
                      rnd(SURFACE_Y + LH * 3, H * .5f), rnd(.5f, .85f) };
    }

    // thunderstorm: lightning every 8-25 seconds, a quick double flicker that fades.
    // The first bolt comes a few seconds after the storm starts.
    if (boltT >= 0) { if ((boltT += dt) > .6f) boltT = -1; }
    else if (weather != WEATHER_STORM) boltWait = rnd(2, 5);
    else if ((boltWait -= dt) <= 0) { boltT = 0; boltWait = rnd(8, 25); }
    flash = boltT < 0 ? 0 : boltT < .07f ? 1 : boltT < .15f ? .15f : boltT < .22f ? .85f : .85f * (1 - (boltT - .22f) / .38f);
    if (flash < 0) flash = 0;

    // fog outside: faint streaks of mist drifting slowly through the top of the water
    for (int i = 0; i < N_WISPS; i++) {
      auto& m = wisps[i];
      if (m.life > 0) { m.life -= dt; m.x += m.vx * dt; }
      else if (weather == WEATHER_FOG && rnd01() < dt * .5f) {
        float dur = rnd(14, 24);
        wisps[i] = { rnd(-CW * 4, W - CW * 6), rnd(SURFACE_Y + LH * .6f, H * .38f), rnd(-1, 1) * CW * .5f, dur, dur, 7 + (int)rnd(0, 8) };
      }
    }

    // snail inches along the sand, stopping now and then
    if (SHOW_SNAIL) {
      snailTucked = SHOW_CRAB && fabsf((snail.x + SPECIES[SP_SNAIL].w * CW * CRITTER / 2) - (crab.x + SPECIES[SP_CRAB].w * CW * CRAB_SCALE / 2)) <
                                 SPECIES[SP_CRAB].w * CW * CRAB_SCALE / 2 + CW * 7;
      if (snailTucked) { /* stays put inside its shell */ }
      else if (snail.pause > 0) snail.pause -= dt;
      else {
        snail.x += snail.dir * CW * .18f * tempo * dt;
        if (rnd01() < dt * .03f) snail.pause = rnd(3, 10);
        if (rnd01() < dt * .01f) snail.dir = -snail.dir;
      }
      float sw = SPECIES[SP_SNAIL].w * CW * CRITTER;
      if (snail.x < CW) snail.dir = 1;
      if (snail.x + sw > W - CW) snail.dir = -1;
    }

    // the eel peeks out of the rock arch now and then
    if (SHOW_EEL) {
      if (eelOut > 0) { eelOut -= dt; eelAge += dt; }
      else { eelAge = 0; if ((eelWait -= dt) <= 0) { eelOut = rnd(7, 12); eelWait = rnd(30, 75); } }
    }

    // the rare visitor: a whale (or turtle) drifting past far away
    if (VISITOR_EVERY_MINUTES > 0) {
      if (!whaleOn) {
        whaleWait -= dt;
        if (whaleWait <= 0) startWhale(rnd01() < .5f ? SP_WHALE : SP_TURTLE);
      } else {
        Fish& w = whale;
        w.vy = sinf(t * .3f) * LH * .12f;
        w.x += w.vx * dt; w.y += w.vy * dt;
        w.phase += dt * 1.8f;
        if (w.phase > 6.2831853f) w.phase -= 6.2831853f;
        w.blink -= dt;
        if (w.blink <= 0) { w.blinkT = .25f; w.blink = rnd(3, 8); }
        if (w.blinkT > 0) w.blinkT -= dt;
        float half = w.sp->w * CW * w.d / 2 + CW;
        if ((w.vx > 0 && w.x - half > W) || (w.vx < 0 && w.x + half < 0)) {
          whaleOn = false;
          whaleWait = VISITOR_EVERY_MINUTES * 60.f * rnd(.7f, 1.3f);
        }
      }
    }
  }

  // Send a visitor across the back of the tank: SP_WHALE (far, faint) or SP_TURTLE (closer).
  void startWhale(int kind = SP_WHALE) {
    Fish& w = whale;
    w = Fish{};
    w.sp = &SPECIES[kind]; w.d = kind == SP_TURTLE ? .8f : .6f; w.target = -1; w.blink = 3; w.size = 1; w.buddy = -1;
    visitorAlpha = kind == SP_TURTLE ? .5f : .28f;
    float dir = rnd01() < .5f ? 1 : -1, half = w.sp->w * CW * w.d / 2 + CW;
    w.x = dir > 0 ? -half : W + half;
    w.y = rnd(SURFACE_Y + LH * 3, SAND_TOP - LH * 5);
    w.vx = dir * CW * w.sp->speed; w.f = w.ft = dir;
    whaleOn = true;
  }

  // ------------------------------------------------ drawing (dynamic things only)
  void draw(Gfx& g, float t, const char* clockText) {
    // the whale is farthest away, behind everything
    if (whaleOn) drawFish(g, whale, visitorAlpha * (night ? .65f : 1), false);
    // light rippling over the sand, then shadows on top of it
    if (SAND_RIPPLES && !night) drawRipples(g, t);
    if (shadows) drawShadows(g);
    // far plankton
    for (int i = 0; i < N_PLANKTON; i++) if (plankton[i].z < .5f) drawPlankton(g, plankton[i], t, 1);
    // the clock sits behind the fish, like it's etched on the back glass
    // (normally it's baked into the background by bakeClock, and costs nothing here)
    if (clockText && clockText[0] && !clockBaked) drawClock(g, clockText);
    if (!wxBaked) drawWeather(g, wxCode, wxTemp);  // same for the weather corner
    // distant fish
    for (int i = 0; i < nFish; i++) if (fish[i].d < 1) drawFish(g, fish[i], night ? .22f : .33f, false);
    // kelp
    for (int k = 0; k < nKelp; k++) {
      Kelp& s = kelp[k]; float ox = 0;
      Col col = dim(s.color);
      for (int i = 0; i < s.h; i++) {
        ox += sinf(t * .75f + i * .4f + s.phase) * CW * s.amp * ((float)i / s.h + .3f);
        g.glyph(i % 2 ? '(' : ')', s.x + ox + CW / 2, SAND_TOP - (i + 1) * LH + FONT_CELL_H / 2, 1, 1, col, 255);
      }
    }
    // eel, sliding out of the dark cave mouth head first. Nothing behind the cave's back
    // wall is drawn, and it isn't solid, so the dark of the cave stays around it.
    if (SHOW_EEL && eelOut > 0) {
      const Species& e = SPECIES[SP_EEL];
      float back = CAVE_X + CAVE_BACK * CW, k = fminf(1, fminf(eelAge, eelOut) / 1.5f);  // slides out, and back in at the end
      k = k * k * (3 - 2 * k);
      float sway = sinf(t * 1.3f) * 2, nod = sinf(t * 1.1f) * CW * .3f * k;  // leans out and back a little
      float ew = e.w * CW * CRITTER, mid = SAND_TOP - LH * 1.5f;            // the middle of the cave mouth
      float reach = CW * 10;  // only the head and neck come out, like a real moray in its hole
      bool open = EEL_MOUTH == 2 && k >= 1 && sinf(t * 2.4f) > -.2f;        // opens and closes its mouth to breathe
      float part = 0;                                                         // EEL_MOUTH 1: how far the lips are parted (0..1)
      if (EEL_MOUTH == 1 && k >= 1) { part = fminf(1, fmaxf(0, (sinf(t * 2.4f) + .2f) / .8f)); part = part * part * (3 - 2 * part); }
      float ex = back - ew + k * reach + nod, ey = mid - LH * 1.5f * CRITTER + sway, cw = CW * CRITTER, lh = LH * CRITTER;
      // A solid green body under the outline (from under the fin line down to the belly
      // line), so it looks like a thick, muscular eel rather than two thin lines. The snout
      // is narrower. Nothing behind the cave's back wall is drawn.
      Col bodyFill = dim(mixCol(hexCol(COLOR_EEL), rgb(0, 0, 0), .45f));
      int lenFin = strlen(ART_EEL[0]), lenBody = strlen(ART_EEL[1]);
      static const float TIP_TOP[3] = { .3f, .1f, 0 }, TIP_BOTTOM[3] = { .32f, .12f, 0 };  // snout tip .. 2 columns back
      for (int c = 2; c < lenBody; c++) {
        // whole-pixel edges shared with the next strip, so no pixel gets blended twice
        int x0 = (int)lrintf(ex + c * cw), x1 = (int)lrintf(ex + (c + 1) * cw), xb = (int)lrintf(back);
        if (x1 <= xb) continue;
        if (x0 < xb) x0 = xb;
        char fin = c < lenFin ? ART_EEL[0][c] : ' ';
        float yTop = fin != ' ' ? ey + (inkTop(fin) + .12f) * lh : ey + lh * 1.05f;  // under the fin, or the top of the head
        const bool body = c < (open ? 15 : 13);  // behind the jaw's hinge: the body is as deep as the head and jaw
        float yBot = body ? ey + lh * 2.75f : ey + lh * 1.97f;
        int fromTip = lenBody - 1 - c;  // the last columns narrow to a point; the eye still sits on the head
        if (fromTip < 3) { yTop += lh * TIP_TOP[fromTip]; yBot -= lh * TIP_BOTTOM[fromTip]; }
        g.fillRect(x0, (int)yTop, x1, (int)yBot, bodyFill, 225);
        if (body)  // a darker edge along the belly
          g.fillRect(x0, (int)yBot - 2, x1, (int)yBot, mixCol(bodyFill, rgb(0, 0, 0), .45f), 235);
      }
      // the lower jaw, from its hinge (just behind the gill) to under the snout. Open, it hangs
      // a row lower with the teeth along its top edge; shut, it sits right under the head.
      {
        const int hinge = open ? 15 : 13, tip = 19;  // open, only the front of the jaw drops (a small gape)
        float jawTop = open ? ey + lh * 2.55f : ey + lh * 1.97f;   // just under the teeth / the head
        float jawBot = open ? ey + lh * 3.3f : ey + lh * 2.9f;      // down to its outline
        for (int c = hinge; c <= tip; c++) {
          int x0 = (int)lrintf(ex + c * cw), x1 = (int)lrintf(ex + (c + 1) * cw), xb = (int)lrintf(back);
          if (x1 <= xb) continue;
          if (x0 < xb) x0 = xb;
          float top = (open && c == hinge) ? ey + lh * 1.97f : jawTop;  // the hinge joins it to the head
          float bot = jawBot - (c >= tip - 1 ? lh * .18f * (c - tip + 2) : 0);  // rounded off at the front
          g.fillRect(x0, (int)top, x1, (int)bot, bodyFill, 225);
          if (!open)  // the same darker edge along the bottom as the body's belly
            g.fillRect(x0, (int)bot - 2, x1, (int)bot, mixCol(bodyFill, rgb(0, 0, 0), .45f), 235);
          if (!open && c > hinge) {  // shut: a thin dark line where the jaws meet; parted, a narrow wedge toward the front
            int y0 = (int)(ey + lh * 1.97f);
            Col lip = mixCol(bodyFill, rgb(0, 0, 0), .6f);
            if (part <= 0) g.fillRect(x0, y0, x1, y0 + 2, lip, 255);
            else {  // tapers smoothly, pixel by pixel, from the hinge to the snout
              float xh = ex + (hinge + 1) * cw, span = (tip + 1) * cw - (hinge + 1) * cw;
              for (int x = x0; x < x1; x++)
                g.fillRect(x, y0, x + 1, y0 + 2 + (int)(part * lh * .22f * fminf(1, (x - xh) / span)), lip, 255);
            }
          }
        }
      }
      drawCritter(g, SPECIES[open ? SP_EEL : SP_EEL_SHUT], ex, ey, 1, nullptr, back, false, CRITTER);
    }
    // bubbles
    for (int i = 0; i < nBubbles; i++) {
      Bubble& b = bubbles[i];
      char ch = b.age < .7f ? '.' : b.age < 2 ? 'o' : 'O';
      float fade = clampf((b.y - SURFACE_Y) / (LH * 2), 0, 1);
      g.glyph(ch, b.x, b.y, 1, 1, dim(rgb(215, 242, 255)), int(255 * .75f * fade));
    }
    // the chest with its lid up (closed, it's just part of the background)
    if (SHOW_CHEST && chestOpen > 0) drawCritter(g, SPECIES[SP_CHEST_OPEN], CHEST_X, SAND_TOP - LH * 4, 1);
    if (SHOW_JELLYFISH && !night) drawJelly(g, t);
    // fish
    for (int i = 0; i < nFish; i++) if (fish[i].d >= 1) drawFish(g, fish[i], 1, glow);
    // food
    for (int i = 0; i < nFood; i++) {
      Food& p = food[i];
      float fade = p.landed > 10 ? 1 - (p.landed - 10) / 4 : 1;
      g.glyph(sinf(t * 4 + p.spin) > 0 ? '*' : '+', p.x, p.y, 1, 1, dim(rgb(240, 190, 110)), int(255 * fade));
    }
    // grass tufts
    for (int k = 0; k < nGrass; k++) {
      Grass& s = grass[k]; float ox = 0;
      Col col = dim(s.color);
      for (int i = 0; i < s.h; i++) {
        float dx = sinf(t * 1.1f + i * .6f + s.phase) * CW * .22f;
        ox += dx;
        char ch = dx > CW * .07f ? '/' : dx < -CW * .07f ? '\\' : '|';
        g.glyph(ch, s.x + ox + CW / 2, SAND_TOP - (i + .6f) * LH + FONT_CELL_H / 2, 1, 1, col, 255);
      }
    }
    // sand kicked up by the crab or by food landing
    for (int k = 0; k < N_PUFFS; k++) if (puffs[k].life > 0)
      g.glyph(k & 1 ? '.' : ',', puffs[k].x, puffs[k].y, 1, 1, dim(hexCol(0xD9BF86)), int(255 * fminf(1, puffs[k].life * 2.5f)));
    // snail
    if (SHOW_SNAIL)
      drawCritter(g, SPECIES[SP_SNAIL], snail.x, SAND_TOP - LH * 1.75f * CRITTER, snail.dir, snailTucked ? ART_SNAIL_TUCKED : nullptr,
                  -1e9f, true, CRITTER);
    // crab
    if (SHOW_CRAB) drawCrab(g, t);
    // near plankton
    for (int i = 0; i < N_PLANKTON; i++) if (plankton[i].z >= .5f) drawPlankton(g, plankton[i], t, 2);
    // surface waves (one dirty band instead of 85 small ones)
    bool m = g.marking; g.marking = false;
    float minY = 1e9, maxY = -1e9;
    for (float x = CW / 2; x < W; x += CW) {
      float y = waveY(x, t);
      float a = .35f + .35f * (sinf(x * .03f + t * 1.5f) * .5f + .5f);
      Col c = night ? rgb(130, 180, 220) : rgb(200, 240, 255);
      a = night ? a * .6f : a;
      if (flash > 0) { c = mixCol(c, rgb(255, 255, 255), flash); a += (1 - a) * flash; }  // lit up by lightning
      g.glyph('~', x, y + FONT_CELL_H / 2, 1, 1, c, int(255 * a));
      minY = fminf(minY, y); maxY = fmaxf(maxY, y + FONT_CELL_H);
    }
    g.marking = m;
    g.mark(0, (int)minY - 1, SCR_W, (int)maxY + 2);
    // rings where bubbles just popped
    for (int k = 0; k < N_POPS; k++) if (pops[k].life > 0) {
      float q = 1 - pops[k].life / .35f, r = CW * (.3f + q * .9f), y = waveY(pops[k].x, t) + FONT_CELL_H / 2 + 3;
      int a = int(200 * (1 - q) * (night ? .6f : 1));
      g.glyph('(', pops[k].x - r, y, .8f, .7f, rgb(215, 242, 255), a);
      g.glyph(')', pops[k].x + r, y, .8f, .7f, rgb(215, 242, 255), a);
    }
    // sparkles riding on the waves
    for (int i = 0; i < N_GLINTS; i++) if (glints[i].life > 0) {
      float k = sinf(glints[i].life / glints[i].dur * 3.14159f);
      g.glyph(k > .6f ? '*' : '+', glints[i].x, waveY(glints[i].x, t) + FONT_CELL_H / 2 - 2, .8f, .8f,
              rgb(255, 255, 235), int(230 * k), glow, rgb(200, 240, 255), int(90 * k));
    }
    // snowflakes: bright in the air, fading as they sink and melt
    for (int i = 0; i < N_FLAKES; i++) if (flakes[i].y < flakes[i].end) {
      const auto& f = flakes[i];
      float q = fminf(1, fmaxf(0, (f.y - SURFACE_Y) / (f.end - SURFACE_Y)));
      int a = int(230 * (1 - q * q) * (night ? .6f : 1));
      if (a > 4) g.glyph('*', f.x, f.y, f.s, f.s, rgb(240, 248, 255), a);
    }
    // fog: each wisp is a row of stretched, faint '~', fading in and out, thinner at its ends
    for (int i = 0; i < N_WISPS; i++) if (wisps[i].life > 0) {
      const auto& m = wisps[i];
      float k = fminf(1, fminf(m.dur - m.life, m.life) / 4);  // fade in / out over 4 s
      for (int j = 0; j < m.len; j++) {
        float edge = fminf(1, fminf(j + 1, m.len - j) / 2.f);
        int a = int((night ? 55 : 85) * k * edge);
        if (a > 3) g.glyph('~', m.x + j * CW * 1.3f, m.y + sinf(t * .4f + j * .7f + i) * 2, 1.5f, .8f, rgb(205, 222, 235), a);
      }
    }
    if (SHOW_JELLYFISH && night) drawJelly(g, t);
  }

 private:
  int wxCode = -1; float wxTemp = NAN;
  bool wxBaked = false, wxSaved = false;
  char wxShown[24] = "";
  Gfx* gNow = nullptr;  // the Gfx renderBackground is drawing with (used by drawCave)
  bool clockBaked = false, clockSaved = false;
  char clockShown[72] = "";
  bool bgNight = false;
  float gPr, gPg, gPb;
  // night dimming + glass vignette + glare for a background color at (x, y); result in gPr/gPg/gPb
  Col glass(Col, float x, float y, float pr, float pg, float pb) {
    if (bgNight) { pr = pr * NIGHT_COLORS; pg = pg * NIGHT_COLORS + 2.5f; pb = pb * NIGHT_COLORS + 8.4f; }
    float dx = x - W / 2, dy = y - H / 2, dist = sqrtf(dx * dx + dy * dy);
    float r0 = H * .35f, r1 = W * .75f;
    float v = clampf((dist - r0) / (r1 - r0), 0, 1);
    v = v * v * (3 - 2 * v) * .45f;
    pr *= 1 - v; pg *= 1 - v; pb *= 1 - v;
    float gx = W * .5f, gy = H * .6f;
    float proj = (x * gx + y * gy) / (gx * gx + gy * gy);
    float ga = proj < .4f ? .06f * (1 - proj / .4f) : 0;
    pr += (255 - pr) * ga; pg += (255 - pg) * ga; pb += (255 - pb) * ga;
    gPr = pr; gPg = pg; gPb = pb;
    return rgb(int(clampf(pr, 0, 255)), int(clampf(pg, 0, 255)), int(clampf(pb, 0, 255)));
  }
  // one background character, shaded like the glass/night would shade it
  void bgChar(Gfx& g, char ch, float x, float y, Col col) {
    Col c = glass(col, x + CW / 2, y + FONT_CELL_H / 2, col.r, col.g, col.b);
    g.glyph((uint8_t)ch, x + CW / 2, y + FONT_CELL_H / 2, 1, 1, c, 255);
  }

  Col dim(Col c) {
    if (!night) return c;
    return { uint8_t(c.r * NIGHT_COLORS), uint8_t(c.g * NIGHT_COLORS + 2.5f), uint8_t(c.b * NIGHT_COLORS + 8.4f) };
  }

  void dot(Gfx& g, float x, float y, int size, Col c, float a) {
    int ix = (int)x, iy = (int)y;
    g.fillRect(ix, iy, ix + size, iy + size, dim(c), int(255 * a));
  }

  void drawArt(Gfx& g, const char** art, int rows, float x, const uint32_t* colors, int nCols) {
    for (int i = 0; i < rows; i++)
      for (int k = 0; art[i][k]; k++)
        if (art[i][k] != ' ') bgChar(g, art[i][k], x + k * CW, SAND_TOP - (rows - i) * LH, hexCol(colors[i < nCols ? i : nCols - 1]));
  }

  // Blend a color over part of the background, shaded like the glass/night shades it.
  void bgFill(uint16_t* dst, int x0, int y0, int x1, int y1, Col c, float a) {
    for (int y = y0 < 0 ? 0 : y0; y < y1 && y < SCR_H; y++)
      for (int x = x0 < 0 ? 0 : x0; x < x1 && x < SCR_W; x++) {
        Col s = glass(c, x, y, c.r, c.g, c.b);
        dst[y * SCR_W + x] = blend565(dst[y * SCR_W + x], s, int(255 * a));
      }
  }

  // How far down its cell a character's ink starts (0 = top, 1 = bottom), and where an edge
  // character's stroke crosses its cell at height fy (0 = top, 1 = bottom), as a fraction of
  // the cell's width. Used to make the rock's grey fit exactly inside its outline.
  static float inkTop(char ch) {
    switch (ch) {
      case '\'': case '`': case '"': return .2f;
      case ':': return .32f;
      case '-': case '~': return .5f;
      case '.': return .7f;
      case ',': return .72f;
      case '_': return .9f;
      default: return 0;
    }
  }
  static float strokeX(char ch, float fy) {
    if (ch == '/') return .9f - fy * .8f;
    if (ch == '\\') return .1f + fy * .8f;
    return .5f;
  }

  // The eel's rock: a dark rounded cave mouth on its right side (darkest at the back wall),
  // a grey rock body that fills the outline exactly, then the outline and texture on top.
  void drawCave(uint16_t* dst) {
    const Species& sp = SPECIES[SP_CAVE];
    float top = SAND_TOP - sp.rows * LH;
    // 1) the mouth: half an oval opening to the right, under the overhang
    static const uint8_t bayer[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };
    float cx = CAVE_X + CAVE_BACK * CW, cy = SAND_TOP - LH * 1.5f, rx = CW * 10, ry = LH * 1.6f;
    for (int y = int(cy - ry); y < int(SAND_TOP) + 2; y++)
      for (int x = int(cx - CW * .4f); x < int(cx + rx); x++) {
        float dx = (x - cx) / rx, dy = (y - cy) / ry, q = 1 - dx * dx - dy * dy;
        if (q <= 0) continue;
        float dark = .9f * fminf(1, q * 1.8f) * (1 - .45f * fmaxf(0, dx));
        uint16_t px = dst[y * SCR_W + x];
        float r = (px >> 8) & 0xF8, gr = (px >> 3) & 0xFC, b = (px << 3) & 0xF8, d = bayer[y & 3][x & 3] / 16.f - .47f;
        dst[y * SCR_W + x] = pack565((int)clampf(r + (2 - r) * dark + d * 8, 0, 255), (int)clampf(gr + (5 - gr) * dark + d * 4, 0, 255),
                                     (int)clampf(b + (12 - b) * dark + d * 8, 0, 255));
      }
    // 2) the grey. Each row is filled from its first character to its last, following the
    //    slope of / \ | edges; from the top, each column starts just under its highest
    //    character (so the grey sits under the _ . - ' line). The overhang's columns have
    //    nothing below their row, so they stay a thin ledge over the dark mouth.
    Col rock = mixCol(hexCol(COLOR_ROCK), rgb(0, 0, 0), .55f);
    int first[MAX_ROWS], last[MAX_ROWS], len[MAX_ROWS];
    for (int r = 0; r < sp.rows; r++) {
      const char* line = sp.art[r];
      len[r] = strlen(line);
      int a = 0, b = len[r] - 1;
      while (a < len[r] && line[a] == ' ') a++;
      while (b > a && line[b] == ' ') b--;
      first[r] = a; last[r] = b;
    }
    for (int y = (int)top; y < (int)SAND_TOP; y++) {
      int r = (int)((y - top) / LH);
      if (r >= sp.rows) break;
      float fy = (y - top) / LH - r;
      float xl = CAVE_X + (first[r] + strokeX(sp.art[r][first[r]], fy)) * CW;
      float xr = CAVE_X + (last[r] + strokeX(sp.art[r][last[r]], fy)) * CW;
      for (int x = (int)ceilf(xl); x < (int)xr; x++) {
        int c = (int)((x - CAVE_X) / CW), r0 = 0;
        while (r0 < sp.rows && !(c >= first[r0] && c <= last[r0])) r0++;  // highest row this column is part of
        if (r0 >= sp.rows) continue;
        float colTop = top + (r0 + inkTop(c < len[r0] ? sp.art[r0][c] : ' ')) * LH;
        if (y < colTop) continue;
        bgFill(dst, x, y, x + 1, y + 1, rock, .8f);
      }
    }
    // 3) the outline and texture
    bgArt(*gNow, sp, CAVE_X);
  }

  // one piece of scenery, drawn into the background with its feet on the sand
  void bgArt(Gfx& g, const Species& sp, float x) {
    for (int r = 0; r < sp.rows; r++) {
      const char* pl = sp.paint[r];
      for (int c = 0; sp.art[r][c]; c++) {
        char p = *pl ? *pl++ : 'b';
        if (sp.art[r][c] != ' ') bgChar(g, sp.art[r][c], x + c * CW, SAND_TOP - (sp.rows - r) * LH, paintColor(&sp, p, 0, c));
      }
    }
  }

  void addBubble(float x, float y, float s) {
    if (nBubbles < MAX_BUBBLES) bubbles[nBubbles++] = { x, y, 0, rnd(0, 6), s };
  }

  void compactFood() {
    int n = 0;
    for (int i = 0; i < nFood; i++) if (food[i].alive) {
      // keep fish targets pointing at the right piece of food
      if (n != i) for (int k = 0; k < nFish; k++) if (fish[k].target == i) fish[k].target = n;
      food[n++] = food[i];
    } else {
      for (int k = 0; k < nFish; k++) if (fish[k].target == i) fish[k].target = -1;
    }
    nFood = n;
  }

  // ------------------------------------------------ setup
  bool overlapsDecor(float x) {
    struct { float x0, w; } d[] = { { CAVE_X, 22 }, { CHEST_X, 8 }, { W * .40f, 8 }, { W * .52f, 7 }, { CASTLE_X, 16 }, { W * .89f, 7 } };
    for (auto& r : d) if (x > r.x0 - CW * 1.5f && x < r.x0 + r.w * CW + CW * .5f) return true;
    return false;
  }

  void layout() {
    const uint32_t kelpCols[4] = { 0x3FAE5A, 0x56C271, 0x2E8B57, 0x4FB06A };
    nKelp = NUM_KELP > MAX_KELP ? MAX_KELP : NUM_KELP;
    for (int i = 0; i < nKelp; i++) {
      float x; int tries = 0;
      do { x = rnd(CW, W - CW * 2); } while (overlapsDecor(x) && tries++ < 40);
      int hmax = (int)fmaxf(5, (SAND_TOP - SURFACE_Y) / LH * .5f);
      kelp[i] = { x, (int)rnd(4, (float)hmax), rnd(0, 6.28f), rnd(.12f, .22f), hexCol(kelpCols[rngNext() % 4]) };
    }
    const uint32_t grassCols[3] = { 0x7BD389, 0x5FBF72, 0x93E09F };
    int tufts = (int)lrintf(W / (CW * 14));
    nGrass = 0;
    for (int i = 0; i < tufts && nGrass + 3 <= MAX_GRASS; i++) {
      float cx; int tries = 0;  // not on the eel's rock, in front of its cave, or on the scenery
      do { cx = rnd(0, W); } while ((cx < CAVE_X + 23 * CW || overlapsDecor(cx)) && tries++ < 40);
      for (int b = 0; b < 3; b++)
        grass[nGrass++] = { cx + (b - 1) * CW * .7f, 1 + (int)(rngNext() % 2), rnd(0, 6.28f), hexCol(grassCols[rngNext() % 3]) };
    }
  }

  float swimTop(float fh) { return SURFACE_Y + LH * .6f + fh / 2; }
  float swimBot(float fh) { return SAND_TOP - LH * .2f - fh / 2; }

  void addFish(int kind, float depth = 1, float x = -1, float y = -1) {
    if (nFish >= MAX_FISH) return;
    Fish& f = fish[nFish++];
    f = Fish{};
    f.sp = &SPECIES[kind]; f.d = depth;
    float fh = f.sp->rows * LH * depth * FISH_SIZE;
    float dir = rnd01() < .5f ? 1 : -1;
    f.x = x >= 0 ? x : rnd(W * .12f, W * .88f);
    f.y = y >= 0 ? y : rnd(swimTop(fh), swimBot(fh));
    f.vx = dir * f.sp->speed * CW * depth * .6f;
    f.f = f.ft = dir;
    f.phase = rnd(0, 6.28f);
    f.blink = rnd(2, 6);
    f.target = -1;
    f.buddy = -1;
    f.size = FISH_SIZE * (FISH_GROW ? .85f : 1);
    if (FISH_PERSONALITIES && depth >= 1) {
      if (kind == SP_CLOWN) f.mood = MOOD_SHY;
      if (kind == SP_PUFFER) f.mood = MOOD_CURIOUS;
      // the second, fourth... classic fish each swim with the one before
      if (kind == SP_CLASSIC && nFish >= 2 && fish[nFish - 2].sp == f.sp && fish[nFish - 2].buddy < 0 && fish[nFish - 2].d >= 1)
        f.buddy = nFish - 2;
    }
    pickTarget(f);
  }

  void populate() {
    nFish = 0;
    for (int i = 0; i < NUM_DELUXE; i++) addFish(SP_DELUXE);
    for (int i = 0; i < NUM_CLASSIC; i++) addFish(SP_CLASSIC);
    for (int i = 0; i < NUM_CLOWN; i++) addFish(SP_CLOWN);
    for (int i = 0; i < NUM_ANGEL; i++) addFish(SP_ANGEL);
    for (int i = 0; i < NUM_GROUPER; i++) addFish(SP_GROUPER);
    for (int i = 0; i < NUM_PUFFER; i++) addFish(SP_PUFFER);
    for (int i = 0; i < NUM_LIONFISH; i++) addFish(SP_LIONFISH);
    for (int i = 0; i < NUM_BETTA; i++) addFish(SP_BETTA);
    school = { W * .5f, (SURFACE_Y + SAND_TOP) / 2, 0 };
    for (int i = 0; i < NUM_MINNOWS; i++) {
      int before = nFish;
      addFish(SP_MINNOW, 1, W * .3f + rnd(-3, 3) * CW, school.ty + rnd(-1, 1) * LH);
      if (nFish == before) break;
      Fish& m = fish[nFish - 1];
      m.school = &school;
      m.offx = ((i - (NUM_MINNOWS - 1) / 2.f) * 2.2f * CW + rnd(-1, 1) * CW) * FISH_SIZE;
      m.offy = (((i % 3) - 1) * 1.3f * LH + rnd(-.3f, .3f) * LH) * FISH_SIZE;
    }
    const int distantKinds[5] = { SP_CLASSIC, SP_MINNOW, SP_MINNOW, SP_DELUXE, SP_CLASSIC };
    for (int i = 0; i < NUM_DISTANT; i++) addFish(distantKinds[i % 5], .55f);

    jelly.x = rnd(W * .15f, W * .6f); jelly.y = rnd(SURFACE_Y + LH * 3, SAND_TOP - LH * 9);
    jelly.vx = rnd(-.25f, .25f); jelly.pulse = rnd(0, 6);
    crab = {}; crab.x = rnd(W * .3f, W * .5f); crab.dir = 1; crab.blink = 3;
    for (int i = 0; i < N_PLANKTON; i++) plankton[i] = { rnd(0, W), rnd(SURFACE_Y, SAND_TOP), rnd01(), rnd(0, 6.28f) };
    // now that everyone (and the jellyfish) is placed, pick first destinations in open water
    for (int i = 0; i < nFish; i++) if (!fish[i].school) pickTarget(fish[i]);
    for (int i = 0; i < N_GLINTS; i++) glints[i] = { 0, 0, 1 };
    for (int i = 0; i < N_POPS; i++) pops[i] = { 0, 0 };
    for (int i = 0; i < N_FLAKES; i++) flakes[i] = { 0, 0, 0, 0, 0, 0 };
    for (int i = 0; i < N_WISPS; i++) wisps[i] = { 0, 0, 0, 0, 1, 0 };
    for (int i = 0; i < N_PUFFS; i++) puffs[i] = { 0, 0, 0, 0, 0 };
    snail = { rnd(W * .1f, W * .8f), rnd01() < .5f ? 1.f : -1.f, 0 };
    whaleOn = false;
    whaleWait = VISITOR_EVERY_MINUTES * 60.f * rnd(.3f, 1);  // first visit comes a bit sooner
  }

  // Choose where to wander next: try a few spots and keep the one in the most open water.
  void pickTarget(Fish& f, bool arrived = false) {
    float fw = f.sp->w * CW * fishScale(f), fh = f.sp->rows * LH * fishScale(f), bestS = -1;
    bool local = rnd01() < (arrived ? .55f : .2f);  // after arriving, often hang around a while
    if (f.buddy >= 0) {  // swims with a friend: head for a spot just behind and below wherever it's going
      const Fish& b = fish[f.buddy];
      f.tx = clampf(b.tx - (b.ft >= 0 ? 1 : -1) * CW * 7, fw, W - fw);
      f.ty = clampf(b.ty + LH * 1.4f, swimTop(fh), swimBot(fh));
      f.tTimer = rnd(2, 4);
      return;
    }
    bool homebody = f.mood == MOOD_SHY && rnd01() < .7f;      // shy: stays near the castle
    bool visiting = f.mood == MOOD_CURIOUS && rnd01() < .5f;  // curious: goes to look at the snail or crab
    for (int n = 0; n < 8; n++) {
      float tx, ty;
      if (homebody) {
        tx = clampf(CASTLE_X + CW * 8 + rnd(-1, 1) * W * .17f, fw, W - fw);
        ty = rnd(swimTop(fh) + (swimBot(fh) - swimTop(fh)) * .4f, swimBot(fh));
      } else if (visiting) {
        float cx = (n & 1) && SHOW_CRAB ? crab.x + SPECIES[SP_CRAB].w * CW * CRAB_SCALE / 2 : snail.x;
        tx = clampf(cx + rnd(-4, 4) * CW, fw, W - fw);
        ty = swimBot(fh) - rnd(0, 2.5f) * LH;
      } else if (local) {
        tx = clampf(f.x + rnd(-4, 4) * CW, fw, W - fw);
        ty = clampf(f.y + rnd(-1, 1) * LH, swimTop(fh), swimBot(fh));
      } else {
        int tries = 0;
        do { tx = rnd(W * .06f + fw / 2, W * .94f - fw / 2); } while (fabsf(tx - f.x) < W * .22f && tries++ < 6);
        ty = rnd(swimTop(fh), swimBot(fh));
        if (SLEEPY_NIGHT && night) ty = rnd(swimTop(fh) + (swimBot(fh) - swimTop(fh)) * .45f, swimBot(fh));  // rest low
      }
      float s = openness(tx, ty, fw, fh, f.d, &f, nullptr);
      if (s > bestS) { bestS = s; f.tx = tx; f.ty = ty; }
      if (local && n == 2 && bestS >= 1) break;  // a small drift is fine if it's not crowded
    }
    // give it enough time to actually get there (slow fish need longer), plus a little extra
    float cruise = f.sp->speed * CW * f.d * (night ? .45f : 1) * .7f + 1;
    f.tTimer = hypotf(f.tx - f.x, f.ty - f.y) / cruise + rnd(2, 5);
  }

  // How much free water is around a w x h box at (x, y): the distance to the nearest other
  // fish at this depth (where it is now and where it's heading), measured in combined body
  // sizes, so 1 = just touching. Bigger is emptier. Also counts the jellyfish.
  float openness(float x, float y, float w, float h, float d, const Fish* self, const School* skip) {
    float best = 4;
    for (int i = 0; i < nFish; i++) {
      const Fish& o = fish[i];
      if (&o == self || o.d != d || (skip && o.school == skip)) continue;
      float rx = (w + o.sp->w * CW * d * o.size) / 2 + CW, ry = (h + o.sp->rows * LH * d * o.size) / 2 + LH * .5f;
      float hx = o.school ? o.school->tx + o.offx : o.tx, hy = o.school ? o.school->ty + o.offy : o.ty;
      best = fminf(best, fmaxf(fabsf(x - o.x) / rx, fabsf(y - o.y) / ry));
      best = fminf(best, fmaxf(fabsf(x - hx) / rx, fabsf(y - hy) / ry));
    }
    if (SHOW_JELLYFISH && d >= 1) {
      float jw = JELLY_COLS * CW, jh = JELLY_ROWS * LH;
      best = fminf(best, fmaxf(fabsf(x - jelly.x - jw / 2) / ((w + jw) / 2 + CW), fabsf(y - jelly.y - jh / 2) / ((h + jh) / 2 + LH)));
    }
    return best;
  }

  // Push (sx, sy) away when inside a box of half-size (rx, ry) around a neighbor that's
  // (ox, oy) away. Returns how deep inside we are (0 = outside, 1 = dead center).
  static float repel(float ox, float oy, float rx, float ry, float tie, float k, float& sx, float& sy) {
    float nx = ox / rx, ny = oy / ry, m = fmaxf(fabsf(nx), fabsf(ny));
    if (m >= 1) return 0;
    float depth = 1 - m;
    if (fabsf(ny) < .05f) ny = .05f * tie;  // exactly level: one goes up, the other down
    float len = sqrtf(nx * nx + ny * ny);
    sx += nx / len * depth * k * .5f;
    sy += (ny / len * .8f + (ny > 0 ? .4f : -.4f)) * depth * k;
    return depth;
  }

  void mouthOf(const Fish& f, float& mx, float& my) {
    float s = f.f >= 0 ? 1 : -1;
    mx = f.x + s * f.sp->w * CW * fishScale(f) / 2;
    my = f.y + (f.sp->eyeRow + .5f - f.sp->rows / 2.f) * LH * fishScale(f);
  }

  void updateFish(Fish& f, float dt, float tempo) {
    float d = f.d, fw = f.sp->w * CW * fishScale(f), fh = f.sp->rows * LH * fishScale(f);
    float base = f.sp->speed * CW * d;
    float maxS = base * tempo;
    if (f.burst > 0) { f.burst -= dt; maxS *= 2.2f; }
    float top = swimTop(fh), bot = swimBot(fh);

    float tx = 0, ty = 0; bool seeking = false;
    f.target = -1;
    if (d >= 1 && nFood) {
      int best = -1; float bd = W * .7f;
      for (int i = 0; i < nFood; i++) {
        if (!food[i].alive || food[i].landed > 0) continue;
        float dd = hypotf(food[i].x - f.x, food[i].y - f.y);
        if (dd < bd) { bd = dd; best = i; }
      }
      if (best >= 0) {
        float s = sgnf(food[best].x - f.x); if (s == 0) s = f.f >= 0 ? 1 : -1;
        tx = food[best].x - s * fw * .42f;
        ty = food[best].y - (f.sp->eyeRow + .5f - f.sp->rows / 2.f) * LH * fishScale(f);
        seeking = true; maxS *= 1.7f; f.target = best;
      }
    }
    if (!seeking) {
      if (f.school) { tx = f.school->tx + f.offx; ty = f.school->ty + f.offy; }
      else {
        f.tTimer -= dt;
        bool arrived = hypotf(f.tx - f.x, f.ty - f.y) < CW * 2;
        if (f.tTimer <= 0 || arrived) pickTarget(f, arrived);
        tx = f.tx; ty = f.ty;
      }
    }
    ty = clampf(ty, top, bot);

    float dx = tx - f.x, dy = ty - f.y, dist = hypotf(dx, dy); if (dist < 1e-3f) dist = 1;
    float ease = seeking ? fminf(1, dist / (CW * 3)) + .3f : clampf(dist / (CW * 6), .35f, 1);
    float ax = dx / dist * maxS * ease - f.vx;
    float ay = dy / dist * maxS * ease * .7f - f.vy;

    // personal space: steer away from fish at the same depth (and the jellyfish) that are
    // touching or about to. Mostly up/down, since fish swim sideways, but some sideways too.
    float sepX = 0, sepY = 0, crowd = 0;
    float sepK = base * 2.6f * (seeking ? .35f : 1);
    for (int i = 0; i < nFish; i++) {
      Fish& o = fish[i];
      if (&o == &f || o.d != d) continue;
      bool mates = f.school && f.school == o.school;  // a school keeps its own spacing
      float rx = (fw + o.sp->w * CW * d * o.size) / 2 + CW * (mates ? .3f : 1.5f) * d;
      float ry = (fh + o.sp->rows * LH * d * o.size) / 2 + LH * (mates ? .1f : .5f) * d;
      crowd += repel(f.x - o.x, f.y - o.y, rx, ry, (&o < &f) ? 1 : -1, sepK * (mates ? .5f : 1), sepX, sepY);
    }
    if (SHOW_JELLYFISH && d >= 1) {
      float jw = JELLY_COLS * CW, jh = JELLY_ROWS * LH;
      crowd += repel(f.x - (jelly.x + jw / 2), f.y - (jelly.y + jh / 2), (fw + jw) / 2 + CW, (fh + jh) / 2 + LH * .4f,
                     1, sepK, sepX, sepY);
    }
    // can't dodge up/down because we're at the surface or the sand? slip past sideways instead
    if ((sepY < 0 && f.y - top < LH * .5f) || (sepY > 0 && bot - f.y < LH * .5f)) {
      sepX += (sepX != 0 ? sgnf(sepX) : (f.f >= 0 ? 1 : -1)) * fabsf(sepY) * .7f;
      sepY = 0;
    }
    ax += sepX; ay += sepY;
    // boxed in and barely moving? give up on this spot sooner and go somewhere else
    if (crowd > .5f && !f.school && hypotf(f.vx, f.vy) < base * .3f) f.tTimer -= dt * 3;
    float m = CW * 3, left = f.x - fw / 2, right = f.x + fw / 2;
    if (left < m) ax += base * 3 * (1 - left / m);
    if (right > W - m) ax -= base * 3 * (1 - (W - right) / m);

    float ag = f.sp->agility * (seeking ? 1.6f : 1);
    f.vx += ax * ag * dt; f.vy += ay * ag * dt;
    float sp = hypotf(f.vx, f.vy), lim = maxS * 1.15f;
    if (sp > lim) { f.vx *= lim / sp; f.vy *= lim / sp; }
    f.x += f.vx * dt; f.y += f.vy * dt;
    if (f.y < top) { f.y = top; f.vy = fmaxf(0, f.vy); }
    if (f.y > bot) { f.y = bot; f.vy = fminf(0, f.vy); }

    float th = base * .1f;
    if (f.vx > th) f.ft = 1; else if (f.vx < -th) f.ft = -1;
    float turn = 2.2f + f.sp->agility;
    f.f += clampf(f.ft - f.f, -turn * dt, turn * dt);

    f.phase += dt * (2.4f + fminf(1.6f, sp / (base + 1e-6f)) * 8);
    if (f.phase > 6.2831853f) f.phase -= 6.2831853f;

    f.blink -= dt;
    if (f.blink <= 0) { f.blinkT = .13f; f.blink = rnd(2.5f, 7); }
    if (f.blinkT > 0) f.blinkT -= dt;
    if (f.gulp > 0) f.gulp = fmaxf(0, f.gulp - dt * 3);
    if (f.puff > 0) f.puff -= dt;
    else if (f.sp->puffRows && rnd01() < dt * .03f) f.puff = rnd(2, 3.5f);
    if (!seeking && rnd01() < dt * .02f) f.burst = rnd(.5f, 1.1f);

    float mx, my; mouthOf(f, mx, my);
    if (f.target >= 0 && hypotf(mx - food[f.target].x, my - food[f.target].y) < CW * 1.5f) {
      food[f.target].alive = false;
      f.target = -1;
      f.gulp = 1;
      for (int i = 0; i < 2; i++) addBubble(mx, my, .6f);
    } else if (d >= 1 && rnd01() < dt * .1f * tempo) {
      addBubble(mx, my, .7f);
    }
  }

  void drawFish(Gfx& g, const Fish& f, float alpha, bool glow) {
    const Species* sp = f.sp;
    bool puffing = f.puff > 0 && sp->puffRows;
    const char* const* art = puffing ? sp->puff : sp->art;
    const char* const* paint = puffing ? sp->puffPaint : sp->paint;
    int rows = puffing ? sp->puffRows : sp->rows, w = puffing ? sp->puffW : sp->w;
    float sgn = f.f >= 0 ? 1 : -1;
    float sx = fabsf(f.f) < .12f ? .12f * sgn : f.f;
    const float artDir = sp->facesLeft ? -1 : 1;  // which way the drawing itself points
    sx *= artDir;
    float tilt = clampf(atan2f(f.vy, fabsf(f.vx) + CW * .3f), -.4f, .4f) * .85f;
    float sc = fishScale(f) * (1 + f.gulp * .12f) * (puffing ? 1.1f : 1);
    float speedN = fminf(1, hypotf(f.vx, f.vy) / (sp->speed * CW * f.d + 1e-6f));
    float amp = LH * (.07f + .15f * speedN);
    float ang = tilt * sgn, ca = cosf(ang), sa = sinf(ang);
    float x0 = -w * CW / 2, y0 = -rows * LH / 2;
    int a8 = int(255 * alpha);
    Col glowCol = dim(sp->pal[PC_BODY]);
    int glowA = night ? 70 : 105;
    // screen position of character (r, c), including the swimming wave and the tilt
    // While turning around, the head swings first and the tail follows: each column gets
    // its own facing, a little ahead (head) or behind (tail) of the fish's overall turn.
    float turnDir = f.ft - f.f, bend = (1 - fabsf(f.f)) * (turnDir > 0 ? .5f : turnDir < 0 ? -.5f : 0);
    auto facing = [&](int c) {
      float h = ((float)c / (w > 1 ? w - 1 : 1) - .5f) * artDir;  // -.5 tail .. +.5 head
      float v = clampf(f.f + bend * h * 2, -1, 1);
      return (fabsf(v) < .12f ? (v >= 0 ? .12f : -.12f) : v) * artDir;
    };
    auto place = [&](int r, int c, float& X, float& Y) {
      float k = (float)c / (w > 1 ? w - 1 : 1);
      k = sp->facesLeft ? k : 1 - k;   // 1 at the tail (which waves most), 0 at the head
      float yo = sinf(f.phase - c * .6f) * amp * k * k;
      float lx = (x0 + c * CW + CW / 2) * sx * sc, ly = (y0 + r * LH + FONT_CELL_H / 2 + yo) * sc;
      X = f.x + lx * ca - ly * sa; Y = f.y + lx * sa + ly * ca;
    };
    if (solid && f.d >= 1) {
      // solid body: clear the water behind the fish's outline, so whatever swims
      // behind it (other fish, kelp, bubbles) is hidden instead of jumbling together
      float hw = CW * fabsf(sx) * sc / 2, hh = LH * sc / 2;
      for (int r = 0; r < rows; r++) {
        const char* line = art[r];
        int a = 0, b = (int)strlen(line) - 1;
        while (line[a] == ' ') a++;
        while (b > a && line[b] == ' ') b--;
        for (int c = a; c <= b; c++) {
          float X, Y; place(r, c, X, Y);
          g.clearToBg((int)floorf(X - hw), (int)floorf(Y - hh), (int)ceilf(X + hw), (int)ceilf(Y + hh));
        }
      }
    }
    int lens[MAX_ROWS];
    for (int r = 0; r < rows; r++) lens[r] = strlen(art[r]);
    auto solidAt = [&](int r, int c) { return r >= 0 && r < rows && c >= 0 && c < lens[r] && art[r][c] != ' '; };
    bool sleepy = SLEEPY_NIGHT && night && f.d >= 1 && f.target < 0 && f.blink > .8f;
    for (int r = 0; r < rows; r++) {
      const char* line = art[r];
      const char* pl = paint[r];  // walks along the paint string in step with the art
      for (int c = 0; line[c]; c++) {
        char ch = line[c];
        char p = *pl ? *pl++ : 'b';
        if (ch == ' ') continue;
        // eyes: blink now and then; at night, resting fish keep them shut most of the time
        if (p == 'e' && (f.blinkT > 0 || sleepy)) ch = '-';
        Col col = paintColor(sp, p, f.phase, c);
        float X, Y; place(r, c, X, Y);
        // a letter with neighbors on all four sides is inside the body, where its glow
        // can't be seen; skipping those saves a lot of drawing on the big fish
        bool inner = solidAt(r, c - 1) && solidAt(r, c + 1) && solidAt(r - 1, c) && solidAt(r + 1, c);
        bool lit = glow && !inner && (!glowLight || ((r + c) & 1) == 0);
        g.glyph((uint8_t)ch, X, Y, (TURN_HEAD_FIRST ? facing(c) : sx) * sc, sc, dim(col), a8, lit, glowCol, glowA);
      }
    }
  }

  void sandPuff(float x, int count) {
    if (!SAND_PUFFS) return;
    for (int k = 0; k < N_PUFFS && count > 0; k++) if (puffs[k].life <= 0) {
      puffs[k] = { x + rnd(-1, 1) * CW, SAND_TOP - LH * .15f, rnd(-1.5f, 1.5f) * CW, -rnd(.6f, 1.4f) * LH, rnd(.5f, .9f) };
      count--;
    }
  }

  // Sunlight rippling on the sand: soft bright patches that drift and fade in and out.
  // Their positions come straight from the clock, so there's nothing to keep track of.
  void drawRipples(Gfx& g, float t) {
    for (int i = 0; i < N_RIPPLES; i++) {
      float seed = i * 12.9898f;
      float speed = (6 + 7 * fabsf(sinf(seed * 1.7f))) * (i & 1 ? 1 : -1);
      float x = fmodf(i * (W + 160) / N_RIPPLES + t * speed + 4000 * (W + 160), W + 160) - 80;
      float y = SAND_TOP + 6 + (H - SAND_TOP - 16) * fabsf(sinf(seed * 3.1f));
      float w = 26 + 34 * fabsf(sinf(seed * 2.3f));
      float a = .13f * (.5f + .5f * sinf(t * (.9f + .5f * fabsf(sinf(seed))) + seed));
      if (a < .012f) continue;
      for (int r = 0; r < 3; r++) {
        float hw = r == 1 ? w : w * .65f;
        g.fillRect(int(x - hw), int(y + r), int(x + hw), int(y + r + 1), rgb(255, 240, 190), int(255 * a));
      }
    }
  }

  // the height of the water surface at x
  static float waveY(float x, float t) {
    return SURFACE_Y - LH * .75f + sinf(x * .015f + t * 1.2f) * LH * .14f + sinf(x * .043f - t * .8f) * LH * .06f;
  }

  // Soft dark ovals on the sand under each fish: darker and tighter the closer it swims
  // to the bottom, faint and spread out when it's up high. Mostly a daytime thing.
  void drawShadows(Gfx& g) {
    float strength = night ? .1f : .28f, sy = SAND_TOP + LH * .25f;
    for (int i = 0; i < nFish; i++) {
      const Fish& f = fish[i];
      if (f.d < 1) continue;
      float gap = clampf((SAND_TOP - (f.y + f.sp->rows * LH * f.size / 2)) / (SAND_TOP - SURFACE_Y), 0, 1);
      float a = strength * (1 - gap) * (1 - gap);
      if (a < .02f) continue;
      float w = f.sp->w * CW * f.size * .75f * fmaxf(.35f, fabsf(f.f)) * (1 + gap * .6f);
      for (int r = 0; r < 5; r++) {
        float k = (r - 2) / 2.6f, hw = w * sqrtf(1 - k * k) / 2;
        g.fillRect(int(f.x - hw), int(sy + r), int(f.x + hw), int(sy + r + 1), rgb(0, 8, 16), int(255 * a));
      }
    }
  }

  // Plankton: faint specks by day; at night they twinkle blue-green (bioluminescence).
  void drawPlankton(Gfx& g, const Plankton& p, float t, int size) {
    if (night && GLOWING_PLANKTON) {
      float s = sinf(t * .6f + p.phase * 5), tw = s > 0 ? s * s * s * s : 0;
      g.fillRect((int)p.x, (int)p.y, (int)p.x + size, (int)p.y + size, rgb(90, 255, 220), int(255 * (.1f + .55f * tw)));
      if (size > 1 && tw > .75f && glow)
        g.glyph('.', p.x + 1, p.y - 5, 1, 1, rgb(160, 255, 235), int(255 * tw), true, rgb(40, 200, 180), int(110 * tw));
      return;
    }
    float s = sinf(t * (size > 1 ? 1.3f : 1) + p.phase);
    dot(g, p.x, p.y, size, size > 1 ? rgb(220, 245, 255) : rgb(200, 235, 255), size > 1 ? .15f + .25f * s * s : .12f + .15f * s * s);
  }

  // A small creature drawn straight (no swimming wave), facing dir (+1 right, -1 left).
  // art = different drawing with the same rows/paint (optional). Nothing left of clipX is drawn.
  // opaque = false: don't hide what's behind it, even when creatures are solid.
  // sc = drawing size (1 = full size); (x, y) is its top-left corner.
  void drawCritter(Gfx& g, const Species& sp, float x, float y, float dir, const char* const* art = nullptr,
                   float clipX = -1e9f, bool opaque = true, float sc = 1) {
    if (!art) art = sp.art;
    const float cw = CW * sc, lh = LH * sc;
    // first blank out its whole outline (hides the grass, scenery and anything else behind
    // it), then draw the letters, so one row's blanking can't clip the row above
    if (solid && opaque)
      for (int r = 0; r < sp.rows; r++) {
        const char* line = art[r];
        int a = 0, b = (int)strlen(line) - 1;
        while (line[a] == ' ') a++;
        if (b < a) continue;
        float x0 = x + (dir > 0 ? a : sp.w - 1 - b) * cw, x1 = x + (dir > 0 ? b + 1 : sp.w - a) * cw;
        if (x0 < clipX) x0 = clipX;
        if (x0 < x1) g.clearToBg((int)x0, (int)(y + r * lh), (int)ceilf(x1), (int)ceilf(y + (r + 1) * lh + 2));
      }
    for (int r = 0; r < sp.rows; r++) {
      const char* line = art[r];
      const char* pl = sp.paint[r];
      for (int c = 0; line[c]; c++) {
        char p = *pl ? *pl++ : 'b';
        if (line[c] == ' ') continue;
        int col = dir > 0 ? c : sp.w - 1 - c;
        if (x + col * cw < clipX) continue;
        g.glyph((uint8_t)line[c], x + col * cw + cw / 2, y + r * lh + FONT_CELL_H * sc / 2, (dir > 0 ? 1 : -1) * sc, sc,
                dim(paintColor(&sp, p, 0, c)), 255);
      }
    }
  }

  // The crab: legs alternate while it walks, it bobs a little, blinks, and its claws
  // snap shut when it eats (both, quickly) or just for fun (one at a time).
  void drawCrab(Gfx& g, float t) {
    const Species& sp = SPECIES[SP_CRAB];
    bool walking = crab.pause <= 0;
    const char* const* legs = (walking && sinf(crab.phase) > 0) ? ART_CRAB_STEP : ART_CRAB;
    char top[24];
    strncpy(top, legs[0], sizeof top - 1); top[sizeof top - 1] = 0;
    int n = strlen(top);
    bool eating = crab.snap > 0 && sinf(t * 30) > 0;
    if (eating || (crab.click > 0 && crab.clickSide == 0)) { top[1] = '|'; top[2] = '|'; }
    if (eating || (crab.click > 0 && crab.clickSide == 1)) { top[n - 3] = '|'; top[n - 2] = '|'; }
    if (crab.blinkT > 0) for (int i = 0; i < n; i++) if (top[i] == DEG) top[i] = '-';
    const char* rows[MAX_ROWS] = { top, legs[1], legs[2], legs[3] };
    float bob = walking ? fabsf(sinf(crab.phase)) * LH * .1f : 0;
    drawCritter(g, sp, crab.x, SAND_TOP - (LH * 3.55f + bob) * CRAB_SCALE, 1, rows, -1e9f, true, CRAB_SCALE);
  }

  // The clock. Text is digits, ':' (or '_' for a colon that's blinked off), then an
  // optional space and "am"/"pm". CLOCK_BIG draws the digits three rows tall.
  void drawClock(Gfx& g, const char* text) {
    Col col = night ? rgb(130, 175, 210) : rgb(200, 236, 255);
    int alpha = night ? 160 : 225, glowA = night ? 30 : 40;  // a soft glow, like the weather corner
    const float pitch = CLOCK_BIG ? 10 : CW;          // distance between letters
    // measure the time (up to the '|' that starts the date), so it can sit in either top corner
    float width = 0;
    for (int i = 0; text[i] && text[i] != '|'; i++) {
      char ch = text[i];
      width += !CLOCK_BIG ? pitch : (ch >= '0' && ch <= '9') ? pitch * 3 + 6 : (ch == ':' || ch == '_') ? pitch + 4 : pitch;
    }
    float x = CLOCK_CORNER == 1 ? W - CW * 2 - width : CW * 2;
    float y = SURFACE_Y + LH * .9f, xStart = x;
    for (int i = 0; text[i]; i++) {
      char ch = text[i];
      if (ch == '|') {  // the rest: small lines under the time, split at each '|' (date, weather)
        float dy = y + (CLOCK_BIG ? 3 * LH + 4 : LH) + FONT_CELL_H / 2;
        const char* line = text + i + 1;
        while (*line) {
          const char* end = strchr(line, '|');
          int n = end ? (int)(end - line) : (int)strlen(line);
          float dx = CLOCK_CORNER == 1 ? W - CW * 2 - n * CW : xStart;
          for (int k = 0; k < n; k++) g.glyph((uint8_t)line[k], dx + k * CW + CW / 2, dy, 1, 1, col, alpha * 3 / 4, true, col, glowA);
          dy += LH;
          line = end ? end + 1 : line + n;
        }
        break;
      }
      if (!CLOCK_BIG) {
        if (ch != '_') g.glyph((uint8_t)ch, x + pitch / 2, y + FONT_CELL_H / 2, 1, 1, col, alpha, true, col, glowA);
        x += pitch;
      } else if (ch >= '0' && ch <= '9') {
        for (int r = 0; r < 3; r++)
          for (int c = 0; c < 3; c++) {
            char d = CLOCK_DIGITS[ch - '0'][r][c];
            if (d != ' ') g.glyph((uint8_t)d, x + c * pitch + pitch / 2, y + r * LH + FONT_CELL_H / 2, 1, 1, col, alpha, true, col, glowA);
          }
        x += pitch * 3 + 6;
      } else if (ch == ':' || ch == '_') {
        if (ch == ':')
          for (int r = 1; r < 3; r++) g.glyph('.', x + pitch / 2, y + r * LH + FONT_CELL_H / 2 - 4, 1, 1, col, alpha, true, col, glowA);
        x += pitch + 4;
      } else {  // am / pm, small, lined up with the bottom of the digits
        if (ch != ' ') g.glyph((uint8_t)ch, x + pitch / 2, y + 2 * LH + FONT_CELL_H / 2, 1, 1, col, alpha, true, col, glowA);
        x += pitch;
      }
    }
  }

  // Where the weather corner goes: the top corner the clock isn't in.
  void wxBox(int& x0, int& y0) const {
    x0 = CLOCK_CORNER == 1 ? (int)(CW * 2) - 4 : SCR_W - (int)(CW * 2) - WX_W + 4;
    y0 = (int)(SURFACE_Y + LH * .9f) - 18;  // the picture itself starts 20 px lower, level with the clock
  }

  // Which picture goes with a weather code (the WMO numbers weather services use); -1 = none.
  int weatherPicture(int code) const {
    if (code < 0) return -1;
    if (code == 0) return night ? WXI_MOON : WXI_SUN;
    if (code <= 2) return night ? WXI_PARTLY_NIGHT : WXI_PARTLY;
    if (code == 45 || code == 48) return WXI_FOG;
    if (code >= 51 && code <= 67) return WXI_RAIN;
    if (code >= 80 && code <= 82) return night ? WXI_RAIN : WXI_SHOWERS;
    if ((code >= 71 && code <= 77) || code == 85 || code == 86) return WXI_SNOW;
    if (code >= 95) return WXI_STORM;
    return WXI_CLOUD;
  }

  static Col wxColor(char p) {
    switch (p) {
      case 's': return hexCol(0xFFD34D);  // sun
      case 'd': return hexCol(0x98A6B8);  // back cloud
      case 'r': return hexCol(0x9AD4FF);  // rain
      case 'n': return hexCol(0xFFFFFF);  // snow
      case 'z': return hexCol(0xFFE45C);  // lightning
      case 'g': return hexCol(0xB8C4D0);  // fog
      case 'm': return hexCol(0xF2EBC8);  // moon
      default: return hexCol(0xD8E4F0);   // cloud
    }
  }

  // The weather picture (from Weather Icons, see weather_icons.h) with a soft glow, the
  // temperature under it, and the conditions in words under that. Its top lines up with the
  // clock's first line, so the two sit side by side across the top of the tank. It's drawn
  // into the background, so none of this costs anything while the fish swim.
  void drawWeather(Gfx& g, int code, float temp) {
    if (!SHOW_WEATHER) return;
    int x0, y0;
    wxBox(x0, y0);
    float top = y0 + 20, cx = x0 + WX_W / 2.f, y = top;
    int kind = weatherPicture(code);
    if (kind == WXI_MOON) {  // clear night: a smooth crescent with a few stars
      drawMoon(g, cx, top + 34, 27);
      y = top + 74;
    } else if (kind >= 0) {
      const WxiIcon& ic = WX_ICONS[kind];
      bool moonBehind = kind == WXI_PARTLY_NIGHT;
      int contentTop = moonBehind ? 22 : 999, contentBottom = 0;  // where the picture really starts and ends
      for (int i = 0; i < ic.layers; i++) {
        if (ic.layer[i].y < contentTop) contentTop = ic.layer[i].y;
        if (ic.layer[i].y + ic.layer[i].h > contentBottom) contentBottom = ic.layer[i].y + ic.layer[i].h;
      }
      int ix = (int)lrintf(cx - WXI_W / 2.f), iy = (int)top - contentTop;
      if (ic.glow) g.alphaMap(ic.glow, ic.gw, ic.gh, ix + ic.gx, iy + ic.gy, dim(wxColor(ic.glowColor)), night ? 80 : 95);
      if (moonBehind) drawMoon(g, ix + 68, iy + 36, 13, false);  // peeking out from behind the cloud
      for (int i = 0; i < ic.layers; i++) {
        const WxiLayer& l = ic.layer[i];
        g.alphaMap(l.alpha, l.w, l.h, ix + l.x, iy + l.y, dim(wxColor(l.color)), night ? 215 : 245);
      }
      y = iy + contentBottom + 6;
    }
    Col ink = night ? rgb(150, 192, 225) : rgb(222, 244, 255), soft = night ? rgb(105, 145, 180) : rgb(160, 205, 235);
    if (temp == temp) {  // (not NAN)
      char s[12];
      snprintf(s, sizeof s, "%d\xB0%c", (int)lrintf(temp), TEMP_FAHRENHEIT ? 'F' : 'C');
      float tx = cx - strlen(s) * CW / 2.f;
      for (int i = 0; s[i]; i++)
        g.glyph((uint8_t)s[i], tx + i * CW + CW / 2, y + FONT_CELL_H / 2, 1, 1, ink, night ? 190 : 240, true, soft, night ? 40 : 50);
      y += LH;
    }
    const char* word = weatherWord(code);
    if (word[0]) g.text(word, cx - strlen(word) * CW / 2.f, y, soft, night ? 140 : 190);
  }

  // A crescent moon: a bright disc with a second, offset disc taken out of it, edges smoothed,
  // a faint halo around it, and three small stars nearby.
  void drawMoon(Gfx& g, float cx, float cy, float R, bool stars = true) {
    Col moon = wxColor('m'), glowCol = hexCol(0xC8D8F0);
    float sx = R * .52f, sy = -R * .18f, sr = R * .9f;  // the "shadow" disc's offset and size
    int x0 = (int)(cx - R * 1.7f), x1 = (int)(cx + R * 1.7f), y0 = (int)(cy - R * 1.7f), y1 = (int)(cy + R * 1.7f);
    for (int y = y0; y < y1; y++)
      for (int x = x0; x < x1; x++) {
        float dx = x + .5f - cx, dy = y + .5f - cy, d = sqrtf(dx * dx + dy * dy);
        float ex = dx - sx, ey = dy - sy, ds = sqrtf(ex * ex + ey * ey);
        float lit = clampf(R - d + .5f, 0, 1) * clampf(ds - sr + .5f, 0, 1);  // inside the moon, outside the shadow
        // a soft glow centered on the bright part (left of the disc's middle), fading out smoothly
        float hx = dx + R * .35f, hy = dy - R * .05f, hd = sqrtf(hx * hx + hy * hy);
        float halo = .2f * expf(-fmaxf(0, hd - R * .55f) / (R * .5f));
        if (lit <= 0 && halo < .01f) continue;
        g.blendPixel(x, y, glowCol, (int)(255 * halo));
        if (lit > 0) {
          float shade = .82f + .18f * clampf((ds - sr) / (R * .5f), 0, 1);  // a little darker along the inner edge
          g.blendPixel(x, y, scaleCol(moon, shade), (int)(255 * lit));
        }
      }
    g.mark(x0, y0, x1, y1);
    if (!stars) return;
    const float STARS[3][3] = { { 2.0f, -1.1f, .9f }, { 1.6f, .9f, .7f }, { -1.7f, -1.3f, .6f } };
    for (auto& s : STARS)
      g.glyph(s[2] > .8f ? '+' : '.', cx + s[0] * R, cy + s[1] * R, s[2] * .85f, s[2] * .85f, rgb(230, 238, 255), 200, true,
              rgb(160, 190, 240), 60);
  }

  void drawJelly(Gfx& g, float t) {
    float c = fmaxf(0, sinf(jelly.pulse));
    float kx = 1 + c * .06f, ky = 1 - c * .1f;
    if (solid)  // the bell hides what's behind it (the trailing arms stay see-through)
      for (int i = 0; i < 3; i++) {
        const char* l = JELLY[i];
        int a = 0, b = (int)strlen(l) - 1;
        while (l[a] == ' ') a++;
        g.clearToBg((int)(jelly.x + a * CW * kx), (int)(jelly.y + i * LH * ky), (int)ceilf(jelly.x + (b + 1) * CW * kx),
                    (int)ceilf(jelly.y + (i + 1) * LH * ky + 2));
      }
    for (int i = 0; i < JELLY_ROWS; i++) {
      float xo = i >= 3 ? sinf(t * 2.2f + i * .9f) * CW * .35f * (i - 2) : 0;
      Col col = i < 3 ? rgb(255, 170, 230) : rgb(255, 150, 220);
      int a = i < 3 ? 230 : 166;
      const char* l = JELLY[i];
      for (int k = 0; l[k]; k++) {
        if (l[k] == ' ') continue;
        float lx = (xo + k * CW + CW / 2) * kx, ly = (i * LH + FONT_CELL_H / 2) * ky;
        g.glyph((uint8_t)l[k], jelly.x + lx, jelly.y + ly, kx, ky, col, a, true, hexCol(0xFF8FD6), night ? 120 : 80);
      }
    }
  }
};

// ---------------------------------------------------------------- dusk and dawn
// Fades between day and night: the backlight dims, the tank switches to the other
// background while it's darkest, then the light comes up to the new level.
struct DayNight {
  bool night = false;   // what the tank shows right now
  float t = -1;         // seconds into a fade, or -1 when not fading
  bool to = false;      // where the current fade is going

  void set(bool n) { night = to = n; t = -1; }

  // Call once per frame. Returns true at the moment the background should change.
  bool step(bool wantNight, float dt) {
    const float half = DUSK_DAWN_SECONDS / 2.f;
    if (t < 0) {
      if (wantNight == night) return false;
      to = wantNight; t = 0;
      if (half <= 0) { night = to; t = -1; return true; }
    }
    t += dt;
    bool swap = false;
    if (t >= half && night != to) { night = to; swap = true; }
    if (t >= half * 2) t = -1;
    return swap;
  }

  // Backlight percent for this moment.
  int brightness() const {
    auto level = [](bool n) { return (float)(n ? BRIGHTNESS_NIGHT : BRIGHTNESS_DAY); };
    if (t < 0) return (int)level(night);
    const float half = DUSK_DAWN_SECONDS / 2.f;
    float from = level(!to), dest = level(to), low = fminf(from, dest) * .3f;
    float k = t < half ? t / half : (t - half) / half;
    k = k * k * (3 - 2 * k);  // ease in and out
    return (int)lrintf(t < half ? from + (low - from) * k : low + (dest - low) * k);
  }
};

// Text for the clock from the time of day, following the clock settings.
// weekday: 0 = Sunday; month: 0 = January. Leave weekday at -1 for no date line.
static inline void formatClock(char* out, size_t size, int hour, int minute, int second,
                        int weekday = -1, int month = 0, int dayOfMonth = 0) {
  static const char* DAYS[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
  static const char* MONTHS[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
  char colon = (CLOCK_BLINK && (second & 1)) ? '_' : ':';
  int n;
  if (CLOCK_24_HOUR) n = snprintf(out, size, "%d%c%02d", hour, colon, minute);
  else {
    int h12 = hour % 12; if (h12 == 0) h12 = 12;
    n = snprintf(out, size, "%d%c%02d %s", h12, colon, minute, hour < 12 ? "am" : "pm");
  }
  if (CLOCK_SHOW_DATE && weekday >= 0 && weekday < 7 && month >= 0 && month < 12 && n > 0 && (size_t)n < size)
    snprintf(out + n, size - n, "|%s %s %d", DAYS[weekday], MONTHS[month], dayOfMonth);
}

// ---------------------------------------------------------------- sun and weather
// Is the sun up? Works out sunrise and sunset for a place and day of the year (no internet
// needed) and compares them with the current time in UTC. marginMinutes stretches the day
// at both ends, so "night" starts a little after sunset.
static inline bool isDaylight(int dayOfYear, float utcMinutes, float latitude, float longitude, float marginMinutes) {
  const float PI_F = 3.14159265f, RAD = PI_F / 180;
  float g = 2 * PI_F / 365 * (dayOfYear - 1);
  float eqTime = 229.18f * (0.000075f + 0.001868f * cosf(g) - 0.032077f * sinf(g) - 0.014615f * cosf(2 * g) - 0.040849f * sinf(2 * g));
  float decl = 0.006918f - 0.399912f * cosf(g) + 0.070257f * sinf(g) - 0.006758f * cosf(2 * g) + 0.000907f * sinf(2 * g) -
               0.002697f * cosf(3 * g) + 0.00148f * sinf(3 * g);
  float c = cosf(90.833f * RAD) / (cosf(latitude * RAD) * cosf(decl)) - tanf(latitude * RAD) * tanf(decl);
  if (c <= -1) return true;    // midnight sun
  if (c >= 1) return false;    // polar night
  float ha = acosf(c) / RAD;   // half the day's length, in degrees
  float rise = 720 - 4 * (longitude + ha) - eqTime - marginMinutes;  // minutes after midnight UTC
  float set = 720 - 4 * (longitude - ha) - eqTime + marginMinutes;
  return fmodf(utcMinutes - rise + 2880, 1440) < set - rise;
}

// Weather codes (the international WMO numbers weather services use) -> what the tank shows.
static inline uint8_t weatherFromCode(int code) {
  if (code < 0 || code <= 1) return WEATHER_CLEAR;            // clear, mainly clear (or unknown)
  if (code >= 95) return WEATHER_STORM;                        // thunderstorm
  if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) return WEATHER_RAIN;
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) return WEATHER_SNOW;  // snow, snow showers
  if (code == 45 || code == 48) return WEATHER_FOG;
  return WEATHER_CLOUDY;
}

// A short word for a weather code (the WMO numbers weather services use).
static inline const char* weatherWord(int code) {
  if (code < 0) return "";
  if (code == 0) return "Clear";
  if (code <= 2) return "Partly cloudy";
  if (code == 3) return "Cloudy";
  if (code == 45 || code == 48) return "Fog";
  if (code >= 51 && code <= 57) return "Drizzle";
  if (code >= 61 && code <= 67) return "Rain";
  if (code >= 71 && code <= 77) return "Snow";
  if (code >= 80 && code <= 82) return "Showers";
  if (code == 85 || code == 86) return "Snow showers";
  if (code >= 95) return "Thunderstorm";
  return "Cloudy";
}

