# Constants Reference

← [Home](Home)

---

A complete reference for every compile-time constant in the codebase.

---

## `game.h` Constants

These are available to every file that `#include "game.h"`.

### Window

| Constant | Value | Description |
|----------|-------|-------------|
| `WINDOW_TITLE` | `"Super Mango"` | Text shown in the OS title bar |
| `WINDOW_W` | `800` | OS window width in physical pixels |
| `WINDOW_H` | `600` | OS window height in physical pixels |

> **Do not use `WINDOW_W` / `WINDOW_H` for game object math.** All game objects live in logical space.

### Logical Canvas

| Constant | Value | Description |
|----------|-------|-------------|
| `GAME_W` | `400` | Internal canvas width in logical pixels |
| `GAME_H` | `300` | Internal canvas height in logical pixels |

`SDL_RenderSetLogicalSize(renderer, GAME_W, GAME_H)` makes SDL scale every draw call from 400×300 up to 800×600 automatically, producing a **2× pixel scale** (each logical pixel = 2×2 physical pixels).

### Timing

| Constant | Value | Description |
|----------|-------|-------------|
| `TARGET_FPS` | `60` | Desired frames per second |

Used to compute `frame_ms = 1000 / TARGET_FPS` (≈ 16 ms), which is the manual frame-cap duration when VSync is unavailable.

### Tiles & Floor

| Constant | Value | Expression | Description |
|----------|-------|-----------|-------------|
| `TILE_SIZE` | `48` | literal | Width and height of one grass tile (px) |
| `FLOOR_Y` | `252` | `GAME_H - TILE_SIZE` | Y coordinate of the floor's top edge |

The floor is drawn by repeating the 48×48 grass tile from `x=0` to `x=GAME_W` at `y=FLOOR_Y`.

### Physics

| Constant | Value | Type | Description |
|----------|-------|------|-------------|
| `GRAVITY` | `800.0f` | `float` | Downward acceleration in px/s² |

Every frame while airborne: `player->vy += GRAVITY * dt`.

At 60 FPS (`dt ≈ 0.016s`) gravity adds ~12.8 px/s per frame. The jump impulse (`-399.0f` px/s) produces an arc that peaks in ~0.5 s and lands in ~1 s.

---

## `player.c` Local Constants

These are `#define`s local to `player.c` (not visible to other files).

| Constant | Value | Description |
|----------|-------|-------------|
| `FRAME_W` | `48` | Width of one sprite frame in the sheet (px) |
| `FRAME_H` | `48` | Height of one sprite frame in the sheet (px) |
| `FLOOR_SINK` | `16` | Visual overlap onto the floor tile to prevent floating feet |

### Why `FLOOR_SINK`?

The `Player.png` sprite sheet has transparent padding at the **bottom** of each 48×48 frame. Without the sink offset, the physics floor edge (`y + h = FLOOR_Y`) would leave the character visually floating 16 px above the grass. `FLOOR_SINK` compensates:

```
floor_snap = FLOOR_Y - player->h + FLOOR_SINK
           = 252      - 48        + 16
           = 220
```

The character's sprite appears to rest naturally on the grass at that Y.

---

## Animation Tables in `player.c`

Static arrays indexed by `AnimState` (0 = `ANIM_IDLE`, 1 = `ANIM_WALK`, 2 = `ANIM_JUMP`, 3 = `ANIM_FALL`):

```c
static const int ANIM_FRAME_COUNT[4] = { 4,   4,   2,   1   };
static const int ANIM_FRAME_MS[4]    = { 150, 100, 150, 200 };
static const int ANIM_ROW[4]         = { 0,   1,   2,   3   };
```

| Index | State | Frames | ms/frame | Sheet row |
|-------|-------|--------|----------|-----------|
| 0 | `ANIM_IDLE` | 4 | 150 | Row 0 |
| 1 | `ANIM_WALK` | 4 | 100 | Row 1 |
| 2 | `ANIM_JUMP` | 2 | 150 | Row 2 |
| 3 | `ANIM_FALL` | 1 | 200 | Row 3 |

---

## Hard-Coded Values in `player.c`

| Value | Location | Description |
|-------|----------|-------------|
| `160.0f` | `player_init` | `player->speed` — horizontal max speed (px/s) |
| `-399.0f` | `player_handle_input` | Jump vertical impulse (upward, px/s) |

---

## Audio Constants in `main.c`

| Value | Description |
|-------|-------------|
| `44100` | Audio sample rate (Hz) |
| `MIX_DEFAULT_FORMAT` | 16-bit signed samples |
| `2` | Stereo channels |
| `2048` | Mixer buffer size (samples) |
| `13` | Music volume (0–128, ≈ 10%) set in `game_init` |

---

## Derived Values Quick Reference

| Expression | Result | Meaning |
|------------|--------|---------|
| `GAME_W / WINDOW_W` | `2×` | Pixel scale factor |
| `GAME_H / WINDOW_H` | `2×` | Pixel scale factor |
| `1000 / TARGET_FPS` | `≈16 ms` | Frame budget |
| `GAME_H - TILE_SIZE` | `252` | `FLOOR_Y` |
| `FLOOR_Y - FRAME_H + FLOOR_SINK` | `220` | Player start / floor snap Y |
| `GAME_W / TILE_SIZE` | `≈8.3` | Tiles needed to fill the floor |
| `WATER_FRAMES × WATER_ART_W` | `128` | `WATER_PERIOD` — seamless repeat distance |

---

## `platform.h` Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `MAX_PLATFORMS` | `8` | Maximum number of platforms in the game |

---

## `water.h` Constants

| Constant | Value | Type | Description |
|----------|-------|------|-------------|
| `WATER_FRAMES` | `8` | `int` | Total animation frames in `Water.png` |
| `WATER_FRAME_W` | `48` | `int` | Full slot width per frame in the sheet (px) |
| `WATER_ART_DX` | `16` | `int` | Left offset to visible art within each slot |
| `WATER_ART_W` | `16` | `int` | Width of actual art pixels per frame |
| `WATER_ART_Y` | `17` | `int` | First visible row within each frame |
| `WATER_ART_H` | `31` | `int` | Height of visible art pixels |
| `WATER_PERIOD` | `128` | `int` | Pattern repeat distance: `WATER_FRAMES × WATER_ART_W` |
| `WATER_SCROLL_SPEED` | `40.0f` | `float` | Rightward scroll speed (px/s) |

---

## `spider.h` Constants

| Constant | Value | Type | Description |
|----------|-------|------|-------------|
| `MAX_SPIDERS` | `4` | `int` | Maximum simultaneous spider enemies |
| `SPIDER_FRAMES` | `4` | `int` | Animation frames in `Spider_1.png` |
| `SPIDER_FRAME_W` | `48` | `int` | Width of one frame slot in the sheet (px) |
| `SPIDER_ART_Y` | `22` | `int` | First visible row within each frame slot |
| `SPIDER_ART_H` | `10` | `int` | Height of visible art (rows 22–31) |
| `SPIDER_SPEED` | `50.0f` | `float` | Walk speed (px/s) |
| `SPIDER_FRAME_MS` | `150` | `int` | Milliseconds each animation frame is held |

---

## `fog.h` Constants

| Constant | Value | Type | Description |
|----------|-------|------|-------------|
| `FOG_TEX_COUNT` | `2` | `int` | Number of fog texture assets in rotation |
| `FOG_MAX` | `4` | `int` | Maximum concurrent fog instances |
