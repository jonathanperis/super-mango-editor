# Roadmap

## Current: Level Editor (feat/editor-mode)

Visual level editor — standalone C/SDL2 application for creating and editing game levels.

**Decisions finalized:** JSON + C export pipeline (D-001), custom SDL2/TTF UI (D-002), no game JSON loader (D-003).

| Phase | Scope | Tasks | Key Deliverables |
|-------|-------|-------|-----------------|
| 1 — Infrastructure | cJSON vendor, serializer, exporter, Makefile, skeleton | T-001 to T-005 | `make editor` builds, JSON round-trip works |
| 2 — Canvas | Viewport, camera, grid, entity rendering | T-006 to T-008 | Scrollable world with all entities visible |
| 3 — UI Framework | Immediate-mode widgets, palette, properties | T-009 to T-011 | Entity selection + property editing |
| 4 — Editing Tools | Select, move, place, delete, undo/redo | T-012 to T-015 | Full CRUD on entities with undo |
| 5 — File Operations | Save/load JSON, C export | T-016 to T-017 | Ctrl+S/O/E workflow |
| 6 — Polish | Toolbar, status bar, seed JSON, rail/gap tools | T-018 to T-020 | Complete editor experience |

**Spec:** [spec.md](../features/level-editor/spec.md) — 10 requirements (R-001 to R-010)
**Design:** [design.md](../features/level-editor/design.md) — architecture, component design, struct definitions
**Tasks:** [tasks.md](../features/level-editor/tasks.md) — 20 atomic tasks (T-001 to T-020) with dependency graph

## Backlog

- Multi-level campaign system
- Boss encounters
- Power-up system
- Mobile touch controls
