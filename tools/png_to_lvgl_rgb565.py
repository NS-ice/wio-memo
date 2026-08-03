#!/usr/bin/env python3
"""Convert a transparent PNG into an opaque LVGL RGB565 C asset."""

import argparse
from pathlib import Path

from PIL import Image


def rgb565(red: int, green: int, blue: int) -> int:
    return ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)


def render(source: Path, width: int, height: int, background: str) -> Image.Image:
    image = Image.open(source).convert("RGBA")
    alpha_box = image.getchannel("A").getbbox()
    if alpha_box:
        image = image.crop(alpha_box)
    margin = 4
    image.thumbnail((width - margin * 2, height - margin * 2), Image.Resampling.LANCZOS)
    bg = tuple(int(background[index:index + 2], 16) for index in (0, 2, 4))
    canvas = Image.new("RGBA", (width, height), (*bg, 255))
    canvas.alpha_composite(image, ((width - image.width) // 2, (height - image.height) // 2))
    return canvas.convert("RGB")


def emit_asset(out, symbol: str, image: Image.Image) -> None:
    values = [rgb565(*pixel) for pixel in image.getdata()]
    out.write(f"LV_ATTRIBUTE_MEM_ALIGN static const uint16_t {symbol}_pixels[] = {{\n")
    for offset in range(0, len(values), 10):
        chunk = ", ".join(f"0x{value:04X}" for value in values[offset:offset + 10])
        out.write(f"  {chunk},\n")
    out.write("};\n\n")
    out.write(f"const lv_img_dsc_t {symbol} = {{\n")
    out.write("  .header = {.cf = LV_IMG_CF_TRUE_COLOR, .always_zero = 0, .reserved = 0, ")
    out.write(f".w = {image.width}, .h = {image.height}}},\n")
    out.write(f"  .data_size = sizeof({symbol}_pixels),\n")
    out.write(f"  .data = reinterpret_cast<const uint8_t *>({symbol}_pixels),\n")
    out.write("};\n\n")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--header", type=Path, required=True)
    parser.add_argument("--background", default="F7F3EA")
    args = parser.parse_args()
    assets = (
        ("memo_mascot_large", render(args.input, 138, 164, args.background)),
        ("memo_mascot_small", render(args.input, 64, 76, args.background)),
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.header.parent.mkdir(parents=True, exist_ok=True)
    with args.header.open("w", encoding="utf-8", newline="\n") as out:
        out.write("#pragma once\n\n#include <lvgl.h>\n\n")
        for symbol, _ in assets:
            out.write(f"extern const lv_img_dsc_t {symbol};\n")
    with args.output.open("w", encoding="utf-8", newline="\n") as out:
        out.write('#include "wio_memo/presentation/memo_mascot_assets.h"\n\n')
        for symbol, image in assets:
            emit_asset(out, symbol, image)


if __name__ == "__main__":
    main()
