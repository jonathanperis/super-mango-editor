# Design: Visual Level Editor

## Current Design

`super-mango-editor` is a standalone C11/raylib application under `src/editor/`. It shares `LevelDef` and game constants with runtime code, but owns its own window, renderer, UI state, canvas, palette, properties, tools, undo stack, TOML serializer, validation, recent-file/autosave state, playtest launcher, and file-dialog helpers.

```text
super-mango-editor
├── editor_main.c       raylib window/audio init, entry point
├── editor.c/.h         lifecycle, loop, global EditorState
├── editor_frame/events/chrome/panels/layout.c   per-frame UI and input routing
├── canvas.c/.h         world viewport, camera (x and y), render preview
├── hit_test.c/.h       entity bounds and click priority (reverse draw order)
├── palette.c/.h        placeable entity palette
├── properties.c/.h     selected entity fields
├── tools.c/.h          select/place/delete/drag interactions
├── entity_meta.c/.h    per-type names, capacities, shared read/insert/remove
├── editor_clipboard.c/.h copy/paste (rail riders keep their rail)
├── undo.c/.h, editor_undo_apply.c/.h  undo/redo command stack
├── editor_files/session/playtest.c/.h  save/load, recovery, recent files, playtest
├── editor_validation.c/.h in-memory validation report
├── file_dialog.c/.h, dialog_choice.c  native pickers and decision dialogs
└── ../shared/ui.c/.h, serializer*.c/.h  immediate-mode widgets; LevelDef <-> TOML
```

## Build Integration

| Target | Purpose |
|--------|---------|
| `make editor` | Build `out/super-mango-editor`. |
| `make run-editor` | Build and run editor. |
| `make test` | Run 15 native regression binaries plus Python level-validation and web-host checks. |
| `make validate-levels` | Run `python3 tools/validate_levels.py`. |

`make test` currently runs 15 native binaries: level-serializer, level-validate, runtime-load, rail, entity-utils, collision, phase-transition, editor-validation, gameplay-damage, gameplay-config, gameplay-score, game-overlay, game-events, session, and game-checkpoint. `tests/validate_levels_test.py` and `tools/check_web_boot_contract.py` run as Python host checks in the same target.

CI also builds the editor natively, runs game/editor smoke tests, checks WebAssembly artifacts, and runs docs lint/build for docs PRs.

Editor links the pinned static raylib 6.0 build, `tomlc17`, and `src/surfaces/rail.c`. Text uses raylib's built-in font.

## Data Model

TOML files in `levels/` are source of truth for shipped editable levels. `LevelDef` remains in-memory schema shared by runtime loader and editor.

Current level files:

- `levels/00_sandbox_01.toml`
- `levels/01_lugio_01.toml`
- `levels/02_lugio_02.toml`

Top-level TOML data includes metadata, screen count, player spawn, music, floor tile, game rules, physics overrides, background/foreground/fog layers, floor gaps, optional authored `[[checkpoints]]`, and entity array tables. Checkpoint records require finite numeric `x` and `y`; runtime resolves the furthest crossed record as respawn state, while levels without records keep legacy screen-boundary respawns.

## Editor Inventory

`ENT_COUNT` currently covers 31 placeable types:

| Category | Editor types |
|----------|--------------|
| World/static | `ENT_PLAYER_SPAWN`, `ENT_FLOOR_GAP`, `ENT_CHECKPOINT`, `ENT_RAIL`, `ENT_PLATFORM` |
| Collectibles | `ENT_COIN`, `ENT_STAR_YELLOW`, `ENT_STAR_GREEN`, `ENT_STAR_RED`, `ENT_LAST_STAR` |
| Enemies | `ENT_SPIDER`, `ENT_JUMPING_SPIDER`, `ENT_BIRD`, `ENT_FASTER_BIRD`, `ENT_FISH`, `ENT_FASTER_FISH` |
| Hazards | `ENT_AXE_TRAP`, `ENT_CIRCULAR_SAW`, `ENT_SPIKE_ROW`, `ENT_SPIKE_PLATFORM`, `ENT_SPIKE_BLOCK`, `ENT_BLUE_FLAME`, `ENT_FIRE_FLAME` |
| Surfaces | `ENT_FLOAT_PLATFORM`, `ENT_BRIDGE`, `ENT_BOUNCEPAD_SMALL`, `ENT_BOUNCEPAD_MEDIUM`, `ENT_BOUNCEPAD_HIGH` |
| Climbables/decor | `ENT_VINE`, `ENT_LADDER`, `ENT_ROPE` |
| Spawn | `ENT_PLAYER_SPAWN` |

## Asset Path Rules

Use categorized asset paths. Bare legacy paths such as `assets/<sprite>.png` are stale.

| Category | Example path |
|----------|--------------|
| Collectibles | `assets/sprites/collectibles/coin.png` |
| Entities | `assets/sprites/entities/spider.png` |
| Hazards | `assets/sprites/hazards/blue_flame.png` |
| Level tiles | `assets/sprites/levels/grass_tileset.png` |
| Surfaces | `assets/sprites/surfaces/bouncepad_medium.png` |
| Foregrounds | `assets/sprites/foregrounds/water.png` |
| Backgrounds | `assets/sprites/backgrounds/sky_blue.png` |
| Fonts | raylib built-in default font (no file) |
| Sounds | `assets/sounds/<category>/<file>.wav` |

## Validation Flow

Current validation exists in both CLI/CI and editor UI/status:

```text
TOML file → tools/validate_levels.py → parse/schema/count/path checks → CI / make validate-levels
LevelDef validation C tests → make test
EditorState.level → editor_validate_level → status/panel summary + save/autosave/playtest blocking
```

Next design step: turn the shipped validation summary into a richer diagnostics panel with selectable issue rows.

## Next Enhancement Designs

### Validation Panel Polish

- Runs current validation against active file or in-memory serialized temp file.
- Groups issues by severity: error, warning, info.
- Links issue rows to entity/property selection when possible.
- Preserve current blocking semantics for save, autosave, and playtest.

### Metadata Editor UX

- Preserve the shipped top-level TOML fields in the dense Level Config panel.
- Validates asset paths using categorized `assets/` roots.
- Preserves multiline `description` and author credit.
- Edits arrays for backgrounds, foregrounds, fog layers.

### Playtest UX Polish

- Preserve current validation → private TOML snapshot → `out/super-mango --level <path>` launch flow.
- Show status and stderr/stdout summary in editor.

### TOML Serializer Regression

- Add representative fixture levels.
- Assert serializer round-trip preserves metadata, arrays, counts, paths, and enum strings.

### Recent Files + Autosave

- Build on the current recent-file list and recovery snapshots in the per-user preference directory.
- Preserve the shipped recovery choice and improve its presentation when snapshots are available.
