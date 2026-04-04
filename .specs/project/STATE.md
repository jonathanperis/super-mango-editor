# Project State

## Active Work

| Item | Status | Notes |
|------|--------|-------|
| Level Editor | Designing | Spec + design + tasks complete, decisions finalized, ready for implementation |

## Decisions (Finalized)

### D-001: Serialization Format — JSON + C Pipeline
**Decision:** JSON is the canonical level format for the editor. A separate C code exporter generates `level_XX.c` + `level_XX.h` files that the game compiles as `const LevelDef`. The game executable never links cJSON — it only sees compiled C structs.

**Rationale:**
- JSON is human-readable, git-diffable, and trivial to parse with cJSON
- Game stays zero-dependency on JSON — levels remain static `const` data with no runtime overhead
- Round-trip fidelity: JSON schema maps 1:1 to LevelDef fields
- Export step is automatable (`make export` or Ctrl+E in editor)

**Implications:**
- `vendor/cJSON/` is editor-only (not linked into game target)
- `src/editor/serializer.c` handles JSON ↔ LevelDef
- `src/editor/exporter.c` generates C source matching `level_01.c` style exactly
- Level files live as `.json` in `levels/` directory, exported `.c` in `src/levels/`

### D-002: Editor UI Framework — Custom SDL2 + SDL2_ttf
**Decision:** Build a minimal immediate-mode UI system using SDL2 primitives and SDL2_ttf for text. No external UI library.

**Rationale:**
- The editor needs limited widgets: buttons, labels, panels, int/float fields, dropdowns
- SDL2_ttf + `round9x13.ttf` are already available in the project
- Avoids C++ dependency (Dear ImGui) or additional vendor code (Nuklear)
- Custom code gives full control over look and interaction model
- Fits the project's "learning resource" philosophy — UI code is readable C

**Implications:**
- `src/editor/ui.c` implements ~7 widget functions (~300-400 lines estimated)
- All widgets use immediate-mode pattern: call each frame, return interaction state
- Text input for property fields handled via `SDL_TEXTINPUT` events
- Color scheme: dark panels (#2D2D2D), light text (#E0E0E0), blue accent (#4A90D9)

### D-003: Game JSON Loader — No (v1)
**Decision:** The game executable does not load JSON. Levels enter the game exclusively as compiled C (`const LevelDef`).

**Rationale:**
- Keeps game build simple and free of cJSON dependency
- No runtime file I/O needed — level data is baked into the binary
- Play-testing workflow: Ctrl+E export → `make run` (fast enough for iteration)
- If hot-reload becomes needed later, cJSON can be added to the game in v2

**Implications:**
- No `--level path.json` flag on the game executable
- Play-test cycle: edit in editor → Ctrl+E export → `make run`
- Editor and game are fully independent executables with no runtime coupling

## Lessons Learned

- LevelDef has 25 placement types across 30+ array fields — serializer must handle each explicitly
- All entity render functions share `(entity, renderer, texture, cam_x)` signature — editor can render previews without game logic
- `level_01.c` uses designated initializers with section comments — exporter must reproduce this formatting exactly
- Blue flames are auto-derived from sea_gaps — editor doesn't need a blue_flame placement tool (they appear automatically)
- Bouncepads are split into 3 separate arrays (small/medium/high) matching 3 texture slots — serializer must handle each variant separately
- `last_star` is a single struct (not an array) in LevelDef — only one per level

## MAX_* Constants Reference (for serializer bounds checking)

| Constant | Value | Entity |
|----------|-------|--------|
| MAX_SEA_GAPS | 8 | Sea gaps |
| MAX_RAILS | 4 | Rail paths |
| MAX_PLATFORMS | 8 | Ground pillars |
| MAX_COINS | 24 | Coins |
| MAX_YELLOW_STARS | 3 | Yellow stars |
| MAX_SPIDERS | 4 | Spiders |
| MAX_JUMPING_SPIDERS | 4 | Jumping spiders |
| MAX_BIRDS | 4 | Birds |
| MAX_FASTER_BIRDS | 4 | Faster birds |
| MAX_FISH | 4 | Fish |
| MAX_FASTER_FISH | 4 | Faster fish |
| MAX_AXE_TRAPS | 4 | Axe traps |
| MAX_CIRCULAR_SAWS | 4 | Circular saws |
| MAX_SPIKE_ROWS | 4 | Spike rows |
| MAX_SPIKE_PLATFORMS | 4 | Spike platforms |
| MAX_SPIKE_BLOCKS | 4 | Spike blocks |
| MAX_FLOAT_PLATFORMS | 6 | Float platforms |
| MAX_BRIDGES | 2 | Bridges |
| MAX_BOUNCEPADS_SMALL | 4 | Green bouncepads |
| MAX_BOUNCEPADS_MEDIUM | 4 | Wood bouncepads |
| MAX_BOUNCEPADS_HIGH | 4 | Red bouncepads |
| MAX_VINES | 24 | Vines |
| MAX_LADDERS | 8 | Ladders |
| MAX_ROPES | 8 | Ropes |

## Deferred Ideas

- Tilemap painting for custom floor/gap layouts
- Multi-level campaign editor with level ordering
- Undo history saved to disk for crash recovery
- Hot-reload: game reads JSON directly for instant play-test (v2)
- Play-test shortcut (F5 → export + compile + run)
