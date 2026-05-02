# Constants Reference

[← Home](index.md)

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

`SDL_RenderSetLogicalSize(renderer, GAME_W, GAME_H)` makes SDL scale every draw call from 400x300 up to 800x600 automatically, producing a **2x pixel scale** (each logical pixel = 2x2 physical pixels).

### Timing

| Constant | Value | Description |
|----------|-------|-------------|
| `TARGET_FPS` | `60` | Desired frames per second |

Used to compute `frame_ms = 1000 / TARGET_FPS` (approximately 16 ms), which is the manual frame-cap duration when VSync is unavailable.

### Tiles and Floor

| Constant | Value | Expression | Description |
|----------|-------|-----------|-------------|
| `TILE_SIZE` | `48` | literal | Width and height of one grass tile (px) |
| `FLOOR_Y` | `252` | `GAME_H - TILE_SIZE` | Y coordinate of the floor's top edge |

The floor is drawn by repeating the 48x48 grass tile across the full `WORLD_W` at `y=FLOOR_Y`, with gaps cut out at each `sea_gaps[]` position.

### Physics

| Constant | Value | Type | Description |
|----------|-------|------|-------------|
| `GRAVITY` | `800.0f` | `float` | Downward acceleration in px/s^2 |
| `SEA_GAP_W` | `32` | `int` | Width of each sea gap in logical pixels |
| `MAX_SEA_GAPS` | `8` | `int` | Maximum number of sea gaps per level |

Every frame while airborne: `player->vy += GRAVITY * dt`.

At 60 FPS (`dt` approximately 0.016s) gravity adds ~12.8 px/s per frame. The jump impulse (`-325.0f` px/s) produces a moderate arc.

### Camera

| Constant | Value | Type | Description |
|----------|-------|------|-------------|
| `WORLD_W` | `1600` | `int` | Total logical level width (4 x GAME_W) |
| `CAM_LOOKAHEAD` | `40` | `int` | Forward-look offset in px added in the player's facing direction |
| `CAM_SMOOTHING` | `8.0f` | `float` | Lerp speed factor (per second); higher = snappier follow |
| `CAM_SNAP_THRESHOLD` | `0.5f` | `float` | Sub-pixel distance at which the camera snaps exactly to target |

`WORLD_W` defines the full scrollable level width. The visible canvas is always `GAME_W` (400 px); the `Camera` struct tracks the left edge of the viewport in world coordinates.

---

## `player.c` Local Constants

These are `#define`s local to `player.c` (not visible to other files).

| Constant | Value | Description |
|----------|-------|-------------|
| `FRAME_W` | `48` | Width of one sprite frame in the sheet (px) |
| `FRAME_H` | `48` | Height of one sprite frame in the sheet (px) |
| `FLOOR_SINK` | `16` | Visual overlap onto the floor tile to prevent floating feet |
| `PHYS_PAD_X` | `15` | Pixels trimmed from each horizontal side of the frame for the physics box (physics width = 48 - 30 = 18 px) |
| `PHYS_PAD_TOP` | `18` | Pixels trimmed from the top of the frame for the physics box (physics height = 48 - 18 - 16 = 14; combined with FLOOR_SINK gives a 30 px tall box) |

### Why `FLOOR_SINK`?

The `player.png` sprite sheet has transparent padding at the **bottom** of each 48x48 frame. Without the sink offset, the physics floor edge (`y + h = FLOOR_Y`) would leave the character visually floating 16 px above the grass. `FLOOR_SINK` compensates:

```
floor_snap = FLOOR_Y - player->h + FLOOR_SINK
           = 252      - 48        + 16
           = 220
```

The character's sprite appears to rest naturally on the grass at that Y.

---

## Animation Tables in `player.c`

Static arrays indexed by `AnimState` (0 = `ANIM_IDLE`, 1 = `ANIM_WALK`, 2 = `ANIM_JUMP`, 3 = `ANIM_FALL`, 4 = `ANIM_CLIMB`):

```c
static const int ANIM_FRAME_COUNT[5] = { 4,   4,   2,   1,   2   };
static const int ANIM_FRAME_MS[5]    = { 150, 100, 150, 200, 100 };
static const int ANIM_ROW[5]         = { 0,   1,   2,   3,   4   };
```

| Index | State | Frames | ms/frame | Sheet row |
|-------|-------|--------|----------|-----------|
| 0 | `ANIM_IDLE` | 4 | 150 | Row 0 |
| 1 | `ANIM_WALK` | 4 | 100 | Row 1 |
| 2 | `ANIM_JUMP` | 2 | 150 | Row 2 |
| 3 | `ANIM_FALL` | 1 | 200 | Row 3 |
| 4 | `ANIM_CLIMB` | 2 | 100 | Row 4 |

---

## Hard-Coded Values in `player.c`

| Value | Location | Description |
|-------|----------|-------------|
| `160.0f` | `player_init` | `player->speed` -- horizontal max speed (px/s) |
| `-325.0f` | `player_handle_input` | Jump vertical impulse -- keyboard (ground + vine dismount) |
| `-500.0f` | `player_handle_input` | Jump vertical impulse -- gamepad (ground + vine dismount) |

## Vine Climbing Constants in `player.c`

| Constant | Value | Type | Description |
|----------|-------|------|-------------|
| `CLIMB_SPEED` | `80.0f` | `float` | Vertical climbing speed on vines (px/s) |
| `CLIMB_H_SPEED` | `80.0f` | `float` | Horizontal drift speed while on vine (px/s) |
| `VINE_GRAB_PAD` | `4` | `int` | Extra pixels on each side of vine sprite that count as the grab zone (total grab width = VINE_W + 2 x 4 = 24 px) |

---

## Audio Constants in `main.c`

| Value | Description |
|-------|-------------|
| `44100` | Audio sample rate (Hz) |
| `MIX_DEFAULT_FORMAT` | 16-bit signed samples |
| `2` | Stereo channels |
| `2048` | Mixer buffer size (samples) |
| `13` | Music volume (0-128, approximately 10%) set in `game_init` |

---

## Derived Values Quick Reference

| Expression | Result | Meaning |
|------------|--------|---------|
| `GAME_W / WINDOW_W` | `2x` | Pixel scale factor |
| `GAME_H / WINDOW_H` | `2x` | Pixel scale factor |
| `1000 / TARGET_FPS` | `~16 ms` | Frame budget |
| `GAME_H - TILE_SIZE` | `252` | `FLOOR_Y` |
| `FLOOR_Y - FRAME_H + FLOOR_SINK` | `220` | Player start / floor snap Y |
| `GAME_W / TILE_SIZE` | `~8.3` | Tiles needed to fill the floor |
| `WATER_FRAMES x WATER_ART_W` | `128` | `WATER_PERIOD` -- seamless repeat distance |

---

## `platform.h` Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `MAX_PLATFORMS` | `8` | Maximum number of platforms in the game |

---

## `water.h` Constants

| Constant | Value | Type | Description |
|----------|-------|------|-------------|
| `WATER_FRAMES` | `8` | `int` | Total animation frames in `water.png` |
| `WATER_FRAME_W` | `48` | `int` | Full slot width per frame in the sheet (px) |
| `WATER_ART_DX` | `16` | `int` | Left offset to visible art within each slot |
| `WATER_ART_W` | `16` | `int` | Width of actual art pixels per frame |
| `WATER_ART_Y` | `17` | `int` | First visible row within each frame |
| `WATER_ART_H` | `31` | `int` | Height of visible art pixels |
| `WATER_PERIOD` | `128` | `int` | Pattern repeat distance: `WATER_FRAMES x WATER_ART_W` |
| `WATER_SCROLL_SPEED` | `40.0f` | `float` | Rightward scroll speed (px/s) |

---

## `spider.h` Constants

| Constant | Value | Type | Description |
|----------|-------|------|-------------|
| `MAX_SPIDERS` | `4` | `int` | Maximum simultaneous spider enemies |
| `SPIDER_FRAMES` | `3` | `int` | Animation frames in `spider.png` (192/64 = 3) |
| `SPIDER_FRAME_W` | `64` | `int` | Width of one frame slot in the sheet (px) |
| `SPIDER_ART_X` | `20` | `int` | First visible col within each frame slot |
| `SPIDER_ART_W` | `25` | `int` | Width of visible art (cols 20-44) |
| `SPIDER_ART_Y` | `22` | `int` | First visible row within each frame slot |
| `SPIDER_ART_H` | `10` | `int` | Height of visible art (rows 22-31) |
| `SPIDER_SPEED` | `50.0f` | `float` | Walk speed (px/s) |
| `SPIDER_FRAME_MS` | `150` | `int` | Milliseconds each animation frame is held |

---

## `fog.h` Constants

| Constant | Value | Type | Description |
|----------|-------|------|-------------|
| `FOG_TEX_COUNT` | `2` | `int` | Number of fog texture assets in rotation |
| `FOG_MAX` | `4` | `int` | Maximum concurrent fog instances |

---

## `parallax.h` Constants

| Constant | Value | Type | Description |
|----------|-------|------|-------------|
| `PARALLAX_MAX_LAYERS` | `8` | `int` | Maximum number of background layers the system can hold |

---

## `coin.h` Constants

| Constant | Value | Type | Description |
|----------|-------|------|-------------|
| `MAX_COINS` | `24` | `int` | Maximum simultaneous coins on screen |
| `COIN_DISPLAY_W` | `16` | `int` | Render width in logical pixels |
| `COIN_DISPLAY_H` | `16` | `int` | Render height in logical pixels |
| `COIN_SCORE` | `100` | `int` | Score awarded per coin collected |
| `SCORE_PER_LIFE` | `1000` | `int` | Score multiple that grants a bonus life |

---

## `vine.h` Constants

| Constant | Value | Type | Description |
|----------|-------|------|-------------|
| `MAX_VINES` | `24` | `int` | Maximum number of vine instances |
| `VINE_W` | `16` | `int` | Sprite width in logical pixels |
| `VINE_H` | `32` | `int` | Content height after removing transparent padding |
| `VINE_SRC_Y` | `8` | `int` | First pixel row with content in vine.png |
| `VINE_SRC_H` | `32` | `int` | Height of content area in vine.png |
| `VINE_STEP` | `19` | `int` | Vertical spacing between stacked tiles (px) |

---

## `fish.h` Constants

| Constant | Value | Type | Description |
|----------|-------|------|-------------|
| `MAX_FISH` | `4` | `int` | Maximum simultaneous fish instances |
| `FISH_FRAMES` | `2` | `int` | Horizontal frames in `fish.png` (96x48 sheet) |
| `FISH_FRAME_W` | `48` | `int` | Width of one frame slot in the sheet (px) |
| `FISH_FRAME_H` | `48` | `int` | Height of one frame slot in the sheet (px) |
| `FISH_RENDER_W` | `48` | `int` | On-screen render width in logical pixels |
| `FISH_RENDER_H` | `48` | `int` | On-screen render height in logical pixels |
| `FISH_SPEED` | `70.0f` | `float` | Horizontal patrol speed (px/s) |
| `FISH_JUMP_VY` | `-280.0f` | `float` | Upward jump impulse (px/s) |
| `FISH_JUMP_MIN` | `1.4f` | `float` | Minimum seconds before next jump |
| `FISH_JUMP_MAX` | `3.0f` | `float` | Maximum seconds before next jump |
| `FISH_HITBOX_PAD_X` | `16` | `int` | Horizontal inset for fair AABB collision (hitbox width = 16 px) |
| `FISH_HITBOX_PAD_Y` | `13` | `int` | Vertical inset for fair AABB collision (hitbox height = 19 px) |
| `FISH_FRAME_MS` | `120` | `int` | Milliseconds per swim animation frame |

---

## `hud.h` Constants

| Constant | Value | Type | Description |
|----------|-------|------|-------------|
| `MAX_HEARTS` | `3` | `int` | Maximum hearts the player can have |
| `DEFAULT_LIVES` | `3` | `int` | Lives the player starts with |
| `HUD_MARGIN` | `4` | `int` | Pixel margin from screen edges |
| `HUD_HEART_SIZE` | `16` | `int` | Display size of each heart icon (px) |
| `HUD_HEART_GAP` | `2` | `int` | Horizontal gap between heart icons (px) |
| `HUD_ICON_W` | `16` | `int` | Display width of the player icon (px) |
| `HUD_ICON_H` | `13` | `int` | Display height of the player icon (px) |
| `HUD_ROW_H` | `16` | `int` | Row height for text alignment (font px) |
| `HUD_COIN_ICON_SIZE` | `12` | `int` | Display size of the coin count icon (px) |

---

## `bouncepad.h` Constants

| Constant | Value | Type | Description |
|----------|-------|------|-------------|
| `MAX_BOUNCEPADS` | `4` | `int` | Maximum simultaneous bouncepad instances |
| `BOUNCEPAD_W` | `48` | `int` | Display width of one bouncepad frame (px) |
| `BOUNCEPAD_H` | `48` | `int` | Display height of one bouncepad frame (px) |
| `BOUNCEPAD_VY_SMALL` | `-380.0f` | `float` | Small bouncepad launch impulse (px/s) |
| `BOUNCEPAD_VY_MEDIUM` | `-536.25f` | `float` | Medium bouncepad launch impulse (px/s) |
| `BOUNCEPAD_VY_HIGH` | `-700.0f` | `float` | High bouncepad launch impulse (px/s) |
| `BOUNCEPAD_VY` | `-536.25f` | `float` | Default launch impulse (alias for `BOUNCEPAD_VY_MEDIUM`) |
| `BOUNCEPAD_FRAME_MS` | `80` | `int` | Milliseconds per animation frame during release |
| `BOUNCEPAD_SRC_Y` | `14` | `int` | First non-transparent row in the frame |
| `BOUNCEPAD_SRC_H` | `18` | `int` | Height of the art region (rows 14-31) |
| `BOUNCEPAD_ART_X` | `16` | `int` | First non-transparent col in the frame |
| `BOUNCEPAD_ART_W` | `16` | `int` | Width of the art region (cols 16-31) |

---

## `rail.h` Constants

| Constant | Value | Type | Description |
|----------|-------|------|-------------|
| `RAIL_N` | `1 << 0` | bitmask | Tile opens upward |
| `RAIL_E` | `1 << 1` | bitmask | Tile opens rightward |
| `RAIL_S` | `1 << 2` | bitmask | Tile opens downward |
| `RAIL_W` | `1 << 3` | bitmask | Tile opens leftward |
| `RAIL_TILE_W` | `16` | `int` | Width of one tile in the sprite sheet (px) |
| `RAIL_TILE_H` | `16` | `int` | Height of one tile in the sprite sheet (px) |
| `MAX_RAIL_TILES` | `128` | `int` | Maximum tiles in a single Rail path |
| `MAX_RAILS` | `4` | `int` | Maximum Rail instances per level |

---

## `spike_block.h` Constants

| Constant | Value | Type | Description |
|----------|-------|------|-------------|
| `SPIKE_DISPLAY_W` | `24` | `int` | On-screen width in logical pixels (16x16 scaled up) |
| `SPIKE_DISPLAY_H` | `24` | `int` | On-screen height in logical pixels |
| `SPIKE_SPIN_DEG_PER_SEC` | `360.0f` | `float` | Rotation speed -- one full turn per second |
| `SPIKE_SPEED_SLOW` | `1.5f` | `float` | Rail traversal: 1.5 tiles/s |
| `SPIKE_SPEED_NORMAL` | `3.0f` | `float` | Rail traversal: 3.0 tiles/s |
| `SPIKE_SPEED_FAST` | `6.0f` | `float` | Rail traversal: 6.0 tiles/s |
| `SPIKE_PUSH_SPEED` | `220.0f` | `float` | Horizontal push impulse magnitude (px/s) |
| `SPIKE_PUSH_VY` | `-150.0f` | `float` | Upward push component on collision (px/s) |
| `MAX_SPIKE_BLOCKS` | `4` | `int` | Maximum spike block instances per level |

---

## `debug.h` Constants

| Constant | Value | Type | Description |
|----------|-------|------|-------------|
| `DEBUG_LOG_MAX_ENTRIES` | `8` | `int` | Maximum visible log messages |
| `DEBUG_LOG_MSG_LEN` | `64` | `int` | Max characters per log message (incl. null) |
| `DEBUG_LOG_DISPLAY_SEC` | `4.0f` | `float` | Seconds each log entry stays visible |
| `DEBUG_FPS_SAMPLE_MS` | `500` | `int` | Milliseconds between FPS counter refreshes |

---

## `jumping_spider.h` Constants

| Constant | Value | Type | Description |
|----------|-------|------|-------------|
| `MAX_JUMPING_SPIDERS` | `4` | `int` | Maximum simultaneous jumping spider instances |
| `JSPIDER_FRAMES` | `3` | `int` | Animation frames in `jumping_spider.png` (192/64 = 3) |
| `JSPIDER_FRAME_W` | `64` | `int` | Width of one frame slot in the sheet (px) |
| `JSPIDER_ART_X` | `20` | `int` | First visible col within each frame |
| `JSPIDER_ART_W` | `25` | `int` | Width of visible art (cols 20-44) |
| `JSPIDER_ART_Y` | `22` | `int` | First visible row within each frame |
| `JSPIDER_ART_H` | `10` | `int` | Height of visible art (rows 22-31) |
| `JSPIDER_SPEED` | `55.0f` | `float` | Walk speed (px/s) |
| `JSPIDER_FRAME_MS` | `150` | `int` | Milliseconds per animation frame |
| `JSPIDER_JUMP_VY` | `-200.0f` | `float` | Upward jump impulse (px/s) |
| `JSPIDER_GRAVITY` | `600.0f` | `float` | Downward acceleration while airborne (px/s^2) |

---

## `bird.h` Constants

| Constant | Value | Type | Description |
|----------|-------|------|-------------|
| `MAX_BIRDS` | `4` | `int` | Maximum simultaneous bird instances |
| `BIRD_FRAMES` | `3` | `int` | Animation frames in `bird.png` (144/48 = 3) |
| `BIRD_FRAME_W` | `48` | `int` | Width of one frame slot in the sheet (px) |
| `BIRD_ART_X` | `17` | `int` | First visible col within each frame |
| `BIRD_ART_W` | `15` | `int` | Width of visible art (cols 17-31) |
| `BIRD_ART_Y` | `17` | `int` | First visible row within each frame |
| `BIRD_ART_H` | `14` | `int` | Height of visible art (rows 17-30) |
| `BIRD_SPEED` | `45.0f` | `float` | Horizontal flight speed (px/s) |
| `BIRD_FRAME_MS` | `140` | `int` | Milliseconds per wing animation frame |
| `BIRD_WAVE_AMP` | `20.0f` | `float` | Sine-wave amplitude in logical pixels |
| `BIRD_WAVE_FREQ` | `0.015f` | `float` | Sine cycles per pixel of horizontal travel |

---

## `faster_bird.h` Constants

| Constant | Value | Type | Description |
|----------|-------|------|-------------|
| `MAX_FASTER_BIRDS` | `4` | `int` | Maximum simultaneous faster bird instances |
| `FBIRD_FRAMES` | `3` | `int` | Animation frames in `faster_bird.png` (144/48 = 3) |
| `FBIRD_FRAME_W` | `48` | `int` | Width of one frame slot in the sheet (px) |
| `FBIRD_ART_X` | `17` | `int` | First visible col within each frame |
| `FBIRD_ART_W` | `15` | `int` | Width of visible art (cols 17-31) |
| `FBIRD_ART_Y` | `17` | `int` | First visible row within each frame |
| `FBIRD_ART_H` | `14` | `int` | Height of visible art (rows 17-30) |
| `FBIRD_SPEED` | `80.0f` | `float` | Horizontal speed -- nearly 2x the slow bird |
| `FBIRD_FRAME_MS` | `90` | `int` | Faster wing animation (ms per frame) |
| `FBIRD_WAVE_AMP` | `15.0f` | `float` | Tighter sine-wave amplitude (px) |
| `FBIRD_WAVE_FREQ` | `0.025f` | `float` | Higher frequency -- more erratic curves |

---

## `float_platform.h` Constants

| Constant | Value | Type | Description |
|----------|-------|------|-------------|
| `FLOAT_PLATFORM_PIECE_W` | `16` | `int` | Width of each 3-slice piece (px) |
| `FLOAT_PLATFORM_H` | `16` | `int` | Height of the platform sprite (px) |
| `MAX_FLOAT_PLATFORMS` | `6` | `int` | Maximum float platform instances per level |
| `CRUMBLE_STAND_LIMIT` | `0.75f` | `float` | Seconds of standing before crumble-fall starts |
| `CRUMBLE_FALL_GRAVITY` | `250.0f` | `float` | Downward acceleration during crumble fall (px/s^2) |
| `CRUMBLE_FALL_INITIAL_VY` | `20.0f` | `float` | Initial downward velocity on crumble-start (px/s) |

---

## `bridge.h` Constants

| Constant | Value | Type | Description |
|----------|-------|------|-------------|
| `MAX_BRIDGES` | `2` | `int` | Maximum bridge instances per level |
| `MAX_BRIDGE_BRICKS` | `16` | `int` | Maximum bricks in a single bridge |
| `BRIDGE_TILE_W` | `16` | `int` | Width of one bridge.png tile (px) |
| `BRIDGE_TILE_H` | `16` | `int` | Height of one bridge.png tile (px) |
| `BRIDGE_FALL_DELAY` | `0.1f` | `float` | Seconds between touch and first brick falling |
| `BRIDGE_CASCADE_DELAY` | `0.06f` | `float` | Extra seconds between successive bricks cascading outward |
| `BRIDGE_FALL_GRAVITY` | `250.0f` | `float` | Downward acceleration per brick during fall (px/s^2) |
| `BRIDGE_FALL_INITIAL_VY` | `20.0f` | `float` | Initial downward velocity on fall-start (px/s) |

---

## `yellow_star.h` Constants

| Constant | Value | Type | Description |
|----------|-------|------|-------------|
| `MAX_YELLOW_STARS` | `3` | `int` | Maximum yellow star instances per level |
| `YELLOW_STAR_DISPLAY_W` | `16` | `int` | Display width (logical px) |
| `YELLOW_STAR_DISPLAY_H` | `16` | `int` | Display height (logical px) |

---

## `last_star.h` Constants

| Constant | Value | Type | Description |
|----------|-------|------|-------------|
| `LAST_STAR_DISPLAY_W` | `24` | `int` | Display width (logical px) |
| `LAST_STAR_DISPLAY_H` | `24` | `int` | Display height (logical px) |

---

## `axe_trap.h` Constants

| Constant | Value | Type | Description |
|----------|-------|------|-------------|
| `AXE_FRAME_W` | `48` | `int` | Source sprite width (px) |
| `AXE_FRAME_H` | `64` | `int` | Source sprite height (px) |
| `AXE_DISPLAY_W` | `48` | `int` | On-screen display width (logical px) |
| `AXE_DISPLAY_H` | `64` | `int` | On-screen display height (logical px) |
| `AXE_SWING_AMPLITUDE` | `60.0f` | `float` | Maximum pendulum angle from vertical (degrees) |
| `AXE_SWING_PERIOD` | `2.0f` | `float` | Time for one full pendulum cycle (s) |
| `AXE_SPIN_SPEED` | `180.0f` | `float` | Rotation speed for spin variant (degrees/s) |
| `MAX_AXE_TRAPS` | `4` | `int` | Maximum axe trap instances per level |

---

## `circular_saw.h` Constants

| Constant | Value | Type | Description |
|----------|-------|------|-------------|
| `SAW_FRAME_W` | `32` | `int` | Source sprite width (px) |
| `SAW_FRAME_H` | `32` | `int` | Source sprite height (px) |
| `SAW_DISPLAY_W` | `32` | `int` | On-screen display width (logical px) |
| `SAW_DISPLAY_H` | `32` | `int` | On-screen display height (logical px) |
| `SAW_SPIN_DEG_PER_SEC` | `720.0f` | `float` | Rotation speed (degrees/s) |
| `SAW_PATROL_SPEED` | `180.0f` | `float` | Horizontal patrol speed (px/s) |
| `SAW_PUSH_SPEED` | `220.0f` | `float` | Push impulse magnitude (px/s) |
| `SAW_PUSH_VY` | `-150.0f` | `float` | Upward bounce component on collision (px/s) |
| `MAX_CIRCULAR_SAWS` | `4` | `int` | Maximum circular saw instances per level |

---

## `blue_flame.h` Constants

| Constant | Value | Type | Description |
|----------|-------|------|-------------|
| `BLUE_FLAME_FRAME_W` | `48` | `int` | Animation frame width (px) |
| `BLUE_FLAME_FRAME_H` | `48` | `int` | Animation frame height (px) |
| `BLUE_FLAME_DISPLAY_W` | `48` | `int` | On-screen display width (logical px) |
| `BLUE_FLAME_DISPLAY_H` | `48` | `int` | On-screen display height (logical px) |
| `BLUE_FLAME_FRAME_COUNT` | `2` | `int` | Number of animation frames |
| `BLUE_FLAME_ANIM_SPEED` | `0.1f` | `float` | Seconds between frame advances |
| `BLUE_FLAME_LAUNCH_VY` | `-550.0f` | `float` | Initial upward impulse (px/s) |
| `BLUE_FLAME_RISE_DECEL` | `800.0f` | `float` | Deceleration during rise (px/s^2) |
| `BLUE_FLAME_APEX_Y` | `60.0f` | `float` | World-space y coordinate at apex (px) |
| `BLUE_FLAME_FLIP_DURATION` | `0.12f` | `float` | Time to rotate 180 degrees at apex (s) |
| `BLUE_FLAME_WAIT_DURATION` | `1.5f` | `float` | Time hidden below floor before next eruption (s) |
| `MAX_BLUE_FLAMES` | `8` | `int` | Maximum blue flame instances per level |

---

## `faster_fish.h` Constants

| Constant | Value | Type | Description |
|----------|-------|------|-------------|
| `MAX_FASTER_FISH` | `4` | `int` | Maximum faster fish instances per level |
| `FFISH_FRAMES` | `2` | `int` | Number of animation frames |
| `FFISH_FRAME_W` | `48` | `int` | Frame width (px) |
| `FFISH_FRAME_H` | `48` | `int` | Frame height (px) |
| `FFISH_RENDER_W` | `48` | `int` | Render width (logical px) |
| `FFISH_RENDER_H` | `48` | `int` | Render height (logical px) |
| `FFISH_SPEED` | `120.0f` | `float` | Patrol speed (px/s) |
| `FFISH_JUMP_VY` | `-420.0f` | `float` | Jump impulse (px/s) |
| `FFISH_JUMP_MIN` | `1.0f` | `float` | Minimum delay between jumps (s) |
| `FFISH_JUMP_MAX` | `2.2f` | `float` | Maximum delay between jumps (s) |
| `FFISH_HITBOX_PAD_X` | `16` | `int` | Horizontal hitbox inset (px) |
| `FFISH_HITBOX_PAD_Y` | `13` | `int` | Vertical hitbox inset (px) |
| `FFISH_FRAME_MS` | `100` | `int` | Frame animation duration (ms) |

---

## `spike.h` Constants

| Constant | Value | Type | Description |
|----------|-------|------|-------------|
| `MAX_SPIKE_ROWS` | `4` | `int` | Maximum spike row instances per level |
| `MAX_SPIKE_TILES` | `16` | `int` | Maximum tiles in a single spike row |
| `SPIKE_TILE_W` | `16` | `int` | Spike tile width (px) |
| `SPIKE_TILE_H` | `16` | `int` | Spike tile height (px) |

---

## `spike_platform.h` Constants

| Constant | Value | Type | Description |
|----------|-------|------|-------------|
| `MAX_SPIKE_PLATFORMS` | `4` | `int` | Maximum spike platform instances per level |
| `SPIKE_PLAT_PIECE_W` | `16` | `int` | Width of one 3-slice piece (px) |
| `SPIKE_PLAT_H` | `16` | `int` | Full frame height (px) |
| `SPIKE_PLAT_SRC_Y` | `5` | `int` | First content row in each piece (px) |
| `SPIKE_PLAT_SRC_H` | `11` | `int` | Content height (rows 5-15, px) |

---

## `ladder.h` Constants

| Constant | Value | Type | Description |
|----------|-------|------|-------------|
| `MAX_LADDERS` | `8` | `int` | Maximum ladder instances per level |
| `LADDER_W` | `16` | `int` | Sprite width (px) |
| `LADDER_H` | `22` | `int` | Content height after cropping padding (px) |
| `LADDER_SRC_Y` | `13` | `int` | First pixel row with content |
| `LADDER_SRC_H` | `22` | `int` | Height of content area (px) |
| `LADDER_STEP` | `8` | `int` | Vertical overlap when tiling (px) |

---

## `rope.h` Constants

| Constant | Value | Type | Description |
|----------|-------|------|-------------|
| `MAX_ROPES` | `8` | `int` | Maximum rope instances per level |
| `ROPE_W` | `12` | `int` | Display width with padding (px) |
| `ROPE_H` | `36` | `int` | Display height with padding (px) |
| `ROPE_SRC_X` | `2` | `int` | Source crop x offset (px) |
| `ROPE_SRC_Y` | `6` | `int` | Source crop y offset (px) |
| `ROPE_SRC_W` | `12` | `int` | Source crop width (px) |
| `ROPE_SRC_H` | `36` | `int` | Source crop height (px) |
| `ROPE_STEP` | `34` | `int` | Vertical spacing between stacked tiles (px) |

---

## `bouncepad_small.h` / `bouncepad_medium.h` / `bouncepad_high.h` Constants

| Constant | Value | Type | Description |
|----------|-------|------|-------------|
| `MAX_BOUNCEPADS_SMALL` | `4` | `int` | Maximum small bouncepad instances |
| `MAX_BOUNCEPADS_MEDIUM` | `4` | `int` | Maximum medium bouncepad instances |
| `MAX_BOUNCEPADS_HIGH` | `4` | `int` | Maximum high bouncepad instances |
