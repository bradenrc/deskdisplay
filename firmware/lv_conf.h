/* LVGL settings shared by the desktop simulator and the ESP32 build.
 * Anything not set here takes LVGL's default. */
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16 /* RGB565, what the panel uses */

#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB
#define LV_USE_OS LV_OS_NONE

#define LV_DEF_REFR_PERIOD 16
#define LV_DRAW_SW_COMPLEX 1

#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

/* Built-in font, used only as a fallback. The UI uses the fonts in ui/fonts. */
#define LV_FONT_MONTSERRAT_14 1

/* JPEG decoder (baseline only), used for the photo frame. */
#define LV_USE_TJPGD 1

#define LV_THEME_DEFAULT_DARK 1

#ifdef DESK_SIM
#define LV_USE_SDL 1
#define LV_SDL_INCLUDE_PATH <SDL2/SDL.h>
#define LV_SDL_DIRECT_EXIT 1
#endif

#endif
