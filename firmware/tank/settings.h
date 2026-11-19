// =====================================================================
//  DESK AQUARIUM SETTINGS  —  change things here, then click Upload.
// =====================================================================
#pragma once

// ---------------- Wi-Fi (only used to get the time for the clock / night mode)
// Your Wi-Fi name and password go in secrets.h (copy secrets_example.h to secrets.h and fill
// it in). It's kept out of git so the password never gets uploaded. Without it, Wi-Fi is
// skipped (night mode then follows NIGHT_MODE below).
// Must be a 2.4 GHz network. A phone hotspot works too (turn on "maximize compatibility").
// WIFI_NAME_2 / WIFI_PASSWORD_2: a second network to try if the first isn't there (for
// example your phone's hotspot, so the clock still gets set when you take the tank somewhere else).
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef WIFI_NAME
#define WIFI_NAME       ""
#define WIFI_PASSWORD   ""
#endif
#ifndef WIFI_NAME_2
#define WIFI_NAME_2     ""
#define WIFI_PASSWORD_2 ""
#endif

// Your time zone. Mountain Time (Boise) is the default.
//   Pacific:  "PST8PDT,M3.2.0,M11.1.0"     Mountain: "MST7MDT,M3.2.0,M11.1.0"
//   Central:  "CST6CDT,M3.2.0,M11.1.0"     Eastern:  "EST5EDT,M3.2.0,M11.1.0"
#define TIME_ZONE       "MST7MDT,M3.2.0,M11.1.0"

// ---------------- Lights
// 0 = automatic by clock (NIGHT_STARTS / NIGHT_ENDS), 1 = always day, 2 = always night,
// 3 = follow the real sunrise and sunset at LATITUDE / LONGITUDE below
#define NIGHT_MODE      0
#define NIGHT_STARTS    22     // hour (24h clock) the tank dims
#define NIGHT_ENDS      7      // hour it brightens again
#define BRIGHTNESS_DAY  85     // screen backlight, percent
#define BRIGHTNESS_NIGHT 35
#define NIGHT_COLORS    0.68f  // how bright everything is drawn at night (1.0 = same as day), on top of the backlight
// Where the tank lives, for NIGHT_MODE 3 and the weather. Boise is the default.
// North and east are positive, south and west negative. Stays on the board unless
// USE_WEATHER is on.
#define LATITUDE        43.62
#define LONGITUDE       -116.20
#define SUN_MARGIN_MINUTES 30  // NIGHT_MODE 3: stay "day" this long before sunrise and after sunset

// ---------------- Clock
// The board has no clock of its own: the time comes from Wi-Fi, so the clock only shows
// up once WIFI_NAME / WIFI_PASSWORD above are filled in and it has connected.
#define SHOW_CLOCK      1      // 1 = show the time, 0 = no clock
#define CLOCK_BIG       0     // 1 = big three-row digits, 0 = small text
#define CLOCK_24_HOUR   0      // 0 = 3:07 pm, 1 = 15:07
#define CLOCK_BLINK     1      // 1 = the colon blinks once a second
#define CLOCK_CORNER    0      // 0 = top-left, 1 = top-right
#define CLOCK_SHOW_DATE 1      // 1 = small "Sat Nov 14" line under the time

// ---------------- Weather
// 1 = once an hour, look up the temperature and weather outside. A little weather picture
// (sun, moon, partly cloudy, cloud, rain, showers, snow, storm, fog) with the temperature under
// it shows across from the clock, and the tank follows the sky: rain makes rings on the surface, snow
// drifts down into the water and melts, thunderstorms flash the backlight, fog drifts as mist,
// cloud dims the light a little and stops the sparkles. To do this the board sends LATITUDE and
// LONGITUDE above (rounded to about a kilometer) to the free weather service open-meteo.com,
// and turns Wi-Fi on for a few seconds every hour instead of once a day. 0 = off.
// Needs Wi-Fi, and only shows once the clock is set.
#define USE_WEATHER     1
#define TEMP_FAHRENHEIT 1      // 1 = °F, 0 = °C
#define SHOW_WEATHER    1      // 1 = show the weather picture and temperature in the top corner

// ---------------- Feeding (no touch screen, so the tank feeds itself)
#define FEED_EVERY_MINUTES  40 // 0 = never
#define FEED_AT_NIGHT       0  // 1 = keep feeding during night mode

// ---------------- Fish
// How many of each fish swim in the front of the tank.
// (The fish drawings themselves are in art.h.)
#define NUM_DELUXE      1      //  big pink fancy fish with shimmering scales
#define NUM_CLASSIC     2      //  ><((((°>
#define NUM_CLOWN       1      //  orange clownfish with white bands
#define NUM_ANGEL       1      //  tall striped angelfish
#define NUM_GROUPER     1      //  big spotted coral grouper
#define NUM_PUFFER      1      //  pufferfish (puffs up now and then)
#define NUM_LIONFISH    1      //  striped lionfish with white spiky fins
#define NUM_BETTA       1      //  small betta with a big flowing tail
#define NUM_MINNOWS     6      //  ><°>  a school that swims together
#define NUM_DISTANT     5      //  faint little fish far in the background

#define SPEED_MULTIPLIER 1.0f  // 1.0 = normal, 0.5 = calm, 1.5 = busy
#define CRAB_SIZE       0.6f   // how big the crab is drawn (same scale as FISH_SIZE)
#define FISH_SIZE       0.75f  // how big the fish (and the snail and eel) are drawn: 1.0 = full
                               // size, 0.75 = smaller (less crowded). Below about 0.6 the letters
                               // start to look rough.
#define FISH_PERSONALITIES 1   // clownfish is shy (stays by the castle), puffer is curious (visits the
                               // snail and crab), classic fish swim in pairs
#define MINNOWS_SCATTER 1      // the minnow school darts away when a big fish swims into it
#define FISH_GROW       0      // 1 = fish start a little smaller and grow over about two weeks.
                               // Off by default: fish that aren't full size look slightly rougher.
#define FISH_GLOW       1      // 1 = soft glow around fish, 2 = lighter glow (faster), 0 = off (fastest)
#define FISH_SOLID      1      // 1 = fish hide what's behind them, 0 = see-through (letters overlap)

// ---------------- Colors (hex RGB). Change these to recolor fish.
// Lighter/darker shades, white bands and dark stripes are made from these automatically.
#define COLOR_DELUXE    0xFF6FAE
#define COLOR_DELUXE_FINS 0xFFC2E0
#define COLOR_CLASSIC   0xFFB347
#define COLOR_CLASSIC_FINS 0xFF7A1A
#define COLOR_MINNOW    0x9FD8FF
#define COLOR_MINNOW_FINS 0xE2F5FF
#define COLOR_CLOWN     0xFF8A2A
#define COLOR_CLOWN_FINS 0x2B1B14
#define COLOR_ANGEL     0xF4EFB0
#define COLOR_ANGEL_FINS 0xE0A83A
#define COLOR_GROUPER   0xE8664F
#define COLOR_GROUPER_SPOTS 0x5FB8E8
#define COLOR_PUFFER    0xC8E66B
#define COLOR_PUFFER_FINS 0xF0FFC0
#define COLOR_LIONFISH  0xD9533A
#define COLOR_LIONFISH_FINS 0xF4EDE4
#define COLOR_BETTA     0xB03ADC
#define COLOR_BETTA_FINS 0xFF6FD8
#define COLOR_WHALE     0x6F8FB0
#define COLOR_WHALE_FINS 0xA9C4DC
#define COLOR_SNAIL_SHELL 0xC98B5A
#define COLOR_SNAIL_BODY 0xE8DCC0
#define COLOR_CASTLE    0xC8B796
#define COLOR_CASTLE_FLAG 0xFF6B6B
#define COLOR_CHEST     0xC9973F
#define COLOR_CHEST_GOLD 0xFFD76A
#define COLOR_EEL       0x7FB069
#define COLOR_ROCK      0x9AA7B6
#define COLOR_TURTLE    0x5FA36A
#define COLOR_TURTLE_SHELL 0xC9B46A
#define COLOR_CRAB      0xFF6F4F
#define COLOR_CRAB_CLAWS 0xFFB199

// ---------------- Scenery
#define NUM_KELP        8      // tall swaying seaweed strands
#define SHOW_CASTLE     1
#define SHOW_CHEST      1      // treasure chest that opens now and then and lets out bubbles
#define SHOW_CRAB       1
#define SHOW_JELLYFISH  1
#define SHOW_SNAIL      1      // a snail slowly crawling along the sand
#define SHOW_EEL        1      // an eel that slides out of its cave in the rock now and then
#ifndef EEL_MOUTH
#define EEL_MOUTH       1      // 0 = mouth stays shut, 1 = lips part slightly as it breathes, 2 = opens wide with teeth
#endif
#define VISITOR_EVERY_MINUTES 20  // a whale or a turtle drifts past in the distance now and then (0 = never)

// ---------------- Little touches
#define FISH_SHADOWS     1     // soft shadows on the sand under fish swimming near the bottom
#define SAND_RIPPLES     1     // sunlight rippling across the sand (daytime)
#define SAND_PUFFS       1     // sand kicked up when the crab walks or food lands
#define BUBBLE_POPS      1     // bubbles pop with a little ring at the surface
#define TURN_HEAD_FIRST  1     // when a fish turns around, its head swings first
#define SURFACE_SPARKLES 1     // glints of light twinkling on the water surface (daytime)
#define GLOWING_PLANKTON 1     // at night the plankton twinkles blue-green
#define SLEEPY_NIGHT     1     // at night fish rest lower in the tank with their eyes mostly shut
#define DUSK_DAWN_SECONDS 30   // how long the lights take to fade between day and night (0 = instant)

// ---------------- Smoothness
// If animation drops below MIN_FPS, the tank turns effects down one step at a time (lighter
// glow, no glow, see-through fish, no shadows) and says so in the Serial Monitor. 0 = never.
#define AUTO_QUALITY    1
#define MIN_FPS         12     // desk display: lowered from 18 because it runs with 2 screen buffers

// ---------------- Memory
// 1 = remember across power cuts: which effects were turned down for speed, whether it was
// night, and how much the fish have grown. 0 = start fresh every time.
#define REMEMBER_STATE  1

// ---------------- Touch screen (only the touch version of the board: ESP32-S3-Touch-LCD-7B)
// Tap the water near the top to drop food there. Tap the left or right edge of the screen to
// bring out buttons: day/night, sky (sunny/cloudy/rain/snow/storm/fog), feed, brightness, clock on/off, and
// call a visitor. They slide away again after BUTTONS_HIDE_SECONDS.
// Day/night from the button lasts until the schedule next switches; a chosen sky lasts an hour.
// Boards without a touchscreen just say so in the Serial Monitor and carry on.
#define USE_TOUCH       1
#define BUTTONS_HIDE_SECONDS 8
// If taps land in the wrong place (advanced):
#define TOUCH_SWAP_XY   0      // 1 = the touch chip's left-right and up-down are swapped
#define TOUCH_FLIP_X    0      // 1 = taps land mirrored left-right
#define TOUCH_FLIP_Y    0      // 1 = taps land mirrored up-down

// ---------------- Checking a new board
// 1 = show a test picture instead of the aquarium (color bars, color ramps, fine patterns and
// a frame around the edge), while the backlight steps from dim to bright. README.md says what
// to look for. Set back to 0 for the aquarium.
#define SCREEN_TEST     0

// ---------------- Advanced (leave alone unless the screen misbehaves)
#define TARGET_FPS      30
#define LCD_PCLK_HZ     (30 * 1000 * 1000)   // try 24000000 if the image drifts or flickers
#define BACKLIGHT_INVERTED 1                 // flip to 0 if day looks dim and night looks bright
#define SCREEN_BUFFERS  2                    // 3 = smoother; try 2 if the screen glitches or won't start
#define SWAP_RED_BLUE   0                    // set to 1 if the test picture's red bar shows blue (and blue shows red)
