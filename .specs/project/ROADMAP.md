# Roadmap

## Current: Audit Remediation (2026-10-06)

Branch: `fix/audit-remediation`

Goal: resolve both codebase audit rounds (security, runtime, editor, build/CI, assets) while keeping specs and contributor docs aligned with TOML-only runtime loading, fixed-step simulation, authored checkpoints, the sandbox-first campaign, the 15-binary native test inventory, validation tooling, and CI smoke/docs gates.

## Shipped Baseline

| Area | Status | Verification |
|------|--------|--------------|
| Game build | Shipped | `make` |
| Campaign selector + direct TOML load | Shipped | bare launch reads `levels/campaigns/main.toml` in Creator's Playground (sandbox) → Volcanic Depths 1 → 2 order; `make run-level LEVEL=levels/labs/01_collision.toml` bypasses it |
| Authored checkpoints | Shipped | Optional `[[checkpoints]]` use exact x/y respawns; records disable legacy screen-boundary fallback |
| Standalone editor | Shipped | `make editor`, `make run-editor`; palette/canvas/properties/undo/playtest support checkpoints |
| Tests | Shipped | `make test` runs 15 native binaries plus Python level-validation and web-host checks |
| Level validation | Shipped | `make validate-levels` |
| CI smoke gates | Shipped | Native game/editor smoke and WebAssembly artifact smoke in `build.yml` |
| Docs checks | Shipped | `docs.yml` runs docs lint/build |
| Web build | Shipped target | `make web` |

## Near-Term Work Groups

| Group | Scope | Deliverable |
|-------|-------|-------------|
| Validation UX | Expand current validation status/blocking into clickable diagnostics. | Designers jump from issues to fields/entities, including checkpoints. |
| Metadata Editing | Polish shipped metadata/layer/physics editing ergonomics. | Editor can maintain full TOML schema comfortably. |
| Playtest Flow | Polish shipped validate/private-snapshot/launch loop with richer output and failure UX. | One-button editor-to-game loop remains safe and informative. |
| Serializer Regression | Expand TOML serializer coverage with representative level fixtures. | Future schema edits fail tests when saved TOML drifts. |
| Recent Files + Recovery | Improve recovery presentation and cleanup around shipped MRU/recovery baseline. | Safer long editing sessions. |

## Specs

- [Level editor spec](../features/level-editor/spec.md)
- [Level editor design](../features/level-editor/design.md)
- [Level editor tasks](../features/level-editor/tasks.md)

## Backlog

- Campaign-manifest editing in the visual editor.
- Boss encounters.
- Power-up system.
- Mobile touch controls.
