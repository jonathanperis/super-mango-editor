# Tasks: Visual Level Editor

## Status

Baseline editor is shipped. Old JSON/cJSON implementation tasks are closed as historical planning and no longer describe current work. Current work should be grouped into fewer PRs around validation polish, metadata coverage, playtest UX, regression fixtures, and session recovery.

## Completed Baseline

| Area | Status | Verification |
|------|--------|--------------|
| Standalone editor executable | Complete | `make editor`, `make run-editor` |
| SDL2/SDL2_ttf UI shell | Complete | `src/editor/ui.*`, `editor.*` |
| Canvas/palette/properties/tools | Complete baseline | `src/editor/canvas.*`, `palette.*`, `properties.*`, `tools.*` |
| Undo/redo | Complete baseline | `src/editor/undo.*` |
| Copy/paste | Complete baseline | `src/editor/tools.*`, `src/editor/undo.*` |
| TOML serializer | Complete baseline | `src/editor/serializer.*`, `vendor/tomlc17/` |
| Runtime TOML levels | Complete | `levels/*.toml`, `make run-level LEVEL=...` |
| Level validation tool | Complete baseline | `tools/validate_levels.py`, `make validate-levels`, CI |
| C validation tests | Complete baseline | `make test` runs 11 binaries |
| Editor trust safeguards | Complete baseline | Validation blocks save/export/playtest; status summary shown |
| Playtest launch | Complete baseline | Editor saves, validates, and launches game with `--level` |
| Recent files + autosave | Complete baseline | Recent paths and valid dirty-level autosave under `out/autosave/` |
| Smoke gates | Complete baseline | Game/editor native smoke and WebAssembly artifact smoke in CI |
| Docs checks | Complete baseline | `docs.yml` runs docs lint/build |

## Next Task Groups

### G-001: Editor Validation Panel Polish

**Goal:** make validation visible before playtest.

**Work:**
- Build on shipped `editor_validate_level` status/blocking.
- Surface same classes of checks as `make validate-levels` for active level where practical.
- Display parse/schema/count/path diagnostics in editor panel.
- Classify severity: error/warning/info.
- Select entity/property from issue row where mapping exists.
- Keep save/export/playtest blocked on errors.

**Verify:** malformed TOML, missing asset path, over-MAX entity count, bad `next_phase`, and valid shipped levels produce expected panel states.

### G-002: Metadata Editor

**Goal:** editor can maintain full top-level TOML schema, not only entity arrays.

**Work:**
- Add UI for `name`, `description`, `generated_by`, `screen_count`, `next_phase`.
- Add player spawn fields.
- Add music path/volume and floor tile path fields.
- Add game-rule fields: hearts, lives, score per life, coin score.
- Add physics override fields.
- Add background/foreground/fog layer array editor.

**Verify:** editing metadata, saving, reloading, and running validation preserves all fields.

### G-003: Playtest UX Polish

**Goal:** reduce editor-to-game loop to one action.

**Work:**
- Preserve shipped save + validation + game launch with current TOML level (`--level <path>` semantics).
- Show launch result and validation output in status/panel.

**Verify:** valid level launches; invalid level blocks with actionable error list.

### G-004: Serializer/Exporter Regression Coverage

**Goal:** prevent schema drift and generated-output regressions.

**Work:**
- Add fixtures for current shipped levels and edge-case level data.
- Round-trip TOML through `LevelDef` and compare metadata, counts, arrays, paths, enum values.
- Cover exporter path if exporter remains supported.
- Include checks in `make test` where practical.

**Verify:** intentional schema mismatch fails tests; current shipped levels pass.

### G-005: Recent Files + Autosave Recovery

**Goal:** safer editor sessions.

**Work:**
- Build on shipped recent-files list, dirty indicator, and autosave interval.
- Recovery prompt when autosave is newer than saved file.
- Confirm unsaved changes on new/open/quit.

**Verify:** crash/reopen path offers recovery; clean close removes stale recovery file.

## Contributor Checks for Editor/Level Changes

Run targeted checks before PR:

```sh
make test
make validate-levels
make editor
cd docs && bun run lint && bun run build   # when docs changed
```

## References

- `EXECUTIVE_PROJECT_ENHANCEMENT_REPORT.md`
- `EXECUTIVE_PROJECT_ACHIEVABLES_PLAN.md`
