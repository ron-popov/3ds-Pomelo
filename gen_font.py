#!/usr/bin/env python3
"""
gen_font.py  <font.ttf>  [size=16]

Rasterises a TTF file at the given pixel size and writes the bitmap glyph
data for printable ASCII (0x20-0x7F) into source/font.h, next to this script.

The output is a C header defining:
  FONT_GLYPH_W        advance width in pixels
  FONT_GLYPH_H        cell height in pixels  (ascent + descent)
  FONT_BYTES_PER_ROW  ceil(FONT_GLYPH_W / 8)
  font[96][FONT_GLYPH_H][FONT_BYTES_PER_ROW]

Bit layout: within each row byte, bit 0 (LSB) is the leftmost pixel.
"""

import sys
from pathlib import Path
from PIL import ImageFont, Image, ImageDraw

def usage():
    print(__doc__)
    sys.exit(1)

def generate(ttf_path: str, size: int, out_path: Path) -> None:
    font = ImageFont.truetype(ttf_path, size)

    ascent, descent = font.getmetrics()
    glyph_h = ascent + descent
    glyph_w = round(font.getlength("A"))
    bytes_per_row = (glyph_w + 7) // 8

    lines = [
        "#pragma once",
        "#include <stdint.h>",
        "",
        f"#define FONT_GLYPH_W       {glyph_w}",
        f"#define FONT_GLYPH_H       {glyph_h}",
        f"#define FONT_BYTES_PER_ROW {bytes_per_row}",
        "",
        f"// {Path(ttf_path).name} {size}px, ASCII 0x20-0x7F",
        f"static const uint8_t font[96][FONT_GLYPH_H][FONT_BYTES_PER_ROW] = {{",
    ]

    for code in range(0x20, 0x80):
        char = chr(code)

        img = Image.new("1", (glyph_w, glyph_h), 0)
        draw = ImageDraw.Draw(img)
        draw.text((0, 0), char, font=font, fill=1)

        rows = []
        for y in range(glyph_h):
            row_bytes = []
            for bi in range(bytes_per_row):
                val = 0
                for bit in range(8):
                    px = bi * 8 + bit
                    if px < glyph_w and img.getpixel((px, y)):
                        val |= (1 << bit)
                row_bytes.append(val)
            rows.append(row_bytes)

        row_strs = ", ".join(
            "{" + ", ".join(f"0x{b:02X}" for b in row) + "}"
            for row in rows
        )
        lines.append(f"    {{ {row_strs} }},   // U+{code:04X} ({repr(char)})")

    lines.append("};")

    out_path.write_text("\n".join(lines) + "\n")
    print(f"Wrote {out_path}  ({glyph_w}x{glyph_h}px, {bytes_per_row} bytes/row, 96 glyphs)")

def main():
    args = sys.argv[1:]
    if not args:
        usage()

    ttf_path = args[0]
    size = int(args[1]) if len(args) > 1 else 16

    script_dir = Path(__file__).parent
    out_path = script_dir / "source" / "font.h"

    generate(ttf_path, size, out_path)

if __name__ == "__main__":
    main()
