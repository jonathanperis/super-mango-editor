# Stale Documentation Cleanup Implementation Plan

> **For Hermes:** Use subagent-driven-development skill to implement this plan task-by-task.

**Goal:** Bring `README.md` and `docs/` back in sync with the current Super Mango source tree, build system, CI workflows, and verified local toolchain.

**Architecture:** This is a documentation-only cleanup. Update stale statements in the public README and the Astro docs wiki first, then optionally refresh the historical `EXECUTIVE_AUDIT_REPORT.md` so it no longer contradicts the current state. Validate with docs lint/build plus native level/test checks.

**Tech Stack:** Markdown, Astro docs content, Bun, GNU Make, C11/SDL2 project metadata.

---

## Current Evidence Baseline

Use this baseline while implementing the plan:

- Repository: `/opt/data/github/jonathanperis/super-mango-editor`
- Branch: `main`
- Current source state:
  - `src/game.h` exists.
  - `src/game.c` does **not** exist.
  - Lifecycle/runtime implementation is split across `src/core/`, including:
    - `src/core/game_lifecycle.c`
    - `src/core/game_loop.c`
    - `src/core/game_resources.c`
    - `src/core/game_update.c`
    - `src/core/game_actors.c`
    - `src/core/game_hazards.c`
    - `src/core/game_completion.c`
- Current levels:
  - `levels/00_sandbox_01.toml`: `screen_count = 4`, 1600 px
  - `levels/01_lugio_01.toml`: `screen_count = 8`, 3200 px
  - `levels/02_lugio_02.toml`: `screen_count = 11`, 4400 px
- Current counts:
  - 30 editor placeable entity types (`ENT_COUNT`)
  - 11 native regression tests
  - 73 PNG assets
  - 14 WAV assets
  - 1 TTF font
  - 13 `docs/wiki/*.md` pages
- Current docs tooling:
  - `docs/package.json` has `"lint": "astro check"`
  - `docs.yml` uses `bun run lint` and `bun run build`
  - `deploy.yml` uses `bun run build`
- GNU Make caveat:
  - `Makefile` says `CC ?= clang`, but GNU Make's built-in default can resolve `CC = cc` unless `CC=clang` is passed explicitly.
  - Use `CC=clang` for CI/local parity commands.

---

## Task 1: Update README feature summary and source map

**Objective:** Remove stale single-stage wording and stale `game.c` references from the main public README.

**Files:**
- Modify: `README.md`

**Changes:**

1. Replace the feature bullet near `README.md:29`:

```md
- 2D side-scrolling platformer with a multi-screen forest stage (1600px world, 4 screens wide)
```

with:

```md
- 2D side-scrolling platformer with dynamic multi-screen TOML worlds, from the 4-screen sandbox to longer volcanic stages
```

2. Replace source-map entry near `README.md:159`:

```md
│   ├── game.h / game.c               GameState struct, window, renderer, game loop
```

with:

```md
│   ├── game.h                        Shared GameState/constants declarations
│   ├── core/                         Runtime lifecycle, window/resources, loop, update, camera, completion
```

3. Review surrounding `README.md` tree entries to avoid duplicating `core/` if it already appears later in the tree. If `core/` already exists in the source tree section, prefer updating its description instead of adding a duplicate.

**Verification:**

```sh
grep -n "game\.c\|1600px world\|multi-screen forest stage" README.md
```

Expected:
- No `game.c` reference in the source tree.
- No “1600px world, 4 screens wide” framing in the top feature bullets.

---

## Task 2: Fix compiler/default wording in README

**Objective:** Make README build instructions clear that `CC=clang` is the recommended explicit local/CI parity setting.

**Files:**
- Modify: `README.md`

**Changes:**

1. In the command block near `README.md:133-143`, change build/test examples from implicit compiler defaults to explicit clang where relevant:

```sh
make CC=clang                         # build the game
make run CC=clang                     # build and run
make run-debug CC=clang               # build and run with debug overlay
make run-level CC=clang LEVEL=levels/00_sandbox_01.toml
make run-level-debug CC=clang LEVEL=levels/00_sandbox_01.toml
make editor CC=clang                  # build the level editor
make run-editor CC=clang              # build and run the level editor
make test CC=clang                    # build and run 11 native regression tests
```

Keep `make validate-levels`, `make web`, and `make clean` as-is.

2. Add a short note after the command block:

```md
> For local/CI parity, pass `CC=clang` explicitly. GNU Make has a built-in `CC=cc`, so the Makefile's `CC ?= clang` default may not select clang on every machine unless overridden.
```

**Verification:**

```sh
grep -n "CC=clang\|GNU Make" README.md
```

Expected: README documents explicit clang usage and the Make caveat.

---

## Task 3: Fix `docs/wiki/build-system.md` compiler model

**Objective:** Align the build-system wiki page with actual Make behavior and local verification commands.

**Files:**
- Modify: `docs/wiki/build-system.md`

**Changes:**

1. Replace the snippet near `docs/wiki/build-system.md:11-14` from:

```makefile
CC      = clang
```

with:

```makefile
CC      ?= clang
```

2. Update the Key Variables table row near `docs/wiki/build-system.md:57` from:

```md
| `CC` | `clang` | C compiler (override with `CC=gcc` if needed) |
```

with:

```md
| `CC` | `clang` intended; pass explicitly for parity | C compiler. Use `CC=clang` for CI/local parity; GNU Make may otherwise use its built-in `CC=cc`. |
```

3. Add a small subsection after Key Variables:

```md
### Compiler Selection Caveat

For reproducible local results, pass `CC=clang` explicitly:

```sh
make test CC=clang
make smoke CC=clang
```

The Makefile uses `CC ?= clang`, but GNU Make defines a built-in `CC=cc`, so some local invocations resolve to `cc` unless the compiler is overridden on the command line.
```

**Verification:**

```sh
grep -n "Compiler Selection Caveat\|CC=clang\|CC.*cc" docs/wiki/build-system.md
```

Expected: The caveat and explicit `CC=clang` commands are present.

---

## Task 4: Remove stale `game.c` references from `docs/wiki/source-files.md`

**Objective:** Make the source-files wiki match the actual source tree.

**Files:**
- Modify: `docs/wiki/source-files.md`

**Changes:**

1. In the file map near `docs/wiki/source-files.md:10-13`, replace:

```md
├── game.c                        Thin public wrapper that includes split core implementation
```

with either nothing or a current `core/` description. The preferred map should be:

```md
├── game.h                        Shared constants + GameState struct (included everywhere)
├── core/                         Runtime lifecycle, resources, loop/update, camera, completion
```

2. Replace section heading near `docs/wiki/source-files.md:248`:

```md
## `game.c` and `core/game_lifecycle.c`
```

with:

```md
## Runtime Core (`core/game_lifecycle.c`, `core/game_loop.c`, `core/game_resources.c`)
```

3. Replace role text near `docs/wiki/source-files.md:250` with:

```md
**Role:** Runtime lifecycle and game-loop implementation lives under `src/core/`. `game_lifecycle.c` owns `game_init` / `game_cleanup`, `game_loop.c` owns `game_loop`, and resource loading/reloading lives in `game_resources.c`.
```

4. Scan the rest of the file for `game.c`:

```sh
grep -n "game\.c" docs/wiki/source-files.md
```

Expected: no stale `game.c` references unless explicitly saying the file was removed/history, which should be avoided in user-facing docs.

**Verification:**

```sh
test ! -e src/game.c
grep -n "game\.c" docs/wiki/source-files.md || true
```

Expected: `src/game.c` absent and no stale docs references remain.

---

## Task 5: Update developer guide implementation instructions away from `game.c`

**Objective:** Ensure contributors adding entities/resources are pointed to the current core modules.

**Files:**
- Modify: `docs/wiki/developer-guide.md`

**Changes:**

Replace stale references:

1. Around `docs/wiki/developer-guide.md:130`, replace heading:

```md
#### 4. Wire up in `game.c`
```

with:

```md
#### 4. Wire up in the runtime core
```

2. In that section, direct implementers to current files:

- Texture/audio/resource loading: `src/core/game_resources.c`
- Init/cleanup orchestration: `src/core/game_lifecycle.c`
- Per-frame update orchestration: `src/core/game_update.c` or the relevant specialized helper in `src/core/`
- Entity update/render helpers: `src/core/game_actors.c`, `src/core/game_hazards.c`, `src/core/game_bouncepads.c`, `src/core/game_float_platforms.c`, etc.
- Collision/pickup behavior: `src/collision/game_collision.c` or focused collision module.

3. Replace lines like:

```md
Also add `debug_log` calls in `game.c` for any significant entity events...
```

with:

```md
Also add `debug_log` calls in the module that owns the event, such as `src/collision/game_collision.c`, `src/core/game_update.c`, or the relevant focused runtime helper.
```

4. Replace checklist items near `docs/wiki/developer-guide.md:394` and `:402` so they no longer name `game.c`.

**Verification:**

```sh
grep -n "game\.c" docs/wiki/developer-guide.md || true
```

Expected: no stale `game.c` instructions remain.

---

## Task 6: Update docs index compiler wording

**Objective:** Keep the docs landing page consistent with the explicit compiler recommendation.

**Files:**
- Modify: `docs/wiki/index.md`

**Changes:**

Replace the tech table row near `docs/wiki/index.md:65`:

```md
| Compiler | `clang` (default), `gcc` compatible |
```

with:

```md
| Compiler | `clang` recommended for CI/local parity; `gcc` compatible |
```

Optionally add a short line in the Quick Start section:

```md
For reproducible local builds, pass `CC=clang` to native Make targets.
```

**Verification:**

```sh
grep -n "Compiler\|CC=clang" docs/wiki/index.md
```

Expected: compiler row no longer implies clang is always selected implicitly.

---

## Task 7: Refresh optional historical audit report or mark it clearly historical

**Objective:** Prevent `EXECUTIVE_AUDIT_REPORT.md` from contradicting the current repository state if users read it.

**Files:**
- Modify: `EXECUTIVE_AUDIT_REPORT.md`

**Decision:** This file is outside the user's requested `README.md` + `./docs` scope, but it contains stale statements found during evaluation. Either refresh it or mark stale sections as historical. Preferred: add a clear status banner and update the current status table.

**Changes:**

1. Add near the top, after the title:

```md
> **Current status note:** This report is retained as historical audit context. Current repository health should be verified with `make test CC=clang`, `make validate-levels`, `make smoke CC=clang`, `make web`, and `cd docs && bun run lint && bun run build`.
```

2. Update stale current-status rows:

- Web build row: change from “Not run locally: `emcc` absent” to “Pass locally when emsdk is active; CI installs Emscripten.”
- Source size row: either remove exact `.c/.h` counts or update them from a fresh count command.
- Docs lint evidence: replace old ESLint wording with current `astro check` wording.
- Test coverage row: mention native smoke exists instead of saying gameplay/editor smoke remains the next gap.
- Deployment command note: remove old `npm run build` mismatch if `deploy.yml` now uses `bun run build`.

**Verification:**

```sh
grep -n "emcc absent\|npm run build\|ESLint 9\|55 `.c`\|gameplay/editor smoke tests remain" EXECUTIVE_AUDIT_REPORT.md || true
```

Expected: no stale current-state claims remain. Historical claims are acceptable only if explicitly labeled historical.

---

## Task 8: Run full documentation and representative repository validation

**Objective:** Prove documentation edits did not break the Astro docs site and still reflect validated repository commands.

**Files:**
- No source edits expected.

**Commands:**

```sh
cd /opt/data/github/jonathanperis/super-mango-editor
export PATH=/opt/data/.local/bin:/opt/data/.cargo/bin:/opt/data/go/bin:$PATH
export LD_LIBRARY_PATH=/opt/data/.local/lib:${LD_LIBRARY_PATH:-}
export PKG_CONFIG_PATH=/opt/data/.local/lib/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/share/pkgconfig:${PKG_CONFIG_PATH:-}
export SDL2CFG=/opt/data/.local/bin/sdl2-config
eval "$(/opt/data/.local/bin/mise activate bash)"

cd docs
bun run lint
bun run build

cd ..
make validate-levels
make test CC=clang
```

Optional but recommended if time allows:

```sh
make smoke CC=clang SMOKE_FRAMES=5 SMOKE_SEED=1
export EMSDK_QUIET=1
source /opt/data/.local/share/mise/installs/emsdk/5.0.7/emsdk_env.sh >/dev/null
make web
```

**Expected:**

- `bun run lint`: `0 errors`, `0 warnings`, `0 hints`
- `bun run build`: 14 pages built
- `make validate-levels`: `validate_levels: ok (3 levels, 88 assets)`
- `make test CC=clang`: all 11 native tests pass
- Optional smoke/web checks pass if run

---

## Task 9: Final stale-reference scan

**Objective:** Catch obvious remaining stale terms before commit.

**Files:**
- Read-only scan.

**Commands:**

```sh
grep -RIn \
  -e "game\.c" \
  -e "1600px world" \
  -e "multi-screen forest stage" \
  -e "CC      = clang" \
  -e "emcc absent" \
  -e "npm run build" \
  -e "ESLint 9" \
  README.md docs EXECUTIVE_AUDIT_REPORT.md
```

**Expected:**

- No stale `game.c` references in `README.md` or `docs/wiki/*.md`.
- No stale single-stage `1600px world` README framing.
- No stale compiler-default snippet in docs.
- If any historical audit-report references remain, they must be explicitly labeled as historical rather than current status.

---

## Task 10: Review diff and commit

**Objective:** Commit a documentation-only cleanup with verified commands.

**Commands:**

```sh
git diff -- README.md docs EXECUTIVE_AUDIT_REPORT.md
git status --short
git add README.md docs EXECUTIVE_AUDIT_REPORT.md
git commit -m "docs: refresh stale build and source references"
```

**Commit note:** If Task 7 is skipped because the user wants only README + docs, commit only:

```sh
git add README.md docs
git commit -m "docs: refresh stale build and source references"
```

**Post-commit verification:**

```sh
git status --short
git log -1 --oneline
```

Expected:
- Working tree clean.
- Latest commit is the docs cleanup commit.

---

## Acceptance Criteria

- `README.md` no longer documents a non-existent `src/game.c` or frames the game as only a single 1600px forest stage.
- `docs/wiki/source-files.md` describes the actual `src/core/` runtime split.
- `docs/wiki/developer-guide.md` points contributors to current runtime/resource/collision files instead of `game.c`.
- `docs/wiki/build-system.md` and `docs/wiki/index.md` explain explicit `CC=clang` usage for local/CI parity.
- Optional: `EXECUTIVE_AUDIT_REPORT.md` is either refreshed or clearly marked historical so stale findings do not read as current truth.
- Docs lint/build pass.
- `make validate-levels` and `make test CC=clang` pass.
- No dev/preview server is left running.
