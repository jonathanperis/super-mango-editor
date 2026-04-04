# Design: Visual Level Editor

## Architecture Overview

```
super-mango-editor (standalone executable)
│
├── editor_main.c          ← SDL init, window (1280x720), entry point
│
├── EditorState            ← Central struct (like GameState for the game)
│   ├── SDL_Renderer       ← Shared renderer
│   ├── LevelDef           ← The level being edited (mutable, not const)
│   ├── entity textures    ← All game textures loaded for preview
│   ├── camera (scroll/zoom)
│   ├── tool state         ← Current mode (place/select/move)
│   ├── selection          ← Currently selected entity reference
│   ├── undo stack         ← Command history
│   └── UI state           ← Panel layouts, palette selection
│
├── Editor Loop            ← poll events → handle input → update UI → render
│   ├── Input Phase        ← Mouse/keyboard → tool actions
│   ├── UI Phase           ← Palette, properties panel, toolbar
│   └── Render Phase       ← Canvas viewport + UI overlay
│
└── Serializer             ← JSON ↔ LevelDef ↔ C code generation
```

## Code Organization

```
src/
├── editor/                     ← Editor-exclusive code
│   ├── editor_main.c           ← Entry point: SDL init, EditorState, main loop
│   ├── editor.h                ← EditorState struct, editor constants
│   ├── editor.c                ← Editor loop: events → update → render
│   ├── canvas.h/.c             ← World viewport: scroll, zoom, grid, entity rendering
│   ├── palette.h/.c            ← Entity type selection panel
│   ├── properties.h/.c         ← Property editor panel for selected entity
│   ├── tools.h/.c              ← Place/select/move/delete tool logic
│   ├── undo.h/.c               ← Command stack (undo/redo)
│   ├── serializer.h/.c         ← JSON load/save using cJSON
│   ├── exporter.h/.c           ← C code generation from LevelDef
│   └── ui.h/.c                 ← Minimal immediate-mode UI widgets
├── levels/
│   ├── level.h                 ← LevelDef struct (shared, unchanged)
│   └── ...
├── (game source files unchanged)
└── ...

vendor/
└── cJSON/
    ├── cJSON.h
    └── cJSON.c
```

### Shared vs Editor-Only Code

| Shared (used by both game and editor) | Editor-only |
|---------------------------------------|-------------|
| `src/levels/level.h` — LevelDef struct, placement structs, MAX_* constants | `src/editor/*` — all editor modules |
| `src/game.h` — GAME_W, GAME_H, TILE_SIZE, FLOOR_Y, WORLD_W constants | `vendor/cJSON/` — JSON library |
| Entity header files — struct definitions for collision rect sizes | |
| `assets/*.png` — sprite textures (loaded at runtime, no code sharing needed) | |

**Key constraint:** The editor includes `level.h` and `game.h` for struct definitions and constants, but does NOT include `game.c`, `player/`, or any game logic. The editor has its own main loop and rendering pipeline.

## Component Design

### Canvas (viewport)

- Renders the 1600x300 world in a viewport occupying ~70% of the editor window
- Camera state: `cam_x` (float), `zoom` (1.0, 2.0, 4.0)
- Grid: draws lines at every TILE_SIZE interval, color-coded:
  - Light gray: tile grid
  - Blue dashed: screen boundaries (x = 0, 400, 800, 1200)
  - Red dashed: FLOOR_Y line
- Entity rendering: loads all game textures, draws each entity's first frame (static, no animation) at its world position, offset by camera
- Hover highlight: when mouse is over an entity, draw selection outline
- Ghost preview: when in placement mode, draw semi-transparent entity at cursor position

### Palette

- Vertical panel on the right side
- Categories as collapsible sections:
  - World: Sea Gaps, Rails
  - Collectibles: Coin, Yellow Star, Last Star
  - Enemies: Spider, Jumping Spider, Bird, Faster Bird, Fish, Faster Fish
  - Hazards: Axe Trap, Circular Saw, Spike Row, Spike Platform, Spike Block
  - Surfaces: Platform, Float Platform, Bridge, Bouncepad (S/M/H)
  - Decorations: Vine, Ladder, Rope
- Each entry shows a small icon (first frame of sprite) + name
- Click to select → enters placement mode for that type

### Properties Panel

- Below the palette (or toggled)
- Shows editable fields for the currently selected entity
- Field types:
  - Float input: x, y, vx, base_y, patrol_left, patrol_right
  - Int input: tile_count, brick_count, rail_index, start_frame
  - Enum dropdown: mode (PENDULUM/SPIN, STATIC/CRUMBLE/RAIL, GREEN/WOOD/RED)
  - Bool toggle: active
- Changes are applied immediately and recorded in undo stack

### Tool System

```
enum EditorTool {
    TOOL_SELECT,     ← Default. Click to select, drag to move
    TOOL_PLACE,      ← Active after palette selection. Click to create
    TOOL_DELETE       ← Click to remove entity
};
```

- **Select/Move**: click entity → select. Drag selected → move. Click empty → deselect.
- **Place**: click canvas → create new entity of palette type at snapped position. Stays in place mode (click again to place another).
- **Delete**: click entity → remove from LevelDef array, compact remaining. Esc → back to select.

### Undo/Redo

Command pattern with a stack:

```c
typedef enum {
    CMD_PLACE,          /* entity_type, index, placement data */
    CMD_DELETE,         /* entity_type, index, placement data (for restore) */
    CMD_MOVE,           /* entity_type, index, old_x, old_y, new_x, new_y */
    CMD_PROPERTY,       /* entity_type, index, field_id, old_value, new_value */
} CommandType;

typedef struct {
    CommandType type;
    int entity_type;    /* which placement array */
    int entity_index;   /* index within that array */
    /* union of data for each command type */
} Command;
```

- Undo stack: max 256 commands
- Redo stack: cleared on new action (standard behavior)

### Serializer (JSON ↔ LevelDef)

Uses cJSON (MIT, single-file C library).

**JSON schema** — mirrors LevelDef exactly:

```json
{
  "name": "Sandbox",
  "sea_gaps": [0, 192, 560, 928, 1152],
  "rails": [
    { "layout": "RECT", "origin_col": 10, "origin_row": 2, "cols": 10, "rows": 6, "closed": true }
  ],
  "platforms": [
    { "pillar_height": 2 }
  ],
  "coins": [
    { "x": 120.0, "y": 200.0 }
  ],
  "spiders": [
    { "x": 300.0, "vx": 50.0, "patrol_left": 250.0, "patrol_right": 380.0, "start_frame": 0 }
  ]
}
```

**Functions:**
- `cJSON *level_to_json(const LevelDef *def)` — serialize all fields
- `int level_from_json(const cJSON *json, LevelDef *def)` — deserialize, validate counts against MAX_*
- `int level_save_json(const LevelDef *def, const char *path)` — write file
- `int level_load_json(const char *path, LevelDef *def)` — read file

### Exporter (LevelDef → C code)

Generates a compilable C source file matching the style of `level_01.c`:

- Header file: `extern const LevelDef level_XX;`
- Source file: `const LevelDef level_XX = { ... };` with all arrays populated
- Formatted identically to hand-written levels (struct designator syntax, comments per section)

### UI System

Minimal immediate-mode UI rendered with SDL2 + SDL2_ttf:

```c
/* Core widgets */
int  ui_button(EditorState *es, int x, int y, int w, int h, const char *label);
void ui_label(EditorState *es, int x, int y, const char *text);
int  ui_text_field(EditorState *es, int x, int y, int w, char *buf, int buf_size);
int  ui_int_field(EditorState *es, int x, int y, int w, int *value);
int  ui_float_field(EditorState *es, int x, int y, int w, float *value);
int  ui_dropdown(EditorState *es, int x, int y, int w, const char **options, int count, int *selected);
void ui_panel(EditorState *es, int x, int y, int w, int h, const char *title);
```

- All widgets return 1 if interacted with (for undo recording)
- Font: `assets/round9x13.ttf` (already in the project)
- Colors: dark gray panels, light text, blue highlights for selection

## Makefile Integration

```makefile
# New variables
EDITOR_DIR  = src/editor
EDITOR_SRCS = $(wildcard $(EDITOR_DIR)/*.c) vendor/cJSON/cJSON.c
EDITOR_OBJS = $(EDITOR_SRCS:.c=.o)

# Shared objects needed by editor (level.h constants, no game logic)
EDITOR_SHARED_OBJS = # none — editor only needs headers, not .o files

# New targets
editor: $(EDITOR_OBJS)
	$(CC) $(CFLAGS) -o out/super-mango-editor $(EDITOR_OBJS) $(LDFLAGS)

run-editor: editor
	./out/super-mango-editor

clean: # updated to also remove editor objects
	rm -rf out/ $(EDITOR_DIR)/*.o vendor/cJSON/*.o
```

## Decision Points (Pending)

| ID | Decision | Impact |
|----|----------|--------|
| D-001 | Serialization: JSON-only vs JSON+C pipeline | Determines if exporter.c is needed |
| D-002 | UI: Custom vs Nuklear | Determines ui.c complexity |
| D-003 | Game JSON loader | Determines if cJSON is also added to the game build |
