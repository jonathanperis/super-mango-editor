# Roadmap

## Current: Level Validation + Editor Documentation Cleanup

Branch: `quality/level-validation-docs-cleanup`

Goal: keep specs and contributor docs aligned with shipped TOML runtime loading, standalone editor, tests, validation tooling, CI smoke/docs gates, and current entity inventory.

## Shipped Baseline

| Area | Status | Verification |
|------|--------|--------------|
| Game build | Shipped | `make` |
| Runtime TOML loader | Shipped | `make run-level LEVEL=levels/00_sandbox_01.toml` |
| Standalone editor | Shipped | `make editor`, `make run-editor` |
| Tests | Shipped | `make test` |
| Level validation | Shipped | `make validate-levels` |
| CI smoke gates | Shipped | Native game/editor smoke and WebAssembly artifact smoke in `build.yml` |
| Docs checks | Shipped | `docs.yml` runs docs lint/build |
| Web build | Shipped target | `make web` |

## Near-Term Work Groups

| Group | Scope | Deliverable |
|-------|-------|-------------|
| Validation UX | Expand current validation status/blocking into clickable diagnostics. | Designers jump from issues to fields/entities. |
| Metadata Editing | Broaden metadata editing for background/foreground/fog arrays and physics override fields. | Editor can maintain full TOML schema comfortably. |
| Playtest Flow | Polish shipped save/validate/launch loop with richer output and failure UX. | One-button editor-to-game loop remains safe and informative. |
| Exporter Regression | Expand current exporter test with representative level fixtures. | Future schema edits fail tests when output drifts. |
| Recent Files + Autosave | Add recovery prompt and cleanup around shipped MRU/autosave baseline. | Safer long editing sessions. |

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
