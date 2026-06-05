#!/usr/bin/env python3
"""Convert RGBA PNG to LVGL 9 ARGB8888 C asset (B,G,R,A bytes per pixel)."""
import sys
from pathlib import Path

from PIL import Image


def main() -> None:
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} <input.png> <output.c>")
        sys.exit(1)

    png_path = Path(sys.argv[1])
    c_path = Path(sys.argv[2])
    img = Image.open(png_path).convert("RGBA")
    w, h = img.size
    pixels = img.tobytes()
    buf = bytearray()
    for i in range(0, len(pixels), 4):
        r, g, b, a = pixels[i], pixels[i + 1], pixels[i + 2], pixels[i + 3]
        buf.extend((b, g, r, a))
    stride = w * 4
    data_size = len(buf)

    lines = []
    for i in range(0, len(buf), 16):
        chunk = buf[i : i + 16]
        lines.append("    " + ",".join(f"0x{b:02x}" for b in chunk) + ",")
    body = "\n".join(lines)

    text = (
        "/**\n"
        " * toyota_logo.c — LVGL 9.x ARGB8888 image asset.\n"
        f" * Auto-generated from {png_path.name}.\n"
        " * Format: LV_COLOR_FORMAT_ARGB8888 (B,G,R,A per pixel, little-endian).\n"
        f" * Dimensions: {w} x {h} px   Data: {data_size} bytes\n"
        " */\n\n"
        '#include "lvgl.h"\n\n'
        "LV_ATTRIBUTE_LARGE_CONST\n"
        "static const uint8_t toyota_logo_map[] = {\n"
        f"{body}\n"
        "};\n\n"
        "const lv_image_dsc_t toyota_logo = {\n"
        "    .header = {\n"
        "        .cf = LV_COLOR_FORMAT_ARGB8888,\n"
        f"        .w = {w}, .h = {h}, .stride = {stride},\n"
        "    },\n"
        f"    .data_size = {data_size},\n"
        "    .data = toyota_logo_map,\n"
        "};\n"
    )
    c_path.parent.mkdir(parents=True, exist_ok=True)
    c_path.write_text(text, newline="\n")
    print(f"wrote {c_path} ({w}x{h}, {data_size} bytes)")


if __name__ == "__main__":
    main()
