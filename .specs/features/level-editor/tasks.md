# Tasks: Visual Level Editor

All decisions finalized: JSON + C export pipeline (D-001), custom SDL2/TTF UI (D-002), no game JSON loader (D-003).

---

## Phase 1 — Infrastructure

### T-001: Integrate cJSON vendor library
**Requires:** None
**Refs:** R-006 (JSON serialization)
**Files:** `vendor/cJSON/cJSON.h`, `vendor/cJSON/cJSON.c`
**Work:**
- Download cJSON 1.7.18 (Dave Gamble, MIT license) — single .c + .h
- Place in `vendor/cJSON/`
- Add `LICENSE` file from cJSON repo for attribution
- Verify compiles standalone: `clang -std=c11 -Wall -Wextra -Wpedantic -c vendor/cJSON/cJSON.c`
**Verify:** Compiles without warnings. `cJSON_CreateObject()` / `cJSON_Delete()` link successfully in a minimal test.
**Commit:** `chore: add cJSON 1.7.18 vendor library for editor JSON serialization`

---

### T-002: Create JSON serializer (LevelDef ↔ JSON)
**Requires:** T-001
**Refs:** R-006, design/Serializer section
**Files:** `src/editor/serializer.h`, `src/editor/serializer.c`
**Work:**
Implement 4 functions:

`level_to_json(const LevelDef *def)` — serialize all 25 entity arrays:
- `name` → string
- `sea_gaps` → int array
- `rails` → object array with layout as string enum ("RECT"/"HORIZ")
- `platforms` → object array (x: float, tile_height: int)
- `coins`, `yellow_stars` → object arrays (x, y: float)
- `last_star` → single object (x, y: float)
- `spiders` → object array (x, vx, patrol_x0, patrol_x1: float, frame_index: int)
- `jumping_spiders` → object array (x, vx, patrol_x0, patrol_x1: float)
- `birds`, `faster_birds` → object array (x, base_y, vx, patrol_x0, patrol_x1: float, frame_index: int)
- `fish`, `faster_fish` → object array (x, vx, patrol_x0, patrol_x1: float)
- `axe_traps` → object array (pillar_x: float, mode: string "PENDULUM"/"SPIN")
- `circular_saws` → object array (x, patrol_x0, patrol_x1: float, direction: int)
- `spike_rows` → object array (x: float, count: int)
- `spike_platforms` → object array (x, y: float, tile_count: int)
- `spike_blocks` → object array (rail_index: int, t_offset, speed: float)
- `float_platforms` → object array (mode: string, x, y: float, tile_count, rail_index: int, t_offset, speed: float)
- `bridges` → object array (x, y: float, brick_count: int)
- `bouncepads_small/medium/high` → object arrays (x, launch_vy: float, pad_type: string)
- `vines`, `ladders`, `ropes` → object arrays (x, y: float, tile_count: int)

`level_from_json(const cJSON *json, LevelDef *def)`:
- Parse each key, populate corresponding LevelDef field
- Validate array lengths against MAX_* constants — return -1 with fprintf if exceeded
- Missing arrays → count = 0 (lenient parsing)
- Enum strings → integer mapping: `"RECT"→RAIL_LAYOUT_RECT`, `"PENDULUM"→AXE_MODE_PENDULUM`, etc.
- `last_star` is a single object, not an array

`level_save_json(const LevelDef *def, const char *path)`:
- Call `level_to_json()`, then `cJSON_Print()` for pretty-printed output
- Write to file with `fopen/fwrite/fclose`
- Free cJSON tree after writing

`level_load_json(const char *path, LevelDef *def)`:
- Read entire file into buffer
- `cJSON_Parse()` → `level_from_json()` → `cJSON_Delete()`
- Return -1 on parse error or validation failure

**Verify:** Write a standalone test:
1. Build `level_01_def` const data in memory
2. `level_to_json(&level_01_def)` → JSON string
3. `level_from_json(json, &loaded)` → LevelDef
4. `memcmp` all scalar fields + manual comparison of each array element
5. All fields must match exactly (float equality is ok since we're round-tripping our own output)
**Commit:** `feat(editor): add JSON serializer for LevelDef — all 25 placement types`

---

### T-003: Create C code exporter
**Requires:** T-002
**Refs:** R-007, design/Exporter section
**Files:** `src/editor/exporter.h`, `src/editor/exporter.c`
**Work:**
`level_export_c(const LevelDef *def, const char *var_name, const char *dir_path)`:

Generate header file (`{dir_path}/{var_name}.h`):
```c
#pragma once
#include "level.h"
extern const LevelDef {var_name}_def;
```

Generate source file (`{dir_path}/{var_name}.c`):
- Include header + entity headers for speed constants
- Open `const LevelDef {var_name}_def = {`
- For each section, write section separator comment + designated initializer block
- Field ordering must match `level_01.c` exactly:
  1. name
  2. sea_gaps + sea_gap_count
  3. rails + rail_count
  4. platforms + platform_count
  5. coins + coin_count
  6. yellow_stars + yellow_star_count
  7. last_star
  8. spiders + spider_count
  9. jumping_spiders + jumping_spider_count
  10. birds + bird_count
  11. faster_birds + faster_bird_count
  12. fish + fish_count
  13. faster_fish + faster_fish_count
  14. axe_traps + axe_trap_count
  15. circular_saws + circular_saw_count
  16. spike_rows + spike_row_count
  17. spike_platforms + spike_platform_count
  18. spike_blocks + spike_block_count
  19. float_platforms + float_platform_count
  20. bridges + bridge_count
  21. bouncepads_small + bouncepad_small_count
  22. bouncepads_medium + bouncepad_medium_count
  23. bouncepads_high + bouncepad_high_count
  24. vines + vine_count
  25. ladders + ladder_count
  26. ropes + rope_count
- Close `};`
- Floats formatted with 1 decimal (%.1f), enum values as C identifiers (RAIL_LAYOUT_RECT, not "RECT")
- Use speed constant names where recognizable (SPIDER_SPEED, BIRD_SPEED, etc.) — or fall back to numeric

**Verify:**
1. Export `level_01_def` → `level_01_export.c` + `.h`
2. Diff against original `level_01.c` — structure and values must match (formatting may differ slightly)
3. Compile exported file: `clang -std=c11 -c level_01_export.c` — zero warnings
4. Link into game in place of original — game behavior identical
**Commit:** `feat(editor): add C code exporter — generates level .c/.h from LevelDef`

---

### T-004: Add `editor` target to Makefile
**Requires:** None
**Refs:** R-001 (standalone executable)
**Files:** `Makefile`
**Work:**
Add to Makefile:
- `EDITOR_DIR = src/editor`
- `VENDOR_DIR = vendor/cJSON`
- `EDITOR_SRCS = $(wildcard $(EDITOR_DIR)/*.c) $(VENDOR_DIR)/cJSON.c`
- `EDITOR_OBJS = $(EDITOR_SRCS:.c=.o)`
- Compilation rule for `$(EDITOR_DIR)/%.o` and `$(VENDOR_DIR)/%.o` using `$(CFLAGS) -I$(VENDOR_DIR)`
- `editor` target: link `$(EDITOR_OBJS)` → `out/super-mango-editor` with SDL2 + SDL2_image + SDL2_ttf (no SDL2_mixer)
- `run-editor` target: build + run
- Update `clean` to remove `$(EDITOR_DIR)/*.o $(VENDOR_DIR)/*.o`
- Ensure `make` (default game target) does NOT compile `src/editor/` files

**Verify:**
1. `make editor` builds `out/super-mango-editor` (even if just a stub main.c)
2. `make` (game) still works identically — no editor files compiled
3. `make clean` removes both game and editor objects
**Commit:** `build: add editor target to Makefile — standalone level editor executable`

---

### T-005: Editor window and main loop skeleton
**Requires:** T-004
**Refs:** R-001, design/EditorState section
**Files:** `src/editor/editor_main.c`, `src/editor/editor.h`, `src/editor/editor.c`
**Work:**

`editor.h`:
- Define `EditorState` struct (see design doc for full definition)
- Define `EntityType` enum (ENT_SEA_GAP through ENT_ROPE, 25 values)
- Define `EditorTool` enum (TOOL_SELECT, TOOL_PLACE, TOOL_DELETE)
- Define `Selection` struct (entity_type + index)
- Define `EditorCamera` struct (x, zoom)
- Define `EntityTextures` struct (25 SDL_Texture pointers)
- Editor constants: `EDITOR_W 1280`, `EDITOR_H 720`, `CANVAS_W 896`, `PANEL_W 384`, `TOOLBAR_H 32`, `STATUS_H 32`

`editor.c`:
- `editor_init(EditorState *es)`: create window, renderer, load font, init camera (x=0, zoom=2.0), set tool=TOOL_SELECT, clear level
- `editor_loop(EditorState *es)`: 60 FPS loop — poll events (quit, keyboard shortcuts), clear screen (#1A1A1A), present
- `editor_cleanup(EditorState *es)`: destroy font, renderer, window in reverse order, NULL all pointers
- Keyboard: ESC or window close → `es->running = 0`
- Handle `SDL_TEXTINPUT` events (enable via `SDL_StartTextInput()`)

`editor_main.c`:
- `main(int argc, char *argv[])`: SDL_Init, IMG_Init, TTF_Init, call editor_init, editor_loop, editor_cleanup, teardown
- Parse optional `argv[1]` as JSON file path to open on startup
- No SDL_mixer initialization (editor has no audio)

**Verify:**
1. `make run-editor` opens a 1280x720 window titled "Super Mango Editor"
2. Window background is dark (#1A1A1A)
3. ESC closes cleanly (no leaks with valgrind or Address Sanitizer)
4. `make` (game) still works independently
**Commit:** `feat(editor): add editor skeleton — window, main loop, EditorState`

---

## Phase 2 — Canvas and Rendering

### T-006: Canvas viewport with camera
**Requires:** T-005
**Refs:** R-003, R-010, design/Canvas section
**Files:** `src/editor/canvas.h`, `src/editor/canvas.c`
**Work:**
- Define canvas viewport rect: x=0, y=TOOLBAR_H, w=CANVAS_W, h=EDITOR_H-TOOLBAR_H-STATUS_H (656px)
- `SDL_RenderSetClipRect` to constrain drawing to canvas area
- Camera: `cam_x` (float, 0 to WORLD_W - CANVAS_W/zoom), `zoom` (float: 1.0, 2.0, 4.0)
- WASD keys: scroll cam_x ±200*dt pixels per second
- Middle mouse button drag: pan cam_x by dx/zoom pixels
- Scroll wheel: cycle zoom 1.0 → 2.0 → 4.0 → 1.0
- Coordinate transforms:
  - `world_to_screen(float wx, float wy, EditorCamera cam)` → screen x, y
  - `screen_to_world(int sx, int sy, EditorCamera cam)` → world x, y
- Clamp camera at world boundaries
- Render solid sky-blue background (#87CEEB) within canvas area

**Verify:**
1. Canvas shows sky-blue rectangle in left 896px of window
2. WASD scrolls the view horizontally
3. Scroll wheel changes zoom (visible by background scaling)
4. Camera stops at x=0 and x=WORLD_W boundaries
**Commit:** `feat(editor): add canvas viewport with camera scroll and zoom`

---

### T-007: Grid overlay and reference lines
**Requires:** T-006
**Refs:** R-003, design/Canvas section
**Files:** `src/editor/canvas.c` (extend)
**Work:**
- Draw vertical lines at every TILE_SIZE (48px) world interval — light gray (#404040, 25% alpha)
- Draw horizontal lines at every TILE_SIZE interval — same color
- Screen boundaries: vertical lines at x=0,400,800,1200 in blue (#4A90D9, 50% alpha), 2px wide
- FLOOR_Y line: horizontal at y=252 in red (#D94A4A, 50% alpha), 2px wide
- Grid density: at zoom=1.0, only draw every 2nd grid line (TILE_SIZE is 48px, already dense); at zoom=4.0 draw all
- G key toggles `es->show_grid` flag
- Labels at screen boundaries: "Screen 1", "Screen 2", "Screen 3", "Screen 4" using SDL2_ttf

**Verify:**
1. Grid visible at all zoom levels, adapts density
2. Screen boundaries clearly marked with labels
3. FLOOR_Y red line visible
4. G toggles grid on/off
**Commit:** `feat(editor): add grid overlay, screen boundaries, and FLOOR_Y reference`

---

### T-008: Load game textures and render entities
**Requires:** T-006
**Refs:** R-003, design/Canvas section
**Files:** `src/editor/canvas.c` (extend)
**Work:**

Load all 25 entity textures in `editor_init()` via `IMG_LoadTexture()`:
- Use exact same asset paths as game (e.g., `"assets/Spider.png"`, `"assets/Coin.png"`)
- Non-fatal: if a texture fails, set to NULL and skip rendering for that type (warn to stderr)

Render floor:
- Fill non-gap regions of FLOOR_Y row with `floor_tile` texture (48px tiles)
- Fill sea-gap regions with blue rectangle (#1A6BA0) to represent water

Render entities from LevelDef — first frame only (no animation), static preview:
- For each entity type: iterate array, compute `SDL_Rect src` (frame 0) and `SDL_Rect dst` (world→screen)
- Apply camera transform to dst rect
- Render order (back to front): floor/water → rails → platforms → surfaces → decorations → collectibles → enemies → hazards
- Rail paths: draw as connected line segments (dotted green lines between rail tile positions)

Entity sprite frame sizes (for source rects):
- Most entities: 48x48 (spider, bird, fish, coin, etc.)
- Verify each against actual PNG dimensions using `SDL_QueryTexture()`

**Verify:**
1. Load `level_01.json` (from T-019 seed, or hardcode level_01_def temporarily)
2. All entities visible at correct positions in canvas
3. Floor tiles and water gaps render correctly
4. Scrolling and zooming shows entities at proper scale
**Commit:** `feat(editor): load game textures and render all entity types in canvas`

---

## Phase 3 — UI Framework

### T-009: Minimal immediate-mode UI system
**Requires:** T-005
**Refs:** D-002, design/UI System section
**Files:** `src/editor/ui.h`, `src/editor/ui.c`
**Work:**

Implement widgets using SDL2 draw primitives + SDL2_ttf:

`ui_button(es, x, y, w, h, label)` → int (1 on click):
- Fill rect with button color; hover state via mouse position check
- Render label text centered with TTF_RenderText_Blended

`ui_label(es, x, y, text)`:
- Render text at position (cache texture for same text to avoid re-rendering every frame)

`ui_panel(es, x, y, w, h, title)`:
- Filled rect (#2D2D2D) + optional title bar (#3D3D3D) with title text

`ui_int_field(es, x, y, w, value)` → int (1 on change):
- Display current value as text; click to focus; keyboard input via SDL_TEXTINPUT
- Validate: only digits + minus sign. Enter confirms, Esc cancels.

`ui_float_field(es, x, y, w, value)` → int (1 on change):
- Same as int_field but allows decimal point. Display with 1 decimal.

`ui_dropdown(es, x, y, w, options, count, selected)` → int (1 on change):
- Display current option text. Click to expand list. Click option to select. Click outside to close.

`ui_separator(es, x, y, w)`:
- Thin horizontal line (#404040)

Internal state: track `hot_widget` (hovered), `active_widget` (focused/clicked), `text_input_buf` for active text fields. Use widget position as implicit ID (immediate-mode convention).

Color constants:
```c
#define UI_BG        0x2D2D2DFF
#define UI_TITLE_BG  0x3D3D3DFF
#define UI_BTN       0x4D4D4DFF
#define UI_BTN_HOT   0x5D5D5DFF
#define UI_ACCENT    0x4A90D9FF
#define UI_TEXT       0xE0E0E0FF
#define UI_INPUT_BG  0x1D1D1DFF
```

**Verify:**
1. Panel renders with title bar
2. Button highlights on hover, returns 1 on click
3. Int field accepts keyboard input, displays value, returns 1 on change
4. Float field same with decimal support
5. Dropdown opens/closes, selection works
**Commit:** `feat(editor): add immediate-mode UI system — buttons, fields, dropdowns`

---

### T-010: Entity palette panel
**Requires:** T-009, T-008
**Refs:** R-004, design/Palette section
**Files:** `src/editor/palette.h`, `src/editor/palette.c`
**Work:**

Define palette data structure — static array of entries:
```c
typedef struct {
    const char *name;       /* display name */
    EntityType  type;       /* enum value */
    const char *category;   /* "World", "Collectibles", etc. */
} PaletteEntry;
```

Hardcode all 25 entries grouped by 6 categories (World, Collectibles, Enemies, Hazards, Surfaces, Decorations).

Render palette in right panel (x=CANVAS_W, y=TOOLBAR_H, w=PANEL_W):
- Category headers as bold/larger labels
- Each entry: 32x32 sprite thumbnail (scale first frame of entity texture) + name
- Click entry → set `es->palette_type = entry.type`, set `es->tool = TOOL_PLACE`
- Highlight selected entry with accent color background
- Scroll support if entries exceed visible height (track scroll_y offset, mouse wheel within panel)

**Verify:**
1. All 25 entity types visible in palette, grouped by category
2. Clicking an entry highlights it and switches to TOOL_PLACE
3. Scroll works when list exceeds panel height
4. Sprite thumbnails render correctly for each type
**Commit:** `feat(editor): add entity palette panel with categories and sprite thumbnails`

---

### T-011: Properties panel
**Requires:** T-009, T-010
**Refs:** R-005, design/Properties Panel section
**Files:** `src/editor/properties.h`, `src/editor/properties.c`
**Work:**

Render properties panel below palette when `es->selection.index >= 0`:
- Header: entity type name + index (e.g., "Spider #2")
- Dynamically generate UI fields based on `es->selection.type`

For each entity type, map struct fields to UI widgets:
- `CoinPlacement`: x (float_field), y (float_field)
- `SpiderPlacement`: x, vx, patrol_x0, patrol_x1 (float_fields), frame_index (int_field)
- `BirdPlacement`: x, base_y, vx, patrol_x0, patrol_x1 (float_fields), frame_index (int_field)
- `AxeTrapPlacement`: pillar_x (float_field), mode (dropdown: "Pendulum"/"Spin")
- `FloatPlatformPlacement`: mode (dropdown: "Static"/"Crumble"/"Rail"), x, y, t_offset, speed (float_fields), tile_count, rail_index (int_fields)
- `SpikeBlockPlacement`: rail_index (int_field), t_offset, speed (float_fields)
- (all 25 types fully mapped per design doc table)

Get pointer to actual placement data in `es->level` based on selection type + index.
When `ui_*_field()` returns 1 (value changed): set `es->modified = 1`.
Undo integration (T-015): before applying change, snapshot old value into undo command.

**Verify:**
1. Select an entity in canvas → properties panel appears with correct fields
2. Edit a float value → entity position updates in canvas immediately
3. Edit an enum dropdown → mode changes correctly
4. Deselect → properties panel hides
**Commit:** `feat(editor): add properties panel with per-entity-type field editing`

---

## Phase 4 — Editing Tools

### T-012: Select and move tool
**Requires:** T-008, T-009
**Refs:** R-008, design/Tool System section
**Files:** `src/editor/tools.h`, `src/editor/tools.c`
**Work:**

Hit-test function:
```c
/* Check all entity arrays in LevelDef, return type+index of topmost hit */
Selection hit_test(const LevelDef *level, float world_x, float world_y);
```
- Iterate entity arrays in reverse render order (hazards first → geometry last)
- For each entity, compute bounding box from position + known sprite dimensions
- Return first AABB overlap, or {.index = -1} for no hit

Select behavior:
- Left click in canvas → `screen_to_world()` → `hit_test()`
- Hit → set `es->selection`, show properties panel
- No hit → clear selection, hide properties

Move behavior:
- Mouse down on selected entity → enter drag mode, record start position
- Mouse motion → update entity x/y (via pointer into LevelDef array)
- Shift held → snap to TILE_SIZE grid: `x = roundf(x / TILE_SIZE) * TILE_SIZE`
- Mouse up → end drag, push CMD_MOVE to undo stack if position changed

Selection visual:
- Draw 2px outline around selected entity's bounding box in accent color (#4A90D9)

**Verify:**
1. Click entity → selected (highlight visible)
2. Drag entity → moves in real time
3. Shift+drag → snaps to 48px grid
4. Click empty space → deselected
5. Selection matches correct entity when overlapping
**Commit:** `feat(editor): add select and move tool with grid snapping`

---

### T-013: Place tool
**Requires:** T-010, T-012
**Refs:** R-008, design/Tool System section
**Files:** `src/editor/tools.c` (extend)
**Work:**

Ghost preview:
- When `es->tool == TOOL_PLACE`: render entity sprite at cursor world position with 50% alpha
- `SDL_SetTextureAlphaMod(texture, 128)` before render, restore to 255 after
- Shift → snap ghost to grid

Place on click:
- Get current `es->palette_type`
- Check count against MAX_*:
  - ENT_COIN → `es->level.coin_count < MAX_COINS`
  - (all 25 types mapped to their MAX_*)
- If at limit: show status bar warning, do not place
- If within limit: populate new entry with default values (per design doc defaults table), set x/y from cursor world position
- Increment count
- Push CMD_PLACE to undo stack
- Set `es->modified = 1`
- Stay in TOOL_PLACE mode

Special case — `ENT_LAST_STAR`:
- Only one allowed (not an array). Place overwrites existing position.
- No MAX_* check needed.

Special case — `ENT_SEA_GAP`:
- Value is an int (x position), not a struct. Snap to 32px alignment (SEA_GAP_W).

Esc → return to TOOL_SELECT, clear palette selection.

**Verify:**
1. Select Coin from palette → ghost follows cursor in canvas
2. Click → coin appears at click position with default values
3. Click again → second coin placed (stays in mode)
4. Place 24 coins (MAX_COINS) → next click shows warning
5. Esc → back to select mode
**Commit:** `feat(editor): add place tool with ghost preview, defaults, and MAX limit check`

---

### T-014: Delete tool
**Requires:** T-012
**Refs:** R-008, design/Tool System section
**Files:** `src/editor/tools.c` (extend)
**Work:**

Delete selected (in TOOL_SELECT mode):
- Delete key pressed with selection active → snapshot entity data into CMD_DELETE command
- Remove from array: memmove remaining elements down, decrement count
- Push to undo stack
- Clear selection
- Set `es->modified = 1`

Right-click delete (any mode):
- Right click in canvas → hit_test at cursor position
- If hit: snapshot + remove + undo push (same as above)

Array compaction:
```c
/* Remove element at index from array of n elements, each elem_size bytes */
void array_remove(void *array, int *count, int index, size_t elem_size);
```
- `memmove(&arr[index], &arr[index+1], (count-index-1) * elem_size)`
- Decrement count

**Important:** After deletion, any undo commands referencing indices > deleted index in the same array are now stale. For v1: accept this limitation (undo after delete may reference wrong entity if multiple deletes in sequence). Robust index remapping is a v2 improvement.

**Verify:**
1. Select entity → Delete key → entity removed from canvas
2. Right-click entity → entity removed
3. Undo (Ctrl+Z) → entity restored at original position and index
4. Count in status bar decrements/increments correctly
**Commit:** `feat(editor): add delete tool — Delete key and right-click removal`

---

### T-015: Undo/redo system
**Requires:** T-012, T-013, T-014
**Refs:** R-009, design/Undo section
**Files:** `src/editor/undo.h`, `src/editor/undo.c`
**Work:**

Implement `UndoStack` struct and operations (see design doc for full struct definition):

`undo_push(stack, cmd)`:
- Append command at `stack->top`, increment top
- If top reaches UNDO_MAX, shift array down (drop oldest command)
- Clear redo stack (any new action invalidates redo history)

`undo_pop(stack, out)` → int (1 if command available):
- Pop last command, copy to out
- Push to redo stack

`redo_pop(stack, out)` → int:
- Pop from redo stack, copy to out
- Push back to undo stack

`undo_apply(es, cmd, reverse)`:
- reverse=1 (undo): restore `before` data
- reverse=0 (redo): restore `after` data
- CMD_PLACE reverse: remove entity at index, decrement count
- CMD_PLACE redo: insert entity at index, increment count
- CMD_DELETE reverse: insert entity at index, increment count
- CMD_DELETE redo: remove entity at index, decrement count
- CMD_MOVE reverse: set entity position to before.x/y
- CMD_MOVE redo: set entity position to after.x/y
- CMD_PROPERTY reverse: set field to before value
- CMD_PROPERTY redo: set field to after value

Wire into editor loop:
- Ctrl+Z → `undo_pop()` → `undo_apply(reverse=1)` → set modified=1
- Ctrl+Shift+Z → `redo_pop()` → `undo_apply(reverse=0)` → set modified=1

**Verify:**
1. Place entity → Ctrl+Z → entity removed → Ctrl+Shift+Z → entity restored
2. Move entity → Ctrl+Z → returns to original position
3. Delete entity → Ctrl+Z → entity restored at original position
4. Change property → Ctrl+Z → property reverts
5. New action after undo → redo stack cleared
**Commit:** `feat(editor): add undo/redo system — place, delete, move, property commands`

---

## Phase 5 — File Operations

### T-016: Save and load workflow
**Requires:** T-002, T-005
**Refs:** R-006
**Files:** `src/editor/editor.c` (extend)
**Work:**

Ctrl+S — Save:
- If `es->file_path` is empty (new file): prompt for path via text input overlay (ui_text_field in modal panel)
- Call `level_save_json(&es->level, es->file_path)`
- On success: set `es->modified = 0`, update title bar
- On failure: show error in status bar

Ctrl+O — Load:
- If `es->modified`: show "Unsaved changes! Save first? [Y/N/Cancel]" overlay
- Prompt for JSON path via text input
- Call `level_load_json(path, &es->level)`
- On success: set file_path, modified=0, clear undo stack, clear selection, reset camera
- On failure: show error in status bar, keep current level

Ctrl+N — New:
- If `es->modified`: unsaved changes warning
- `memset(&es->level, 0, sizeof(LevelDef))` — clear all arrays and counts
- Set `es->level.name = "Untitled"`
- Clear file_path, undo stack, selection

Title bar: `"Super Mango Editor — {filename} {*if modified}"`
- Update via `SDL_SetWindowTitle()` on save/load/modify

CLI open: if `argv[1]` provided in editor_main.c, call `level_load_json(argv[1], &es->level)` after init.

**Verify:**
1. Ctrl+S saves JSON → file appears on disk with correct content
2. Ctrl+O loads JSON → all entities appear in canvas
3. Save → quit → reopen → load → identical level state
4. Ctrl+N clears everything
5. Title bar shows filename and * indicator
6. `./out/super-mango-editor levels/level_01.json` opens directly
**Commit:** `feat(editor): add save/load/new workflow — JSON file operations`

---

### T-017: C export workflow
**Requires:** T-003, T-016
**Refs:** R-007
**Files:** `src/editor/editor.c` (extend)
**Work:**

Ctrl+E — Export:
- Show overlay prompting for:
  - Variable name (text field, default: derive from filename, e.g., `level_01`)
  - Output directory (text field, default: `src/levels/`)
- Call `level_export_c(&es->level, var_name, dir_path)`
- On success: status bar shows "Exported {var_name}.c + {var_name}.h"
- On failure: status bar shows error

Validation before export:
- Level must have a name (non-empty)
- Warn if no entities placed (empty level)

**Verify:**
1. Ctrl+E → fill in variable name → exported .c + .h appear in target directory
2. Compile exported file: `clang -std=c11 -c src/levels/level_XX.c` — zero warnings
3. Replace level_01 in game → `make run` → game plays the exported level correctly
**Commit:** `feat(editor): add C code export workflow — Ctrl+E generates compilable level source`

---

## Phase 6 — Polish and Integration

### T-018: Toolbar and status bar
**Requires:** T-009
**Refs:** design/Window Layout section
**Files:** `src/editor/editor.c` (extend)
**Work:**

Top toolbar (y=0, h=TOOLBAR_H=32px, full width):
- Tool buttons: [Select] [Place] [Delete] — highlight active tool
- Zoom display: "Zoom: {1x/2x/4x}" — click to cycle
- Grid toggle: [Grid: ON/OFF]
- File buttons: [New] [Open] [Save] [Export]

Bottom status bar (y=EDITOR_H-STATUS_H, h=32px, full width):
- Left: mouse world coordinates "({x}, {y})" — updated every frame from cursor position
- Center: current tool name + palette type (if in PLACE mode)
- Center-right: total entity count across all arrays
- Right: file path + modified indicator

**Verify:**
1. Toolbar buttons match current tool state
2. Status bar shows real-time mouse world coordinates
3. Entity count updates on place/delete
4. File path and * indicator match save state
**Commit:** `feat(editor): add toolbar and status bar — tool buttons, coords, entity count`

---

### T-019: Seed editor with level_01 conversion
**Requires:** T-002
**Refs:** Success criteria #3, #4
**Files:** `levels/level_01.json` (new, generated)
**Work:**

Write a one-time conversion utility (can be a standalone `main()` in a temp file or a function called from editor):
1. Include `level_01.h` to access `level_01_def`
2. Call `level_to_json(&level_01_def)` → `cJSON_Print()` → write to `levels/level_01.json`
3. Verify round-trip: load the JSON back → compare all fields

This JSON file becomes:
- The reference test case for serializer validation
- The first level editable in the editor
- The template for JSON schema documentation

**Verify:**
1. `levels/level_01.json` contains all 25 entity types with correct values
2. Load in editor → visual layout matches game's Sandbox level exactly
3. Round-trip: JSON → LevelDef → JSON → identical content (byte-level)
4. Export back to .c → compile → game behavior identical to original
**Commit:** `feat(editor): convert level_01 to JSON — seed file for editor and round-trip validation`

---

### T-020: Rail and sea gap editing tools
**Requires:** T-012, T-013
**Refs:** R-002, R-005 (rails and sea gaps are the most complex placement types)
**Files:** `src/editor/tools.c` (extend)
**Work:**

Sea gap tool (within TOOL_PLACE for ENT_SEA_GAP):
- Click on floor region → add sea_gap at snapped x (snap to SEA_GAP_W=32px grid)
- Visual: render gap regions as blue overlay on floor
- Right-click on gap → remove it
- Validation: prevent overlapping gaps (check existing sea_gaps for x collision)

Rail tool (within TOOL_PLACE for ENT_RAIL):
- Two-step creation:
  1. Click to set origin (x, y) — show crosshair
  2. Click again to set extent (w, h in tiles) — show preview rectangle/line
- Mode selection: dropdown in properties panel (RECT / HORIZ)
- RECT: draw closed rectangle path with dotted lines at rail tile positions
- HORIZ: draw horizontal line with optional end-cap indicator
- HORIZ end_cap toggle: checkbox in properties panel

Rail-dependent entities (spike_blocks, float_platforms with mode=RAIL):
- Properties panel shows `rail_index` dropdown listing available rails (0 to rail_count-1)
- Visual: draw dotted line from entity to its referenced rail path

**Verify:**
1. Can place sea gaps on floor — water overlay appears
2. Can remove sea gaps — floor restores
3. Can create RECT and HORIZ rails — path lines visible
4. Spike blocks reference correct rail by index
5. Float platforms in RAIL mode reference correct rail
**Commit:** `feat(editor): add rail path and sea gap editing tools`

---

## Dependency Graph

```
T-001 (cJSON)
  └── T-002 (serializer)
        ├── T-003 (exporter)
        │     └── T-017 (export workflow)
        ├── T-016 (save/load)
        └── T-019 (level_01 JSON seed)

T-004 (Makefile)
  └── T-005 (editor skeleton)
        ├── T-006 (canvas viewport)
        │     ├── T-007 (grid overlay)
        │     └── T-008 (entity rendering)
        │           ├── T-010 (palette)
        │           │     └── T-013 (place tool)
        │           └── T-012 (select/move)
        │                 ├── T-013 (place tool)
        │                 ├── T-014 (delete tool)
        │                 └── T-015 (undo/redo)
        ├── T-009 (UI system)
        │     ├── T-010 (palette)
        │     ├── T-011 (properties)
        │     └── T-018 (toolbar/status)
        └── T-016 (save/load)

T-020 (rails/sea gaps) ← T-012, T-013
```

## Parallelization Opportunities

| Parallel batch | Tasks | Rationale |
|---------------|-------|-----------|
| Batch 1 | T-001 + T-004 | Vendor lib and Makefile are independent |
| Batch 2 | T-002 + T-005 | Serializer (pure C logic) and editor skeleton (SDL window) are independent |
| Batch 3 | T-006 + T-009 | Canvas rendering and UI widgets are independent subsystems |
| Batch 4 | T-007 + T-008 | Grid overlay and entity rendering touch different render layers |
| Batch 5 | T-010 + T-012 | Palette (UI panel) and select tool (input logic) are independent |
| Batch 6 | T-003 + T-018 | Exporter (fprintf logic) and toolbar/status (UI) are independent |

## Implementation Order (Recommended)

Linear path through critical chain, with parallel work where possible:

```
Week 1: T-001 + T-004 → T-002 + T-005 → T-019
         (foundation: cJSON, Makefile, serializer, skeleton, seed JSON)

Week 2: T-006 + T-009 → T-007 + T-008 → T-010
         (visual: canvas, UI widgets, grid, entity rendering, palette)

Week 3: T-012 → T-013 + T-014 → T-015 → T-011
         (interaction: select/move, place, delete, undo, properties)

Week 4: T-003 → T-016 + T-018 → T-017 → T-020
         (file ops: exporter, save/load, toolbar, export workflow, rails/gaps)
```

## Traceability Matrix

| Requirement | Tasks |
|-------------|-------|
| R-001 (Standalone executable) | T-004, T-005 |
| R-002 (Full entity support) | T-008, T-010, T-011, T-012, T-013, T-020 |
| R-003 (Visual canvas) | T-006, T-007, T-008 |
| R-004 (Entity palette) | T-010 |
| R-005 (Property editing) | T-011 |
| R-006 (JSON serialization) | T-001, T-002, T-016, T-019 |
| R-007 (C code export) | T-003, T-017 |
| R-008 (Core editing ops) | T-012, T-013, T-014 |
| R-009 (Undo/redo) | T-015 |
| R-010 (Camera navigation) | T-006 |
