"""Draws the weather icons as small shaded images and writes them to ui/weather_icons.c.

Each icon is drawn four times larger than needed and shrunk, which gives smooth edges, and
is built from soft gradients so a cloud looks rounded and the sun glows. They are original
drawings made by this script; nothing is downloaded.

Run from the firmware folder with the server's Python (needs Pillow):
    ../server/.venv/bin/python tools/make_weather_icons.py
"""

import math
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter

BIG = 256  # working size; every icon is designed in 0..1 coordinates
ROOT = Path(__file__).resolve().parents[1]


def px(v):
    return v * BIG


def blank():
    return Image.new("RGBA", (BIG, BIG), (0, 0, 0, 0))


def vertical(top, bottom, y0=0.0, y1=1.0):
    """A full-canvas image fading from `top` at y0 to `bottom` at y1."""
    image = Image.new("RGB", (BIG, BIG))
    draw = ImageDraw.Draw(image)
    for y in range(BIG):
        t = min(1.0, max(0.0, (y / BIG - y0) / max(1e-6, y1 - y0)))
        draw.line([(0, y), (BIG, y)], fill=tuple(round(a + (b - a) * t) for a, b in zip(top, bottom)))
    return image


def radial(center, inner, outer, radius):
    image = Image.new("RGB", (BIG, BIG), outer)
    draw = ImageDraw.Draw(image)
    steps = 40
    for i in range(steps, 0, -1):
        t = i / steps
        r = px(radius) * t
        colour = tuple(round(a + (b - a) * t) for a, b in zip(inner, outer))
        draw.ellipse([px(center[0]) - r, px(center[1]) - r, px(center[0]) + r, px(center[1]) + r], fill=colour)
    return image


def paste(canvas, colours, mask):
    layer = colours.convert("RGBA")
    layer.putalpha(mask)
    return Image.alpha_composite(canvas, layer)


def circle_mask(cx, cy, r):
    mask = Image.new("L", (BIG, BIG), 0)
    ImageDraw.Draw(mask).ellipse([px(cx - r), px(cy - r), px(cx + r), px(cy + r)], fill=255)
    return mask


def glow(canvas, cx, cy, radius, colour, strength):
    mask = circle_mask(cx, cy, radius * 0.55).filter(ImageFilter.GaussianBlur(px(radius * 0.28)))
    mask = mask.point(lambda v: int(v * strength))
    return paste(canvas, Image.new("RGB", (BIG, BIG), colour), mask)


def sun(canvas, cx=0.5, cy=0.5, s=1.0, rays=True):
    canvas = glow(canvas, cx, cy, 0.46 * s, (255, 176, 46), 0.55)
    if rays:
        mask = Image.new("L", (BIG, BIG), 0)
        draw = ImageDraw.Draw(mask)
        for i in range(8):
            a = i * math.pi / 4
            p1 = (px(cx + math.cos(a) * 0.30 * s), px(cy + math.sin(a) * 0.30 * s))
            p2 = (px(cx + math.cos(a) * 0.40 * s), px(cy + math.sin(a) * 0.40 * s))
            width = px(0.055 * s)
            draw.line([p1, p2], fill=255, width=round(width))
            for p in (p1, p2):
                draw.ellipse([p[0] - width / 2, p[1] - width / 2, p[0] + width / 2, p[1] + width / 2], fill=255)
        canvas = paste(canvas, Image.new("RGB", (BIG, BIG), (255, 190, 60)), mask)
    disc = radial((cx - 0.05 * s, cy - 0.06 * s), (255, 236, 150), (255, 170, 30), 0.30 * s)
    return paste(canvas, disc, circle_mask(cx, cy, 0.215 * s))


def crescent(canvas, cx=0.5, cy=0.5, r=0.30, stars=True):
    canvas = glow(canvas, cx, cy, r * 1.4, (150, 140, 255), 0.16)
    mask = ImageChops.subtract(circle_mask(cx, cy, r), circle_mask(cx + r * 0.46, cy - r * 0.24, r * 0.86))
    canvas = paste(canvas, vertical((250, 248, 255), (176, 168, 240), cy - r, cy + r), mask)
    if stars:
        for sx, sy, size in ((0.76, 0.24, 0.055), (0.86, 0.52, 0.035), (0.66, 0.70, 0.03)):
            star = Image.new("L", (BIG, BIG), 0)
            draw = ImageDraw.Draw(star)
            draw.polygon(
                [
                    (px(sx), px(sy - size)), (px(sx + size * 0.25), px(sy - size * 0.25)),
                    (px(sx + size), px(sy)), (px(sx + size * 0.25), px(sy + size * 0.25)),
                    (px(sx), px(sy + size)), (px(sx - size * 0.25), px(sy + size * 0.25)),
                    (px(sx - size), px(sy)), (px(sx - size * 0.25), px(sy - size * 0.25)),
                ],
                fill=255,
            )
            canvas = paste(canvas, Image.new("RGB", (BIG, BIG), (236, 232, 255)), star)
    return canvas


def cloud_mask(cx, cy, s=1.0):
    mask = Image.new("L", (BIG, BIG), 0)
    draw = ImageDraw.Draw(mask)
    for dx, dy, r in ((-0.20, 0.04, 0.16), (0.02, -0.08, 0.22), (0.24, 0.05, 0.15)):
        draw.ellipse([px(cx + (dx - r) * s), px(cy + (dy - r) * s), px(cx + (dx + r) * s), px(cy + (dy + r) * s)], fill=255)
    draw.rounded_rectangle(
        [px(cx - 0.36 * s), px(cy + 0.02 * s), px(cx + 0.39 * s), px(cy + 0.20 * s)], radius=px(0.09 * s), fill=255
    )
    return mask


def cloud(canvas, cx=0.5, cy=0.48, s=1.0, top=(250, 252, 255), bottom=(176, 198, 228)):
    mask = cloud_mask(cx, cy, s)
    # a soft shadow under the cloud lifts it off whatever is behind
    shadow = ImageChops.offset(mask, 0, round(px(0.03 * s))).filter(ImageFilter.GaussianBlur(px(0.03 * s)))
    canvas = paste(canvas, Image.new("RGB", (BIG, BIG), (10, 20, 40)), shadow.point(lambda v: int(v * 0.35)))
    canvas = paste(canvas, vertical(top, bottom, cy - 0.30 * s, cy + 0.20 * s), mask)
    # darker underside: the part of the cloud not covered by a copy nudged upward
    under = ImageChops.subtract(mask, ImageChops.offset(mask, 0, -round(px(0.05 * s))))
    under = under.filter(ImageFilter.GaussianBlur(px(0.018 * s)))
    under = ImageChops.multiply(under, mask).point(lambda v: int(v * 0.5))
    shade = tuple(round(c * 0.72) for c in bottom)
    return paste(canvas, Image.new("RGB", (BIG, BIG), shade), under)


RAIN_CLOUD = dict(top=(206, 216, 232), bottom=(128, 146, 176))
STORM_CLOUD = dict(top=(170, 180, 204), bottom=(92, 104, 138))


def drops(canvas, xs, length=0.20, colour=(84, 170, 255), width=0.06):
    mask = Image.new("L", (BIG, BIG), 0)
    draw = ImageDraw.Draw(mask)
    for x, y in xs:
        p1, p2 = (px(x + 0.04), px(y)), (px(x - 0.04 * length / 0.2), px(y + length))
        w = px(width)
        draw.line([p1, p2], fill=255, width=round(w))
        for p in (p1, p2):
            draw.ellipse([p[0] - w / 2, p[1] - w / 2, p[0] + w / 2, p[1] + w / 2], fill=255)
    return paste(canvas, vertical((140, 204, 255), colour, 0.66, 0.92), mask)


def flakes(canvas, spots, r=0.065):
    mask = Image.new("L", (BIG, BIG), 0)
    draw = ImageDraw.Draw(mask)
    for x, y in spots:
        for i in range(3):
            a = i * math.pi / 3 + math.pi / 6
            draw.line(
                [(px(x - math.cos(a) * r), px(y - math.sin(a) * r)), (px(x + math.cos(a) * r), px(y + math.sin(a) * r))],
                fill=255,
                width=round(px(0.026)),
            )
        draw.ellipse([px(x - 0.02), px(y - 0.02), px(x + 0.02), px(y + 0.02)], fill=255)
    return paste(canvas, Image.new("RGB", (BIG, BIG), (244, 250, 255)), mask)


def bolt(canvas):
    mask = Image.new("L", (BIG, BIG), 0)
    points = [(0.57, 0.55), (0.39, 0.79), (0.50, 0.79), (0.42, 0.99), (0.66, 0.71), (0.54, 0.71), (0.63, 0.55)]
    ImageDraw.Draw(mask).polygon([(px(x), px(y)) for x, y in points], fill=255)
    canvas = glow(canvas, 0.52, 0.74, 0.20, (255, 200, 60), 0.35)
    return paste(canvas, vertical((255, 240, 150), (255, 176, 30), 0.55, 0.99), mask)


def fog(canvas):
    for y, x1, x2, strength in ((0.34, 0.14, 0.86, 1.0), (0.52, 0.24, 0.80, 0.85), (0.70, 0.12, 0.88, 0.7)):
        mask = Image.new("L", (BIG, BIG), 0)
        ImageDraw.Draw(mask).rounded_rectangle([px(x1), px(y - 0.045), px(x2), px(y + 0.045)], radius=px(0.045), fill=255)
        canvas = paste(canvas, vertical((224, 230, 238), (170, 180, 194), y - 0.05, y + 0.05), mask.point(lambda v, k=strength: int(v * k)))
    return canvas


def sun_event(rising):
    canvas = blank()
    warm, deep = ((255, 228, 130), (255, 160, 30)) if rising else ((255, 190, 110), (232, 92, 44))
    canvas = glow(canvas, 0.5, 0.74, 0.42, deep, 0.5)
    half = circle_mask(0.5, 0.76, 0.25)
    ImageDraw.Draw(half).rectangle([0, px(0.76), BIG, BIG], fill=0)
    canvas = paste(canvas, radial((0.5, 0.70), warm, deep, 0.30), half)
    mask = Image.new("L", (BIG, BIG), 0)
    draw = ImageDraw.Draw(mask)
    draw.rounded_rectangle([px(0.08), px(0.78), px(0.92), px(0.86)], radius=px(0.04), fill=255)
    tip, tail = (0.10, 0.36) if rising else (0.36, 0.10)
    wing = tip + (0.13 if rising else -0.13)
    for a, b in (((0.5, tail), (0.5, tip)), ((0.5, tip), (0.37, wing)), ((0.5, tip), (0.63, wing))):
        draw.line([(px(a[0]), px(a[1])), (px(b[0]), px(b[1]))], fill=255, width=round(px(0.075)))
        for p in (a, b):
            draw.ellipse([px(p[0] - 0.0375), px(p[1] - 0.0375), px(p[0] + 0.0375), px(p[1] + 0.0375)], fill=255)
    return paste(canvas, Image.new("RGB", (BIG, BIG), deep), mask)


def build(key):
    c = blank()
    if key == "clear":
        return sun(c)
    if key == "clear-night":
        return crescent(c)
    if key == "partly":
        return cloud(sun(c, 0.66, 0.34, 0.72), 0.44, 0.60, 0.92)
    if key == "partly-night":
        return cloud(crescent(c, 0.68, 0.32, 0.21, stars=False), 0.44, 0.60, 0.92)
    if key == "cloudy":
        return cloud(cloud(c, 0.62, 0.36, 0.62, top=(196, 208, 228), bottom=(134, 152, 184)), 0.46, 0.56, 0.95)
    if key == "fog":
        return fog(c)
    if key == "drizzle":
        return drops(cloud(c, 0.5, 0.36, **RAIN_CLOUD), [(0.38, 0.70), (0.60, 0.70)], length=0.13)
    if key == "rain":
        return drops(cloud(c, 0.5, 0.36, **RAIN_CLOUD), [(0.30, 0.68), (0.50, 0.68), (0.70, 0.68)])
    if key == "snow":
        return flakes(cloud(c, 0.5, 0.36, **RAIN_CLOUD), [(0.30, 0.76), (0.50, 0.86), (0.70, 0.76)])
    if key == "storm":
        return bolt(cloud(c, 0.5, 0.36, **STORM_CLOUD))
    if key == "sunrise":
        return sun_event(True)
    if key == "sunset":
        return sun_event(False)
    raise KeyError(key)


WEATHER = ["clear", "clear-night", "partly", "partly-night", "cloudy", "fog", "drizzle", "rain", "snow", "storm"]
ICONS = [(key, size) for key in WEATHER for size in (64, 40)] + [("sunrise", 32), ("sunset", 32)]


def encode(image):
    """RGB565 pixels followed by one alpha byte per pixel (LVGL's RGB565A8 format)."""
    colour, alpha = bytearray(), bytearray()
    for r, g, b, a in image.get_flattened_data():
        value = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        colour += bytes((value & 0xFF, value >> 8))
        alpha.append(a)
    return bytes(colour + alpha)


lines = [
    "/* Generated by tools/make_weather_icons.py. Do not edit by hand. */",
    '#include "weather_icons.h"',
    "",
]
table = []
sheet = Image.new("RGB", (len(WEATHER) * 72 + 8, 2 * 72 + 8), (26, 31, 37))
for index, (key, size) in enumerate(ICONS):
    small = build(key).resize((size, size), Image.LANCZOS)
    data = encode(small)
    name = f"icon_{key.replace('-', '_')}_{size}"
    body = ",\n    ".join(", ".join(f"0x{b:02x}" for b in data[i : i + 16]) for i in range(0, len(data), 16))
    lines += [f"static const uint8_t {name}[{len(data)}] = {{\n    {body},\n}};", ""]
    table.append(f'    {{"{key}", {size}, {name}}},')
    column = WEATHER.index(key) if key in WEATHER else (0 if key == "sunrise" else 1)
    row = 0 if size == 64 else 1
    if key in WEATHER:
        sheet.paste(small, (8 + column * 72 + (64 - size) // 2, 8 + row * 72 + (64 - size) // 2), small)
lines += ["const weather_icon_t weather_icons[] = {", *table, "};", f"const int weather_icon_count = {len(table)};", ""]
(ROOT / "ui" / "weather_icons.c").write_text("\n".join(lines))
(ROOT / "ui" / "weather_icons.h").write_text(
    "/* Generated by tools/make_weather_icons.py. */\n"
    "#pragma once\n#include <stdint.h>\n\n"
    "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n"
    "/* `data` holds size*size RGB565 pixels followed by size*size alpha bytes. */\n"
    "typedef struct {\n    const char* key;\n    uint16_t size;\n    const uint8_t* data;\n} weather_icon_t;\n\n"
    "extern const weather_icon_t weather_icons[];\nextern const int weather_icon_count;\n\n"
    "#ifdef __cplusplus\n}\n#endif\n"
)
(ROOT / "build").mkdir(exist_ok=True)
sheet.resize((sheet.width * 3, sheet.height * 3), Image.LANCZOS).save(ROOT / "build" / "weather_icons_preview.png")
print("wrote", len(table), "icons,", sum(s * s * 3 for _, s in ICONS) // 1024, "KB")
