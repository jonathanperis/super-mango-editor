# Feature Spec: Visual Level Editor

## Summary

Standalone SDL2/C application that provides a visual interface for creating, editing, and exporting game levels. Replaces manual `const LevelDef` C struct authoring with a point-and-click editor.

The editor must be capable of recreating the existing Sandbox level (sandbox_00) with pixel-perfect fidelity — every entity at the exact same position and visual appearance as the running game.

## Motivation

- Hand-coding level definitions in C is error-prone and slow
- Positioning entities by guessing pixel coordinates leads to constant compile-test cycles
- No visual feedback until the game runs — impossible to see the level layout during design
- Blocks non-programmers from contributing level designs
- Many entity Y positions and display sizes are derived by the game's loader code; the editor must replicate these derivations visually

## Architectural Decisions (Finalized)

| ID | Decision | Choice |
|----|----------|--------|
| D-001 | Serialization | JSON as canonical format + C code export pipeline. Game never links cJSON. |
| D-002 | UI Framework | Custom immediate-mode SDL2 + SDL2_ttf. No external UI library. |
| D-003 | Game JSON Loader | No. Game only reads compiled `const LevelDef`. Play-test via export → `make run`. |

## Requirements

### R-001: Standalone Executable
The editor MUST be a separate executable built with `make editor`, outputting to `out/super-mango-editor`. It shares headers (`level.h`, `game.h` constants) with the game but has its own main loop and event handling. The game build (`make`) MUST remain unaffected.

### R-002: Full Entity Support
The editor MUST support placing, moving, and configuring all 25 entity placement types currently defined in `LevelDef`:

| Category | Types | Placement Struct | Stored Fields | Derived by Loader (editor must compute for preview) |
|----------|-------|-----------------|---------------|-----------------------------------------------------|
| World Geometry | sea_gaps | `int` | x | — |
| World Geometry | rails | `RailPlacement` | layout, x, y, w, h, end_cap | Rail tile positions (for path preview) |
| Static Geometry | platforms | `PlatformPlacement` | x, tile_height | y = FLOOR_Y - tile_height*TILE_SIZE + 16 |
| Collectibles | coins | `CoinPlacement` | x, y | — |
| Collectibles | star_yellows | `StarYellowPlacement` | x, y | — |
| Collectibles | last_star | `LastStarPlacement` | x, y | Single struct, not array |
| Enemies | spiders | `SpiderPlacement` | x, vx, patrol_x0, patrol_x1, frame_index | y = FLOOR_Y - 10 (SPIDER_ART_H) |
| Enemies | jumping_spiders | `JumpingSpiderPlacement` | x, vx, patrol_x0, patrol_x1 | y = FLOOR_Y - 10 (same art height as spider) |
| Enemies | birds | `BirdPlacement` | x, base_y, vx, patrol_x0, patrol_x1, frame_index | y_preview = base_y (sine wave at runtime) |
| Enemies | faster_birds | `BirdPlacement` | same as birds | y_preview = base_y |
| Enemies | fish | `FishPlacement` | x, vx, patrol_x0, patrol_x1 | y = 245 (GAME_H - WATER_ART_H - RENDER_H/2) |
| Enemies | faster_fish | `FishPlacement` | same as fish | y = 245 |
| Hazards | axe_traps | `AxeTrapPlacement` | pillar_x, mode | x = pillar_x + TILE_SIZE/2, y = FLOOR_Y - 3*TILE_SIZE + 16 |
| Hazards | circular_saws | `CircularSawPlacement` | x, patrol_x0, patrol_x1, direction | y = FLOOR_Y - 2*TILE_SIZE + 16 - 32 (SAW_DISPLAY_H) |
| Hazards | spike_rows | `SpikeRowPlacement` | x, count | y = FLOOR_Y - 16 (SPIKE_TILE_H) |
| Hazards | spike_platforms | `SpikePlatformPlacement` | x, y, tile_count | — |
| Hazards | spike_blocks | `SpikeBlockPlacement` | rail_index, t_offset, speed | x,y = rail interpolation at t_offset |
| Surfaces | float_platforms | `FloatPlatformPlacement` | mode, x, y, tile_count, rail_index, t_offset, speed | RAIL mode: x,y from rail interpolation |
| Surfaces | bridges | `BridgePlacement` | x, y, brick_count | — |
| Surfaces | bouncepads_small | `BouncepadPlacement` | x, launch_vy, pad_type=GREEN | y = FLOOR_Y - 18 (BOUNCEPAD_SRC_H) |
| Surfaces | bouncepads_medium | `BouncepadPlacement` | x, launch_vy, pad_type=WOOD | y = 234 |
| Surfaces | bouncepads_high | `BouncepadPlacement` | x, launch_vy, pad_type=RED | y = 234 |
| Decorations | vines | `VinePlacement` | x, y, tile_count | — |
| Decorations | ladders | `LadderPlacement` | x, y, tile_count | — |
| Decorations | ropes | `RopePlacement` | x, y, tile_count | — |

**Auto-derived entities (NOT placed by editor, shown as preview):**
- **Blue flames** (MAX=8): positions derived from sea_gaps array by `blue_flames_init()`. Editor renders them as ghost preview only.

### R-003: Visual Canvas
The editor MUST display a scrollable, zoomable viewport showing the 1600x300 logical world with:
- 9-slice floor rendering matching the game's `grass_tileset.png` algorithm (edge caps at sea gaps and world boundaries)
- Static water strips in sea-gap regions (simplified: no scroll animation)
- Grid overlay at TILE_SIZE (48px) intervals
- Screen boundary markers at x = 0, 400, 800, 1200
- FLOOR_Y (252px) reference line
- Entity sprites rendered at their actual display sizes (NOT raw frame sizes — many entities crop/scale)
- Blue flame ghost previews auto-derived from sea gap positions

### R-004: Entity Palette
The editor MUST provide a categorized palette panel listing all placeable entity types, rendered with SDL2_ttf text and sprite thumbnails. Selecting a type from the palette enables placement mode with a ghost preview at cursor position.

### R-005: Property Editing
The editor MUST allow editing entity-specific properties after placement via SDL2_ttf-rendered input fields. See design.md for per-entity field mapping.

### R-006: JSON Serialization
The editor MUST save and load levels in JSON format using cJSON (vendor library, editor-only). The JSON schema maps 1:1 to `LevelDef` struct fields. Serializer validates array counts against MAX_* bounds.

### R-007: C Code Export
The editor MUST export a level as a valid C source file (`level_XX.c` + `level_XX.h`) that compiles and runs identically to a hand-written level. Generated code MUST follow `sandbox_00.c` style exactly.

### R-008: Core Editing Operations
Place, select, move, delete — with correct entity display sizes for hit testing.

### R-009: Undo/Redo
Command stack (max 256). Tracked: place, delete, move, property change.

### R-010: Camera Navigation
WASD scroll, middle-mouse drag, scroll-wheel zoom (1x, 2x, 4x).

## Out of Scope (v1)

- Play-testing from within the editor
- Game executable loading JSON directly
- Tilemap painting for custom floor layouts
- Multi-level campaign management
- Copy/paste and multi-select
- Native file dialog (v1 uses text input)
- Minimap widget
- Animated entity previews (v1 shows static first frame)

## Constraints

- Must compile with `clang -std=c11 -Wall -Wextra -Wpedantic`
- Must use SDL2 + SDL2_image + SDL2_ttf (SDL2_mixer not needed)
- Editor window: 1280x720
- cJSON linked only into editor target
- Must not break existing game build
- Font: `assets/round9x13.ttf`

## Success Criteria

1. `make editor` builds without affecting `make` (game)
2. Load sandbox_00.json → visual layout matches game screenshot pixel-for-pixel:
   - All 8 platforms at correct heights and positions
   - All 5 sea gaps with correct floor edge caps and water strips
   - All 16 coins at correct positions (ground and platform tops)
   - All enemies visible at their derived Y positions
   - All hazards at correct positions
   - 3 rail paths visible as connected lines
   - Spike blocks and float platforms shown at rail positions
   - Blue flames shown as ghost preview at sea gap positions
3. Round-trip: sandbox_00 → JSON → LevelDef → all fields match
4. C export: sandbox_00 → JSON → C export → compile → game behavior identical
