# Feature Spec: Visual Level Editor

## Summary

Standalone SDL2/C editor for creating and editing Super Mango TOML levels. Baseline editor is shipped: `make editor` builds `out/super-mango-editor`, and `make run-editor` launches it. It includes TOML save/load, authored checkpoint placement, validation blocking for unsafe persistence/playtest, private-snapshot playtest launch, recent files, recovery snapshots, and smoke-test support. Current work is polish rather than format migration.

## Current Architecture Decisions

| ID | Decision | Current choice |
|----|----------|----------------|
| D-001 | Level format | TOML is canonical. Runtime/editor serialization uses vendored `tomlc17`. |
| D-002 | UI framework | Custom immediate-mode SDL2 + SDL2_ttf. No external UI library. |
| D-003 | Runtime loading | Bare launch selects from v1 `levels/campaigns/main.toml`; `--level <path>` / `make run-level LEVEL=...` loads TOML directly. |
| D-004 | Validation | `tools/validate_levels.py`, `make validate-levels`, C validation tests, editor in-memory validation, and CI validation protect shipped levels. |
| D-005 | CI quality gates | Native game/editor builds, 15 native test binaries plus Python host checks, TOML and campaign-manifest validation, game/editor smoke, WebAssembly artifact smoke, and docs lint/build are CI-gated. |

## Shipped Baseline Requirements

### R-001: Standalone Executable
Editor builds as separate executable with `make editor` and runs with `make run-editor`. Game build remains separate.

### R-002: TOML Level Support
Editor and runtime operate on TOML-backed `LevelDef` data. Current level files:

- `levels/00_onboarding_01.toml`
- `levels/00_sandbox_01.toml`
- `levels/01_lugio_01.toml`
- `levels/02_lugio_02.toml`
- `levels/campaigns/main.toml` is the v1 ordered native-selector manifest, not an editable `LevelDef` file.

### R-003: Entity Coverage
Editor placeable inventory matches `ENT_COUNT` in `src/editor/editor.h`: 31 types.

| Category | Types |
|----------|-------|
| World | player_spawn, floor_gap, checkpoint, rail, platform |
| Collectibles | coin, star_yellow, star_green, star_red, last_star |
| Enemies | spider, jumping_spider, bird, faster_bird, fish, faster_fish |
| Hazards | axe_trap, circular_saw, spike_row, spike_platform, spike_block, blue_flame, fire_flame |
| Surfaces | float_platform, bridge, bouncepad_small, bouncepad_medium, bouncepad_high |
| Climbables/decor | vine, ladder, rope |
| Spawn | player_spawn |

### R-004: Visual Canvas
Editor previews level geometry, floor gaps, checkpoint markers, rails, platform surfaces, collectibles, enemies, hazards, climbables, player spawn, grid/reference lines, and theming assets from `assets/sprites/...` paths.

### R-005: Editing Operations
Editor supports selection, placement, drag, deletion, property inspection/editing, undo/redo, copy/paste, TOML save/load, recent files, recovery snapshots, and playtest launch workflows already present in `src/editor/` modules. Checkpoints use the same workflow and expose required `x`/`y` inspector fields plus a derived screen label.

### R-006: Trust Safeguards
Editor validation runs before save, autosave, and playtest. Errors block unsafe actions, and the Level Config panel/status feedback show the validation summary and messages. Checkpoints require finite `x`/`y`, unique x, an x strictly after the effective start, and in-world coordinates.

### R-008: Authored Checkpoint Schema and Runtime Contract

`[[checkpoints]]` is optional TOML data. Each record requires finite numeric `x` and `y`; at most 99 records are allowed. Runtime advances to the furthest checkpoint at or behind the player and respawns at its exact coordinates. Any authored record disables legacy automatic screen-boundary checkpoints; no records preserves the legacy behavior. Retry, replay, and successful phase transitions reset to the respective level start.

### R-009: Onboarding Campaign Flow

`levels/00_onboarding_01.toml` is first in `levels/campaigns/main.toml`. Its two authored checkpoints bracket the flame-marked gap, and its `[last_star].next_phase` advances to `levels/00_sandbox_01.toml`.

### R-007: Validation Commands
Contributor validation commands are:

```sh
make test
make validate-levels
```

`make test` currently runs 15 native binaries: level-serializer, level-validate, runtime-load, rail, entity-utils, collision, phase-transition, editor-validation, gameplay-damage, gameplay-config, gameplay-score, game-overlay, game-events, session, and game-checkpoint. It also runs `tests/validate_levels_test.py` and the `web-host-contract` Python check.

## Next Requirements

### N-001: Validation Panel Polish
Editor should expand the shipped validation summary/blocking into clickable inline results: TOML parse errors, schema/count bounds, missing assets, bad paths, invalid next-phase links, and gameplay-dangerous placement warnings.

### N-002: Metadata Editor UX
The editor already edits full top-level TOML metadata: `name`, `description`, `generated_by`, `screen_count`, `next_phase`, music path/volume, floor tile, lives/hearts/scoring, player spawn, background/foreground/fog layers, and physics overrides. Improve discoverability and dense-panel ergonomics without adding a second format or a C exporter.

### N-003: Playtest UX Polish
The shipped Play button validates, serializes a private temporary TOML snapshot, and launches the game with `--level <path>` semantics without overwriting the source file. Next work should improve stderr/stdout reporting and recovery when build/launch fails.

### N-004: TOML Serializer Regression
Add regression coverage for TOML serializer paths and representative levels so schema changes do not silently corrupt saved data.

### N-005: Recent Files + Autosave Recovery
Recent files and recovery snapshots exist, including recovery choice UI, stale-snapshot retirement, and dirty-state safeguards around open/new/quit. Improve the recovery presentation and add regression coverage for less common host/file-system failures.

## Out of Scope for This Cleanup

- Modifying C source, Makefile, tools, or workflows.
- Replacing the current editor UI toolkit.
- Changing gameplay rules or entity behavior.
- Full build/run validation.

## Success Criteria

1. Specs describe shipped TOML/tomlc17 state, not JSON/cJSON-era plan.
2. `make editor`, `make test`, `make validate-levels`, docs lint/build, and smoke gates are documented in relevant contributor guidance.
3. Current entity counts, checkpoint semantics, and current TOML level file names appear correctly.
4. Next editor tasks are grouped for fewer, larger PRs.
