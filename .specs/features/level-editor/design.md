# Design: Visual Level Editor

## Current Design

`super-mango-editor` is standalone SDL2/C application under `src/editor/`. It shares `LevelDef` and game constants with runtime code, but owns its own window, renderer, UI state, canvas, palette, properties, tools, undo stack, serializer, exporter, validation, recent-file/autosave state, playtest launcher, and file-dialog helpers.

```text
super-mango-editor
├── editor_main.c       SDL/IMG/TTF init, entry point
├── editor.c/.h         lifecycle, loop, global EditorState
├── canvas.c/.h         world viewport, camera, render preview
├── palette.c/.h        placeable entity palette
├── properties.c/.h     selected entity fields
├── tools.c/.h          select/place/delete interactions
├── undo.c/.h           undo/redo command stack
├── editor_validation.c/.h in-memory validation report
├── serializer.c/.h     LevelDef ↔ TOML data
├── exporter.c/.h       export helpers
├── file_dialog.c/.h    file interactions
└── ui.c/.h             immediate-mode SDL2_ttf widgets
```

## Build Integration

| Target | Purpose |
|--------|---------|
| `make editor` | Build `out/super-mango-editor`. |
| `make run-editor` | Build and run editor. |
| `make test` | Run 10 native C regression tests. |
| `make validate-levels` | Run `python3 tools/validate_levels.py`. |

`make test` currently runs 10 binaries: serializer, level validation, rail, entity-utils, collision, phase-transition, exporter, editor-validation, gameplay-damage, and gameplay-config.

CI also builds the editor natively, runs game/editor smoke tests, checks WebAssembly artifacts, and runs docs lint/build for docs PRs.

Editor links SDL2, SDL2_image, SDL2_ttf, `tomlc17`, and `src/surfaces/rail.c`; it does not link SDL2_mixer.

## Data Model

TOML files in `levels/` are source of truth for shipped editable levels. `LevelDef` remains in-memory schema shared by runtime loader and editor.

Current level files:

- `levels/00_sandbox_01.toml`
- `levels/01_lugio_01.toml`
- `levels/02_lugio_02.toml`

Top-level TOML data includes metadata, screen count, player spawn, music, floor tile, game rules, physics overrides, background/foreground/fog layers, floor gaps, and entity array tables.

## Editor Inventory

`ENT_COUNT` currently covers 30 placeable types:

| Category | Editor types |
|----------|--------------|
| World/static | `ENT_FLOOR_GAP`, `ENT_RAIL`, `ENT_PLATFORM` |
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
| Fonts | `assets/fonts/round9x13.ttf` |
| Sounds | `assets/sounds/<category>/<file>.wav` |

## Validation Flow

Current validation exists in both CLI/CI and editor UI/status:

```text
TOML file → tools/validate_levels.py → parse/schema/count/path checks → CI / make validate-levels
LevelDef validation C tests → make test
EditorState.level → editor_validate_level → status summary + save/export/autosave/playtest blocking
```

Next design step: turn the shipped validation summary into a richer diagnostics panel with selectable issue rows.

## Next Enhancement Designs

### Validation Panel Polish

- Runs current validation against active file or in-memory serialized temp file.
- Groups issues by severity: error, warning, info.
- Links issue rows to entity/property selection when possible.
- Preserve current blocking semantics for save, export, autosave, and playtest.

### Metadata Editor

- Dedicated tab for top-level TOML fields.
- Validates asset paths using categorized `assets/` roots.
- Preserves multiline `description` and author credit.
- Edits arrays for backgrounds, foregrounds, fog layers.

### Playtest UX Polish

- Preserve current save → validation → `out/super-mango --level <path>` launch flow.
- Show status and stderr/stdout summary in editor.

### Exporter Regression

- Add representative fixture levels.
- Assert serializer round-trip preserves metadata, arrays, counts, paths, and enum strings.
- Assert exporter output remains stable where exporter is used.

### Recent Files + Autosave

- Build on current recent-file list and autosave to `out/autosave/`.
- Prompt recovery on next launch when autosave is newer than explicit save.

## Planning References

- `EXECUTIVE_PROJECT_ENHANCEMENT_REPORT.md`
- `EXECUTIVE_PROJECT_ACHIEVABLES_PLAN.md`
