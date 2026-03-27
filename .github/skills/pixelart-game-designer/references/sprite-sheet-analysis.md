# Sprite Sheet Analysis Guide

## Measuring a Sheet Manually

1. Open the PNG in any image editor (Preview, GIMP, Aseprite).
2. Read the total pixel dimensions: **width × height**.
3. Identify the frame size by looking for repeating patterns or transparent gutters.
4. Divide: `cols = width / frame_w`, `rows = height / frame_h`.
5. Verify: `cols × frame_w == width` and `rows × frame_h == height` (no remainder).

## Using Python / PIL

```python
from PIL import Image

img = Image.open("assets/Player.png")
w, h = img.size
print(f"Sheet: {w}×{h}")

# If frame size is known (e.g. 48×48):
fw, fh = 48, 48
cols, rows = w // fw, h // fh
print(f"Grid: {cols} cols × {rows} rows = {cols * rows} frames")
```

## Detecting Frame Size Automatically

When frame size is unknown, look for **fully transparent rows and columns** that act as borders, or sample pixel columns to find repeating alpha patterns.

The [analyze_sprite.py](../scripts/analyze_sprite.py) script does this automatically.

## Reading the Super Mango Asset Pack

The **Super Mango 2D Pixel Art Platformer Asset Pack** (by Juho) uses **48×48 px frames**.

| Asset              | Sheet Size | Grid          | Total Frames |
|--------------------|-----------|---------------|--------------|
| `Player.png`       | 192×288   | 4 cols × 6 rows | 24          |
| `Grass_Tileset.png`| 48×48     | 1×1 (single tile) | 1         |
| `Brick_Tileset.png`| 48×48     | 1×1             | 1           |
| `Coin.png`         | varies    | measure at runtime | —         |
| `Bird_1.png`       | varies    | measure at runtime | —         |

> Always re-measure when in doubt. Use the analysis script.

## SDL_Rect Source Coordinate Formula

```c
// Given: frame index (0-based), cols, FRAME_W, FRAME_H
int col = frame_index % cols;
int row = frame_index / cols;

SDL_Rect src = {
    col * FRAME_W,   // x
    row * FRAME_H,   // y
    FRAME_W,         // w
    FRAME_H          // h
};
```

## Common Pitfalls

- **Off-by-one**: frame indices are 0-based; row 0 is the top row.
- **Pixel padding**: some packs add 1–2 px transparent gutters between frames — account for this in the source rect offset.
- **Non-uniform grids**: some sheets mix frame sizes per row (e.g., walk=8 frames wide, idle=4 frames wide). Measure each row separately.
- **Premultiplied alpha**: if colors look washed out, the PNG uses premultiplied alpha — use `SDL_BLENDMODE_NONE` or re-export without premultiplication.
