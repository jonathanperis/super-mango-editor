# Roadmap

## Current: Level Validation + Editor Documentation Cleanup

Branch: `quality/level-validation-docs-cleanup`

Goal: keep specs and contributor docs aligned with shipped TOML runtime loading, standalone editor, tests, validation tooling, and current entity inventory.

## Shipped Baseline

| Area | Status | Verification |
|------|--------|--------------|
| Game build | Shipped | `make` |
| Runtime TOML loader | Shipped | `make run-level LEVEL=levels/00_sandbox_01.toml` |
| Standalone editor | Shipped | `make editor`, `make run-editor` |
| Tests | Shipped | `make test` |
| Level validation | Shipped | `make validate-levels` |
| Web build | Shipped target | `make web` |

## Near-Term Work Groups

| Group | Scope | Deliverable |
|-------|-------|-------------|
| Validation UX | Editor validation panel; surface `tools/validate_levels.py` output in UI. | Designers see errors before playtest. |
| Metadata Editing | Level name, description, `generated_by`, `next_phase`, music, rules, physics override fields. | Editor can maintain full TOML header/schema. |
| Playtest Flow | Save, validate, then launch game with current TOML level. | One-button editor-to-game loop. |
| Exporter Regression | Add coverage around serializer/exporter paths and representative levels. | Future schema edits fail tests when output drifts. |
| Recent Files + Autosave | MRU list, periodic save, recovery prompt. | Safer long editing sessions. |

## Specs

- [Level editor spec](../features/level-editor/spec.md)
- [Level editor design](../features/level-editor/design.md)
- [Level editor tasks](../features/level-editor/tasks.md)

## Backlog

- Multi-level campaign system.
- Boss encounters.
- Power-up system.
- Mobile touch controls.

## Planning References

- `EXECUTIVE_PROJECT_ENHANCEMENT_REPORT.md`
- `EXECUTIVE_PROJECT_ACHIEVABLES_PLAN.md`
