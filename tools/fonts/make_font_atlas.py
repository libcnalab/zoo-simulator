#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Bakes a TrueType font into glyph atlases the game's interface draws text from.

For every pixel size one PNG (white glyphs, coverage in alpha) and one entry in
<name>.json with the place and metrics of every glyph. The game picks the
atlas nearest to the size it needs on screen, so text stays sharp in any window.

    python3 tools/fonts/make_font_atlas.py \
        --font /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf --name sans --out assets/fonts

Needs Pillow and fontTools. The output is reproducible for a given font file.
"""
import argparse
import json
import math
import os

from fontTools.ttLib import TTFont
from PIL import Image, ImageDraw, ImageFont

SYMBOLS = "←↑→↓▲▼◀▶●■★☆♂♀✓✗⚠☀☁☂❄–—‘’“”•…€♥⌂⚑✚☰⚙❯❮"
SIZES = (13, 16, 20, 26, 34, 46)
PAD = 1


def charset(font_path):
    cmap = TTFont(font_path).getBestCmap()
    wanted = list(range(32, 127)) + list(range(160, 384)) + [ord(c) for c in SYMBOLS]
    return [cp for cp in wanted if cp in cmap]


def bake(font_path, size, codepoints):
    font = ImageFont.truetype(font_path, size)
    ascent, descent = font.getmetrics()
    glyphs = []
    for cp in codepoints:
        ch = chr(cp)
        left, top, right, bottom = font.getbbox(ch)
        glyphs.append((cp, left, top, max(right - left, 0), max(bottom - top, 0), font.getlength(ch)))
    # Shelf packing into a power-of-two width.
    area = sum((g[3] + 2 * PAD) * (g[4] + 2 * PAD) for g in glyphs)
    width = 1 << max(7, math.ceil(math.log2(math.sqrt(area) * 1.15)))
    x = y = shelf = 0
    placed = {}
    for cp, left, top, w, h, advance in sorted(glyphs, key=lambda g: -g[4]):
        if x + w + 2 * PAD > width:
            x, y, shelf = 0, y + shelf, 0
        placed[cp] = (x + PAD, y + PAD)
        x += w + 2 * PAD
        shelf = max(shelf, h + 2 * PAD)
    height = y + shelf
    alpha = Image.new("L", (width, height), 0)
    draw = ImageDraw.Draw(alpha)
    table = {}
    for cp, left, top, w, h, advance in glyphs:
        px, py = placed[cp]
        if w and h:
            draw.text((px - left, py - top), chr(cp), font=font, fill=255)
        table[str(cp)] = [px, py, w, h, left, top, round(advance, 3)]
    image = Image.merge("RGBA", (Image.new("L", alpha.size, 255),) * 3 + (alpha,))
    return image, {"size": size, "ascent": ascent, "descent": descent, "line": ascent + descent,
                   "width": width, "height": height, "glyphs": table}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--font", required=True)
    parser.add_argument("--name", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()
    codepoints = charset(args.font)
    sizes = []
    for size in SIZES:
        image, entry = bake(args.font, size, codepoints)
        entry["image"] = f"{args.name}_{size}"
        image.save(os.path.join(args.out, f"{args.name}_{size}.png"), optimize=True)
        sizes.append(entry)
    with open(os.path.join(args.out, f"{args.name}.json"), "w", encoding="utf-8") as out:
        json.dump({"source": os.path.basename(args.font), "sizes": sizes}, out, separators=(",", ":"))
        out.write("\n")
    print(f"{args.name}: {len(codepoints)} glyphs, sizes {list(SIZES)}")


if __name__ == "__main__":
    main()
