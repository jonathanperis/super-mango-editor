# Roadmap

## Current: Level Editor (feat/editor-mode)

Visual level editor — standalone C/SDL2 application for creating and editing game levels.

| Phase | Scope | Tasks | Status |
|-------|-------|-------|--------|
| 1 — Infrastructure | cJSON, serializer, exporter, Makefile, editor skeleton | T-001 to T-005 | Not started |
| 2 — Canvas | Viewport, camera, grid, entity rendering | T-006 to T-008 | Not started |
| 3 — UI Framework | Immediate-mode widgets, palette, properties panel | T-009 to T-011 | Not started |
| 4 — Editing Tools | Select, move, place, delete, undo/redo | T-012 to T-015 | Not started |
| 5 — File Operations | Save/load JSON, C export | T-016 to T-017 | Not started |
| 6 — Polish | Toolbar, status bar, level_01 conversion, rail/gap tools | T-018 to T-020 | Not started |

**Spec:** [.specs/features/level-editor/spec.md](../features/level-editor/spec.md)
**Design:** [.specs/features/level-editor/design.md](../features/level-editor/design.md)
**Tasks:** [.specs/features/level-editor/tasks.md](../features/level-editor/tasks.md)

## Backlog

- Multi-level campaign system
- Boss encounters
- Power-up system
- Mobile touch controls
