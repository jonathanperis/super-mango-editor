#!/usr/bin/env python3
"""
gen_fire_sprites.py — Generate fire-themed background and foreground sprites.

Creates volcanic/fire-themed parallax background layers to replace the
cold/glacial originals for fire-themed levels. All shapes are preserved
from the originals — only colors are remapped to a warm fire palette.

Fire palette referenced from existing lava.png:
    #C42430, #EA323C, #FFA214, #FFC825

Usage:
    python3 .claude/scripts/gen_fire_sprites.py

Output (13 sprites):
    backgrounds/  sky_fire, sky_fire_lightened,
                  volcanic_mountains, volcanic_mountains_lightened,
                  smoke_bg, smoke_mg_1, smoke_mg_1_lightened,
                  smoke_mg_2, smoke_mg_3, smoke_lonely
    foregrounds/  fog_fire_1, fog_fire_2, smoke
"""

from PIL import Image
import os
import sys

# Resolve paths relative to the repo root (two levels up from this script)
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, "..", ".."))
BASE = os.path.join(REPO_ROOT, "assets", "sprites")
BG = os.path.join(BASE, "backgrounds")
FG = os.path.join(BASE, "foregrounds")


def remap_colors(src, dst, cmap):
    """Load a PNG, remap opaque pixel colors via RGB tuple lookup, save.

    Args:
        src:  path to source PNG
        dst:  path to output PNG
        cmap: dict mapping (R,G,B) → (R,G,B) for color replacement
    """
    img = Image.open(src).convert("RGBA")
    px = img.load()
    w, h = img.size
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            if a == 0:
                continue
            k = (r, g, b)
            if k in cmap:
                px[x, y] = cmap[k] + (a,)
    img.save(dst)
    print(f"  {os.path.basename(dst)} ({w}x{h})")


def main():
    print("=== Generating fire-themed sprites ===\n")

    # ----------------------------------------------------------------
    # 1. SKY — flat fill, single color
    # ----------------------------------------------------------------
    print("[Sky]")

    # sky_fire.png — deep fiery orange (replaces sky_blue #08A9FC)
    Image.new("RGBA", (384, 216), (204, 68, 0, 255)).save(
        os.path.join(BG, "sky_fire.png"))
    print("  sky_fire.png (384x216)")

    # sky_fire_lightened.png — brighter orange (replaces sky_blue_lightened #57B8FA)
    Image.new("RGBA", (384, 216), (221, 85, 17, 255)).save(
        os.path.join(BG, "sky_fire_lightened.png"))
    print("  sky_fire_lightened.png (384x216)")

    # ----------------------------------------------------------------
    # 2. VOLCANIC MOUNTAINS — pure color remap of glacial mountains
    #    Preserves every pixel position; only swaps the palette.
    # ----------------------------------------------------------------
    print("\n[Volcanic Mountains]")

    # glacial_mountains.png palette → fire palette
    #   #FFFFFF (snow)           → #FFC825 (lava yellow glow)
    #   #FFC599 (warm highlight) → #FFA214 (lava orange)
    #   #7BC0E7 (mid ice)        → #8B3A1A (warm mid rock)
    #   #5E83A6 (base tone 2)    → #4A1A08 (dark volcanic)
    #   #5B88A6 (base tone 1)    → #3D1608 (darkest volcanic)
    remap_colors(
        os.path.join(BG, "glacial_mountains.png"),
        os.path.join(BG, "volcanic_mountains.png"),
        {
            (255, 255, 255): (255, 200, 37),
            (255, 197, 153): (255, 162, 20),
            (123, 192, 231): (139, 58, 26),
            (94, 131, 166):  (74, 26, 8),
            (91, 136, 166):  (61, 22, 8),
        })

    # glacial_mountains_lightened.png palette → lighter fire palette
    #   #B5D9E9 (lightest) → #CC7832 (warm highlight)
    #   #79BBE1 (mid light) → #AA5528 (mid volcanic)
    #   #779FC6 (mid)       → #8B3A1A (warm rock)
    #   #5B88A6 (darkest)   → #6B2810 (dark volcanic)
    remap_colors(
        os.path.join(BG, "glacial_mountains_lightened.png"),
        os.path.join(BG, "volcanic_mountains_lightened.png"),
        {
            (181, 217, 233): (204, 120, 50),
            (121, 187, 225): (170, 85, 40),
            (119, 159, 198): (139, 58, 26),
            (91, 136, 166):  (107, 40, 16),
        })

    # ----------------------------------------------------------------
    # 3. SMOKE CLOUDS — color remap of blue cloud layers
    # ----------------------------------------------------------------
    print("\n[Smoke Clouds]")

    # smoke_bg.png (from clouds_bg.png)
    remap_colors(
        os.path.join(BG, "clouds_bg.png"),
        os.path.join(BG, "smoke_bg.png"),
        {
            (113, 193, 252): (107, 58, 32),
            (164, 213, 243): (139, 90, 56),
        })

    # smoke_mg_1.png (from clouds_mg_1.png)
    remap_colors(
        os.path.join(BG, "clouds_mg_1.png"),
        os.path.join(BG, "smoke_mg_1.png"),
        {
            (142, 202, 237): (123, 72, 40),
            (158, 225, 245): (139, 88, 48),
            (161, 225, 246): (142, 91, 50),
            (165, 227, 248): (145, 94, 53),
            (199, 240, 253): (170, 119, 72),
        })

    # smoke_mg_1_lightened.png (from clouds_mg_1_lightened.png)
    remap_colors(
        os.path.join(BG, "clouds_mg_1_lightened.png"),
        os.path.join(BG, "smoke_mg_1_lightened.png"),
        {
            (137, 194, 233): (139, 88, 48),
            (152, 219, 244): (155, 104, 60),
            (165, 227, 248): (170, 119, 72),
            (190, 238, 252): (190, 140, 88),
        })

    # smoke_mg_2.png (from clouds_mg_2.png)
    remap_colors(
        os.path.join(BG, "clouds_mg_2.png"),
        os.path.join(BG, "smoke_mg_2.png"),
        {
            (129, 204, 245): (123, 80, 48),
            (156, 218, 247): (165, 104, 64),
        })

    # smoke_mg_3.png (from clouds_mg_3.png)
    remap_colors(
        os.path.join(BG, "clouds_mg_3.png"),
        os.path.join(BG, "smoke_mg_3.png"),
        {
            (114, 173, 221): (123, 69, 37),
        })

    # smoke_lonely.png (from clouds_lonely.png)
    remap_colors(
        os.path.join(BG, "clouds_lonely.png"),
        os.path.join(BG, "smoke_lonely.png"),
        {
            (113, 193, 252): (107, 58, 32),
            (114, 173, 238): (96, 48, 24),
            (164, 213, 243): (139, 90, 56),
        })

    # ----------------------------------------------------------------
    # 4. FIRE FOG — warm glow replacing cold cyan
    # ----------------------------------------------------------------
    print("\n[Fire Fog]")

    # fog_fire_1.png (from fog_1.png)
    remap_colors(
        os.path.join(FG, "fog_1.png"),
        os.path.join(FG, "fog_fire_1.png"),
        {
            (148, 253, 255): (255, 136, 51),
        })

    # fog_fire_2.png (from fog_2.png)
    remap_colors(
        os.path.join(FG, "fog_2.png"),
        os.path.join(FG, "fog_fire_2.png"),
        {
            (0, 205, 249): (204, 68, 0),
        })

    # ----------------------------------------------------------------
    # 5. FOREGROUND SMOKE — dark ash puffs replacing white clouds
    # ----------------------------------------------------------------
    print("\n[Foreground Smoke]")

    # smoke.png (from foreground clouds.png)
    remap_colors(
        os.path.join(FG, "clouds.png"),
        os.path.join(FG, "smoke.png"),
        {
            (146, 161, 185): (90, 48, 32),
            (199, 207, 221): (123, 80, 64),
            (255, 255, 255): (155, 112, 96),
        })

    print("\n=== Done! 13 fire-themed sprites created ===")


if __name__ == "__main__":
    main()
