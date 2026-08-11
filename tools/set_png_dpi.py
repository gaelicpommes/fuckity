#!/usr/bin/env python3
"""Set a PNG's physical-resolution metadata without external dependencies."""

import argparse
import binascii
import struct
from pathlib import Path

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def set_dpi(path: Path, dpi: float) -> None:
    data = path.read_bytes()
    if not data.startswith(PNG_SIGNATURE):
        raise ValueError(f"not a PNG file: {path}")

    pixels_per_metre = round(dpi / 0.0254)
    payload = struct.pack(">IIB", pixels_per_metre, pixels_per_metre, 1)
    chunk_type = b"pHYs"
    chunk = (
        struct.pack(">I", len(payload))
        + chunk_type
        + payload
        + struct.pack(">I", binascii.crc32(chunk_type + payload) & 0xFFFFFFFF)
    )

    output = bytearray(PNG_SIGNATURE)
    offset = len(PNG_SIGNATURE)
    inserted = False
    while offset < len(data):
        length = struct.unpack(">I", data[offset : offset + 4])[0]
        end = offset + 12 + length
        existing_type = data[offset + 4 : offset + 8]
        if existing_type == b"pHYs":
            if not inserted:
                output.extend(chunk)
                inserted = True
        else:
            output.extend(data[offset:end])
            if existing_type == b"IHDR" and not inserted:
                output.extend(chunk)
                inserted = True
        offset = end

    path.write_bytes(output)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("png", type=Path)
    parser.add_argument("dpi", type=float)
    args = parser.parse_args()
    set_dpi(args.png, args.dpi)


if __name__ == "__main__":
    main()
