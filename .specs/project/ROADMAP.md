# Roadmap

## Current: Level Editor (feat/editor-mode)

Visual level editor — standalone C/SDL2 application. Primary validation: **recreate sandbox_00.c (Sandbox) pixel-perfectly**.

**Decisions:** JSON+C pipeline, custom SDL2/TTF UI, no game JSON loader.

| Phase | Tasks | Key Deliverable |
|-------|-------|-----------------|
| 1 — Infrastructure | T-001 to T-005 | `make editor` builds, JSON round-trip works |
| 2 — Canvas | T-006 to T-008b | 9-slice floor, water, all entities at game-accurate sizes/positions |
| 3 — UI Framework | T-009 to T-011 | Palette, properties panel, immediate-mode widgets |
| 4 — Editing Tools | T-012 to T-015 | Select/place/move/delete with display-size hit-testing, undo |
| 5 — File Operations | T-016 to T-017 | Save/load JSON, C export |
| 6 — Polish | T-018 to T-020 | Toolbar, sandbox_00.json seed, rail/gap tools |

**Critical path:** T-008 (entity rendering) is the most complex task — requires replicating 25 entity display sizes, Y derivations, source rect crops, 9-slice floor, and rail interpolation.

**Spec:** [spec.md](../features/level-editor/spec.md) — 10 requirements, Sandbox validation criteria
**Design:** [design.md](../features/level-editor/design.md) — entity display table, Y derivation table, floor algorithm, render order, hit-test dims
**Tasks:** [tasks.md](../features/level-editor/tasks.md) — 21 atomic tasks with Sandbox checklist

## Backlog

- Multi-level campaign system
- Boss encounters
- Power-up system
- Mobile touch controls
