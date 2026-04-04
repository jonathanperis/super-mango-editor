# Feature Spec: Visual Level Editor

## Summary

Standalone SDL2/C application that provides a visual interface for creating, editing, and exporting game levels. Replaces manual `const LevelDef` C struct authoring with a point-and-click editor.

## Motivation

- Hand-coding level definitions in C is error-prone and slow
- Positioning entities by guessing pixel coordinates leads to constant compile-test cycles
- No visual feedback until the game runs — impossible to see the level layout during design
- Blocks non-programmers from contributing level designs

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

| Category | Types | Placement Struct | Notable Fields |
|----------|-------|-----------------|----------------|
| World Geometry | sea_gaps | `int` (x position) | SEA_GAP_W=32px wide |
| World Geometry | rails | `RailPlacement` | layout (RECT/HORIZ), x, y, w, h, end_cap |
| Static Geometry | platforms | `PlatformPlacement` | x, tile_height (2-3) |
| Collectibles | coins | `CoinPlacement` | x, y |
| Collectibles | yellow_stars | `YellowStarPlacement` | x, y |
| Collectibles | last_star | `LastStarPlacement` | x, y (single, not array) |
| Enemies | spiders | `SpiderPlacement` | x, vx, patrol_x0, patrol_x1, frame_index |
| Enemies | jumping_spiders | `JumpingSpiderPlacement` | x, vx, patrol_x0, patrol_x1 |
| Enemies | birds | `BirdPlacement` | x, base_y, vx, patrol_x0, patrol_x1, frame_index |
| Enemies | faster_birds | `BirdPlacement` | same struct as birds |
| Enemies | fish | `FishPlacement` | x, vx, patrol_x0, patrol_x1 |
| Enemies | faster_fish | `FishPlacement` | same struct as fish |
| Hazards | axe_traps | `AxeTrapPlacement` | pillar_x, mode (PENDULUM/SPIN) |
| Hazards | circular_saws | `CircularSawPlacement` | x, patrol_x0, patrol_x1, direction |
| Hazards | spike_rows | `SpikeRowPlacement` | x, count (16px tiles) |
| Hazards | spike_platforms | `SpikePlatformPlacement` | x, y, tile_count |
| Hazards | spike_blocks | `SpikeBlockPlacement` | rail_index, t_offset, speed |
| Surfaces | float_platforms | `FloatPlatformPlacement` | mode (STATIC/CRUMBLE/RAIL), x, y, tile_count, rail_index, t_offset, speed |
| Surfaces | bridges | `BridgePlacement` | x, y, brick_count |
| Surfaces | bouncepads_small | `BouncepadPlacement` | x, launch_vy, pad_type=GREEN |
| Surfaces | bouncepads_medium | `BouncepadPlacement` | x, launch_vy, pad_type=WOOD |
| Surfaces | bouncepads_high | `BouncepadPlacement` | x, launch_vy, pad_type=RED |
| Decorations | vines | `VinePlacement` | x, y, tile_count |
| Decorations | ladders | `LadderPlacement` | x, y, tile_count |
| Decorations | ropes | `RopePlacement` | x, y, tile_count |

**Note:** Blue flames (MAX=8) are auto-derived from sea_gaps — no editor placement needed.

### R-003: Visual Canvas
The editor MUST display a scrollable, zoomable viewport showing the 1600x300 logical world with:
- Grid overlay at TILE_SIZE (48px) intervals
- Screen boundary markers at x = 0, 400, 800, 1200
- FLOOR_Y (252px) reference line
- Entity sprites rendered at their actual positions using the game's asset textures (first frame, no animation)
- Floor tiles rendered in non-gap regions, water strips in sea-gap regions

### R-004: Entity Palette
The editor MUST provide a categorized palette panel listing all placeable entity types, rendered with SDL2_ttf text and sprite thumbnails. Selecting a type from the palette enables placement mode with a ghost preview at cursor position.

Categories:
- **World**: Sea Gaps, Rails
- **Collectibles**: Coin, Yellow Star, Last Star
- **Enemies**: Spider, Jumping Spider, Bird, Faster Bird, Fish, Faster Fish
- **Hazards**: Axe Trap, Circular Saw, Spike Row, Spike Platform, Spike Block
- **Surfaces**: Platform, Float Platform, Bridge, Bouncepad (Small/Medium/High)
- **Decorations**: Vine, Ladder, Rope

### R-005: Property Editing
The editor MUST allow editing entity-specific properties after placement via SDL2_ttf-rendered input fields. Property types:
- **Float fields**: position (x, y), velocity (vx), patrol bounds (patrol_x0, patrol_x1), base_y, t_offset, speed, launch_vy, pillar_x
- **Int fields**: tile_count, brick_count, count, tile_height, rail_index, frame_index, direction, end_cap
- **Enum dropdowns**: AxeTrapMode (PENDULUM/SPIN), FloatPlatformMode (STATIC/CRUMBLE/RAIL), RailLayout (RECT/HORIZ), BouncepadType (GREEN/WOOD/RED)

### R-006: JSON Serialization
The editor MUST save and load levels in JSON format using cJSON (vendor library, editor-only). The JSON schema maps 1:1 to `LevelDef` struct fields. Operations:
- Save (Ctrl+S) — write current level to .json file
- Load (Ctrl+O) — read a .json file into the editor
- New (Ctrl+N) — create empty level
- CLI: `super-mango-editor [path.json]` to open directly

Serializer MUST validate array counts against MAX_* bounds on load (reject files that exceed limits).

### R-007: C Code Export
The editor MUST export a level as a valid C source file (`level_XX.c` + `level_XX.h`) via Ctrl+E. The generated code MUST:
- Use designated initializers (`.field = value` syntax)
- Include section separator comments (`/* ---- Section ---- */`)
- Format identically to `level_01.c` (same whitespace, comment style, field ordering)
- Produce a `#pragma once` header with `extern const LevelDef level_XX_def;`
- Compile without warnings using `clang -std=c11 -Wall -Wextra -Wpedantic`

### R-008: Core Editing Operations
The editor MUST support:
- **Place**: click on canvas to create entity at position (Shift = snap to TILE_SIZE grid)
- **Select**: click on existing entity to select (AABB hit test against all entities)
- **Move**: drag selected entity to reposition
- **Delete**: Delete key on selection, or right-click on any entity

### R-009: Undo/Redo
The editor MUST support undo (Ctrl+Z) and redo (Ctrl+Shift+Z) for all editing operations using a command stack (max 256 entries). Tracked operations: place, delete, move, property change.

### R-010: Camera Navigation
The editor MUST support:
- Horizontal scrolling via WASD keys or middle-mouse drag
- Zoom levels: 1x, 2x, 4x (scroll wheel)
- Camera clamps at world boundaries (0 to WORLD_W)

## Out of Scope (v1)

- Play-testing from within the editor (F5 shortcut) — use Ctrl+E → `make run`
- Game executable loading JSON directly (no cJSON in game)
- Tilemap painting for custom floor layouts
- Multi-level campaign management
- Copy/paste and multi-select
- Native file dialog (v1 uses text input for paths)
- Minimap widget

## Constraints

- Must compile with `clang -std=c11 -Wall -Wextra -Wpedantic` (same as game)
- Must use SDL2 + SDL2_image + SDL2_ttf (SDL2_mixer not needed in editor)
- Editor window: 1280x720 (canvas 896x720 left + panels 384x720 right)
- cJSON linked only into editor target — game Makefile unchanged
- Must not modify existing game source files in a way that breaks `make`
- Font: `assets/round9x13.ttf` (already in project)

## Success Criteria

1. `make editor` builds `out/super-mango-editor` without affecting `make` (game)
2. Can open editor, place entities visually, save to JSON, load back — zero data loss
3. Ctrl+E exports `.c` + `.h` that compile with game and produce identical behavior to hand-written levels
4. Round-trip: `level_01` const data → JSON → LevelDef → compare all fields match
5. Round-trip: `level_01` const data → JSON → C export → compile → game behavior identical
