# Design: Visual Level Editor

## Architectural Decisions

| ID | Decision | Choice | Impact |
|----|----------|--------|--------|
| D-001 | Serialization | JSON canonical + C export pipeline | cJSON is editor-only; game stays pure C structs |
| D-002 | UI Framework | Custom SDL2 + SDL2_ttf immediate-mode | ~7 widget functions in ui.c, no external deps |
| D-003 | Game JSON Loader | No — game reads only compiled `const LevelDef` | Play-test cycle: export → `make run` |

## Architecture Overview

```
super-mango-editor (standalone executable)
│
├── editor_main.c              ← SDL/IMG/TTF init, window 1280x720, entry point
│
├── EditorState                ← Central struct (analogous to GameState)
│   ├── SDL_Window *window     ← 1280x720 editor window
│   ├── SDL_Renderer *renderer ← GPU drawing context
│   ├── TTF_Font *font         ← round9x13.ttf for all UI text
│   ���── LevelDef level         ← Mutable level data being edited
│   ├── EntityTextures textures← All game sprite textures for preview
│   ├── Camera camera          ← cam_x + zoom for world viewport
│   ├── EditorTool tool        ← SELECT / PLACE / DELETE
│   ├── Selection selection    ← entity_type + index of selected entity
│   ├── PaletteState palette   ← Which category/type is chosen
│   ├─��� UndoStack undo         ← 256-entry command history
│   ├── char file_path[256]    ← Current JSON file path (empty = unsaved)
│   ├── int modified           ← 1 = unsaved changes exist
│   └── int running            ← Loop flag
│
├── Editor Loop                ← 60 FPS: poll events → tools → UI → render
│   ├── Event Phase            ← SDL_PollEvent: keyboard shortcuts, mouse, quit
│   ├── Tool Phase             ← Apply select/place/move/delete based on input
│   ├── UI Phase               ← Palette + properties + toolbar (immediate-mode)
│   └── Render Phase           ← Clear → canvas viewport → UI panels → present
│
└── File Pipeline              ← JSON ↔ LevelDef ↔ C source generation
    ├── serializer.c           ← cJSON read/write (25 placement types)
    └── exporter.c             ← fprintf-based C code generator
```

## Code Organization

```
super-mango-game/
├── src/
│   ├── editor/                         ← NEW: editor-exclusive code
│   │   ├── editor_main.c              ← Entry point: SDL/IMG/TTF init, main()
│   │   ├── editor.h                   ← EditorState struct, constants, enums
│   │   ├── editor.c                   ← editor_init(), editor_loop(), editor_cleanup()
│   │   ├── canvas.h / canvas.c        ← World viewport: camera, grid, entity rendering
│   │   ├── palette.h / palette.c      ← Entity type selection panel
│   │   ├── properties.h / properties.c← Property editor for selected entity
│   │   ├── tools.h / tools.c          ← Select/place/move/delete tool logic
│   │   ├── undo.h / undo.c            ← Command stack (undo/redo)
│   │   ├── serializer.h / serializer.c← JSON ↔ LevelDef using cJSON
│   │   ├── exporter.h / exporter.c    ← LevelDef → .c + .h code generation
│   │   └── ui.h / ui.c               ← Immediate-mode UI widgets (SDL2_ttf)
│   ├── levels/
│   │   ├── level.h                    ← SHARED: LevelDef + all placement structs
│   │   └── level_01.c / .h           ← Existing level (unchanged)
│   ├── game.h                         ← SHARED: constants (GAME_W, TILE_SIZE, etc.)
│   └── (all other game source files — untouched)
├── vendor/
│   └── cJSON/                          ← NEW: editor-only dependency
│       ├── cJSON.h                    ← cJSON API header
│       └── cJSON.c                    ← cJSON implementation (~1500 lines)
├── levels/                             ← NEW: JSON level files (canonical format)
│   └── level_01.json                  ← Converted from level_01.c const data
└── Makefile                            ← Updated with `editor` target
```

### Header Dependency Chain

The editor includes game headers for struct definitions and constants only:

```
editor_main.c
  └── editor.h
        ├── SDL.h, SDL_image.h, SDL_ttf.h
        ├── levels/level.h              ← LevelDef, all placement structs
        │     ├── game.h                ← GAME_W, GAME_H, TILE_SIZE, FLOOR_Y, WORLD_W, MAX_*
        │     ├── surfaces/bouncepad.h  ← BouncepadType enum, MAX_BOUNCEPADS_*
        │     ├── hazards/axe_trap.h    ← AxeTrapMode enum, MAX_AXE_TRAPS
        │     ├── surfaces/float_platform.h ← FloatPlatformMode enum
        │     └── surfaces/rail.h       ← MAX_RAILS
        └── cJSON.h                     ← serializer only
```

**Critical:** `game.h` includes `player/player.h` and all entity headers — the editor will transitively include them. This is acceptable because:
- The editor only uses struct definitions and `#define` constants from these headers
- The editor does NOT link any game `.o` files (no `game.c`, `player.c`, etc.)
- If this becomes a compilation issue, extract constants into a `game_constants.h` (deferred to v2)

### What the Editor Links vs What the Game Links

| Component | Game (`make`) | Editor (`make editor`) |
|-----------|---------------|----------------------|
| `src/editor/*.c` | No | Yes |
| `vendor/cJSON/cJSON.c` | No | Yes |
| `src/game.c` | Yes | No |
| `src/player/*.c` | Yes | No |
| `src/entities/*.c` | Yes | No |
| `src/hazards/*.c` | Yes | No |
| `src/collectibles/*.c` | Yes | No |
| `src/surfaces/*.c` | Yes | No |
| `src/effects/*.c` | Yes | No |
| `src/screens/*.c` | Yes | No |
| `src/core/*.c` | Yes | No |
| `src/levels/*.c` | Yes | No |
| SDL2 + SDL2_image | Yes | Yes |
| SDL2_ttf | Yes (linked) | Yes (actively used) |
| SDL2_mixer | Yes | No (not needed) |

## Component Design

### EditorState Struct (editor.h)

```c
/* Entity type identifiers — index into LevelDef arrays */
typedef enum {
    ENT_SEA_GAP, ENT_RAIL, ENT_PLATFORM,
    ENT_COIN, ENT_YELLOW_STAR, ENT_LAST_STAR,
    ENT_SPIDER, ENT_JUMPING_SPIDER, ENT_BIRD, ENT_FASTER_BIRD, ENT_FISH, ENT_FASTER_FISH,
    ENT_AXE_TRAP, ENT_CIRCULAR_SAW, ENT_SPIKE_ROW, ENT_SPIKE_PLATFORM, ENT_SPIKE_BLOCK,
    ENT_FLOAT_PLATFORM, ENT_BRIDGE,
    ENT_BOUNCEPAD_SMALL, ENT_BOUNCEPAD_MEDIUM, ENT_BOUNCEPAD_HIGH,
    ENT_VINE, ENT_LADDER, ENT_ROPE,
    ENT_COUNT  /* 25 total */
} EntityType;

typedef enum { TOOL_SELECT, TOOL_PLACE, TOOL_DELETE } EditorTool;

typedef struct {
    EntityType type;   /* which placement array */
    int        index;  /* index within that array, -1 = none */
} Selection;

/* All game textures needed for entity preview rendering */
typedef struct {
    SDL_Texture *floor_tile;
    SDL_Texture *platform;
    SDL_Texture *water;
    SDL_Texture *spider;
    SDL_Texture *jumping_spider;
    SDL_Texture *bird;
    SDL_Texture *faster_bird;
    SDL_Texture *fish;
    SDL_Texture *faster_fish;
    SDL_Texture *coin;
    SDL_Texture *yellow_star;
    SDL_Texture *last_star;
    SDL_Texture *axe_trap;
    SDL_Texture *circular_saw;
    SDL_Texture *spike;
    SDL_Texture *spike_platform;
    SDL_Texture *spike_block;
    SDL_Texture *float_platform;
    SDL_Texture *bridge;
    SDL_Texture *bouncepad_small;
    SDL_Texture *bouncepad_medium;
    SDL_Texture *bouncepad_high;
    SDL_Texture *vine;
    SDL_Texture *ladder;
    SDL_Texture *rope;
    SDL_Texture *rail;
} EntityTextures;

typedef struct {
    float x;       /* left edge of visible world viewport */
    float zoom;    /* 1.0, 2.0, or 4.0 */
} EditorCamera;

typedef struct {
    SDL_Window   *window;
    SDL_Renderer *renderer;
    TTF_Font     *font;

    LevelDef      level;        /* the level being edited (mutable) */
    EntityTextures textures;    /* all game sprites for preview */
    EditorCamera  camera;
    EditorTool    tool;
    Selection     selection;
    EntityType    palette_type; /* which entity type is selected in palette */
    UndoStack     undo;

    char          file_path[256]; /* current JSON path, empty = new file */
    int           modified;       /* 1 = unsaved changes */
    int           show_grid;      /* 1 = grid overlay visible */
    int           running;
} EditorState;
```

### Canvas — World Viewport (canvas.c)

**Layout:** Left 896px of 1280px window (70%). Height: full 720px.

**Camera math:**
```c
/* World → screen: position an entity on screen given camera state */
int screen_x = (int)((world_x - camera.x) * camera.zoom);
int screen_y = (int)(world_y * camera.zoom);
int screen_w = (int)(entity_w * camera.zoom);
int screen_h = (int)(entity_h * camera.zoom);

/* Screen → world: convert mouse click to world coordinates */
float world_x = (float)mouse_x / camera.zoom + camera.x;
float world_y = (float)mouse_y / camera.zoom;
```

**Camera bounds:**
```c
float max_cam_x = WORLD_W - (CANVAS_W / camera.zoom);
if (camera.x < 0.0f) camera.x = 0.0f;
if (camera.x > max_cam_x) camera.x = max_cam_x;
```

**Zoom levels:** 1x (full world width visible), 2x (2 screens visible), 4x (1 screen visible, pixel-level editing).

**Grid rendering:**
- TILE_SIZE (48px) grid lines in light gray (#404040), scaled by zoom
- Screen boundaries (x = 0, 400, 800, 1200) as blue lines (#4A90D9)
- FLOOR_Y (252px) as red horizontal line (#D94A4A)
- Grid toggleable via G key; auto-hide fine grid at 1x zoom (too dense)

**Entity rendering order** (back to front):
1. Background (solid sky color #87CEEB or simplified parallax)
2. Floor tiles (48px grass repeated in non-gap regions)
3. Water strips (in sea-gap regions, static blue)
4. Rails (dotted path lines)
5. Surfaces: platforms, float platforms, bridges, bouncepads
6. Decorations: vines, ladders, ropes
7. Collectibles: coins, yellow stars, last star
8. Enemies: spiders, jumping spiders, birds, faster birds, fish, faster fish
9. Hazards: axe traps, circular saws, spike rows, spike platforms, spike blocks
10. Selection highlight (2px outline on selected entity)
11. Ghost preview (semi-transparent entity at cursor in TOOL_PLACE mode)

### Palette Panel (palette.c)

**Layout:** Right 384px of window. Top portion (scrollable).

**Structure:**
```
┌─ PALETTE ──────────────────┐
│ ▼ World                    │
│   [□] Sea Gap              │
│   [□] Rail                 │
│ ▼ Collectibles             │
│   [●] Coin                 │
│   [★] Yellow Star          │
│   [★] Last Star            │
│ ▼ Enemies                  │
│   [🕷] Spider               │
│   [🕷] Jumping Spider       │
│   [🐦] Bird                 │
│   ... (14 more types)      │
│ ▼ Surfaces                 │
│   ...                      │
│ ▼ Decorations              │
│   ...                      │
└────────────────────────────┘
```

- Each entry: 32x32 thumbnail (first frame of sprite, scaled) + entity name (SDL2_ttf)
- Category headers as bold labels, collapsible (click to fold/unfold)
- Selected entry highlighted with blue background (#4A90D9)
- Scroll if list exceeds visible area (mouse wheel within panel bounds)

### Properties Panel (properties.c)

**Layout:** Right 384px, below palette when entity is selected.

**Per-type field mapping:**

| Entity Type | Editable Fields | Widget Types |
|-------------|----------------|--------------|
| Sea Gap | x (int) | int_field |
| Rail | layout, x, y, w, h, end_cap | dropdown + int_fields |
| Platform | x, tile_height | float_field + int_field |
| Coin | x, y | float_fields |
| Yellow Star | x, y | float_fields |
| Last Star | x, y | float_fields |
| Spider | x, vx, patrol_x0, patrol_x1, frame_index | float_fields + int_field |
| Jumping Spider | x, vx, patrol_x0, patrol_x1 | float_fields |
| Bird | x, base_y, vx, patrol_x0, patrol_x1, frame_index | float_fields + int_field |
| Faster Bird | (same as Bird) | float_fields + int_field |
| Fish | x, vx, patrol_x0, patrol_x1 | float_fields |
| Faster Fish | (same as Fish) | float_fields |
| Axe Trap | pillar_x, mode | float_field + dropdown |
| Circular Saw | x, patrol_x0, patrol_x1, direction | float_fields + int_field |
| Spike Row | x, count | float_field + int_field |
| Spike Platform | x, y, tile_count | float_fields + int_field |
| Spike Block | rail_index, t_offset, speed | int_field + float_fields |
| Float Platform | mode, x, y, tile_count, rail_index, t_offset, speed | dropdown + float/int_fields |
| Bridge | x, y, brick_count | float_fields + int_field |
| Bouncepad (any) | x, launch_vy | float_fields |
| Vine | x, y, tile_count | float_fields + int_field |
| Ladder | x, y, tile_count | float_fields + int_field |
| Rope | x, y, tile_count | float_fields + int_field |

### UI System (ui.c)

Custom immediate-mode widgets using SDL2 + SDL2_ttf:

```c
/* Render a clickable button. Returns 1 on the frame it is clicked. */
int ui_button(EditorState *es, int x, int y, int w, int h, const char *label);

/* Render a text label at position. */
void ui_label(EditorState *es, int x, int y, const char *text);

/* Render a filled panel with optional title. */
void ui_panel(EditorState *es, int x, int y, int w, int h, const char *title);

/* Editable text field. Returns 1 when value changes. */
int ui_text_field(EditorState *es, int x, int y, int w, char *buf, int buf_size);

/* Editable integer field. Returns 1 when value changes. */
int ui_int_field(EditorState *es, int x, int y, int w, int *value);

/* Editable float field. Returns 1 when value changes. */
int ui_float_field(EditorState *es, int x, int y, int w, float *value);

/* Dropdown selector. Returns 1 when selection changes. */
int ui_dropdown(EditorState *es, int x, int y, int w,
                const char **options, int count, int *selected);

/* Separator line (horizontal). */
void ui_separator(EditorState *es, int x, int y, int w);
```

**Input handling:**
- Mouse: `SDL_MOUSEBUTTONDOWN`, `SDL_MOUSEMOTION` for clicks and hover
- Text: `SDL_TEXTINPUT` events feed into active text/int/float fields
- Keyboard: `SDL_KEYDOWN` for Backspace, Enter (confirm), Escape (cancel), Tab (next field)

**Visual style:**
- Panel background: #2D2D2D
- Panel title bar: #3D3D3D
- Button normal: #4D4D4D, hover: #5D5D5D, active: #4A90D9
- Text color: #E0E0E0
- Selection highlight: #4A90D9
- Input field background: #1D1D1D, focused border: #4A90D9
- Font size: 13px (round9x13.ttf native size)

### Tool System (tools.c)

```c
typedef enum { TOOL_SELECT, TOOL_PLACE, TOOL_DELETE } EditorTool;
```

**TOOL_SELECT (default):**
1. Mouse down on canvas → hit-test all entities (AABB, iterate all arrays in LevelDef)
2. Hit → set `selection = {type, index}`, show properties panel
3. Drag on selected entity → update x/y position, record move start for undo
4. Mouse up after drag → push CMD_MOVE to undo stack
5. Shift held during drag → snap position to TILE_SIZE grid
6. Click empty space → clear selection, hide properties

**Hit-test order** (topmost entity wins, reverse of render order):
Hazards → Enemies → Collectibles → Surfaces → Decorations → World Geometry

**TOOL_PLACE (activated by palette click):**
1. Ghost preview: render entity sprite at cursor position with 50% alpha (`SDL_SetTextureAlphaMod`)
2. Shift held → snap ghost to TILE_SIZE grid
3. Click → check `MAX_*` limit for the entity type
4. If within limit: append to array, increment count, push CMD_PLACE to undo, set modified=1
5. If at limit: show status bar warning "Maximum [type] reached ([MAX])"
6. Stay in PLACE mode (click again for another). Esc → return to SELECT.

**Default placement values** (when creating new entity):

| Entity | Defaults |
|--------|----------|
| Spider | vx=50, patrol_x0=x-50, patrol_x1=x+50, frame_index=0 |
| Jumping Spider | vx=55, patrol_x0=x-50, patrol_x1=x+50 |
| Bird | base_y=100, vx=45, patrol_x0=x-80, patrol_x1=x+80, frame_index=0 |
| Faster Bird | base_y=80, vx=80, patrol_x0=x-80, patrol_x1=x+80, frame_index=0 |
| Fish | vx=70, patrol_x0=x-60, patrol_x1=x+60 |
| Faster Fish | vx=120, patrol_x0=x-60, patrol_x1=x+60 |
| Axe Trap | mode=PENDULUM |
| Circular Saw | patrol_x0=x-48, patrol_x1=x+48, direction=1 |
| Spike Row | count=3 |
| Spike Platform | tile_count=3 |
| Spike Block | rail_index=0, t_offset=0, speed=3.0 |
| Float Platform | mode=STATIC, tile_count=3 |
| Bridge | brick_count=8 |
| Bouncepad Small | launch_vy=-350, pad_type=GREEN |
| Bouncepad Medium | launch_vy=-450, pad_type=WOOD |
| Bouncepad High | launch_vy=-600, pad_type=RED |
| Platform | tile_height=2 |
| Vine/Ladder/Rope | tile_count=3 |

**TOOL_DELETE:**
1. Click on entity → hit-test, if hit: store entity data, remove from array (compact), push CMD_DELETE
2. Also: Delete key on selected entity in SELECT mode triggers same logic
3. Also: right-click in any mode → delete entity under cursor

### Undo/Redo System (undo.c)

```c
#define UNDO_MAX 256

typedef enum { CMD_PLACE, CMD_DELETE, CMD_MOVE, CMD_PROPERTY } CommandType;

/* Generic storage for any placement struct (largest is FloatPlatformPlacement) */
typedef union {
    CoinPlacement           coin;
    SpiderPlacement         spider;
    JumpingSpiderPlacement  jumping_spider;
    BirdPlacement           bird;
    FishPlacement           fish;
    AxeTrapPlacement        axe_trap;
    CircularSawPlacement    circular_saw;
    SpikeRowPlacement       spike_row;
    SpikePlatformPlacement  spike_platform;
    SpikeBlockPlacement     spike_block;
    FloatPlatformPlacement  float_platform;
    BridgePlacement         bridge;
    BouncepadPlacement      bouncepad;
    PlatformPlacement       platform;
    VinePlacement           vine;
    LadderPlacement         ladder;
    RopePlacement           rope;
    RailPlacement           rail;
    int                     sea_gap;       /* sea_gap is just an int */
    LastStarPlacement       last_star;
    YellowStarPlacement     yellow_star;
} PlacementData;

typedef struct {
    CommandType   type;
    EntityType    entity_type;
    int           entity_index;
    PlacementData before;   /* state before the action (for undo) */
    PlacementData after;    /* state after the action (for redo)  */
} Command;

typedef struct {
    Command commands[UNDO_MAX];
    int     top;    /* next write position (0 = empty) */
    int     count;  /* total commands in stack          */
    /* Redo: separate stack */
    Command redo[UNDO_MAX];
    int     redo_top;
    int     redo_count;
} UndoStack;

void undo_push(UndoStack *stack, Command cmd);    /* push + clear redo */
int  undo_pop(UndoStack *stack, Command *out);     /* pop last, push to redo */
int  redo_pop(UndoStack *stack, Command *out);     /* pop redo, push to undo */
void undo_apply(EditorState *es, const Command *cmd, int reverse); /* execute/reverse */
```

### Serializer — JSON ↔ LevelDef (serializer.c)

**Library:** cJSON 1.7.x (Dave Gamble, MIT license). Single .c + .h, ~1500 lines. Zero dependencies.

**JSON schema** (mirrors LevelDef field-for-field):

```json
{
  "name": "Sandbox",
  "sea_gaps": [0, 192, 560, 928, 1152],
  "rails": [
    {
      "layout": "RECT",
      "x": 480, "y": 108,
      "w": 10, "h": 6,
      "end_cap": 0
    }
  ],
  "platforms": [
    { "x": 96.0, "tile_height": 2 }
  ],
  "coins": [
    { "x": 120.0, "y": 200.0 }
  ],
  "yellow_stars": [
    { "x": 300.0, "y": 150.0 }
  ],
  "last_star": { "x": 1500.0, "y": 100.0 },
  "spiders": [
    {
      "x": 300.0, "vx": 50.0,
      "patrol_x0": 250.0, "patrol_x1": 380.0,
      "frame_index": 0
    }
  ],
  "jumping_spiders": [
    { "x": 600.0, "vx": 55.0, "patrol_x0": 560.0, "patrol_x1": 700.0 }
  ],
  "birds": [
    {
      "x": 200.0, "base_y": 100.0, "vx": 45.0,
      "patrol_x0": 120.0, "patrol_x1": 350.0,
      "frame_index": 0
    }
  ],
  "faster_birds": [],
  "fish": [
    { "x": 500.0, "vx": 70.0, "patrol_x0": 450.0, "patrol_x1": 600.0 }
  ],
  "faster_fish": [],
  "axe_traps": [
    { "pillar_x": 96.0, "mode": "PENDULUM" }
  ],
  "circular_saws": [
    { "x": 800.0, "patrol_x0": 750.0, "patrol_x1": 900.0, "direction": 1 }
  ],
  "spike_rows": [
    { "x": 400.0, "count": 4 }
  ],
  "spike_platforms": [
    { "x": 600.0, "y": 180.0, "tile_count": 3 }
  ],
  "spike_blocks": [
    { "rail_index": 0, "t_offset": 0.0, "speed": 3.0 }
  ],
  "float_platforms": [
    {
      "mode": "STATIC",
      "x": 200.0, "y": 150.0,
      "tile_count": 3,
      "rail_index": 0, "t_offset": 0.0, "speed": 0.0
    }
  ],
  "bridges": [
    { "x": 300.0, "y": 200.0, "brick_count": 8 }
  ],
  "bouncepads_small": [
    { "x": 150.0, "launch_vy": -350.0, "pad_type": "GREEN" }
  ],
  "bouncepads_medium": [],
  "bouncepads_high": [],
  "vines": [
    { "x": 100.0, "y": 50.0, "tile_count": 5 }
  ],
  "ladders": [],
  "ropes": []
}
```

**Enum serialization** (string, not integer):
- RailLayout: `"RECT"`, `"HORIZ"`
- AxeTrapMode: `"PENDULUM"`, `"SPIN"`
- FloatPlatformMode: `"STATIC"`, `"CRUMBLE"`, `"RAIL"`
- BouncepadType: `"GREEN"`, `"WOOD"`, `"RED"`

**Validation on load:**
- Array counts checked against MAX_* (reject file if exceeded, report which array)
- Missing arrays treated as empty (count=0) — allows partial/minimal JSON
- Unknown keys ignored (forward compatibility)
- Required field: `"name"` (string)

**Functions:**
```c
cJSON *level_to_json(const LevelDef *def);
int    level_from_json(const cJSON *json, LevelDef *def);  /* returns 0=ok, -1=error */
int    level_save_json(const LevelDef *def, const char *path);
int    level_load_json(const char *path, LevelDef *def);
```

### Exporter — LevelDef → C Code (exporter.c)

Generates compilable C matching `level_01.c` format exactly.

**Header output** (`level_XX.h`):
```c
#pragma once
#include "level.h"
extern const LevelDef level_XX_def;
```

**Source output** (`level_XX.c`) — uses `fprintf` to write each section:
```c
#include "level_XX.h"
/* ... entity header includes for speed constants ... */

const LevelDef level_XX_def = {
    .name = "Level Name",

    /* ---- World geometry ---- */
    .sea_gaps      = { 0, 192, 560 },
    .sea_gap_count = 3,

    /* ---- Rails ---- */
    .rails = {
        { .layout = RAIL_LAYOUT_RECT, .x = 480, .y = 108, .w = 10, .h = 6, .end_cap = 0 },
    },
    .rail_count = 1,

    /* ... (same pattern for all 25 sections) ... */
};
```

**Functions:**
```c
int level_export_c(const LevelDef *def, const char *var_name, const char *dir_path);
```
- `var_name`: e.g. `"level_02"` → generates `level_02_def` variable
- `dir_path`: output directory (e.g. `"src/levels/"`)
- Returns 0 on success, -1 on file write error

## Makefile Integration

```makefile
# ---- Editor-specific variables ----
EDITOR_DIR    = src/editor
VENDOR_DIR    = vendor/cJSON
EDITOR_SRCS   = $(wildcard $(EDITOR_DIR)/*.c) $(VENDOR_DIR)/cJSON.c
EDITOR_OBJS   = $(EDITOR_SRCS:.c=.o)

# Editor needs game.h and level.h headers but not game .o files
EDITOR_CFLAGS = $(CFLAGS) -I$(VENDOR_DIR)

# ---- Editor targets ----
editor: out $(EDITOR_OBJS)
	$(CC) $(EDITOR_CFLAGS) -o out/super-mango-editor $(EDITOR_OBJS) $(LDFLAGS_NO_MIXER)

run-editor: editor
	./out/super-mango-editor

# LDFLAGS_NO_MIXER: same as LDFLAGS but without -lSDL2_mixer (editor has no audio)
# Add -lSDL2_ttf explicitly if not already in LDFLAGS

# ---- Updated clean ----
clean:
	rm -rf out/ $(SRC_DIRS:%=%/*.o) $(EDITOR_DIR)/*.o $(VENDOR_DIR)/*.o
```

**Note:** The editor's `.o` compilation rule uses the same `CFLAGS` + `-I vendor/cJSON` for cJSON header access. The existing game `$(SRCS)` wildcard only covers `src/*.c` and subdirectories already listed — `src/editor/` is NOT in the game's source list, so `make` (game) never compiles editor files.

## Window Layout

```
┌─────────────────────────────────────────────────────────────────┐
│ [Select] [Place] [Delete]  │  Zoom: 2x  │  Grid: ON   │  File │ ← Toolbar (32px)
├────────────────────────────────────────────┬────────────────────┤
│                                            │ ▼ PALETTE          │
│                                            │   World            │
│          CANVAS VIEWPORT                   │   Collectibles     │
│          (896 x 656 px)                    │   Enemies          │
│                                            │   Hazards          │
│     World 1600x300 with camera scroll      │   Surfaces         │
│     Grid overlay, entity sprites           │   Decorations      │
│     Selection highlight                    │                    │
│     Ghost preview in place mode            ├────────────────────┤
│                                            │ PROPERTIES         │
│                                            │   x: [120.0]       │
│                                            │   y: [200.0]       │
│                                            │   (fields vary     │
│                                            │    by entity type) │
├────────────────────────────────────────────┴────────────────────┤
│ Mouse: (450, 200)  │ Tool: Select  │ Entities: 47  │ level.json│ ← Status bar (32px)
└─────────────────────────────────────────────────────────────────┘
         896px                              384px
                        1280px total
```
