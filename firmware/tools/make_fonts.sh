#!/bin/sh
# Regenerates the bitmap fonts in ui/fonts from Montserrat (SIL Open Font License),
# which ships inside the LVGL source tree. Needs Node.js; run from the firmware folder.
set -e
TTF=.deps/lvgl/scripts/built_in_font/Montserrat-Medium.ttf
# Basic Latin, Latin-1, and the punctuation that turns up in headlines and event titles.
TEXT="0x20-0x7F,0xA0-0xFF,0x2013,0x2014,0x2018,0x2019,0x201C,0x201D,0x2022,0x2026"
for size in 16 18 20 24 28 40; do
  npx --yes lv_font_conv@1.5.3 --font "$TTF" -r "$TEXT" --size $size --bpp 4 --format lvgl \
    --no-compress --lv-include lvgl.h --lv-font-name font_text_$size -o ui/fonts/font_text_$size.c
done
# The clock: digits and a colon only, so a very large size stays small in flash.
npx --yes lv_font_conv@1.5.3 --font "$TTF" -r "0x30-0x3A,0x20" --size 140 --bpp 4 --format lvgl \
  --no-compress --lv-include lvgl.h --lv-font-name font_clock_140 -o ui/fonts/font_clock_140.c
npx --yes lv_font_conv@1.5.3 --font "$TTF" -r "0x30-0x3A,0x20" --size 124 --bpp 4 --format lvgl \
  --no-compress --lv-include lvgl.h --lv-font-name font_clock_124 -o ui/fonts/font_clock_124.c
npx --yes lv_font_conv@1.5.3 --font "$TTF" -r "0x30-0x3A,0x20" --size 56 --bpp 4 --format lvgl \
  --no-compress --lv-include lvgl.h --lv-font-name font_clock_56 -o ui/fonts/font_clock_56.c
