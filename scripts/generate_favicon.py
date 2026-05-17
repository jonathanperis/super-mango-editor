#!/usr/bin/env python3
"""Generate Super Mango favicon assets for the GitHub Pages site."""
from __future__ import annotations

from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
PUBLIC = ROOT / "docs" / "public"
SIZES = [16, 24, 32, 48, 64, 128, 256]

# Low-glare arcade palette from the site, with mango/player accent colors.
BG = "#0f0e17"
PANEL = "#171322"
GRID = "#2a2338"
GREEN = "#7cff6b"
YELLOW = "#ffd166"
ORANGE = "#ff9f1c"
RED = "#ff4d5a"
PINK = "#ff6b8a"
CREAM = "#ffe8b6"
BROWN = "#6b2d2d"
SHADOW = "#09080d"
WHITE = "#fff7d6"


def rect(draw: ImageDraw.ImageDraw, scale: int, box: tuple[int, int, int, int], fill: str) -> None:
    x0, y0, x1, y1 = box
    draw.rectangle((x0 * scale, y0 * scale, (x1 + 1) * scale - 1, (y1 + 1) * scale - 1), fill=fill)


def generate(size: int) -> Image.Image:
    # Draw at a fixed 32x32 pixel-art grid, then upscale/downscale with NEAREST.
    scale = max(1, size // 32)
    canvas_size = 32 * scale
    img = Image.new("RGBA", (canvas_size, canvas_size), BG)
    d = ImageDraw.Draw(img)

    # Cabinet tile with neon arcade frame.
    rect(d, scale, (1, 1, 30, 30), PANEL)
    for x in range(3, 29, 4):
        rect(d, scale, (x, 2, x + 1, 2), GREEN)
        rect(d, scale, (x, 29, x + 1, 29), RED)
    for y in range(5, 28, 4):
        rect(d, scale, (2, y, 2, y + 1), YELLOW)
        rect(d, scale, (29, y, 29, y + 1), GREEN)
    rect(d, scale, (4, 4, 27, 27), BG)
    for x in range(6, 27, 5):
        rect(d, scale, (x, 6, x, 25), GRID)
    for y in range(6, 27, 5):
        rect(d, scale, (6, y, 25, y), GRID)

    # Stylized mango/player head. Simplified at small sizes but faithful to red cap/mango palette.
    rect(d, scale, (10, 12, 21, 22), ORANGE)
    rect(d, scale, (9, 14, 22, 20), ORANGE)
    rect(d, scale, (11, 11, 19, 23), YELLOW)
    rect(d, scale, (12, 12, 20, 21), ORANGE)
    rect(d, scale, (13, 13, 20, 20), CREAM)
    rect(d, scale, (8, 11, 22, 14), RED)
    rect(d, scale, (10, 9, 18, 12), RED)
    rect(d, scale, (18, 10, 23, 13), PINK)
    rect(d, scale, (7, 14, 10, 16), RED)
    rect(d, scale, (21, 14, 24, 16), RED)
    rect(d, scale, (12, 16, 14, 18), WHITE)
    rect(d, scale, (18, 16, 20, 18), WHITE)
    rect(d, scale, (13, 17, 13, 18), BROWN)
    rect(d, scale, (19, 17, 19, 18), BROWN)
    rect(d, scale, (14, 21, 18, 22), BROWN)
    rect(d, scale, (10, 23, 21, 24), SHADOW)

    if size != canvas_size:
        img = img.resize((size, size), Image.Resampling.NEAREST)
    return img


def main() -> None:
    PUBLIC.mkdir(parents=True, exist_ok=True)
    images = {s: generate(s) for s in SIZES}
    images[256].save(PUBLIC / "favicon.png")
    images[32].save(PUBLIC / "favicon-32x32.png")
    images[180].save(PUBLIC / "apple-touch-icon.png") if 180 in images else generate(180).save(PUBLIC / "apple-touch-icon.png")
    images[256].save(PUBLIC / "favicon.ico", sizes=[(s, s) for s in SIZES])
    print("generated", PUBLIC / "favicon.ico")


if __name__ == "__main__":
    main()
