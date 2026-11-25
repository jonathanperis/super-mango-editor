#!/usr/bin/env python3
"""Copy recorded render snapshots into tests/snapshots/, compressed.

  python3 tools/render_snapshots.py OUTDIR/snapshots tests/snapshots

The snapshot tests (tests/render_snapshot.c) write their frames as PNGs with
uncompressed ("stored") deflate blocks, because raylib is built without image
export and a real deflate encoder is more code than a test helper should
carry. A 1280x720 frame is then 3.6 MB. This script re-encodes each PNG's
pixel data with zlib, which keeps the pixels and makes the committed copy a
few tens of kilobytes. `make render-snapshots-update` runs it after the
tests have recorded the new frames.
"""

from __future__ import annotations

import struct
import sys
import zlib
from pathlib import Path

SIGNATURE = b"\x89PNG\r\n\x1a\n"


def chunks(data: bytes):
    """(type, body) for each chunk after the signature."""
    at = len(SIGNATURE)
    while at < len(data):
        length, kind = struct.unpack(">I4s", data[at:at + 8])
        yield kind, data[at + 8:at + 8 + length]
        at += 12 + length


def chunk(kind: bytes, body: bytes) -> bytes:
    return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body))


def recompress(data: bytes) -> bytes:
    """The same PNG with its IDAT data compressed at zlib level 9."""
    if not data.startswith(SIGNATURE):
        raise ValueError("not a PNG file")
    header = b""
    pixels = b""
    for kind, body in chunks(data):
        if kind == b"IHDR":
            header = body
        elif kind == b"IDAT":
            pixels += body
    raw = zlib.decompress(pixels)
    return SIGNATURE + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(raw, 9)) + \
        chunk(b"IEND", b"")


def main(argv: list[str]) -> int:
    if len(argv) != 3:
        print(__doc__.strip().splitlines()[2].strip())
        return 2
    source, target = Path(argv[1]), Path(argv[2])
    pngs = sorted(source.glob("*.png"))
    if not pngs:
        print(f"render snapshots: no PNG files in {source}")
        return 1
    target.mkdir(parents=True, exist_ok=True)
    for png in pngs:
        out = target / png.name
        out.write_bytes(recompress(png.read_bytes()))
        print(f"render snapshots: {out} ({out.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
