# Super Mango Audit Roadmap and Expansion Plan

> **For Hermes:** Use subagent-driven-development skill to implement this plan task-by-task.

**Goal:** Convert the full audit findings into an ordered implementation roadmap with progress tracking, next PR/commit queue, validation gates, and expansion plans for Super Mango.

**Architecture:** Preserve the current C11/SDL2 architecture: data-driven TOML levels, focused runtime modules under `src/core/`, rendering under `src/render/`, gameplay systems under specialized folders, editor systems under `src/editor/`, and Astro docs under `docs/`. Prefer small incremental hardening and content/product additions over broad rewrites.

**Tech Stack:** C11, SDL2/SDL2_image/SDL2_ttf/SDL2_mixer, TOML via vendored `tomlc17`, GNU Make, Bun + Astro docs, Emscripten WebAssembly, GitHub Actions.

---

## Planning Progress Snapshot

**Planning state as of:** 2026-05-16 17:10 UTC  
**Repository:** `/opt/data/github/jonathanperis/super-mango-editor`  
**Branch/ref inspected:** `main` @ `6983977`  
**Working tree state during planning:** clean  
**Previous remediation plan:** `.hermes/plans/2026-05-15-stale-docs-cleanup.md`  
**Historical audit report:** `EXECUTIVE_AUDIT_REPORT.md`

| Track | Planned | Evidence checked | Implementation status | Progress |
|---|---:|---:|---|---:|
| Baseline audit/orientation | yes | yes | complete enough for roadmap | 100% |
| Stale docs cleanup | yes | yes | appears committed on `main`; verify in release gates | 100% |
| Product feedback-loop hardening | yes | partial source/test scan | next implementation queue | 25% |
| Editor workflow expansion | yes | README/Makefile/test scan | next sprint queue | 20% |
| Gameplay/content expansion | yes | level/assets/feature inventory | roadmap only | 15% |
| Release/web/demo polish | yes | README/Makefile/docs workflow shape | roadmap only | 20% |
| Full validation pass | yes | dry-run/manifest only | still needs command execution before next commit | 30% |

**Overall planning progress:** ~80% complete. The remaining 20% is converting each roadmap lane into bite-sized implementation plans immediately before work starts, using current diffs and fresh validation output.

---

## Current Verified Context

### Repository health and size

- `README.md` now describes:
  - multi-screen TOML worlds;
  - level-completion summary and next-phase flow;
  - explicit `CC=clang` local/CI parity commands;
  - current `src/core/` split instead of stale `src/game.c` wording.
- `EXECUTIVE_AUDIT_REPORT.md` is marked as historical audit context and points readers to current verification commands.
- Codebase composition from `pygount` excluding `.git`, `node_modules`, `dist`, `build`, `out`, `.astro`, and `vendor`:
  - 444 files total scanned;
  - 20,299 code lines;
  - 16,151 C code lines in 232 C-classified files;
  - 70 Markdown files, counted as docs/comment content by pygount;
  - 88 binary assets in the scanned tree.
- Native test target currently runs 11 executables:
  - `level-serializer-test`
  - `level-validate-test`
  - `runtime-load-test`
  - `rail-test`
  - `entity-utils-test`
  - `collision-test`
  - `phase-transition-test`
  - `exporter-test`
  - `editor-validation-test`
  - `gameplay-damage-test`
  - `gameplay-config-test`

### Current feature state

- Start menu exists and supports level selection over:
  - `levels/00_sandbox_01.toml`
  - `levels/01_lugio_01.toml`
  - `levels/02_lugio_02.toml`
- Level-completion overlay exists in `src/render/render_overlay.c` and shows:
  - title;
  - score;
  - coin count;
  - lives;
  - elapsed time;
  - next-level/final-game hints.
- Completion event handling exists in `src/input/game_events.c`:
  - Enter/Space continues after completion;
  - controller Start continues after completion;
  - pending `next_phase` calls `game_load_next_phase()`;
  - final completion exits the run.
- `GameState` already has partial state grouping:
  - `GameRuntimeState runtime`
  - `GameRules rules`
  - `GameLoopState loop`
  - `GameCompletionState completion`
- Window-focus pause exists via `gs->paused`; player-initiated pause overlay is not implemented yet.
- Game-over behavior currently resets silently through damage/reset logic; `gameplay_damage_test.c` verifies reset behavior, not player-facing game-over UI.
- Start menu text rendering still creates/destroys text textures per frame; comments acknowledge this is simple and acceptable for now, but it is still a known polish/perf improvement.

---

## Roadmap Priorities

### P0 — Validation and release safety gates

**Objective:** Keep every change shippable and avoid regressions while expanding the game.

**Why now:** The project is healthy enough to grow, but game/editor additions touch many C modules. A strong gate prevents content work from breaking runtime, editor serialization, docs, or the web demo.

**Files likely to change:**
- `.github/workflows/build.yml`
- `.github/workflows/docs.yml`
- `.github/workflows/deploy.yml`
- `Makefile`
- `README.md`
- `docs/wiki/build-system.md`
- `docs/wiki/developer-guide.md`
- optionally a new `docs/wiki/release-checklist.md`

**Baseline validation commands:**

```sh
cd /opt/data/github/jonathanperis/super-mango-editor
export PATH=/opt/data/.local/bin:/opt/data/.cargo/bin:/opt/data/go/bin:$PATH
export LD_LIBRARY_PATH=/opt/data/.local/lib:${LD_LIBRARY_PATH:-}
export PKG_CONFIG_PATH=/opt/data/.local/lib/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/share/pkgconfig:${PKG_CONFIG_PATH:-}
export SDL2CFG=/opt/data/.local/bin/sdl2-config
eval "$(/opt/data/.local/bin/mise activate bash)"

make validate-levels
make test CC=clang
make smoke CC=clang SMOKE_FRAMES=5 SMOKE_SEED=1
cd docs && bun run lint && bun run build
```

**Optional web gate when Emscripten is active:**

```sh
cd /opt/data/github/jonathanperis/super-mango-editor
export EMSDK_QUIET=1
source /opt/data/.local/share/mise/installs/emsdk/5.0.7/emsdk_env.sh >/dev/null
make web
```

**Acceptance criteria:**
- Native tests pass.
- All TOML levels validate.
- Smoke test covers every `levels/*.toml` plus editor smoke.
- Docs lint/build pass.
- Web build passes when Emscripten is active.
- No preview/dev server is left running.

---

### P1 — Player-facing pause and game-over overlays

**Objective:** Close the remaining feedback-loop gaps so players see clear state transitions instead of silent reset/exit behavior.

**Current state:**
- Completion overlay already exists.
- Focus-loss pause exists, but no user pause overlay.
- ESC in gameplay exits immediately.
- Controller Start exits during active gameplay, but continues only on completion.
- Game-over resets level/lives/score silently; `tests/gameplay_damage_test.c` verifies the reset.

**Proposed architecture:**
- Add an explicit overlay enum to `src/game.h`, replacing ad-hoc booleans only where needed:

```c
typedef enum GameOverlayState {
    GAME_OVERLAY_NONE = 0,
    GAME_OVERLAY_PAUSED,
    GAME_OVERLAY_LEVEL_COMPLETE,
    GAME_OVERLAY_GAME_OVER
} GameOverlayState;
```

- Keep `GameCompletionState complete` as data initially if that minimizes churn, but route render/input decisions through a single overlay-state helper.
- Add render helpers in `src/render/render_overlay.c`:
  - `render_pause_overlay(GameState *gs)`
  - `render_game_over_overlay(GameState *gs)`
  - keep `render_level_complete_overlay(GameState *gs)`
- Update event handling in `src/input/game_events.c`:
  - ESC toggles pause during active gameplay instead of immediate exit;
  - while paused: Enter/Space resumes, ESC quits or returns to menu depending final product decision;
  - controller Start toggles pause unless completion/game-over overlay is active;
  - completion controls keep current behavior.
- Update damage/game-over flow in `src/collision/collision_damage.c` or the function owning lethal damage reset:
  - when lives reach game-over state, snapshot needed summary and show `GAME_OVERLAY_GAME_OVER`;
  - only reset/restart after player confirmation.

**Likely files:**
- Modify: `src/game.h`
- Modify: `src/input/game_events.c`
- Modify: `src/core/game_loop.c`
- Modify: `src/render/game_render.c`
- Modify: `src/render/render_overlay.c`
- Modify: `src/render/game_render.h`
- Modify: `src/collision/collision_damage.c`
- Test: `tests/gameplay_damage_test.c`
- Test: potentially new `tests/game_overlay_test.c`
- Modify: `Makefile` to add any new test executable
- Docs: `README.md`, `docs/wiki/architecture.md`, `docs/wiki/controls.md` if created

**Implementation tasks:**

1. Add overlay-state enum and initialize it to `GAME_OVERLAY_NONE`.
2. Add unit tests for overlay state transitions that do not need SDL rendering.
3. Route completion overlay state through helper functions without changing visible behavior.
4. Add pause toggle behavior and tests for event-handling state changes where feasible.
5. Add pause overlay rendering.
6. Change game-over from immediate silent reset to explicit overlay + confirm-to-restart.
7. Update gameplay damage tests to assert the new game-over contract.
8. Update docs/README controls wording.
9. Run full validation gates.

**Acceptance criteria:**
- Active gameplay ESC no longer silently quits without feedback.
- Player can pause/resume using keyboard and controller.
- Game-over displays clear text and waits for confirmation before reset/restart.
- Completion overlay behavior remains unchanged.
- Native tests and smoke tests pass.

---

### P1 — Start menu productization

**Objective:** Make the game feel like a complete playable product from first boot.

**Current state:**
- Start menu exists.
- Level selection exists via left/right or A/D.
- Play button exists.
- The menu still has per-frame text texture creation and minimal UI affordances.

**Proposed improvements:**

1. Add visible level selector arrows and selected-level metadata:
   - level name;
   - theme;
   - screens/length;
   - difficulty label.
2. Add a controls/help panel:
   - Move, jump, climb, pause, debug note if relevant.
3. Cache static menu text textures:
   - create a small `MenuText` or `CachedText` struct in `src/screens/start_menu.c`;
   - rebuild only when selected level changes;
   - destroy in `start_menu_cleanup()`.
4. Add menu smoke assertions if practical:
   - default selection is sandbox;
   - selection wraps left/right;
   - selected path is copied safely.

**Likely files:**
- Modify: `src/screens/start_menu.c`
- Modify: `src/screens/start_menu.h`
- Test: new `tests/start_menu_test.c` if pure helpers can be isolated
- Modify: `Makefile`
- Docs: `README.md`, `docs/wiki/controls.md` or `docs/wiki/index.md`

**Acceptance criteria:**
- Start menu clearly communicates how to play.
- Level selector is obvious.
- No avoidable per-frame texture churn for static text.
- Menu still works in native and web builds.

---

### P1 — Editor workflow hardening

**Objective:** Make the level editor safer for real content production.

**Current strengths:**
- TOML save/load.
- C export.
- Undo/redo.
- Native dialogs.
- Validation blocks save/export/playtest.
- Editor smoke test exists.

**Recommended next tests/features:**

1. Add richer save/load round-trip regression:
   - multi-screen level;
   - mixed enemies/hazards/collectibles;
   - background/fog/water/foreground settings;
   - `next_phase`;
   - custom platform tile paths.
2. Add autosave recovery test/flow:
   - create dirty level;
   - write autosave;
   - load/recover explicitly.
3. Add recent-files persistence smoke.
4. Improve validation messages:
   - clickable or entity-index references in editor UI;
   - clearer asset-path diagnostics.
5. Add editor playtest return path docs.

**Likely files:**
- `src/editor/serializer*.c`
- `src/editor/editor_session.c`
- `src/editor/editor_validation.c` or equivalent validation files
- `tests/editor_validation_test.c`
- new fixture(s) under `tests/fixtures/` if the project accepts fixtures
- `docs/wiki/editor.md` or current editor documentation page

**Acceptance criteria:**
- Complex TOML round-trip is covered by automated tests.
- Editor validation failures are actionable for level creators.
- Autosave/recovery semantics are documented and tested.

---

### P2 — Gameplay/content expansion

**Objective:** Expand the playable game while keeping mechanics testable and editor-supported.

**Expansion rules:**
- Every new gameplay object must include:
  - runtime struct/constants;
  - TOML parser/serializer support;
  - editor palette support;
  - validation rules;
  - render/update/collision behavior;
  - at least one regression test for pure logic/serialization;
  - docs entry.
- Prefer one mechanic at a time. Do not add multiple entities in one broad commit.

**Recommended mechanic queue:**

1. **Checkpoint flags**
   - Why: improves longer levels and reduces frustration.
   - Files: `src/levels/level.h`, `src/editor/*serializer*`, `src/core/game_update.c`, `src/collision/*`, `src/render/*`, tests.
   - Acceptance: player respawns at latest checkpoint, editor can place checkpoint, TOML round-trips.

2. **Moving platforms with path nodes**
   - Why: increases level-design variety.
   - Build on existing floating platform/rail patterns.
   - Acceptance: deterministic movement, editor path editing, collision/riding test.

3. **Collectible keys and locked gates**
   - Why: adds puzzle structure without requiring combat AI.
   - Acceptance: key inventory state, gate unlock, TOML/editor support.

4. **Boss/mini-boss arena primitive**
   - Why: creates milestone content for demo expansion.
   - Scope cautiously: one simple boss pattern first.
   - Acceptance: deterministic state machine tests for phases/hits.

**Level/content queue:**

1. Expand `01_lugio_01` with checkpoint tuning after checkpoint mechanic lands.
2. Add `03_*` level only after the feedback-loop and checkpoint work is stable.
3. Add a small “mechanics showcase” level for web demo onboarding.
4. Keep `make validate-levels` and `make smoke` mandatory after every level change.

---

### P2 — Web demo and release polish

**Objective:** Improve public demo quality and confidence in GitHub Pages releases.

**Recommended work:**

1. Add web boot smoke documentation:
   - exact local `make web` command;
   - expected output artifact paths;
   - browser smoke checklist.
2. Add a docs page for controls and browser caveats.
3. Add release checklist:
   - native tests;
   - smoke tests;
   - docs lint/build;
   - web build;
   - asset count/update notes;
   - CI status check.
4. Consider a tiny Playwright/browser smoke for the Pages artifact if CI time is acceptable.

**Likely files:**
- `docs/wiki/release-checklist.md`
- `docs/wiki/controls.md`
- `docs/src/lib/docsSidebar.ts`
- `.github/workflows/deploy.yml`
- `.github/workflows/build.yml`

**Acceptance criteria:**
- A new contributor can verify a release without tribal knowledge.
- Web build and native build instructions are consistent.
- CI covers the highest-value public demo regressions.

---

### P3 — Architecture cleanup without rewrite

**Objective:** Continue reducing central-state pressure only when feature work touches those areas.

**Do not do a standalone mega-refactor.** Use incremental extraction with tests.

**Candidate extractions:**

1. `GameOverlayState` / overlay helpers.
2. `MenuText` cached text helper for start menu.
3. `LevelRuntime` additions for checkpoints and gates.
4. Shared UI text rendering/cache helper if start menu + overlays need repeated text caching.
5. More focused tests around:
   - collision hitboxes;
   - phase transitions;
   - level resource application;
   - editor serializer round trips.

**Acceptance criteria:**
- Each extraction is behavior-preserving or paired with a small feature.
- Each extraction has tests or smoke coverage.
- Public docs stay aligned with source layout.

---

## Next Commit Queue

### Commit 1: `test: add overlay transition coverage`

**Purpose:** Establish tests for pause/completion/game-over transitions before UI changes.

**Files likely touched:**
- Create: `tests/game_overlay_test.c`
- Modify: `Makefile`
- Possibly modify: `src/game.h` if helper declarations are needed

**Validation:**

```sh
make test CC=clang
```

---

### Commit 2: `feat: add gameplay pause overlay`

**Purpose:** Let players pause/resume without exiting immediately.

**Files likely touched:**
- `src/game.h`
- `src/input/game_events.c`
- `src/core/game_loop.c`
- `src/render/game_render.c`
- `src/render/render_overlay.c`
- `src/render/game_render.h`

**Validation:**

```sh
make test CC=clang
make smoke CC=clang SMOKE_FRAMES=5 SMOKE_SEED=1
```

---

### Commit 3: `feat: show game-over overlay before restart`

**Purpose:** Replace silent reset with a clear game-over feedback loop.

**Files likely touched:**
- `src/collision/collision_damage.c`
- `src/input/game_events.c`
- `src/render/render_overlay.c`
- `tests/gameplay_damage_test.c`
- maybe `tests/game_overlay_test.c`

**Validation:**

```sh
make test CC=clang
make smoke CC=clang SMOKE_FRAMES=5 SMOKE_SEED=1
```

---

### Commit 4: `feat: improve start menu level selection UX`

**Purpose:** Make the first screen more product-like and self-explanatory.

**Files likely touched:**
- `src/screens/start_menu.c`
- `src/screens/start_menu.h`
- tests if helper logic is extracted
- README/docs controls updates

**Validation:**

```sh
make test CC=clang
make smoke CC=clang SMOKE_FRAMES=5 SMOKE_SEED=1
cd docs && bun run lint && bun run build
```

---

### Commit 5: `test: expand editor TOML round-trip coverage`

**Purpose:** Protect content-creation workflows before adding more mechanics/levels.

**Files likely touched:**
- `tests/editor_validation_test.c` or new serializer fixture tests
- `tests/fixtures/*` if fixtures are introduced
- `Makefile`

**Validation:**

```sh
make test CC=clang
make validate-levels
```

---

### Commit 6: `docs: add release and controls checklist`

**Purpose:** Make public release verification repeatable.

**Files likely touched:**
- `docs/wiki/release-checklist.md`
- `docs/wiki/controls.md`
- `docs/src/lib/docsSidebar.ts`
- `README.md`

**Validation:**

```sh
cd docs && bun run lint && bun run build
```

---

## Reporting Template for Future Progress Updates

Use this format when reporting back after each planning or implementation chunk:

```md
## Super Mango Progress Update

**Overall roadmap progress:** N%

| Lane | Status | Progress | Notes |
|---|---|---:|---|
| Audit/remediation | done/in progress/blocked | N% | ... |
| Feedback overlays | done/in progress/blocked | N% | ... |
| Start menu/product UX | done/in progress/blocked | N% | ... |
| Editor hardening | done/in progress/blocked | N% | ... |
| Content expansion | done/in progress/blocked | N% | ... |
| Release/web/docs | done/in progress/blocked | N% | ... |

**Completed this pass:**
- ...

**Evidence:**
- command/result or file refs

**Next recommended move:**
- ...
```

---

## Risks and Tradeoffs

- **Overlay refactor risk:** Completion overlay already works; avoid breaking it while adding pause/game-over. Start with tests and helper functions.
- **Game-over design risk:** Waiting for confirmation changes current reset timing. Make this explicit in tests and docs.
- **SDL event testing risk:** Some event paths are easier to smoke-test than unit-test. Extract pure transition helpers where practical.
- **Text caching risk:** Premature general UI abstraction could become larger than the menu issue. Start local to `start_menu.c` unless overlays need the same helper.
- **Content expansion risk:** Adding new mechanics before editor/serializer tests will multiply regression surface.
- **Web build risk:** Emscripten activation is environment-sensitive. Keep it optional locally but mandatory in CI/release checklist when possible.

---

## Final Acceptance Criteria for This Roadmap

- The roadmap is saved under `.hermes/plans/`.
- The user can see overall planning progress and lane-by-lane status.
- The next implementation queue is ordered by risk and product impact.
- Each lane includes likely files and verification gates.
- No source implementation was performed while in planning mode.
- Working tree remains clean except for this plan file if not yet committed.
