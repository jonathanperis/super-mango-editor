# Tasks: Visual Level Editor

All decisions finalized. Primary validation target: **recreate sandbox_00.c (Sandbox) with pixel-perfect fidelity**.

---

## Phase 1 — Infrastructure

### T-001: Integrate cJSON vendor library
**Requires:** None | **Refs:** R-006
**Files:** `vendor/cJSON/cJSON.h`, `vendor/cJSON/cJSON.c`, `vendor/cJSON/LICENSE`
**Work:**
- Download cJSON 1.7.18 (Dave Gamble, MIT license) — single .c + .h
- Place in `vendor/cJSON/` with LICENSE file
- Verify: `clang -std=c11 -Wall -Wextra -Wpedantic -c vendor/cJSON/cJSON.c`
**Verify:** Compiles without warnings.
**Commit:** `chore: add cJSON 1.7.18 vendor library for editor JSON serialization`

---

### T-002: Create JSON serializer (LevelDef ↔ JSON)
**Requires:** T-001 | **Refs:** R-006, design/Serializer
**Files:** `src/editor/serializer.h`, `src/editor/serializer.c`
**Work:**

Implement 4 functions. The serializer handles all 25 placement types plus `last_star` (single struct, not array).

**`level_to_json(const LevelDef *def)`** — produces:
```json
{
  "name": "Sandbox",
  "sea_gaps": [0, 192, 560, 928, 1152],
  "rails": [{ "layout": "RECT", "x": 444, "y": 35, "w": 10, "h": 6, "end_cap": 0 }],
  "platforms": [{ "x": 80.0, "tile_height": 2 }],
  "coins": [{ "x": 120.0, "y": 236.0 }],
  "star_yellows": [{ "x": 268.0, "y": 108.0 }],
  "last_star": { "x": 1492.0, "y": 100.0 },
  "spiders": [{ "x": 600.0, "vx": 50.0, "patrol_x0": 580.0, "patrol_x1": 730.0, "frame_index": 0 }],
  "jumping_spiders": [{ "x": 192.0, "vx": 55.0, "patrol_x0": 180.0, "patrol_x1": 224.0 }],
  "birds": [{ "x": 200.0, "base_y": 100.0, "vx": 45.0, "patrol_x0": 120.0, "patrol_x1": 350.0, "frame_index": 0 }],
  "faster_birds": [{ ... }],
  "fish": [{ "x": 10.0, "vx": 70.0, "patrol_x0": -10.0, "patrol_x1": 50.0 }],
  "faster_fish": [{ ... }],
  "axe_traps": [{ "pillar_x": 256.0, "mode": "PENDULUM" }],
  "circular_saws": [{ "x": 1350.0, "patrol_x0": 1350.0, "patrol_x1": 1446.0, "direction": 1 }],
  "spike_rows": [{ "x": 780.0, "count": 4 }],
  "spike_platforms": [{ "x": 370.0, "y": 200.0, "tile_count": 3 }],
  "spike_blocks": [{ "rail_index": 0, "t_offset": 0.0, "speed": 1.5 }],
  "float_platforms": [{ "mode": "STATIC", "x": 135.0, "y": 150.0, "tile_count": 3, "rail_index": 0, "t_offset": 0.0, "speed": 0.0 }],
  "bridges": [{ "x": 1350.0, "y": 172.0, "brick_count": 8 }],
  "bouncepads_small": [{ "x": 370.0, "launch_vy": -350.0, "pad_type": "GREEN" }],
  "bouncepads_medium": [{ ... }],
  "bouncepads_high": [{ ... }],
  "vines": [{ "x": 128.0, "y": 172.0, "tile_count": 5 }],
  "ladders": [{ "x": 1552.0, "y": -962.0, "tile_count": 150 }],
  "ropes": [{ "x": 460.0, "y": 172.0, "tile_count": 5 }]
}
```

Enum string mappings:
- RailLayout: `RAIL_LAYOUT_RECT` ↔ `"RECT"`, `RAIL_LAYOUT_HORIZ` ↔ `"HORIZ"`
- AxeTrapMode: `AXE_MODE_PENDULUM` ↔ `"PENDULUM"`, `AXE_MODE_SPIN` ↔ `"SPIN"`
- FloatPlatformMode: `FLOAT_PLATFORM_STATIC` ↔ `"STATIC"`, `_CRUMBLE` ↔ `"CRUMBLE"`, `_RAIL` ↔ `"RAIL"`
- BouncepadType: `BOUNCEPAD_GREEN` ↔ `"GREEN"`, `_WOOD` ↔ `"WOOD"`, `_RED` ↔ `"RED"`

**`level_from_json`**: validate counts against MAX_*. Missing arrays → count=0. `last_star` is single object.

**Verify:** Convert `sandbox_00_def` → JSON → LevelDef → compare every field. Must match exactly (float equality ok for round-trip of own output).
**Commit:** `feat(editor): add JSON serializer for LevelDef — all 25 placement types`

---

### T-003: Create C code exporter
**Requires:** T-002 | **Refs:** R-007, design/Exporter
**Files:** `src/editor/exporter.h`, `src/editor/exporter.c`
**Work:**

`level_export_c(def, var_name, dir_path)` generates two files matching `sandbox_00.c` style:

**Header** (`{var_name}.h`):
```c
#pragma once
#include "level.h"
extern const LevelDef {var_name}_def;
```

**Source** (`{var_name}.c`): must reproduce this exact pattern:
- Include header + entity headers for speed constants (SPIDER_SPEED, BIRD_SPEED, etc.)
- `const LevelDef {var_name}_def = {`
- Section order matching LevelDef field order (see level.h lines 300-371):
  1. `.name = "Level Name",`
  2. `.sea_gaps = { ... }, .sea_gap_count = N,`
  3. `.rails = { ... }, .rail_count = N,`
  4. `.platforms = { ... }, .platform_count = N,`
  5. `.coins = { ... }, .coin_count = N,`
  6. `.star_yellows = { ... }, .star_yellow_count = N,`
  7. `.last_star = { .x = ..., .y = ... },`
  8. All enemies (spiders through faster_fish)
  9. All hazards (axe_traps through spike_blocks)
  10. All surfaces (float_platforms through bouncepads_high)
  11. All decorations (vines, ladders, ropes)
- Enum values as C identifiers: `RAIL_LAYOUT_RECT`, `AXE_MODE_PENDULUM`, etc.
- Floats as `%.1ff` (e.g., `80.0f`)
- Speed constants where recognizable: `SPIDER_SPEED` for 50.0f, `BIRD_SPEED` for 45.0f
- Section separators: `/* ---- World geometry ---- */`

**Verify:**
1. Export `sandbox_00_def` → compile: zero warnings with `-std=c11 -Wall -Wextra -Wpedantic`
2. Replace original in game → `make run` → identical behavior
**Commit:** `feat(editor): add C code exporter — generates compilable level source`

---

### T-004: Add `editor` target to Makefile
**Requires:** None | **Refs:** R-001
**Files:** `Makefile`
**Work:**
- Add `EDITOR_DIR`, `VENDOR_DIR`, `EDITOR_SRCS`, `EDITOR_OBJS` variables
- Editor compilation rule with `-I$(VENDOR_DIR)` for cJSON header
- `editor` target → `out/super-mango-editor` linked with SDL2 + SDL2_image + SDL2_ttf (no SDL2_mixer)
- `run-editor` target
- Update `clean` to include editor/vendor objects
- Game `$(SRCS)` must NOT glob `src/editor/`

**Verify:** `make editor` builds. `make` (game) unaffected. `make clean` removes both.
**Commit:** `build: add editor target to Makefile — standalone level editor executable`

---

### T-005: Editor window and main loop skeleton
**Requires:** T-004 | **Refs:** R-001
**Files:** `src/editor/editor_main.c`, `src/editor/editor.h`, `src/editor/editor.c`
**Work:**

`editor.h`:
- `EditorState` struct (full definition from design.md)
- `EntityType` enum (25 values), `EditorTool` enum, `Selection` struct
- `EntityTextures` struct (27 texture pointers)
- `EditorCamera` struct
- Constants: `EDITOR_W 1280`, `EDITOR_H 720`, `CANVAS_W 896`, `PANEL_W 384`, `TOOLBAR_H 32`, `STATUS_H 32`

`editor.c`:
- `editor_init()`: window, renderer, font (`assets/round9x13.ttf`), camera (x=0, zoom=2.0), tool=TOOL_SELECT
- `editor_loop()`: 60 FPS loop with `SDL_PollEvent`, clear (#1A1A1A), present
- `editor_cleanup()`: destroy in reverse init order, NULL all pointers

`editor_main.c`:
- `main()`: SDL_Init + IMG_Init + TTF_Init, editor_init → editor_loop → editor_cleanup, teardown
- Parse `argv[1]` for optional JSON path
- No SDL_mixer init (editor has no audio)

**Verify:** `make run-editor` opens 1280x720 dark window. ESC closes cleanly.
**Commit:** `feat(editor): add editor skeleton — window, main loop, EditorState`

---

## Phase 2 — Canvas and Rendering (Game-Accurate)

### T-006: Canvas viewport with camera
**Requires:** T-005 | **Refs:** R-003, R-010
**Files:** `src/editor/canvas.h`, `src/editor/canvas.c`
**Work:**
- Viewport rect: x=0, y=TOOLBAR_H, w=CANVAS_W (896), h=EDITOR_H-TOOLBAR_H-STATUS_H (656)
- `SDL_RenderSetClipRect` to constrain drawing
- Camera: cam_x (float), zoom (1.0, 2.0, 4.0)
- WASD scroll ±200*dt px/s, middle-mouse drag, scroll wheel zoom
- Coordinate transforms:
  ```c
  /* World → screen */
  screen_x = (int)((world_x - cam.x) * cam.zoom);
  screen_y = (int)(world_y * cam.zoom) + TOOLBAR_H;
  /* Screen → world */
  world_x = (mouse_x) / cam.zoom + cam.x;
  world_y = (mouse_y - TOOLBAR_H) / cam.zoom;
  ```
- Camera clamp: 0 ≤ cam_x ≤ WORLD_W - CANVAS_W/zoom
- Sky background: solid #87CEEB fill in canvas area

**Verify:** Sky-blue canvas, WASD scrolls through 1600px world, zoom scales correctly.
**Commit:** `feat(editor): add canvas viewport with camera scroll and zoom`

---

### T-007: 9-slice floor rendering with sea gap edge caps
**Requires:** T-006 | **Refs:** R-003, design/Floor Rendering Algorithm
**Files:** `src/editor/canvas.c` (extend)
**Work:**

Replicate the game's 9-slice floor algorithm:

1. Load `grass_tileset.png` (48×48, 3×3 grid of 16×16 pieces)
2. Define piece constants: `P = 16` (tile piece size)
3. Iterate from `floor_start_x = (cam_left / P) * P` to `cam_right` in P-steps
4. For each column x, draw 3 rows: FLOOR_Y (grass), FLOOR_Y+P (dirt), FLOOR_Y+2P (base)
5. Row selection: y==FLOOR_Y → row 0; y+P>=GAME_H → row 2; else → row 1
6. Column selection (gap edge caps):
   - x==0 �� col 0 (left world edge)
   - x+P>=WORLD_W → col 2 (right world edge)
   - x+P == gap_x → col 2 (right gap edge cap)
   - x == gap_x + SEA_GAP_W → col 0 (left gap edge cap)
   - Inside gap entirely → skip (show water)
   - Else → col 1 (seamless fill)
7. Apply camera transform and zoom to dst rects

**Static water in gaps:**
- For each sea_gap: draw solid blue rect (#1A6BA0) from y=GAME_H-31 to y=GAME_H, width=SEA_GAP_W
- Optionally render one static water frame from `water.png` for visual fidelity

**Verify:** Load sandbox_00 data. Floor matches game exactly:
- 5 sea gaps with clean edge caps at positions 0, 192, 560, 928, 1152
- Grass tiles seamlessly fill between gaps
- Water visible in gap regions
**Commit:** `feat(editor): add 9-slice floor rendering with sea gap edge caps and water`

---

### T-008: Load textures and render all entities at correct display sizes
**Requires:** T-006 | **Refs:** R-002, R-003, design/Entity Display Size Table, design/Y Position Derivation Table
**Files:** `src/editor/canvas.c` (extend)
**Work:**

**Load all textures** in `editor_init()`:
- Use exact asset paths from game (see design.md Asset Path Reference)
- All non-fatal: warn to stderr if missing, set pointer to NULL, skip rendering

**Entity rendering functions** — each must use the correct source rect crop and derived Y position:

```c
/* === ENEMIES === */

/* Spider: frame 64×48, display 64×10 (art band y=22..31) */
src = {frame_col * 64, 22, 64, 10};
dst_y = FLOOR_Y - 10;  /* = 242 */

/* Jumping Spider: same art dimensions as spider */
src = {frame_col * 64, 22, 64, 10};
dst_y = FLOOR_Y - 10;  /* = 242 */

/* Bird: frame 48×48, display 48×14 (art band y=17..30) */
src = {frame_col * 48, 17, 48, 14};
dst_y = placement.base_y;  /* sine wave at runtime, static preview */

/* Faster Bird: same crop as bird */
src = {frame_col * 48, 17, 48, 14};
dst_y = placement.base_y;

/* Fish: frame 48×48, display 48×48 (full frame) */
src = {0, 0, 48, 48};
dst_y = 245;  /* (300 - 31) - 24 */

/* Faster Fish: same as fish */
dst_y = 245;

/* === COLLECTIBLES === */

/* Coin: full texture, display 16×16 */
src = {0, 0, tex_w, tex_h};  /* full texture */
dst = {x, y, 16, 16};  /* scaled to display size */

/* Star Yellow: display 16×16 */
dst = {x, y, 16, 16};

/* Last Star: display 24×24 */
dst = {x, y, 24, 24};

/* === HAZARDS === */

/* Axe Trap: frame 48×64, display 48×64 */
src = {0, 0, 48, 64};
dst_x = placement.pillar_x + TILE_SIZE/2 - 24;  /* centered on pivot */
dst_y = 124;  /* FLOOR_Y - 3*TILE_SIZE + 16 */

/* Circular Saw: frame 32×32, display 32×32 */
src = {0, 0, 32, 32};
dst_y = 140;  /* FLOOR_Y - 2*TILE_SIZE + 16 - 32 */

/* Spike Row: 16×16 per tile */
for (int t = 0; t < count; t++)
    dst = {x + t*16, 236, 16, 16};  /* y = FLOOR_Y - 16 */

/* Spike Platform: 16×11 per piece (src y=5, h=11) */
for (int t = 0; t < tile_count; t++)
    src = {t==0 ? 0 : t==tile_count-1 ? 32 : 16, 5, 16, 11};
    dst = {x + t*16, y, 16, 11};

/* Spike Block: frame 16×16, display 24×24 */
/* Position from rail interpolation (see rail computation below) */
dst = {rail_x - 12, rail_y - 12, 24, 24};

/* === SURFACES === */

/* Platform (pillar): 9-slice grass tile, 48 × (tile_height * 48) */
/* Use same 3×3 grid as floor tile but for pillar rendering */
dst_y = FLOOR_Y - tile_height * TILE_SIZE + 16;

/* Float Platform: 3-slice, 16×16 per piece (left/mid/right) */
/* STATIC/CRUMBLE: use placement x,y */
/* RAIL: compute from rail_get_world_pos(rail, t_offset) */
for (int t = 0; t < tile_count; t++)
    piece = t==0 ? LEFT : t==tile_count-1 ? RIGHT : MIDDLE;
    dst = {x + t*16, y, 16, 16};

/* Bridge: 16×16 per brick */
for (int b = 0; b < brick_count; b++)
    dst = {x + b*16, y, 16, 16};

/* Bouncepad: frame 48×48, display 48×18 (src y=14, h=18) */
src = {2 * 48, 14, 48, 18};  /* frame 2 = compressed/idle pose */
dst_y = 234;  /* FLOOR_Y - 18 */

/* === DECORATIONS === */

/* Vine: src y=8, h=32, step=19px overlap */
for (int t = 0; t < tile_count; t++)
    src = {0, 8, 16, 32};
    dst = {x, y + t*19, 16, 32};

/* Ladder: src y=13, h=22, step=8px overlap */
for (int t = 0; t < tile_count; t++)
    src = {0, 13, 16, 22};
    dst = {x, y + t*8, 16, 22};

/* Rope: src x=2, y=6, w=12, h=36, step=34px overlap */
for (int t = 0; t < tile_count; t++)
    src = {2, 6, 12, 36};
    dst = {x, y + t*34, 12, 36};
```

**Rail path rendering:**
- Compute RECT/HORIZ tile positions (see design.md Rail Path Computation)
- Draw each rail tile from `rail.png` (64×64, 4×4 grid of 16×16 direction-dependent tiles)
- For editor preview: draw connected line segments between tile centers in green (#00AA00, 50% alpha)

**Spike block and RAIL float platform positions:**
- Build rail tile arrays from `level.rails[]` using RECT/HORIZ computation
- For each spike_block/RAIL float_platform: interpolate position at `t_offset` along their referenced rail
- Draw at interpolated position

**Blue flame ghost preview:**
- For each `sea_gap[i]`: render blue_flame sprite at 50% alpha, centered in gap
- x = gap_x + (32 - 48) / 2 = gap_x - 8 (centered, may extend outside gap)
- y = FLOOR_Y (visible preview position, not the game's hidden start_y)

**Render order:** follow the 31-layer order from design.md exactly.

**Verify:** Load sandbox_00 data into editor. Compare canvas screenshot with game screenshot:
- All 8 platforms at correct heights (2-tile at y=172, 3-tile at y=124)
- 5 sea gaps with floor edge caps
- 16 coins at correct x,y
- 2 spiders at y=242, 1 jumping spider at y=242
- 2 birds at base_y, 2 faster birds at base_y
- 2 fish at y=245, 1 faster fish at y=245
- 2 axe traps at y=124
- 1 circular saw at y=140
- 1 spike row (4 tiles) at y=236
- 1 spike platform at y=200
- 3 spike blocks at rail positions
- 3 float platforms (1 static, 1 crumble, 1 on rail)
- 1 bridge (8 bricks) at y=172
- 3 bouncepads at y=234
- 5 vines, 1 ladder, 1 rope at correct positions
- 3 rail paths visible as lines/tiles
- 5 blue flame ghosts at sea gap positions
**Commit:** `feat(editor): render all entities at game-accurate display sizes and positions`

---

### T-008b: Grid overlay and reference lines
**Requires:** T-006 | **Refs:** R-003
**Files:** `src/editor/canvas.c` (extend)
**Work:**
- TILE_SIZE grid lines in light gray (#404040, 25% alpha)
- Screen boundaries at x=0,400,800,1200 in blue (#4A90D9), 2px, with "Screen N" labels
- FLOOR_Y (252) horizontal in red (#D94A4A), 2px
- G key toggles grid
- Auto-hide fine grid at zoom=1.0

**Verify:** Grid visible at all zooms. Screen labels readable. G toggles.
**Commit:** `feat(editor): add grid overlay with screen boundaries and FLOOR_Y line`

---

## Phase 3 — UI Framework

### T-009: Minimal immediate-mode UI system
**Requires:** T-005 | **Refs:** D-002
**Files:** `src/editor/ui.h`, `src/editor/ui.c`
**Work:**

7 widget functions using SDL2 draw + SDL2_ttf:
- `ui_button(es, x, y, w, h, label)` → 1 on click
- `ui_label(es, x, y, text)` — rendered text (cache textures for repeated strings)
- `ui_panel(es, x, y, w, h, title)` — dark rect with title bar
- `ui_int_field(es, x, y, w, value)` → 1 on change (SDL_TEXTINPUT for input)
- `ui_float_field(es, x, y, w, value)` → 1 on change (1 decimal display)
- `ui_dropdown(es, x, y, w, options, count, selected)` → 1 on change
- `ui_separator(es, x, y, w)` — thin horizontal line

Colors: #2D2D2D panels, #3D3D3D title bars, #4D4D4D buttons, #5D5D5D hover, #4A90D9 accent, #E0E0E0 text, #1D1D1D input fields.

**Verify:** Panel with buttons, fields, dropdown renders and responds to input.
**Commit:** `feat(editor): add immediate-mode UI system — buttons, fields, dropdowns`

---

### T-010: Entity palette panel
**Requires:** T-009, T-008 | **Refs:** R-004
**Files:** `src/editor/palette.h`, `src/editor/palette.c`
**Work:**

Static array of 25 entries, 6 categories:
- **World** (2): Sea Gap, Rail
- **Collectibles** (3): Coin, Star Yellow, Last Star
- **Enemies** (6): Spider, Jumping Spider, Bird, Faster Bird, Fish, Faster Fish
- **Hazards** (5): Axe Trap, Circular Saw, Spike Row, Spike Platform, Spike Block
- **Surfaces** (7): Platform, Float Platform, Bridge, Bouncepad Small/Medium/High
- **Decorations** (3): Vine, Ladder, Rope

Each entry: 32×32 sprite thumbnail (first frame, scaled from display size) + name.
Click → `tool = TOOL_PLACE`, `palette_type = selected`.
Scroll within panel if list exceeds height.

**Verify:** All 25 types listed. Click highlights and activates placement.
**Commit:** `feat(editor): add entity palette panel with categories and thumbnails`

---

### T-011: Properties panel
**Requires:** T-009, T-010 | **Refs:** R-005
**Files:** `src/editor/properties.h`, `src/editor/properties.c`
**Work:**

Show editable fields when `selection.index >= 0`. Full per-type mapping:

| Entity Type | Fields (widget type) |
|-------------|---------------------|
| Sea Gap | x (int) |
| Rail | layout (dropdown: RECT/HORIZ), x (int), y (int), w (int), h (int), end_cap (int) |
| Platform | x (float), tile_height (int: 1-5) |
| Coin | x (float), y (float) |
| Star Yellow | x (float), y (float) |
| Last Star | x (float), y (float) |
| Spider | x (float), vx (float), patrol_x0 (float), patrol_x1 (float), frame_index (int) |
| Jumping Spider | x (float), vx (float), patrol_x0 (float), patrol_x1 (float) |
| Bird | x (float), base_y (float), vx (float), patrol_x0 (float), patrol_x1 (float), frame_index (int) |
| Faster Bird | same as Bird |
| Fish | x (float), vx (float), patrol_x0 (float), patrol_x1 (float) |
| Faster Fish | same as Fish |
| Axe Trap | pillar_x (float), mode (dropdown: PENDULUM/SPIN) |
| Circular Saw | x (float), patrol_x0 (float), patrol_x1 (float), direction (int: -1/1) |
| Spike Row | x (float), count (int) |
| Spike Platform | x (float), y (float), tile_count (int) |
| Spike Block | rail_index (int: 0..rail_count-1), t_offset (float), speed (float) |
| Float Platform | mode (dropdown: STATIC/CRUMBLE/RAIL), x (float), y (float), tile_count (int), rail_index (int), t_offset (float), speed (float) |
| Bridge | x (float), y (float), brick_count (int) |
| Bouncepad Small | x (float), launch_vy (float) |
| Bouncepad Medium | x (float), launch_vy (float) |
| Bouncepad High | x (float), launch_vy (float) |
| Vine | x (float), y (float), tile_count (int) |
| Ladder | x (float), y (float), tile_count (int) |
| Rope | x (float), y (float), tile_count (int) |

Changes → update LevelDef immediately, set modified=1. Undo integration in T-015.

**Verify:** Select entity → correct fields appear. Edit value → canvas updates. Types with dropdowns work.
**Commit:** `feat(editor): add properties panel with per-entity-type field editing`

---

## Phase 4 — Editing Tools

### T-012: Select and move tool
**Requires:** T-008, T-009 | **Refs:** R-008, design/Hit-Test Dimensions
**Files:** `src/editor/tools.h`, `src/editor/tools.c`
**Work:**

Hit-test uses **display-size bounding boxes**, not sprite frame sizes:
```c
Selection hit_test(const LevelDef *level, float wx, float wy) {
    /* Iterate in reverse render order (topmost first) */
    /* Birds: check AABB(x, base_y, 48, 14) */
    /* Spiders: check AABB(x, 242, 64, 10) */
    /* Coins: check AABB(x, y, 16, 16) */
    /* Spike rows: check AABB(x, 236, count*16, 16) */
    /* etc. for all 25 types — use display dimensions from design doc */
}
```

Move: drag updates placement x (and y where stored). Shift → snap to TILE_SIZE grid.
Selection highlight: 2px outline (#4A90D9) around display bounding box.

**Verify:** Can select any entity in sandbox_00. Click on spider (thin 10px strip) works. Drag moves. Shift snaps.
**Commit:** `feat(editor): add select and move tool with display-size hit testing`

---

### T-013: Place tool
**Requires:** T-010, T-012 | **Refs:** R-008
**Files:** `src/editor/tools.c` (extend)
**Work:**

Ghost preview at cursor using 50% alpha (`SDL_SetTextureAlphaMod(tex, 128)`).
Click → check MAX_* → append to array → increment count → push CMD_PLACE.

Default placement values for sandbox_00 accuracy:

| Entity | Defaults |
|--------|----------|
| Spider | vx=SPIDER_SPEED (50), patrol bounds ±50px from x, frame_index=0 |
| Jumping Spider | vx=JSPIDER_SPEED (55), patrol bounds ±50px |
| Bird | base_y=100, vx=BIRD_SPEED (45), patrol bounds ±80px, frame_index=0 |
| Faster Bird | base_y=80, vx=FBIRD_SPEED (80), patrol bounds ±80px, frame_index=0 |
| Fish | vx=FISH_SWIM_SPEED (70), patrol bounds ±60px |
| Faster Fish | vx=FFISH_SWIM_SPEED (120), patrol bounds ±60px |
| Axe Trap | mode=AXE_MODE_PENDULUM |
| Circular Saw | patrol bounds ±48px, direction=1 |
| Spike Row | count=3 |
| Spike Platform | tile_count=3 |
| Spike Block | rail_index=0, t_offset=0, speed=SPIKE_SPEED_NORMAL |
| Float Platform | mode=FLOAT_PLATFORM_STATIC, tile_count=3 |
| Bridge | brick_count=8, y=172 |
| Bouncepad Small | launch_vy=BOUNCEPAD_VY_SMALL (-350), pad_type=GREEN |
| Bouncepad Medium | launch_vy=BOUNCEPAD_VY_MEDIUM (-450), pad_type=WOOD |
| Bouncepad High | launch_vy=BOUNCEPAD_VY_HIGH (-600), pad_type=RED |
| Platform | tile_height=2 |
| Vine/Ladder/Rope | tile_count=3 |

Special: `ENT_LAST_STAR` overwrites (single struct). `ENT_SEA_GAP` snaps to 32px.

**Verify:** Ghost follows cursor with correct display size. Place coin → appears at 16×16. Place spike_row → 3-tile strip appears at y=236.
**Commit:** `feat(editor): add place tool with ghost preview and defaults`

---

### T-014: Delete tool
**Requires:** T-012 | **Refs:** R-008
**Files:** `src/editor/tools.c` (extend)
**Work:**
- Delete key on selection → snapshot data, memmove compact array, decrement count, push CMD_DELETE
- Right-click → hit_test + delete in one step
- `array_remove(void *arr, int *count, int index, size_t elem_size)` helper

**Verify:** Select entity → Delete → gone. Right-click → gone. Undo restores.
**Commit:** `feat(editor): add delete tool — Delete key and right-click removal`

---

### T-015: Undo/redo system
**Requires:** T-012, T-013, T-014 | **Refs:** R-009
**Files:** `src/editor/undo.h`, `src/editor/undo.c`
**Work:**

256-entry command stack with `PlacementData` union (see design.md):
- `undo_push()`: append + clear redo
- `undo_pop()`: pop → redo
- `redo_pop()`: pop redo → undo
- `undo_apply(es, cmd, reverse)`: execute/reverse action on LevelDef arrays
- Ctrl+Z = undo, Ctrl+Shift+Z = redo

**Verify:** Place → undo → redo cycle. Move → undo → original position. Property → undo → old value.
**Commit:** `feat(editor): add undo/redo system — place, delete, move, property commands`

---

## Phase 5 — File Operations

### T-016: Save and load workflow
**Requires:** T-002, T-005 | **Refs:** R-006
**Files:** `src/editor/editor.c` (extend)
**Work:**
- Ctrl+S → save JSON (prompt path on first save, remember after)
- Ctrl+O → prompt path, load JSON, clear undo, reset camera
- Ctrl+N → clear level, "Untitled"
- Title bar: `"Super Mango Editor — filename.json *"`
- Unsaved warning on load/new/quit
- CLI: `argv[1]` opens JSON on startup

**Verify:** Save → quit → load → identical. Ctrl+N clears. Title bar tracks state.
**Commit:** `feat(editor): add save/load/new workflow — JSON file operations`

---

### T-017: C export workflow
**Requires:** T-003, T-016 | **Refs:** R-007
**Files:** `src/editor/editor.c` (extend)
**Work:**
- Ctrl+E → prompt var_name (default from filename) + output dir (default `src/levels/`)
- Call `level_export_c()`, show status bar confirmation
- Validate: level must have name, warn if empty

**Verify:** Export → compile with game → behavior identical to sandbox_00.
**Commit:** `feat(editor): add C export workflow — Ctrl+E generates compilable level source`

---

## Phase 6 — Polish and Integration

### T-018: Toolbar and status bar
**Requires:** T-009 | **Refs:** design/Window Layout
**Files:** `src/editor/editor.c` (extend)
**Work:**
- Toolbar (top 32px): tool buttons, zoom display, grid toggle, file ops
- Status bar (bottom 32px): world coords, tool name, entity count, file path + *

**Verify:** All info accurate and updates in real-time.
**Commit:** `feat(editor): add toolbar and status bar`

---

### T-019: Seed editor with sandbox_00 JSON conversion
**Requires:** T-002 | **Refs:** Success criteria #3, #4
**Files:** `levels/sandbox_00.json`
**Work:**

One-time conversion: access `sandbox_00_def` → `level_to_json()` → write `levels/sandbox_00.json`.

This JSON becomes:
- The reference file for round-trip validation
- The first level openable in the editor
- The benchmark: load in editor, compare with game screenshot

**Round-trip verification chain:**
1. `sandbox_00_def` (C const) → JSON → LevelDef → field-by-field comparison ✓
2. JSON → LevelDef → C export → compile → game runs identically ✓
3. JSON → editor canvas → visual match with game screenshot ✓

**Verify:** `levels/sandbox_00.json` loads in editor. All 47+ entities at correct positions.
**Commit:** `feat(editor): convert sandbox_00 to JSON — seed and validation benchmark`

---

### T-020: Rail and sea gap editing tools
**Requires:** T-012, T-013 | **Refs:** R-002
**Files:** `src/editor/tools.c` (extend)
**Work:**

**Sea gap tool** (ENT_SEA_GAP in TOOL_PLACE):
- Click on floor → add gap at x snapped to SEA_GAP_W (32px) grid
- Floor re-renders with new edge caps automatically
- Water appears in gap region
- Blue flame ghost preview auto-appears
- Right-click gap → remove

**Rail tool** (ENT_RAIL in TOOL_PLACE):
- Two-click creation: click for origin → click for extent (snap to 16px)
- Mode via properties panel: RECT or HORIZ
- RECT: draw closed rectangle outline (dotted green)
- HORIZ: draw horizontal line with end_cap indicator
- After rail created: spike blocks and RAIL float platforms can reference it by index

**Rail-dependent entity feedback:**
- When selecting a spike_block or RAIL float_platform, highlight the referenced rail path
- Show rail_index in properties as dropdown (0 to rail_count-1)

**Verify:** Can recreate sandbox_00's 5 sea gaps and 3 rails. Spike blocks show at correct rail positions.
**Commit:** `feat(editor): add rail path and sea gap editing tools`

---

## Dependency Graph

```
T-001 (cJSON)
  └── T-002 (serializer)
        ├── T-003 (exporter) → T-017 (export workflow)
        ├── T-016 (save/load)
        └── T-019 (sandbox_00 JSON seed)

T-004 (Makefile)
  └── T-005 (editor skeleton)
        ├── T-006 (canvas viewport)
        │     ├── T-007 (9-slice floor + water)
        │     ├── T-008 (entity rendering — CRITICAL: display sizes + Y derivations)
        │     └── T-008b (grid overlay)
        ├── T-009 (UI system)
        │     ├── T-010 (palette)
        │     ├── T-011 (properties)
        │     └── T-018 (toolbar/status)
        └── T-012 (select/move — uses display-size hit-test)
              ├── T-013 (place tool)
              ├── T-014 (delete tool)
              └── T-015 (undo/redo)

T-020 (rails/sea gaps) ← T-012, T-013
```

## Traceability Matrix

| Requirement | Tasks | Critical Detail |
|-------------|-------|-----------------|
| R-001 (Standalone exe) | T-004, T-005 | Game build must remain unaffected |
| R-002 (Full entity support) | T-008, T-010-T-014, T-020 | 25 types with correct display sizes |
| R-003 (Visual canvas) | T-006-T-008b | 9-slice floor, water, display-accurate entities |
| R-004 (Palette) | T-010 | 25 types in 6 categories |
| R-005 (Properties) | T-011 | Per-type field mapping (25 configs) |
| R-006 (JSON) | T-001, T-002, T-016, T-019 | Round-trip fidelity for sandbox_00 |
| R-007 (C export) | T-003, T-017 | sandbox_00.c style match |
| R-008 (Edit ops) | T-012-T-014 | Display-size hit testing |
| R-009 (Undo/redo) | T-015 | 256 commands, PlacementData union |
| R-010 (Camera) | T-006 | WASD + middle-mouse + zoom |

## Sandbox Validation Checklist (sandbox_00.c)

The following must be visually correct when loading `sandbox_00.json` in the editor:

- [ ] 5 sea gaps at x = 0, 192, 560, 928, 1152 with floor edge caps
- [ ] 3 rails: RECT 10×6 at (444,35), RECT 8×5 at (852,50), HORIZ 14-tile at (1200,112)
- [ ] 8 platforms: 4× 2-tile (y=172) + 4× 3-tile (y=124)
- [ ] 16 coins at exact positions (ground y=236, platform tops y=156/y=108)
- [ ] 3 yellow stars at platform tops
- [ ] 1 last star at (1492, 100)
- [ ] 2 spiders at y=242, different patrol ranges
- [ ] 1 jumping spider at y=242
- [ ] 2 birds at base_y=100/120, patrol ranges spanning 2 screens each
- [ ] 2 faster birds at base_y=80/100
- [ ] 2 fish at y=245
- [ ] 1 faster fish at y=245
- [ ] 2 axe traps at y=124 (pendulum + spin modes)
- [ ] 1 circular saw at y=140
- [ ] 1 spike row (4 tiles) at (780, 236)
- [ ] 1 spike platform at (370, 200)
- [ ] 3 spike blocks on their respective rails
- [ ] 1 static float platform at (135, 150)
- [ ] 1 crumble float platform
- [ ] 1 rail float platform on rail 1
- [ ] 1 bridge (8 bricks) at (1350, 172)
- [ ] 3 bouncepads at y=234 (green, wood, red)
- [ ] 5 vines at platform edges
- [ ] 1 ladder at far right (x=1552)
- [ ] 1 rope at (460, 172)
- [ ] 5 blue flame ghosts at sea gap positions
- [ ] Floor 9-slice correct between all gaps
- [ ] Water strips visible in all 5 gaps
