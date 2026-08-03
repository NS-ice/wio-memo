#!/usr/bin/env python3
"""Build the fixed-layout multi-size QSPI font image used by Wio Memo."""

import argparse
from pathlib import Path


LAYOUT = (
    ("font16", 0x000000),
    ("font12", 0x050000),
    ("font20", 0x080000),
)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--font16", type=Path, required=True)
    parser.add_argument("--font12", type=Path, required=True)
    parser.add_argument("--font20", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    entries = []
    for index, (name, offset) in enumerate(LAYOUT):
        data = getattr(args, name).read_bytes()
        if data[:4] != b"WMF1":
            raise ValueError(f"{name} is not a WMF1 image")
        next_offset = LAYOUT[index + 1][1] if index + 1 < len(LAYOUT) else None
        if next_offset is not None and offset + len(data) > next_offset:
            raise ValueError(f"{name} overlaps the next font slot")
        entries.append((name, offset, data))

    total_size = entries[-1][1] + len(entries[-1][2])
    image = bytearray(b"\xFF" * total_size)
    for name, offset, data in entries:
        image[offset:offset + len(data)] = data
        print(f"{name}: 0x{offset:06X}..0x{offset + len(data):06X} ({len(data)} bytes)")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(image)
    print(f"bundle: {len(image)} bytes -> {args.output}")


if __name__ == "__main__":
    main()
