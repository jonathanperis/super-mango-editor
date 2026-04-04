# Feature Spec: Visual Level Editor

## Summary

Standalone SDL2/C application that provides a visual interface for creating, editing, and exporting game levels. Replaces manual `const LevelDef` C struct authoring with a point-and-click editor.

## Motivation

- Hand-coding level definitions in C is error-prone and slow
- Positioning entities by guessing pixel coordinates leads to constant compile-test cycles
- No visual feedback until the game runs — impossible to see the level layout during design
- Blocks non-programmers from contributing level designs

## Requirements

### R-001: Standalone Executable
The editor MUST be a separate executable built with `make editor`, outputting to `out/super-mango-editor`. It shares headers and rendering code with the game but has its own main loop and event handling.

### R-002: Full Entity Support
The editor MUST support placing, moving, and configuring all 25 entity placement types currently defined in `LevelDef`:

| Category | Types |
|----------|-------|
| World Geometry | sea_gaps, rails |
| Static Geometry | platforms |
| Collectibles | coins, yellow_stars, last_star |
| Enemies | spiders, jumping_spiders, birds, faster_birds, fish, faster_fish |
| Hazards | axe_traps, circular_saws, spike_rows, spike_platforms, spike_blocks |
| Surfaces | float_platforms, bridges, bouncepads (small/medium/high) |
| Decorations | vines, ladders, ropes |

### R-003: Visual Canvas
The editor MUST display a scrollable, zoomable viewport showing the 1600x300 logical world with:
- Grid overlay at TILE_SIZE (48px) intervals
- Screen boundary markers at x = 0, 400, 800, 1200
- FLOOR_Y reference line
- Entity sprites rendered at their actual positions using the game's asset textures

### R-004: Entity Palette
The editor MUST provide a categorized palette panel listing all placeable entity types. Selecting a type from the palette enables placement mode.

### R-005: Property Editing
The editor MUST allow editing entity-specific properties after placement. Each entity type has different editable fields:
- Enemies: patrol bounds, speed, start frame
- Hazards: mode (pendulum/spin), tile count, rail index, speed
- Surfaces: mode (static/crumble/rail), brick count, tile count
- Rails: layout type (RECT/HORIZ), dimensions

### R-006: JSON Serialization
The editor MUST save and load levels in JSON format. The JSON schema MUST map 1:1 to the `LevelDef` C struct fields. Operations:
- Save (Ctrl+S) — write current level to .json file
- Load (Ctrl+O) — read a .json file into the editor
- New (Ctrl+N) — create empty level

### R-007: C Code Export
The editor MUST export a level as a valid C source file (`level_XX.c` + `level_XX.h`) that compiles directly with the game. The generated code MUST follow the exact same style and structure as the existing `level_01.c`.

### R-008: Core Editing Operations
The editor MUST support:
- **Place**: click on canvas to create entity at position (with grid snap)
- **Select**: click on existing entity to select it
- **Move**: drag selected entity to reposition
- **Delete**: remove selected entity (Delete key or right-click)

### R-009: Undo/Redo
The editor MUST support undo (Ctrl+Z) and redo (Ctrl+Shift+Z) for all editing operations using a command stack pattern.

### R-010: Camera Navigation
The editor MUST support:
- Horizontal scrolling via mouse drag (middle button or WASD keys)
- Zoom levels: 1x, 2x, 4x (scroll wheel)

## Out of Scope (v1)

- Play-testing from within the editor (F5 shortcut)
- Tilemap painting for custom floor layouts
- Multi-level campaign management
- Copy/paste and multi-select
- File dialog (v1 uses path input)
- Minimap widget

## Constraints

- Must compile with `clang -std=c11` (same as the game)
- Must use SDL2 + SDL2_image + SDL2_ttf + SDL2_mixer (same dependency set)
- Editor window: 1280x720 or larger (needs more screen space than the 800x600 game window)
- Must not modify any existing game source files in a way that breaks the game build
- New shared code must be extracted without breaking existing `#include` chains

## Success Criteria

1. Can open the editor, place entities visually, save to JSON, and export to .c
2. Exported .c file compiles and runs identically to a hand-written level definition
3. Can load an existing level (converted from level_01) and re-export without data loss
4. Round-trip test: LevelDef → JSON → LevelDef produces identical struct contents
