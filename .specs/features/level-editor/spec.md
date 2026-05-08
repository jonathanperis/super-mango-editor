# Feature Spec: Visual Level Editor

## Summary

Standalone SDL2/C editor for creating and editing Super Mango TOML levels. Baseline editor is shipped: `make editor` builds `out/super-mango-editor`, and `make run-editor` launches it. Current work is hardening validation, metadata editing, playtest flow, exporter regressions, and editing-session safety.

## Current Architecture Decisions

| ID | Decision | Current choice |
|----|----------|----------------|
| D-001 | Level format | TOML is canonical. Runtime/editor serialization uses vendored `tomlc17`. |
| D-002 | UI framework | Custom immediate-mode SDL2 + SDL2_ttf. No external UI library. |
| D-003 | Runtime loading | Game loads TOML directly through `--level <path>` / `make run-level LEVEL=...`. |
| D-004 | Validation | `tools/validate_levels.py`, `make validate-levels`, C validation tests, and CI validation protect shipped levels. |

Historical note: early planning described JSON/cJSON plus C export as canonical. That pipeline is obsolete; keep references only when explaining migration history.

## Shipped Baseline Requirements

### R-001: Standalone Executable
Editor builds as separate executable with `make editor` and runs with `make run-editor`. Game build remains separate.

### R-002: TOML Level Support
Editor and runtime operate on TOML-backed `LevelDef` data. Current level files:

- `levels/00_sandbox_01.toml`
- `levels/01_lugio_01.toml`
- `levels/02_lugio_02.toml`

### R-003: Entity Coverage
Editor placeable inventory matches `ENT_COUNT` in `src/editor/editor.h`: 30 types.

| Category | Types |
|----------|-------|
| World | floor_gap, rail, platform |
| Collectibles | coin, star_yellow, star_green, star_red, last_star |
| Enemies | spider, jumping_spider, bird, faster_bird, fish, faster_fish |
| Hazards | axe_trap, circular_saw, spike_row, spike_platform, spike_block, blue_flame, fire_flame |
| Surfaces | float_platform, bridge, bouncepad_small, bouncepad_medium, bouncepad_high |
| Climbables/decor | vine, ladder, rope |
| Spawn | player_spawn |

### R-004: Visual Canvas
Editor previews level geometry, floor gaps, rails, platform surfaces, collectibles, enemies, hazards, climbables, player spawn, grid/reference lines, and theming assets from `assets/sprites/...` paths.

### R-005: Editing Operations
Editor supports baseline selection, placement, deletion, property inspection/editing, undo/redo, save/load, and exporter-related workflows already present in `src/editor/` modules.

### R-006: Validation Commands
Contributor validation commands are:

```sh
make test
make validate-levels
```

## Next Requirements

### N-001: Validation Panel
Editor should expose level validation results inline: TOML parse errors, schema/count bounds, missing assets, bad paths, invalid next-phase links, and gameplay-dangerous placement warnings.

### N-002: Metadata Editor
Editor should edit full top-level TOML metadata: `name`, `description`, `generated_by`, `screen_count`, `next_phase`, music path/volume, floor tile, lives/hearts/scoring, player spawn, background/foreground/fog layers, and physics overrides.

### N-003: Playtest Button
Editor should save, run validation, then launch game with current file using `--level <path>` semantics. Failed validation blocks playtest unless user explicitly chooses a debug override.

### N-004: Exporter Regression
Add regression coverage for serializer/exporter paths and representative TOML levels so schema changes do not silently corrupt saved data.

### N-005: Recent Files + Autosave
Add MRU list, autosave interval, recovery prompt, and dirty-state safety around open/new/quit.

## Out of Scope for This Cleanup

- Modifying C source, Makefile, tools, or workflows.
- Replacing the current editor UI toolkit.
- Changing gameplay rules or entity behavior.
- Full build/run validation.

## Success Criteria

1. Specs describe shipped TOML/tomlc17 state, not JSON/cJSON-era plan.
2. `make editor`, `make test`, and `make validate-levels` are documented in relevant contributor guidance.
3. Current entity counts and current TOML level file names appear correctly.
4. Next editor tasks are grouped for fewer, larger PRs.
