#!/usr/bin/env python3
"""Upload a WMF1 font image to Wio Terminal's onboard QSPI flash."""

import argparse
import struct
import time
import zlib
from pathlib import Path

import serial


CHUNK_SIZE = 512
REQUEST = struct.Struct("<4sII")


def wait_line(port: serial.Serial, expected: bytes, timeout: float = 15.0) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        line = port.readline().strip()
        if line == expected:
            return
        if line.startswith(b"ERROR"):
            raise RuntimeError(line.decode("ascii", errors="replace"))
    raise TimeoutError(f"device did not return {expected.decode()}")


def upload(port_name: str, image_path: Path) -> None:
    data = image_path.read_bytes()
    if data[:4] != b"WMF1":
        raise ValueError("input is not a WMF1 image")
    checksum = zlib.crc32(data) & 0xFFFFFFFF
    with serial.Serial(port_name, 115200, timeout=1, write_timeout=5) as port:
        time.sleep(2.0)
        port.reset_input_buffer()
        port.write(REQUEST.pack(b"WMUP", len(data), checksum))
        port.flush()
        # A multi-size bundle can span about 1 MiB. Erasing that many 4 KiB
        # sectors on W25Q32 legitimately takes longer than the old 20 s limit.
        wait_line(port, b"READY", timeout=90)
        sent = 0
        for offset in range(0, len(data), CHUNK_SIZE):
            chunk = data[offset:offset + CHUNK_SIZE]
            port.write(chunk)
            port.flush()
            wait_line(port, b"ACK")
            sent += len(chunk)
            print(f"\rUploading {sent * 100 // len(data):3d}%", end="", flush=True)
        wait_line(port, b"DONE", timeout=20)
    print(f"\nUploaded and verified {len(data)} bytes on {port_name}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True, help="Wio Terminal serial port, e.g. COM5")
    parser.add_argument("--file", type=Path, required=True, help="WMF1 file from font_pack.py")
    args = parser.parse_args()
    upload(args.port, args.file)


if __name__ == "__main__":
    main()
