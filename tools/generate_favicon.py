#!/usr/bin/env python3
"""Generate Super Mango favicon assets for the GitHub Pages site.

Writes docs/public/favicon.ico, favicon.png, favicon-32x32.png and
apple-touch-icon.png. A manual tool (needs Pillow), not part of any build:
run `python3 tools/generate_favicon.py` from the repository root after
changing the drawing below, and commit the images it writes.

Like the website, the icon is cut from the game's own art: Mango's idle
frame standing on the grass tile under the sky colour of the title scene.
Every size is drawn on a small pixel grid and enlarged by whole pixels, so
the art stays crisp.
"""
from __future__ import annotations

from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
PUBLIC = ROOT / "docs" / "public"
SPRITES = ROOT / "assets" / "sprites"

SKY = (8, 169, 252, 255)   # flat colour of backgrounds/sky_blue.png
EDGE = (0, 0, 0, 255)      # the site's thick black box edge

# Mango's body inside the 48x48 idle frame (row 0, frame 0 of player.png).
MANGO_BOX = (16, 19, 32, 32)
# The top-middle 16x16 piece of the 3x3 grass tileset: grass over soil.
GRASS_TOP = (16, 0, 32, 16)


def load(path: str, box: tuple[int, int, int, int]) -> Image.Image:
    return Image.open(SPRITES / path).convert("RGBA").crop(box)


MANGO = load("player/player.png", MANGO_BOX)
GRASS = load("levels/grass_tileset.png", GRASS_TOP)


def scene(grid: int, ground: int, framed: bool) -> Image.Image:
    """Mango on `ground` rows of grass, in a grid x grid tile of sky."""
    img = Image.new("RGBA", (grid, grid), SKY)
    for x in range(0, grid, GRASS.width):
        img.alpha_composite(GRASS.crop((0, 0, GRASS.width, ground)), (x, grid - ground))
    left = (grid - MANGO.width) // 2
    img.alpha_composite(MANGO, (left, grid - ground - MANGO.height))
    if framed:
        # One-pixel black edge with the corners knocked out, like a game box.
        for i in range(grid):
            for x, y in ((i, 0), (i, grid - 1), (0, i), (grid - 1, i)):
                img.putpixel((x, y), EDGE)
        for x, y in ((0, 0), (grid - 1, 0), (0, grid - 1), (grid - 1, grid - 1)):
            img.putpixel((x, y), (0, 0, 0, 0))
    return img


def enlarge(img: Image.Image, scale: int) -> Image.Image:
    return img.resize((img.width * scale, img.height * scale), Image.Resampling.NEAREST)


def tiny() -> Image.Image:
    """16 px: no room for a scene, so Mango alone, one source pixel each."""
    img = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    img.alpha_composite(MANGO, (0, 16 - MANGO.height - 1))
    return img


def main() -> None:
    PUBLIC.mkdir(parents=True, exist_ok=True)
    tile32 = scene(32, 10, framed=True)
    icons = [
        tiny(),
        tile32,
        enlarge(scene(24, 7, framed=True), 2),   # 48
        enlarge(tile32, 2),                      # 64
        enlarge(tile32, 4),                      # 128
        enlarge(tile32, 8),                      # 256
    ]
    icons[-1].save(PUBLIC / "favicon.png", optimize=True)
    tile32.save(PUBLIC / "favicon-32x32.png", optimize=True)
    # iOS rounds the corners itself and fills transparency with black, so the
    # touch icon is a full-bleed scene: a 36 px grid at 5x is 180 px.
    enlarge(scene(36, 11, framed=False), 5).convert("RGB").save(PUBLIC / "apple-touch-icon.png", optimize=True)
    icons[-1].save(
        PUBLIC / "favicon.ico",
        sizes=[icon.size for icon in icons],
        append_images=icons[:-1],
    )
    print("generated", PUBLIC / "favicon.ico")


if __name__ == "__main__":
    main()
