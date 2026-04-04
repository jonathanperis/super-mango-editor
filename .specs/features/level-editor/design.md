# Design: Visual Level Editor

## Architectural Decisions

| ID | Decision | Choice | Impact |
|----|----------|--------|--------|
| D-001 | Serialization | JSON canonical + C export pipeline | cJSON is editor-only; game stays pure C structs |
| D-002 | UI Framework | Custom SDL2 + SDL2_ttf immediate-mode | ~7 widget functions in ui.c, no external deps |
| D-003 | Game JSON Loader | No — game reads only compiled `const LevelDef` | Play-test cycle: export → `make run` |

## Primary Goal: Recreate sandbox_00 (Sandbox)

The editor must produce a visual representation of sandbox_00.c that matches the running game. This requires replicating the game's Y-position derivation formulas, display size calculations, source-rect cropping, and floor/water rendering.

## Architecture Overview

```
super-mango-editor (standalone executable)
│
├── editor_main.c              ← SDL/IMG/TTF init, window 1280x720, entry point
│
├── EditorState                ← Central struct (analogous to GameState)
│   ├── SDL_Window *window
│   ├── SDL_Renderer *renderer
│   ├── TTF_Font *font         ← round9x13.ttf
│   ├── LevelDef level         ← Mutable level data
│   ├── EntityTextures textures
│   ├── EditorCamera camera
│   ├── EditorTool tool
│   ├── Selection selection
│   ├── EntityType palette_type
│   ├── UndoStack undo
│   ├── char file_path[256]
│   ├── int modified
│   ├── int show_grid
│   └── int running
│
├── Editor Loop                ← 60 FPS: events → tools → UI → render
│
├── Canvas                     ← World viewport with game-accurate rendering
│   ├── Floor (9-slice grass tileset with gap edge caps)
│   ├── Water (static strip in gap regions)
│   ├── Entity sprites (correct display sizes and Y derivations)
│   └── Grid/reference overlays
│
├── Palette + Properties       ← Entity selection and editing panels
│
├── Tools                      ← Select/place/move/delete with correct hit-test sizes
│
└── File Pipeline              ← JSON ↔ LevelDef ��� C source generation
```

## Code Organization

```
super-mango-game/
├── src/
│   ├── editor/
│   │   ├── editor_main.c
│   │   ├── editor.h              ← EditorState, enums, constants
│   │   ├── editor.c              ← Loop, keyboard shortcuts, file operations
│   │   ├── canvas.h / canvas.c   ← Viewport, camera, floor/water/entity rendering
│   │   ├── palette.h / palette.c
│   │   ├── properties.h / properties.c
│   │   ├── tools.h / tools.c     ← Select/place/move/delete
│   │   ├── undo.h / undo.c
│   │   ├── serializer.h / serializer.c ← JSON ↔ LevelDef
│   │   ├── exporter.h / exporter.c    ← LevelDef → C code
│   │   └── ui.h / ui.c
│   ├── levels/level.h             ← SHARED (LevelDef, placement structs)
│   ├── game.h                     ← SHARED (constants only)
│   └── (game source — untouched)
├── vendor/cJSON/                   ← Editor-only
├── levels/                         ← JSON level files
│   └── sandbox_00.json
└── Makefile                        ← Updated with `editor` target
```

---

## Critical Reference: Entity Display Dimensions and Y Derivations

The game's loader and render functions compute display positions and sizes that differ from the raw sprite sheet frames. The editor MUST replicate these calculations for accurate preview and hit-testing.

### Entity Display Size Table

| Entity | Sprite Frame | Display/Render Size | Source Rect Crop | Notes |
|--------|-------------|---------------------|-----------------|-------|
| Spider | 64×48 | 64×10 | y=22, h=10 (art band) | Only visible art rows rendered |
| Jumping Spider | 64×48 | 64×10 | y=22, h=10 | Same art band as spider |
| Bird | 48×48 | 48×14 | y=17, h=14 (art band) | Only visible art rows |
| Faster Bird | 48×48 | 48×14 | y=17, h=14 | Same crop as bird |
| Fish | 48×48 | 48×48 | Full frame | Full frame rendered |
| Faster Fish | 48×48 | 48×48 | Full frame | Full frame |
| Coin | varies | 16×16 | Full texture | Scaled to display size |
| Star Yellow | varies | 16×16 | Full texture | Scaled to display size |
| Last Star | varies | 24×24 | Full texture | Uses HUD star asset |
| Axe Trap | 48×64 | 48×64 | Full frame | Rotated at pivot point |
| Circular Saw | 32×32 | 32×32 | Full frame | Rotated continuously |
| Spike Row | 16×16 per tile | 16×16 × count | Per tile | Horizontal strip |
| Spike Platform | 16×11 per piece | 16×11 × tile_count | y=5, h=11 | Cropped content area |
| Spike Block | 16×16 | 24×24 | Full frame | Upscaled for display |
| Float Platform | 16×16 per piece | 16×16 × tile_count | 3-slice | Left/mid/right pieces |
| Bridge | 16×16 per brick | 16×16 × brick_count | Per brick | Horizontal strip |
| Bouncepad | 48×48 | 48×18 | y=14, h=18 | Vertical crop, bottom-aligned |
| Vine | 16×48 → 16×32 | 16×32 per tile | y=8, h=32, step=19px | Overlapping tiles |
| Ladder | 16×48 → 16×22 | 16×22 per tile | y=13, h=22, step=8px | Tight overlap |
| Rope | 16×48 → 12×36 | 12×36 per tile | x=2, y=6, w=12, h=36, step=34px | Cropped + overlap |
| Platform (pillar) | 48×48 9-slice | 48 × (tile_height*48) | 3×3 grid of 16px | 9-slice grass tileset |
| Rail | 16×16 per tile | 16×16 per tile | 4×4 variants in 64×64 | Direction-dependent tile selection |

### Y Position Derivation Table

These formulas MUST be used in the editor to position entities for preview. They replicate what `level_loader.c` computes.

| Entity | Y Formula | Computed Value | Source of Truth |
|--------|-----------|---------------|-----------------|
| Spider | `FLOOR_Y - SPIDER_ART_H` | 252 - 10 = **242** | spider.h SPIDER_ART_H=10 |
| Jumping Spider | `FLOOR_Y - JSPIDER_ART_H` | 252 - 10 = **242** | jumping_spider.h |
| Bird | `base_y` (from placement) + sine wave at runtime | **base_y** (static preview) | sandbox_00.c |
| Faster Bird | `base_y` (from placement) | **base_y** (static preview) | sandbox_00.c |
| Fish | `(GAME_H - WATER_ART_H) - FISH_RENDER_H/2` | (300-31) - 24 = **245** | fish.c |
| Faster Fish | same as Fish | **245** | faster_fish.c |
| Axe Trap | `FLOOR_Y - 3*TILE_SIZE + 16` | 252 - 144 + 16 = **124** | level_loader.c |
| Circular Saw | `FLOOR_Y - 2*TILE_SIZE + 16 - SAW_DISPLAY_H` | 252 - 96 + 16 - 32 = **140** | level_loader.c |
| Spike Row | `FLOOR_Y - SPIKE_TILE_H` | 252 - 16 = **236** | level_loader.c |
| Spike Platform | from placement (x, y) | **varies** | sandbox_00.c |
| Spike Block | `rail_get_world_pos(rail, t_offset)` | **rail-dependent** | spike_block.c |
| Float Platform (STATIC/CRUMBLE) | from placement (x, y) | **varies** | sandbox_00.c |
| Float Platform (RAIL) | `rail_get_world_pos(rail, t_offset)` | **rail-dependent** | float_platform.c |
| Bouncepad (all) | `FLOOR_Y - BOUNCEPAD_SRC_H` | 252 - 18 = **234** | bouncepad.c |
| Platform | `FLOOR_Y - tile_height*TILE_SIZE + 16` | 2-tile=**172**, 3-tile=**124** | level_loader.c |
| Bridge | from placement (x, y) | **varies** (typically 172) | sandbox_00.c |
| Vine | from placement (x, y) | **varies** | sandbox_00.c |
| Ladder | from placement (x, y) | **varies** | sandbox_00.c |
| Rope | from placement (x, y) | **varies** | sandbox_00.c |
| Coin | from placement (x, y) | **varies** | sandbox_00.c |
| Star Yellow | from placement (x, y) | **varies** | sandbox_00.c |
| Last Star | from placement (x, y) | **varies** | sandbox_00.c |

### Blue Flame Auto-Derivation (Preview Only)

Blue flames are NOT in LevelDef. The editor computes preview positions from sea_gaps:
```c
for each sea_gap[i]:
    flame.x = sea_gap[i] + (SEA_GAP_W - 48) / 2.0f  /* centered in gap */
    flame.start_y = FLOOR_Y + TILE_SIZE               /* below floor, hidden */
```
The editor renders a ghost (50% alpha) flame sprite at each gap to show where flames will appear in-game.

---

## Asset Path Reference (Case-Sensitive)

The editor must load the exact same textures as the game. All paths are relative to the repo root.

| Variable | Asset Path | Load Priority |
|----------|-----------|--------------|
| floor_tile | `assets/grass_tileset.png` | Fatal |
| platform_tex | `assets/platform.png` | Fatal |
| spider_tex | `assets/spider.png` | Fatal |
| jumping_spider_tex | `assets/jumping_spider.png` | Fatal |
| bird_tex | `assets/bird.png` | Fatal |
| faster_bird_tex | `assets/faster_bird.png` | Fatal |
| fish_tex | `assets/fish.png` | Fatal |
| faster_fish_tex | `assets/faster_fish.png` | Non-fatal |
| coin_tex | `assets/coin.png` | Fatal |
| star_yellow_tex | `assets/star_yellow.png` | Non-fatal |
| last_star_tex | (uses HUD star texture) | Non-fatal |
| axe_trap_tex | `assets/axe_trap.png` | Non-fatal |
| circular_saw_tex | `assets/circular_saw.png` | Non-fatal |
| blue_flame_tex | `assets/blue_flame.png` | Non-fatal |
| spike_tex | `assets/spike.png` | Non-fatal |
| spike_platform_tex | `assets/spike_platform.png` | Non-fatal |
| spike_block_tex | `assets/spike_block.png` | Non-fatal |
| float_platform_tex | `assets/float_platform.png` | Non-fatal |
| bridge_tex | `assets/bridge.png` | Non-fatal |
| bouncepad_small_tex | `assets/bouncepad_small.png` | Non-fatal |
| bouncepad_medium_tex | `assets/bouncepad_medium.png` | Fatal |
| bouncepad_high_tex | `assets/bouncepad_high.png` | Non-fatal |
| vine_tex | `assets/vine.png` | Non-fatal |
| ladder_tex | `assets/ladder.png` | Non-fatal |
| rope_tex | `assets/rope.png` | Non-fatal |
| rail_tex | `assets/rail.png` | Non-fatal |
| water_tex | `assets/water.png` | Non-fatal |

**Note:** For the editor, ALL textures are non-fatal — warn and skip if missing.

---

## Floor Rendering Algorithm

The game uses a 9-slice system for floor tiles. The editor MUST replicate this for accurate Sandbox preview.

**Tileset:** `grass_tileset.png` is 48×48, divided into a 3×3 grid of 16×16 pieces:

```
[TL=0,0] [TC=1,0] [TR=2,0]   ← grass top edge
[ML=0,1] [MC=1,1] [MR=2,1]   ← dirt fill
[BL=0,2] [BC=1,2] [BR=2,2]   ← base edge
```

**Algorithm:**
1. Iterate x from `cam_left` to `cam_right` in 16px steps
2. For each 16px column, iterate y from `FLOOR_Y` to `GAME_H` in 16px steps
3. **Row selection:** y==FLOOR_Y → row 0 (grass); y+16>=GAME_H → row 2 (base); else → row 1 (dirt)
4. **Column selection:**
   - x==0 → col 0 (left world edge)
   - x+16>=WORLD_W → col 2 (right world edge)
   - x+16 == sea_gap.x → col 2 (right gap edge cap)
   - x == sea_gap.x + SEA_GAP_W → col 0 (left gap edge cap)
   - else → col 1 (seamless fill)
5. **Gap skip:** if x is fully within a sea gap, skip tile entirely (show water)

**Sea gaps:** 32px wide holes in the floor. Each gap is defined by its left-edge x position.

---

## Water Rendering (Editor Simplified)

The game's water uses 8-frame animation scrolling in screen-space. The editor renders a **static** water strip:

- Draw solid blue rectangle (#1A6BA0) of height WATER_ART_H (31px) at y = GAME_H - WATER_ART_H (269px)
- Only draw in sea-gap regions (within the gap x range)
- Optionally: render one static water frame from `water.png` for visual fidelity

---

## Rail Path Computation for Preview

The editor must compute rail tile positions to show paths and position spike blocks/float platforms.

**RECT rail:** Closed rectangular loop of tiles.
```c
/* Compute tile positions for a RECT rail */
/* Top edge: left to right */
for (int i = 0; i < w; i++) tiles[n++] = {x + i*16, y};
/* Right edge: top to bottom */
for (int i = 1; i < h; i++) tiles[n++] = {x + (w-1)*16, y + i*16};
/* Bottom edge: right to left */
for (int i = w-2; i >= 0; i--) tiles[n++] = {x + i*16, y + (h-1)*16};
/* Left edge: bottom to top */
for (int i = h-2; i > 0; i--) tiles[n++] = {x, y + i*16};
```

**HORIZ rail:** Open horizontal line.
```c
for (int i = 0; i < w; i++) tiles[n++] = {x + i*16, y};
/* If end_cap: add right-edge tile; else: rider detaches at end */
```

**Position interpolation** (for spike blocks and RAIL float platforms):
```c
/* Get world position at parameter t (in tile units) */
int tile_index = (int)t % tile_count;
float frac = t - (int)t;
world_x = tiles[tile_index].x + frac * (tiles[next].x - tiles[tile_index].x);
world_y = tiles[tile_index].y + frac * (tiles[next].y - tiles[tile_index].y);
```

---

## Render Order (Exact Match to game.c)

The editor renders entities in the same back-to-front order as the game to ensure accurate visual preview:

| Layer | Entity | Notes |
|-------|--------|-------|
| 1 | Sky background | Solid #87CEEB (simplified from parallax) |
| 2 | Platforms (pillars) | Rendered BEFORE floor (grow from ground) |
| 3 | Floor tiles | 9-slice grass with gap edge caps |
| 4 | Float platforms | Hovering/crumbling/rail surfaces |
| 5 | Spike rows | On floor surface |
| 6 | Spike platforms | Elevated spike surfaces |
| 7 | Bridges | Crumble walkways |
| 8 | Bouncepads (medium) | On floor |
| 9 | Bouncepads (small) | On floor |
| 10 | Bouncepads (high) | On floor |
| 11 | Rails | Track lines (dotted or tile) |
| 12 | Vines | On platforms |
| 13 | Ladders | Climbable decoration |
| 14 | Ropes | Climbable decoration |
| 15 | Coins | On ground/platforms |
| 16 | Yellow stars | On platforms |
| 17 | Last star | End-of-level |
| 18 | Blue flames (ghost) | Auto-derived preview, 50% alpha |
| 19 | Fish | Behind water strip |
| 20 | Faster fish | Behind water strip |
| 21 | Water strip | Static blue in gap regions |
| 22 | Spike blocks | Rail-riding hazards |
| 23 | Axe traps | Rendered at pivot, no rotation in editor |
| 24 | Circular saws | No spin in editor |
| 25 | Spiders | Ground patrol |
| 26 | Jumping spiders | Ground patrol |
| 27 | Birds | Sky patrol |
| 28 | Faster birds | Sky patrol |
| 29 | Selection highlight | 2px outline on selected entity |
| 30 | Ghost preview | Semi-transparent placement preview |
| 31 | Grid/reference lines | Overlay on top of everything |

---

## Hit-Test Dimensions for Select Tool

The select tool uses AABB hit-testing. The hit-test rectangle MUST match the entity's **display size**, not its sprite frame size.

| Entity | Hit-Test Width | Hit-Test Height | X Origin | Y Origin |
|--------|---------------|-----------------|----------|----------|
| Spider | 64 | 10 | placement x | 242 (derived) |
| Jumping Spider | 64 | 10 | placement x | 242 (derived) |
| Bird | 48 | 14 | placement x | base_y (placement) |
| Faster Bird | 48 | 14 | placement x | base_y (placement) |
| Fish | 48 | 48 | placement x | 245 (derived) |
| Faster Fish | 48 | 48 | placement x | 245 (derived) |
| Coin | 16 | 16 | placement x | placement y |
| Star Yellow | 16 | 16 | placement x | placement y |
| Last Star | 24 | 24 | placement x | placement y |
| Axe Trap | 48 | 64 | pillar_x + 24 - 24 | 124 (derived) |
| Circular Saw | 32 | 32 | placement x | 140 (derived) |
| Spike Row | count × 16 | 16 | placement x | 236 (derived) |
| Spike Platform | tile_count × 16 | 11 | placement x | placement y |
| Spike Block | 24 | 24 | rail-derived x | rail-derived y |
| Float Platform | tile_count × 16 | 16 | varies by mode | varies by mode |
| Bridge | brick_count × 16 | 16 | placement x | placement y |
| Bouncepad | 48 | 18 | placement x | 234 (derived) |
| Vine | 16 | tile_count × 19 | placement x | placement y |
| Ladder | 16 | tile_count × 8 | placement x | placement y |
| Rope | 12 | tile_count × 34 | placement x | placement y |
| Platform | 48 | tile_height × 48 | placement x | derived y |
| Sea Gap | 32 | 48 | gap x | FLOOR_Y |
| Rail | w × 16 | h × 16 | placement x | placement y |

---

## Component Design Details

### EditorState Struct

```c
typedef enum {
    ENT_SEA_GAP, ENT_RAIL, ENT_PLATFORM,
    ENT_COIN, ENT_STAR_YELLOW, ENT_LAST_STAR,
    ENT_SPIDER, ENT_JUMPING_SPIDER, ENT_BIRD, ENT_FASTER_BIRD,
    ENT_FISH, ENT_FASTER_FISH,
    ENT_AXE_TRAP, ENT_CIRCULAR_SAW, ENT_SPIKE_ROW,
    ENT_SPIKE_PLATFORM, ENT_SPIKE_BLOCK,
    ENT_FLOAT_PLATFORM, ENT_BRIDGE,
    ENT_BOUNCEPAD_SMALL, ENT_BOUNCEPAD_MEDIUM, ENT_BOUNCEPAD_HIGH,
    ENT_VINE, ENT_LADDER, ENT_ROPE,
    ENT_COUNT  /* 25 */
} EntityType;

typedef enum { TOOL_SELECT, TOOL_PLACE, TOOL_DELETE } EditorTool;

typedef struct { EntityType type; int index; } Selection;

typedef struct {
    SDL_Texture *floor_tile, *water;
    SDL_Texture *platform, *spider, *jumping_spider;
    SDL_Texture *bird, *faster_bird, *fish, *faster_fish;
    SDL_Texture *coin, *star_yellow, *last_star;
    SDL_Texture *axe_trap, *circular_saw, *blue_flame;
    SDL_Texture *spike, *spike_platform, *spike_block;
    SDL_Texture *float_platform, *bridge;
    SDL_Texture *bouncepad_small, *bouncepad_medium, *bouncepad_high;
    SDL_Texture *vine, *ladder, *rope, *rail;
} EntityTextures;

typedef struct { float x; float zoom; } EditorCamera;

typedef struct {
    SDL_Window   *window;
    SDL_Renderer *renderer;
    TTF_Font     *font;
    LevelDef      level;
    EntityTextures textures;
    EditorCamera  camera;
    EditorTool    tool;
    Selection     selection;
    EntityType    palette_type;
    UndoStack     undo;
    char          file_path[256];
    int           modified;
    int           show_grid;
    int           running;
} EditorState;
```

### Serializer — JSON Schema

Matches LevelDef field-for-field. Enum values serialized as strings:
- RailLayout: `"RECT"`, `"HORIZ"`
- AxeTrapMode: `"PENDULUM"`, `"SPIN"`
- FloatPlatformMode: `"STATIC"`, `"CRUMBLE"`, `"RAIL"`
- BouncepadType: `"GREEN"`, `"WOOD"`, `"RED"`

`last_star` is a single object, not an array. Missing arrays default to count=0.

### Exporter — C Code Style

Must match `sandbox_00.c` exactly:
- Designated initializers: `.field = value`
- Section separator comments: `/* ---- Section name ---- */`
- Speed constant names where available (e.g., `SPIDER_SPEED` instead of `50.0f`)
- Floating-point values with `.0f` suffix
- Array entries one per line with trailing comma

### Undo System

```c
typedef union {
    CoinPlacement coin; SpiderPlacement spider;
    JumpingSpiderPlacement jumping_spider; BirdPlacement bird;
    FishPlacement fish; AxeTrapPlacement axe_trap;
    CircularSawPlacement circular_saw; SpikeRowPlacement spike_row;
    SpikePlatformPlacement spike_platform; SpikeBlockPlacement spike_block;
    FloatPlatformPlacement float_platform; BridgePlacement bridge;
    BouncepadPlacement bouncepad; PlatformPlacement platform;
    VinePlacement vine; LadderPlacement ladder; RopePlacement rope;
    RailPlacement rail; int sea_gap; LastStarPlacement last_star;
    StarYellowPlacement star_yellow;
} PlacementData;

typedef struct {
    CommandType type; EntityType entity_type;
    int entity_index; PlacementData before, after;
} Command;
```

### UI System

Custom immediate-mode with SDL2_ttf. ~7 widgets: button, label, panel, int_field, float_field, dropdown, separator. Dark theme (#2D2D2D panels, #E0E0E0 text, #4A90D9 accent).

### Window Layout

```
┌────────────────────────────────────────────────────────────────┐
│ [Select] [Place] [Delete]  │  Zoom: 2x  │ Grid │ File ops     │ 32px
├───────────────────────────────────────────┬─────────��──────────┤
│                                           │ PALETTE            │
│            CANVAS VIEWPORT                │  ▼ World           │
│            (896 × 656 px)                 │  ▼ Collectibles    │
│                                           │  ▼ Enemies         ��
│   1600×300 world with camera              │  ▼ Hazards         │
│   9-slice floor, water, entities          │  ▼ Surfaces        │
│   at correct display sizes                │  ▼ Decorations     │
│                                           ├────────────────────┤
│                                           │ PROPERTIES         │
│                                           │  (per-entity)      │
├───────────────────────────────────���───────┴────────────────────┤
│ (320, 200)  │ Select │ 47 entities │ sandbox_00.json *          │ 32px
└────────────────────────────────────────────────────────────────┘
        896px                             384px = 1280px
```

### Makefile Integration

```makefile
EDITOR_DIR  = src/editor
VENDOR_DIR  = vendor/cJSON
EDITOR_SRCS = $(wildcard $(EDITOR_DIR)/*.c) $(VENDOR_DIR)/cJSON.c
EDITOR_OBJS = $(EDITOR_SRCS:.c=.o)

editor: out $(EDITOR_OBJS)
	$(CC) $(CFLAGS) -I$(VENDOR_DIR) -o out/super-mango-editor $(EDITOR_OBJS) $(LDFLAGS_NO_MIXER)

run-editor: editor
	./out/super-mango-editor
```

Game `$(SRCS)` wildcard does NOT include `src/editor/` — builds remain independent.
