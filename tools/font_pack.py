#!/usr/bin/env python3
"""Build a Wio Memo 1-bit WMF font image for the onboard QSPI flash."""

import argparse
import struct
import zlib
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


HEADER = struct.Struct("<4sHBBIIIHHII")
RECORD = struct.Struct("<II")


def gb2312_codepoints() -> list[int]:
    result: set[int] = set()
    for high in range(0xA1, 0xF8):
        for low in range(0xA1, 0xFF):
            try:
                char = bytes((high, low)).decode("gb2312")
            except UnicodeDecodeError:
                continue
            if len(char) == 1 and ord(char) >= 128:
                result.add(ord(char))
    return sorted(result)


def load_codepoints(charset: str, chars_file: Path | None) -> list[int]:
    points = set(gb2312_codepoints() if charset == "gb2312" else [])
    if chars_file:
        points.update(ord(char) for char in chars_file.read_text(encoding="utf-8") if ord(char) >= 128)
    if not points:
        raise ValueError("the selected character set is empty")
    return sorted(points)


def render_glyph(font: ImageFont.FreeTypeFont, char: str, size: int) -> bytes:
    image = Image.new("1", (size, size), 0)
    draw = ImageDraw.Draw(image)
    left, top, right, bottom = draw.textbbox((0, 0), char, font=font)
    x = (size - (right - left)) // 2 - left
    y = (size - (bottom - top)) // 2 - top
    draw.text((x, y), char, font=font, fill=1)
    pixels = image.load()
    packed = bytearray((size * size + 7) // 8)
    bit = 0
    for y in range(size):
        for x in range(size):
            if pixels[x, y]:
                packed[bit >> 3] |= 0x80 >> (bit & 7)
            bit += 1
    return bytes(packed)


def build(font_path: Path, output: Path, size: int, codepoints: list[int]) -> None:
    if not 8 <= size <= 24:
        raise ValueError("font size must be between 8 and 24 pixels")
    font = ImageFont.truetype(str(font_path), size=size)
    glyphs = [(codepoint, render_glyph(font, chr(codepoint), size)) for codepoint in codepoints]
    bytes_per_glyph = (size * size + 7) // 8
    index_offset = HEADER.size
    data_offset = index_offset + len(glyphs) * RECORD.size
    index = b"".join(RECORD.pack(codepoint, position * bytes_per_glyph)
                     for position, (codepoint, _) in enumerate(glyphs))
    bitmaps = b"".join(bitmap for _, bitmap in glyphs)
    payload_crc = zlib.crc32(index + bitmaps) & 0xFFFFFFFF
    header_without_crc = HEADER.pack(
        b"WMF1", 1, size, size, len(glyphs), index_offset, data_offset,
        bytes_per_glyph, 0, payload_crc, 0
    )
    header_crc = zlib.crc32(header_without_crc[:-4]) & 0xFFFFFFFF
    header = header_without_crc[:-4] + struct.pack("<I", header_crc)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(header + index + bitmaps)
    print(f"WMF1: {len(glyphs)} glyphs, {size}x{size}, {output.stat().st_size} bytes -> {output}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--font", type=Path, required=True, help="OpenType/TrueType source font")
    parser.add_argument("--output", type=Path, default=Path("assets/font-16.wmf"))
    parser.add_argument("--size", type=int, default=16)
    parser.add_argument("--charset", choices=("gb2312", "custom"), default="gb2312")
    parser.add_argument("--chars-file", type=Path, help="additional/custom UTF-8 characters")
    args = parser.parse_args()
    build(args.font, args.output, args.size, load_codepoints(args.charset, args.chars_file))


if __name__ == "__main__":
    main()
