# Super Mango Executive Audit Report

Audit date: 2026-05-08

> **Current status note:** This report is retained as historical audit context. Current repository health should be verified with `make test CC=clang`, `make validate-levels`, `make smoke CC=clang`, `make web`, and `cd docs && bun run lint && bun run build`.

Scope: native C11/SDL2 game, standalone level editor, TOML level pipeline, assets, documentation site, CI/CD, local build health.

Remediation status: High-priority and medium-priority findings were addressed on 2026-05-08. The historical findings remain below as audit evidence; current code now has shared level resource reload logic, docs lint, docs route cleanup, stale-reference cleanup, `GameState` runtime/rules/loop subgroups, `LevelDef` count validation, unified game init failure cleanup, and `make test` regression targets wired into native CI.

## Executive Summary

Super Mango is a working, buildable C11/SDL2 platformer with an unusually complete content toolchain for its size. The strongest parts are the data-driven TOML level model, visual editor, categorized assets, and multi-platform CI coverage. Local forced rebuilds passed for both the game and editor, and the Astro documentation site builds.

Main risk is no longer compile health or stale onboarding docs. The resolved audit items tightened level-resource reloads, docs lint, docs routing, stale level-model references, central state grouping, `LevelDef` validation, init cleanup, and native regression tests.

Overall status: healthy prototype / early product codebase. Good foundation. Needs hardening before more level/content scale.

## Audit Snapshot

| Area | Finding |
|---|---|
| Native game build | Pass: `make -B` produced `out/super-mango` with no compiler warnings observed |
| Editor build | Pass: `make -B editor` produced `out/super-mango-editor` with no compiler warnings observed |
| Docs build | Pass: `bun run build` produced the static docs site |
| Docs warning | Resolved: sidebar config moved out of `pages` |
| Docs lint | Pass: `bun run lint` runs `astro check` |
| Web build | Pass locally when emsdk is active; CI installs Emscripten |
| Source size | 125 `.c` files, 110 `.h` files across `src/`, `tests/`, and `vendor/` |
| Assets | 73 PNG sprites, 14 WAV sounds, 1 TTF font |
| Levels | 3 TOML levels in `levels/` |
| Editor palette | 30 placeable entity types (`ENT_COUNT`) |
| CI/CD | Native Linux/macOS/Windows, WebAssembly, CodeQL, Pages deploy |

## Product Readiness

| Dimension | Score | Notes |
|---|---:|---|
| Build reliability | 8/10 | Native/editor/docs build locally. Web passes when a local Emscripten SDK is active, and CI installs Emscripten. |
| Architecture | 7/10 | Clear module split exists, but `GameState` and `game_init` remain large central hubs. |
| Content pipeline | 8/10 | TOML format, editor, exporter, assets, and docs are coherent. |
| Runtime robustness | 7/10 | Shared level resource reload path and init cleanup are improved; native runtime/editor smoke coverage now exists and should keep expanding. |
| Documentation accuracy | 8/10 | Main drift register rows are resolved; keep targeted scans in release flow. |
| Test coverage | 6/10 | Native regression tests, level validation, and game/editor smoke tests exist; deeper gameplay/editor assertions remain the next gap. |
| Release posture | 8/10 | Multi-platform releases, Pages deploy, docs lint, and docs build are in place. |

## Architecture Assessment

The engine follows one central state container: `GameState` in `src/game.h`. Subsystems mutate it through module functions. That pattern is understandable for a small SDL game and keeps ownership visible.

Current architecture layers:

| Layer | Modules |
|---|---|
| Entry/lifecycle | `src/main.c`, `src/game.h`, `src/core/game_lifecycle.c`, `src/core/game_loop.c` |
| State/core | `src/game.h`, `src/core/`, `src/input/`, `src/collision/`, `src/render/` |
| Gameplay | `src/player/`, `src/entities/`, `src/hazards/`, `src/collectibles/`, `src/surfaces/` |
| Level data | `src/levels/level.h`, `src/levels/level_loader.c`, `src/editor/serializer.c`, `levels/*.toml` |
| Tools | `src/editor/` |
| Presentation/docs | `docs/`, `web/`, `.github/workflows/` |

Architecture is readable and source-commented heavily. The biggest resolved issue was level-wide resource drift between initial load and phase transition; remaining maintenance pressure comes from the size of `GameState` and runtime lifecycle/resource orchestration.

## High-Priority Findings

### 1. Phase transitions do not reload all level-wide resources

Evidence:
- Historical finding: the initial load path applied background layers, floor tile override, foreground strip, fog layers, and music in the runtime lifecycle/resource path.
- `game_load_next_phase()` calls `level_load()` and only reinitializes parallax afterward.

Impact:
- A level chain can keep stale floor, foreground strip, fog, or music from the prior phase.
- Custom platform textures loaded by `load_platforms()` can also become lifecycle-sensitive during repeated phase changes.

Recommendation:
- Extract one shared `apply_level_resources(GameState *, const LevelDef *)`.
- Use it from both `game_init()` and `game_load_next_phase()`.
- Before applying new level resources, clean old fog, parallax, music, per-platform textures, and replaceable strip/floor textures deliberately.

### 2. Docs and comments contained stale level model references

Evidence:
- `README.md`, `AGENTS.md`, and `AGENTS.md` referenced obsolete level paths.
- Current levels are `levels/00_sandbox_01.toml`, `levels/01_lugio_01.toml`, and `levels/02_lugio_02.toml`.
- Historical finding: `src/main.c`, `src/core/game_lifecycle.c`, and `src/game.h` comments still said JSON for `--level`.
- `docs/wiki/build-system.md` referenced an obsolete generated-level path that did not match current runtime TOML paths.

Impact:
- New contributors and agents will run wrong commands.
- Documentation undermines the otherwise strong TOML/editor story.

Status:
- Resolved: onboarding docs, wiki build/source references, and C comments now describe TOML levels and current paths.
- Resolved: Makefile docs now describe explicit per-directory wildcards.

### 3. Docs lint command was dead

Evidence:
- Historical finding: `docs/package.json` previously defined `"lint": "eslint"`.
- Historical finding: `bun run lint` previously failed before the docs lint command was moved to Astro checking.

Impact:
- CI or contributors cannot rely on lint.
- Type/style regressions in Astro/React docs are not checked.

Status:
- Resolved: `docs/package.json` now defines `"lint": "astro check"`, and docs lint is wired into CI.

### 4. Astro route warning from sidebar config

Evidence:
- The sidebar config previously lived under `docs/src/pages`, so Astro treated it as a route module.

Impact:
- Docs build is noisy and produces an unintended route surface.

Status:
- Resolved: sidebar config now lives in `docs/src/lib/docsSidebar.ts`.

### 5. No automated runtime regression tests

Evidence:
- No test runner or unit/integration tests found.
- CI checks compile and CodeQL only.

Impact:
- Collision, serialization, level reset, phase transition, editor save/load, and web boot regressions can ship if they compile.

Recommendation:
- Add small C test harnesses for pure logic first: TOML serializer round trip, `LevelDef` bounds handling, rail construction, collision hitboxes.
- Add smoke tests that run `out/super-mango --level levels/00_sandbox_01.toml` headless where SDL dummy drivers are available.

## Medium-Priority Findings

### Central state is large

`GameState` owns window, renderer, audio, all textures, all entity arrays, HUD, debug, level config, and loop state. This is easy to pass around but increasing risk: every feature touches the central struct and broad rebuild surfaces.

Recommendation: introduce small resource groups over time, not as a big refactor. Good first split: `GameResources`, `LevelRuntime`, `GameRules`.

### Resource init has uneven failure cleanup

Early fatal texture failures sometimes clean partial resources; later fatal failures call `exit(EXIT_FAILURE)` directly. OS cleanup handles process exit, but pattern conflicts with stated cleanup discipline and gets risky if init ever becomes restartable.

Recommendation: use one failure path in `game_init()`: set error, jump to cleanup, return/fail. Keep `exit()` in `main()`.

### `level_load()` trusts `LevelDef` counts

TOML parser bounds-checks arrays, but `level_load()` itself copies counts directly from `LevelDef`. That is fine for parser-owned data, less safe for generated or hand-authored C `LevelDef`.

Recommendation: either assert counts or clamp with diagnostic in `level_load()`. Prefer failing loudly during dev builds.

### Star parser uses yellow max constants for green/red arrays

`serializer.c` parses `star_greens` and `star_reds` using `MAX_STAR_YELLOWS`. Today all are 16, so no runtime bug now. It is still a drift trap if per-color limits change.

Recommendation: use `MAX_STAR_GREENS` and `MAX_STAR_REDS`.

### Documentation app build command differed from install tool convention

Historical finding: the deploy workflow used `bun install --frozen-lockfile`, then `npm run build`. This has been resolved; the deploy workflow now uses `bun run build`.

## Strengths

- Native/editor forced rebuilds pass without warnings on local Apple Silicon setup.
- Build system covers native and WebAssembly targets from one Makefile.
- Level format is data-driven TOML, human-readable, and editor-backed.
- Editor is feature-rich: palette, properties, undo, serializer, exporter, playtest flow.
- Asset organization is clean by category.
- CI matrix covers Linux, macOS, Windows, WebAssembly, CodeQL, and Pages deployment.
- Runtime uses logical 400x300 game space scaled to 800x600, which is correct for pixel art.
- Code comments are detailed enough for educational use.
- No TODO/FIXME/HACK markers found outside dependencies/vendor during audit.

## Documentation Drift Register

| File | Status |
|---|---|
| `README.md` | Resolved: current level path, source counts, docs site description, and module tree |
| `AGENTS.md` | Resolved: `.agents` references, docs site description, loader/export wording |
| `AGENTS.md` | Resolved: docs site description and loader/export wording |
| `docs/wiki/build-system.md` | Resolved: explicit per-directory wildcard model, test target, clean behavior |
| `docs/wiki/source-files.md` | Resolved: current module map, constants, loader/validator, star names, floor-gap wording |
| `docs/wiki/constants-reference.md` | Resolved: floor-gap/camera/star constant names |
| `docs/wiki/developer-guide.md` | Resolved: music path, render-order asset names, sprite analysis path |
| `docs/wiki/assets.md` | Resolved: sprite analysis path |
| `docs/wiki/architecture.md` | Resolved: current star/floor-gap/resource names |
| `src/main.c` | Resolved: comments say TOML level load |
| `src/core/game_lifecycle.c` | Resolved: comments say TOML level load |
| `src/game.h` | Resolved: `level_path` comment says TOML |
| `src/levels/level.h` | Resolved: generated export comments updated |
| `src/editor/*` exporter comments | Resolved: current generated export naming |

## Recommended Roadmap

### Next 1-2 days

1. Keep targeted docs drift search in release checklist.
2. Expand SDL dummy-driver smoke coverage beyond the current level/editor smoke paths.
3. Add editor save/load assertions for richer multi-entity TOML workflows.
4. Keep local WebAssembly verification in the release checklist when emsdk is active.

### Next sprint

1. Expand native tests around collision hitboxes and phase transitions.
2. Add visual smoke for WebAssembly boot after Pages artifact assembly.
3. Continue splitting `GameState` when new features touch resource-heavy areas.
4. Add release checklist covering assets, docs, levels, web build, and CI badges.

### Later

1. Add editor regression tests for complex multi-entity save/load/playtest workflows.
2. Consider deeper resource grouping if restartable runtime/editor sessions become a goal.

## Verification Log

Commands run:

```sh
make -B
make -B editor
bun run build
bun run lint
command -v emcc
rg --files
targeted stale-documentation search over README, agent guides, wiki docs, and source comments
file assets/sprites/player/player.png assets/sprites/entities/*.png assets/sprites/hazards/*.png
```

Results:

| Command | Result |
|---|---|
| `make -B` | Pass |
| `make -B editor` | Pass |
| `bun run build` | Pass |
| `bun run lint` | Pass |
| `command -v emcc` | Not found during the 2026-05-08 audit; available when emsdk is activated in the current local setup |
| Stale-doc search | Current targeted stale references resolved |
| TODO/FIXME search | No project TODO/FIXME/HACK markers found |
| Sprite file probe | Key player/entity/hazard PNGs readable |
