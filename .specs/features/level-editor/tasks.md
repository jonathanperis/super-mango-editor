# Tasks: Visual Level Editor

## Phase 1 — Infrastructure

### T-001: Integrate cJSON vendor library
**Requires:** None
**Files:** `vendor/cJSON/cJSON.h`, `vendor/cJSON/cJSON.c`
**Work:**
- Download cJSON 1.7.x (single .c + .h, MIT license)
- Place in `vendor/cJSON/`
- Verify compiles with `clang -std=c11 -c vendor/cJSON/cJSON.c`
**Verify:** `cJSON_CreateObject()` compiles and links without errors

### T-002: Create JSON serializer (LevelDef ↔ JSON)
**Requires:** T-001
**Files:** `src/editor/serializer.h`, `src/editor/serializer.c`
**Work:**
- `level_to_json(const LevelDef *def)` — serialize all 25 placement types
- `level_from_json(const cJSON *json, LevelDef *def)` — deserialize with bounds checking
- `level_save_json(const LevelDef *def, const char *path)` — write to disk
- `level_load_json(const char *path, LevelDef *def)` — read from disk
- Handle all placement struct fields per level.h definitions
**Verify:** Convert level_01 → JSON → LevelDef, compare all fields match original. Round-trip test.

### T-003: Create C code exporter
**Requires:** T-002
**Files:** `src/editor/exporter.h`, `src/editor/exporter.c`
**Work:**
- `level_export_c(const LevelDef *def, const char *var_name, const char *path)` — generates .c + .h
- Output must match `level_01.c` style exactly (struct designators, section comments, formatting)
**Verify:** Export level_01 data → compile exported .c → run game with it → identical behavior

### T-004: Add `editor` target to Makefile
**Requires:** None
**Files:** `Makefile`
**Work:**
- Add EDITOR_DIR, EDITOR_SRCS, EDITOR_OBJS variables
- Add `editor` target that compiles editor sources + cJSON → `out/super-mango-editor`
- Add `run-editor` target
- Add `clean-editor` or update `clean` to include editor objects
- Include path must cover `src/`, `src/levels/`, `src/editor/`, `vendor/`
**Verify:** `make editor` produces `out/super-mango-editor`. `make clean` removes editor artifacts. `make` (game) still works unaffected.

### T-005: Editor window and main loop skeleton
**Requires:** T-004
**Files:** `src/editor/editor_main.c`, `src/editor/editor.h`, `src/editor/editor.c`
**Work:**
- `editor_main.c`: SDL/IMG/TTF init, create 1280x720 window, enter editor loop, cleanup
- `editor.h`: EditorState struct with renderer, window, running flag, font
- `editor.c`: `editor_init()`, `editor_loop()` (poll events, render clear, present), `editor_cleanup()`
- Load `round9x13.ttf` font
- ESC or window close to quit
**Verify:** `make run-editor` opens a blank 1280x720 window with title "Super Mango Editor". Closes cleanly.

---

## Phase 2 — Canvas and Rendering

### T-006: Canvas viewport with camera
**Requires:** T-005
**Files:** `src/editor/canvas.h`, `src/editor/canvas.c`
**Work:**
- Camera state: `cam_x` (0 to WORLD_W - viewport_w), `zoom` (1.0, 2.0, 4.0)
- WASD or middle-mouse drag for horizontal scrolling
- Scroll wheel for zoom
- Viewport occupies left 70% of window (896x720 at 1280x720)
- Transform: world coords → screen coords via `(world_x - cam_x) * zoom`
- Reverse transform for mouse: screen → world coords (for tool input)
**Verify:** Can scroll through full 1600px world. Zoom in/out works. Camera clamps at world boundaries.

### T-007: Grid overlay and reference lines
**Requires:** T-006
**Files:** `src/editor/canvas.c` (extend)
**Work:**
- Draw tile grid (TILE_SIZE intervals) in light gray
- Draw screen boundaries (x=0,400,800,1200) in blue dashed lines
- Draw FLOOR_Y horizontal line in red
- Grid visibility toggleable (G key)
- Grid density adapts to zoom level (hide fine grid at 1x zoom)
**Verify:** Grid, screen markers, and FLOOR_Y line are visible at all zoom levels. G toggles grid.

### T-008: Load game textures and render entities
**Requires:** T-006
**Files:** `src/editor/canvas.c` (extend)
**Work:**
- Load all game asset textures (reuse same paths as game: `assets/*.png`)
- For each entity in LevelDef, render its first frame (static) at world position
- Apply camera offset and zoom to destination rects
- Render floor tiles in non-sea-gap regions
- Render water strips in sea-gap regions
- Layer order: background → floor/water → surfaces → collectibles → enemies → hazards
**Verify:** Load level_01 JSON (from T-002), render in editor. Visual output matches game's level layout.

---

## Phase 3 — UI Framework

### T-009: Minimal immediate-mode UI system
**Requires:** T-005
**Files:** `src/editor/ui.h`, `src/editor/ui.c`
**Work:**
- `ui_button()` — clickable labeled rectangle, returns 1 on click
- `ui_label()` — text rendering at position
- `ui_panel()` — filled rectangle with title bar
- `ui_int_field()` — editable integer with keyboard input
- `ui_float_field()` — editable float with keyboard input
- `ui_dropdown()` — click to open list, select option
- Rendering: SDL2_ttf for text, SDL_RenderFillRect/DrawRect for boxes
- Input: mouse position + click state + keyboard text input
- Color scheme: dark panels (#2D2D2D), light text (#E0E0E0), blue selection (#4A90D9)
**Verify:** Can render a panel with buttons, labels, and input fields. Buttons respond to clicks. Text fields accept keyboard input.

### T-010: Entity palette panel
**Requires:** T-009, T-008
**Files:** `src/editor/palette.h`, `src/editor/palette.c`
**Work:**
- Right panel (30% of window width)
- Scrollable list of entity types, grouped by category
- Each entry: small sprite icon (32x32 thumbnail) + name
- Categories: World, Collectibles, Enemies, Hazards, Surfaces, Decorations
- Click entry → set current tool to TOOL_PLACE with that entity type
- Selected entry highlighted in blue
**Verify:** All 25 entity types listed in correct categories. Clicking sets placement mode. Current selection is highlighted.

### T-011: Properties panel
**Requires:** T-009, T-010
**Files:** `src/editor/properties.h`, `src/editor/properties.c`
**Work:**
- Appears below palette when an entity is selected
- Dynamically shows fields based on entity type
- Each placement struct's fields mapped to UI widgets:
  - CoinPlacement: x (float), y (float)
  - SpiderPlacement: x, vx, patrol_left, patrol_right, start_frame
  - BirdPlacement: base_y, patrol_left, patrol_right, start_frame
  - FloatPlatformPlacement: mode (dropdown), x, y, w_tiles, rail_index
  - SpikeRowPlacement: x, tile_count
  - (all other types similarly mapped)
- Property changes update LevelDef immediately
- Property changes generate undo commands
**Verify:** Select an entity → see its properties. Edit a value → entity updates in canvas. Undo reverts the change.

---

## Phase 4 — Editing Tools

### T-012: Select and move tool
**Requires:** T-008, T-009
**Files:** `src/editor/tools.h`, `src/editor/tools.c`
**Work:**
- Click on entity in canvas → select (check AABB hit test against all entities)
- Selected entity gets highlight outline (2px colored border)
- Drag selected entity → move to new position
- Snap to grid: hold Shift to snap to TILE_SIZE grid
- Click empty space → deselect
- Selection state stored in EditorState: entity_type + index
**Verify:** Can click to select any entity. Drag moves it. Shift snaps to grid. Click empty deselects.

### T-013: Place tool
**Requires:** T-010, T-012
**Files:** `src/editor/tools.c` (extend)
**Work:**
- Active when palette entity is selected
- Ghost preview: semi-transparent sprite follows cursor on canvas
- Click → append new entity to appropriate LevelDef array, increment count
- Check MAX_* limit before placing (warn if full)
- New entity gets default values for all fields (sensible defaults per type)
- After placing, stay in place mode (click again to place another)
- Esc → return to select tool
**Verify:** Select coin from palette. Ghost follows cursor. Click to place. Coin appears in canvas. Count incremented. Can place multiple. Esc exits mode.

### T-014: Delete tool
**Requires:** T-012
**Files:** `src/editor/tools.c` (extend)
**Work:**
- Delete key on selected entity → remove from array, compact remaining
- Right-click on entity → delete without selecting first
- Stores deleted entity data in undo command for restoration
- Decrement count in LevelDef
**Verify:** Select entity, press Delete → gone. Right-click entity → gone. Undo restores it. Count decremented/incremented correctly.

### T-015: Undo/redo system
**Requires:** T-012, T-013, T-014
**Files:** `src/editor/undo.h`, `src/editor/undo.c`
**Work:**
- Command struct stores: type (place/delete/move/property), entity_type, index, before/after data
- Undo stack: array of 256 commands with top pointer
- Redo stack: cleared on new action
- Ctrl+Z → pop undo, push to redo, reverse the action
- Ctrl+Shift+Z → pop redo, push to undo, replay the action
- Actions recorded: place, delete, move (old/new position), property change (old/new value)
**Verify:** Place entity → undo removes it → redo restores it. Move entity → undo returns to old position. Property change → undo reverts value.

---

## Phase 5 — File Operations

### T-016: Save and load workflow
**Requires:** T-002, T-005
**Files:** `src/editor/editor.c` (extend)
**Work:**
- Ctrl+S → save current LevelDef to JSON (prompt for path on first save, remember after)
- Ctrl+O → prompt for JSON file path, load into editor
- Ctrl+N → clear LevelDef to empty, reset editor state
- Title bar shows current file path and modified indicator (*)
- Warn on unsaved changes when loading new file or quitting
- Command line: `super-mango-editor [path.json]` to open a file directly
**Verify:** Save level → quit → reopen → load → all entities preserved. New creates empty level. Unsaved warning appears.

### T-017: C export workflow
**Requires:** T-003, T-016
**Files:** `src/editor/editor.c` (extend)
**Work:**
- Ctrl+E → export current level as .c + .h files
- Prompt for variable name (e.g., `level_02`) and output path
- Generated files compile with the game's Makefile
- Status bar confirmation message on successful export
**Verify:** Export level → copy to `src/levels/` → `make` → game runs with exported level.

---

## Phase 6 — Polish and Integration

### T-018: Toolbar and status bar
**Requires:** T-009
**Files:** `src/editor/editor.c` (extend)
**Work:**
- Top toolbar: tool buttons (Select, Place, Delete), zoom display, grid toggle
- Bottom status bar: mouse world coords, current tool, entity count, file path, save indicator
- Keyboard shortcuts displayed in toolbar tooltips
**Verify:** Toolbar shows current tool state. Status bar updates with mouse movement. All info accurate.

### T-019: Seed editor with level_01 conversion
**Requires:** T-002
**Files:** `levels/level_01.json` (new, generated)
**Work:**
- Write a one-time conversion: read `level_01` const data → call `level_save_json()` → output JSON
- This becomes the reference test file for round-trip validation
- Verify visual accuracy: load in editor, compare with game screenshot
**Verify:** `level_01.json` loads in editor and matches the game's Sandbox level layout exactly.

### T-020: Rail and sea gap editing tools
**Requires:** T-012, T-013
**Files:** `src/editor/tools.c` (extend)
**Work:**
- Sea gaps: click on floor region → mark as gap (32px wide), click again to remove
- Rails: specialized tool — click to place waypoints, define RECT or HORIZ layout
- Visual feedback: sea gaps shown as blue water overlay, rails shown as dotted path lines
- These are the most complex entity types and may need special UI
**Verify:** Can create/remove sea gaps. Can define rail paths. Spike blocks and float platforms reference rails correctly.

---

## Dependency Graph

```
T-001 (cJSON)
  └── T-002 (serializer)
        ├── T-003 (exporter)
        │     └── T-017 (export workflow)
        ├── T-016 (save/load)
        └── T-019 (level_01 conversion)

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

| Can be done in parallel | Because |
|------------------------|---------|
| T-001 + T-004 | Independent: vendor lib vs Makefile |
| T-006 + T-009 | Independent: canvas rendering vs UI widgets |
| T-007 + T-008 | Both extend canvas, but touch different render layers |
| T-003 + T-005 | Exporter is pure C generation, editor skeleton is SDL setup |
| T-010 + T-012 | Palette is UI, select/move is input handling |
