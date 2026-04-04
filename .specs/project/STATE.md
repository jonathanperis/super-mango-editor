# Project State

## Active Work

| Item | Status | Notes |
|------|--------|-------|
| Level Editor | Specifying | Feature spec complete, awaiting architectural decisions |

## Pending Decisions

### D-001: Serialization Format
**Options:** JSON-only, C code generation, Binary, JSON-to-C pipeline
**Recommendation:** JSON as canonical format + pipeline to generate .c for compilation
**Status:** Awaiting user decision

### D-002: Editor UI Framework
**Options:** Custom SDL2 immediate-mode UI, Nuklear (header-only), Dear ImGui (C++)
**Recommendation:** Custom SDL2 with SDL2_ttf for v1
**Status:** Awaiting user decision

### D-003: Game JSON Loader
**Question:** Should the game executable also load JSON directly (for play-test shortcuts)?
**Trade-off:** Faster iteration vs. adding cJSON dependency to the game
**Status:** Awaiting user decision

## Lessons Learned

- LevelDef is already well-structured with consistent placement patterns — serialization mapping will be 1:1
- All entity render functions share the same signature pattern (texture + cam_x) — editor can reuse them

## Deferred Ideas

- Tilemap painting for custom floor/gap layouts
- Multi-level campaign editor with level ordering
- Undo history saved to disk for crash recovery
