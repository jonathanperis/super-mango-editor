# Tasks: Visual Level Editor

## Status

Baseline editor is shipped. Old JSON/cJSON implementation tasks are closed as historical planning and no longer describe current work. Current work should be grouped into fewer PRs around validation, metadata, playtest, regressions, and session safety.

## Completed Baseline

| Area | Status | Verification |
|------|--------|--------------|
| Standalone editor executable | Complete | `make editor`, `make run-editor` |
| SDL2/SDL2_ttf UI shell | Complete | `src/editor/ui.*`, `editor.*` |
| Canvas/palette/properties/tools | Complete baseline | `src/editor/canvas.*`, `palette.*`, `properties.*`, `tools.*` |
| Undo/redo | Complete baseline | `src/editor/undo.*` |
| TOML serializer | Complete baseline | `src/editor/serializer.*`, `vendor/tomlc17/` |
| Runtime TOML levels | Complete | `levels/*.toml`, `make run-level LEVEL=...` |
| Level validation tool | Complete baseline | `tools/validate_levels.py`, `make validate-levels`, CI |
| C validation tests | Complete baseline | `make test` |

## Next Task Groups

### G-001: Editor Validation Panel

**Goal:** make validation visible before playtest.

**Work:**
- Run same checks as `make validate-levels` for active level.
- Display parse/schema/count/path diagnostics in editor panel.
- Classify severity: error/warning/info.
- Select entity/property from issue row where mapping exists.
- Block playtest on errors.

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

### G-003: Playtest Button

**Goal:** reduce editor-to-game loop to one action.

**Work:**
- Save active level or prompt for path.
- Run validation.
- Launch game with current TOML level (`--level <path>` semantics).
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

### G-005: Recent Files + Autosave

**Goal:** safer editor sessions.

**Work:**
- Add recent-files menu/list.
- Track dirty state accurately.
- Autosave dirty buffer on interval.
- Recovery prompt when autosave is newer than saved file.
- Confirm unsaved changes on new/open/quit.

**Verify:** crash/reopen path offers recovery; clean close removes stale recovery file.

## Contributor Checks for Editor/Level Changes

Run targeted checks before PR:

```sh
make test
make validate-levels
make editor
```

If docs changed, also run relevant docs lint/build command when available for the changed docs surface.

## References

- `EXECUTIVE_PROJECT_ENHANCEMENT_REPORT.md`
- `EXECUTIVE_PROJECT_ACHIEVABLES_PLAN.md`
