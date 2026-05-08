# Project State

## Active Work

| Item | Status | Notes |
|------|--------|-------|
| Level validation docs cleanup | In progress | Grouped documentation/spec cleanup on `quality/level-validation-docs-cleanup`. |
| Runtime level loader | Shipped | Game loads TOML files through `tomlc17`; `make run-level LEVEL=levels/00_sandbox_01.toml`. |
| Level editor | Shipped baseline | Standalone SDL2 editor builds with `make editor` / runs with `make run-editor`; reads/writes TOML-backed `LevelDef` data. |
| Level validation | Shipped baseline | `tools/validate_levels.py`, `make validate-levels`, CI validation, and C validation tests exist. |
| CI smoke/docs gates | Shipped baseline | Native editor builds, game/editor smoke tests, WebAssembly artifact smoke, and docs lint/build run in CI. |

## Current Repository Reality

- Runtime levels are TOML files in `levels/`:
  - `levels/00_sandbox_01.toml`
  - `levels/01_lugio_01.toml`
  - `levels/02_lugio_02.toml`
- Parser: vendored `vendor/tomlc17/tomlc17.c`.
- Game target includes `src/editor/serializer.c` for TOML loading support plus `vendor/tomlc17/tomlc17.c`.
- Editor target is separate: `out/super-mango-editor`; no SDL2_mixer link.
- Tests: `make test` runs 8 C test binaries: serializer, level validation, rail, entity-utils, collision, phase-transition, exporter, and editor-validation.
- Validation: `make validate-levels` runs `python3 tools/validate_levels.py` against current TOML levels.
- Editor safeguards: in-memory validation blocks save/export/playtest on errors, status bar shows validation summary, autosave writes valid dirty levels under `out/autosave/`, recent files are tracked, and `--smoke-test` supports CI.

## Decisions (Current)

### D-001: TOML Runtime Level Format
TOML is canonical for editable and shipped levels. `LevelDef` remains in-memory schema. `tomlc17` is vendored and used by runtime/editor serialization paths.

### D-002: Custom SDL2 + SDL2_ttf Editor UI
Standalone editor uses custom immediate-mode SDL2/TTF UI. No external UI toolkit.

### D-003: Direct Level Loading
Game loads TOML levels directly at runtime via `--level <path>` / `make run-level LEVEL=...`. Historical JSON/cJSON/C-export-only planning is obsolete.

## Current Counts

| Area | Count | Source |
|------|-------|--------|
| Editor placeable types | 30 | `ENT_COUNT` in `src/editor/editor.h` |
| Enemy types | 6 | spider, jumping_spider, bird, faster_bird, fish, faster_fish |
| Hazard types | 7 | axe_trap, circular_saw, spike_row, spike_platform, spike_block, blue_flame, fire_flame |
| Collectible types | 5 | coin, star_yellow, star_green, star_red, last_star |
| Surface/climbable types | 9 | platform, float_platform, bridge, bouncepad small/medium/high, vine, ladder, rope |
| Effect systems | 3 | fog, parallax, water |
| TOML level files | 3 | `levels/*.toml` |

## MAX_* Constants Reference

| Constant | Value |
|----------|-------|
| `MAX_FLOOR_GAPS` | 16 |
| `MAX_RAILS` | 16 |
| `MAX_PLATFORMS` | 32 |
| `MAX_COINS` | 64 |
| `MAX_STAR_YELLOWS` | 16 |
| `MAX_STAR_GREENS` | 16 |
| `MAX_STAR_REDS` | 16 |
| `MAX_SPIDERS` | 16 |
| `MAX_JUMPING_SPIDERS` | 16 |
| `MAX_BIRDS` | 16 |
| `MAX_FASTER_BIRDS` | 16 |
| `MAX_FISH` | 16 |
| `MAX_FASTER_FISH` | 16 |
| `MAX_AXE_TRAPS` | 16 |
| `MAX_CIRCULAR_SAWS` | 16 |
| `MAX_SPIKE_ROWS` | 16 |
| `MAX_SPIKE_PLATFORMS` | 16 |
| `MAX_SPIKE_BLOCKS` | 16 |
| `MAX_BLUE_FLAMES` | 16 |
| `MAX_FLOAT_PLATFORMS` | 16 |
| `MAX_BRIDGES` | 16 |
| `MAX_BOUNCEPADS_SMALL` | 16 |
| `MAX_BOUNCEPADS_MEDIUM` | 16 |
| `MAX_BOUNCEPADS_HIGH` | 16 |
| `MAX_VINES` | 24 |
| `MAX_LADDERS` | 16 |
| `MAX_ROPES` | 16 |
| `MAX_BACKGROUND_LAYERS` | 8 |
| `MAX_FOG_TEXTURES` | 4 |

## Deferred Ideas

- Rich editor validation panel with clickable inline TOML diagnostics.
- Richer metadata editor for full background/foreground/fog arrays and physics overrides.
- Exporter/serializer fixture expansion beyond current regression tests.
- Autosave crash-recovery prompt and cleanup policy.
- Tilemap painting for custom floor layouts.
- Multi-level campaign editor with level ordering.

## Planning References

- `EXECUTIVE_PROJECT_ENHANCEMENT_REPORT.md`
- `EXECUTIVE_PROJECT_ACHIEVABLES_PLAN.md`
