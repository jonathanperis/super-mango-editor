---
name: pixelart-gamer-designer
description: "Pixel art game design expert. Use when: analyzing sprite sheets, cropping frames from PNG assets, understanding animation sequences from image grids, mapping frame rows/cols to game states (idle/walk/jump/attack), designing tile layouts, working with pixel art conventions (palette, scaling, tile sizes). Trigger phrases: 'analyze sprite', 'crop frames', 'animation frames', 'sprite sheet', 'pixel art', 'which row is idle', 'how many frames', 'tileset layout', 'frame index', 'SDL_Rect source'."
argument-hint: "Describe the asset or animation question (e.g. 'analyze Player.png', 'map walk animation frames')"
---

# Pixel Art Game Designer

Expert in pixel art sprite sheets, animation frame extraction, and 2D game visual design.

## When to Use

- Analyzing a PNG asset to determine grid dimensions, frame size, and frame count
- Mapping animation sequences: which row/column maps to idle, walk, jump, attack, death
- Generating `SDL_Rect` source coords for specific frames in C/SDL2 code
- Designing floor/tile layouts from tilesets
- Understanding pixel art conventions: palette limits, integer scaling, tile sizes
- Deciding which frames to show for a given game state or direction

## Core Knowledge

### Sprite Sheet Anatomy

A sprite sheet packs multiple animation frames into a single PNG to minimize GPU texture switches.

```
Sheet width  = cols × frame_w
Sheet height = rows × frame_h

frame_col = frame_index % cols
frame_row = frame_index / cols

source_x = frame_col * frame_w
source_y = frame_row * frame_h
```

### Standard Row Layout (most pixel art packs)

| Row | Animation    | Notes                           |
|-----|-------------|----------------------------------|
| 0   | Idle        | 1–4 frames, subtle breathing    |
| 1   | Walk / Run  | 6–8 frames, looping             |
| 2   | Jump (up)   | 2–4 frames, one-shot            |
| 3   | Fall / Land | 2–4 frames                      |
| 4   | Attack      | 4–8 frames, one-shot            |
| 5   | Death / Hurt| 4–6 frames, one-shot            |

> Row order varies by pack. Always measure the sheet and cross-reference with the asset's documentation or README.

### Common Tile and Frame Sizes

| Size   | Common Use                   |
|--------|------------------------------|
| 8×8    | Classic NES-era, very chunky |
| 16×16  | GBA/SNES era, most pixel art packs |
| 32×32  | Modern indie standard        |
| 48×48  | Larger characters with detail |
| 64×64  | Boss sprites, detailed characters |

### Integer Scaling (critical for pixel art)

Always render at whole-number multiples to prevent sub-pixel blur:
- 1× → native (tiny on HD displays)
- 2× → most common for 16×16 / 48×48 assets
- 3× or 4× → for very small source assets (8×8)

Use `SDL_RenderSetLogicalSize(renderer, GAME_W, GAME_H)` to handle scaling automatically.

## Procedure

### Analyzing a New Sprite Sheet

1. Run [analyze_sprite.py](./scripts/analyze_sprite.py) on the PNG:
   ```sh
   python3 .agents/skills/pixelart-gamer-designer/scripts/analyze_sprite.py assets/Player.png
   ```
2. The script outputs: sheet size, detected frame size, grid dimensions, alpha column/row map.
3. Cross-reference with [sprite-sheet-analysis.md](./references/sprite-sheet-analysis.md) to interpret the grid.

### Mapping Animations to Code (SDL2 / C)

1. Identify frame size (`FRAME_W`, `FRAME_H`) from analysis.
2. Determine which row = which animation state — see [animation-sequences.md](./references/animation-sequences.md).
3. In C, set the source rect:
   ```c
   // Row 1, frame 3 of a 48×48 sheet
   player->frame.x = 3 * FRAME_W;   /* column */
   player->frame.y = 1 * FRAME_H;   /* row    */
   player->frame.w = FRAME_W;
   player->frame.h = FRAME_H;
   ```
4. Advance `frame.x` by `FRAME_W` each N milliseconds to animate.

### Tiling a Floor / Background

1. Measure the tile PNG (e.g., `Grass_Tileset.png` = 48×48).
2. Repeat across the logical canvas width:
   ```c
   for (int tx = 0; tx < GAME_W; tx += TILE_SIZE) {
       SDL_Rect dst = {tx, FLOOR_Y, TILE_SIZE, TILE_SIZE};
       SDL_RenderCopy(renderer, floor_tile, NULL, &dst);
   }
   ```
3. For multi-row tilesets (16×16 packs), select the correct source tile by its row/col within the tileset.

## References

- [Sprite Sheet Analysis Guide](./references/sprite-sheet-analysis.md)
- [Animation Sequence Conventions](./references/animation-sequences.md)
- [Analysis Script](./scripts/analyze_sprite.py)
