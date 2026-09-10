# Project State

## Active Work

| Item | Status | Notes |
|------|--------|-------|
| Phase 4 documentation reconciliation | In progress | Align docs/specs with authored checkpoints, onboarding-first campaign flow, removed C exporter, generated artifacts, and current tests. |
| Runtime level loader | Shipped | Bare game launch loads the v1 campaign manifest and opens its selector; `--level <path>` / `make run-level LEVEL=...` loads TOML directly. |
| Authored checkpoints | Shipped | Optional `[[checkpoints]]` records provide runtime respawns, editor placement/editing/feedback, serializer support, and test coverage. |
| Level editor | Shipped baseline | Standalone SDL2 editor builds with `make editor` / runs with `make run-editor`; reads/writes TOML-backed `LevelDef` data and playtests private TOML snapshots. |
| Level validation | Shipped baseline | `tools/validate_levels.py`, `make validate-levels`, CI validation, and C validation tests exist. |
| CI smoke/docs gates | Shipped baseline | Native editor builds, game/editor smoke tests, WebAssembly artifact smoke, and docs lint/build run in CI. |

## Current Repository Reality

- Runtime levels are TOML files in `levels/`:
  - `levels/00_onboarding_01.toml`
  - `levels/00_sandbox_01.toml`
  - `levels/01_lugio_01.toml`
  - `levels/02_lugio_02.toml`
- Campaign manifest: `levels/campaigns/main.toml` is required for the native selector. Its ordered, unique `levels/*.toml` entries form Forest First Steps → Creator's Playground → Volcanic Depths 1 → Volcanic Depths 2 and must form a linear `[last_star].next_phase` chain; the final entry has no `next_phase`.
- Parser: vendored `vendor/tomlc17/tomlc17.c`.
- Game target includes `src/editor/serializer.c` for TOML loading support plus `vendor/tomlc17/tomlc17.c`.
- Editor target is separate: `out/super-mango-editor`; no SDL2_mixer link.
- Tests: `make test` runs 15 native binaries: level-serializer, level-validate, runtime-load, rail, entity-utils, collision, phase-transition, editor-validation, gameplay-damage, gameplay-config, gameplay-score, game-overlay, game-events, session, and game-checkpoint. It also runs `tests/validate_levels_test.py` and `tools/check_web_boot_contract.py`.
- Validation: `make validate-levels` runs `python3 tools/validate_levels.py` against current TOML levels.
- Checkpoints: every record needs finite `x`/`y`; x is unique, strictly after the effective start, and within the world. The furthest crossed record determines `respawn_x`/`respawn_y`; any authored record disables automatic screen-boundary fallback, while no records preserves it. Retry/replay/next level reset to the destination level start.
- Editor safeguards: in-memory validation blocks save/autosave/playtest on errors, status/panel show the validation summary, recovery snapshots use the SDL preference directory, recent files are tracked, and `--smoke-test` supports CI.

## Decisions (Current)

### D-001: TOML Runtime Level Format
TOML is canonical for editable and shipped levels. `LevelDef` remains in-memory schema. `tomlc17` is vendored and used by runtime/editor serialization paths. No C level-export workflow remains.

### D-002: Custom SDL2 + SDL2_ttf Editor UI
Standalone editor uses custom immediate-mode SDL2/TTF UI. No external UI toolkit.

### D-003: Direct Level Loading
Bare launch loads the validated v1 `levels/campaigns/main.toml` selector. `--level <path>` / `make run-level LEVEL=...` bypasses it and loads that TOML level directly, even when the path is not in the manifest.

## Current Counts

| Area | Count | Source |
|------|-------|--------|
| Editor placeable types | 31 | `ENT_COUNT` in `src/editor/editor.h` |
| Authored checkpoint capacity | 99 | `MAX_CHECKPOINTS` in `src/game.h` |
| Enemy types | 6 | spider, jumping_spider, bird, faster_bird, fish, faster_fish |
| Hazard types | 7 | axe_trap, circular_saw, spike_row, spike_platform, spike_block, blue_flame, fire_flame |
| Collectible types | 5 | coin, star_yellow, star_green, star_red, last_star |
| Surface/climbable types | 9 | platform, float_platform, bridge, bouncepad small/medium/high, vine, ladder, rope |
| Effect systems | 3 | fog, parallax, water |
| Playable TOML level files | 4 | `levels/*.toml` |
| Campaign manifests | 1 | `levels/campaigns/main.toml` (v1) |

## MAX_* Constants Reference

| Constant | Value |
|----------|-------|
| `MAX_FLOOR_GAPS` | 16 |
| `MAX_CHECKPOINTS` | 99 |
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
| `MAX_FIRE_FLAMES` | 16 |
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
- Metadata-editor ergonomics for shipped background/foreground/fog arrays and physics overrides.
- Serializer round-trip fixture expansion beyond current regression tests.
- Recovery snapshot presentation and cleanup policy.
- Tilemap painting for custom floor layouts.
- Campaign-manifest editing and ordering support in the visual editor.
