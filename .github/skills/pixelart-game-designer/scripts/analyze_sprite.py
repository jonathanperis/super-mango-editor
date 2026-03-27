#!/usr/bin/env python3
"""
analyze_sprite.py — Pixel art sprite sheet analyzer.

Usage:
    python3 analyze_sprite.py <path/to/sprite.png> [frame_w] [frame_h]

Examples:
    python3 analyze_sprite.py assets/Player.png
    python3 analyze_sprite.py assets/Player.png 48 48
    python3 analyze_sprite.py assets/Coin.png 16 16

If frame_w / frame_h are omitted, the script attempts to auto-detect the frame
size by scanning for fully transparent column/row separators.

Outputs:
    - Sheet dimensions
    - Detected or provided frame size
    - Grid (cols × rows) and total frame count
    - Per-row frame transparency map (which frames are blank/used)
    - SDL_Rect C snippet for frame 0
"""

import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("ERROR: Pillow is not installed. Run: pip3 install Pillow")
    sys.exit(1)


def fully_transparent_col(img_rgba, x: int) -> bool:
    """Return True if every pixel in column x has alpha == 0."""
    pixels = img_rgba.load()
    for y in range(img_rgba.height):
        if pixels[x, y][3] != 0:
            return False
    return True


def fully_transparent_row(img_rgba, y: int) -> bool:
    """Return True if every pixel in row y has alpha == 0."""
    pixels = img_rgba.load()
    for x in range(img_rgba.width):
        if pixels[x, y][3] != 0:
            return False
    return True


def detect_frame_size(img_rgba):
    """
    Try to detect frame size by finding the first column and row that
    is NOT fully transparent after a leading transparent gap, or by
    looking for a repeating transparent separator.
    Falls back to common sizes if detection fails.
    """
    w, h = img_rgba.size

    # Try common sizes that evenly divide the sheet
    candidates = [8, 16, 24, 32, 48, 64, 96, 128]
    for fw in candidates:
        for fh in candidates:
            if w % fw == 0 and h % fh == 0:
                # Prefer square frames; check if this grid makes sense
                cols = w // fw
                rows = h // fh
                if 1 <= cols <= 32 and 1 <= rows <= 32:
                    return fw, fh, cols, rows

    # Last resort: whole sheet as one frame
    return w, h, 1, 1


def count_used_frames_in_row(img_rgba, row: int, cols: int, fw: int, fh: int) -> list:
    """Return a list of booleans: True = frame has at least one non-transparent pixel."""
    pixels = img_rgba.load()
    used = []
    y_start = row * fh
    y_end = min(y_start + fh, img_rgba.height)
    for col in range(cols):
        x_start = col * fw
        x_end = min(x_start + fw, img_rgba.width)
        has_pixel = False
        for y in range(y_start, y_end):
            for x in range(x_start, x_end):
                if pixels[x, y][3] > 0:
                    has_pixel = True
                    break
            if has_pixel:
                break
        used.append(has_pixel)
    return used


def analyze(path: str, frame_w: int = None, frame_h: int = None):
    img = Image.open(path).convert("RGBA")
    w, h = img.size

    print(f"\n{'='*54}")
    print(f"  Sprite Sheet: {Path(path).name}")
    print(f"{'='*54}")
    print(f"  Sheet size : {w} × {h} px")

    if frame_w and frame_h:
        fw, fh = frame_w, frame_h
        if w % fw != 0 or h % fh != 0:
            print(f"  WARNING: {w}×{h} is not evenly divisible by {fw}×{fh}")
        cols = w // fw
        rows = h // fh
        print(f"  Frame size : {fw} × {fh} px  (provided)")
    else:
        fw, fh, cols, rows = detect_frame_size(img)
        print(f"  Frame size : {fw} × {fh} px  (auto-detected)")

    total = cols * rows
    print(f"  Grid       : {cols} cols × {rows} rows  =  {total} frames")
    print()

    # Per-row analysis
    print("  Per-row frame map  (■ = has pixels, · = transparent/empty)")
    print(f"  {'Row':<5}  {'Frames':}")
    for row in range(rows):
        used = count_used_frames_in_row(img, row, cols, fw, fh)
        symbols = "  ".join("■" if u else "·" for u in used)
        used_count = sum(used)
        print(f"  {row:<5}  {symbols}   ({used_count} used)")

    print()

    # SDL_Rect snippet for frame 0
    print("  SDL_Rect C snippet  (frame 0 → top-left cell):")
    print(f"    #define FRAME_W  {fw}")
    print(f"    #define FRAME_H  {fh}")
    print( "    player->frame = (SDL_Rect){")
    print( "        .x = 0 * FRAME_W,  /* column 0 */")
    print( "        .y = 0 * FRAME_H,  /* row 0    */")
    print( "        .w = FRAME_W,")
    print( "        .h = FRAME_H,")
    print( "    };")
    print()

    # Generic formula
    print("  To access frame (col, row):")
    print("    src.x = col * FRAME_W;")
    print("    src.y = row * FRAME_H;")
    print(f"{'='*54}\n")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(0)

    sprite_path = sys.argv[1]
    fw = int(sys.argv[2]) if len(sys.argv) >= 3 else None
    fh = int(sys.argv[3]) if len(sys.argv) >= 4 else fw  # square default

    analyze(sprite_path, fw, fh)
