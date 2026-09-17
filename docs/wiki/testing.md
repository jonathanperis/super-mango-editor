# Testing & Smoke Matrix

<a id="home"></a>

---

Use this page to choose the smallest useful verification set for a change. Run commands from the repository root unless a row says otherwise.

## Quick Matrix

| Changed area | Run locally | Why |
|--------------|-------------|-----|
| C runtime, gameplay, collision, score, overlays, sessions | `make test CC=clang` | Builds and runs 15 native regression binaries plus Python level-validation and web-host checks. |
| Level TOML, campaign manifest, or level schema | `make validate-levels` and `make docs-drift` | Validates root and lab levels, the v1 campaign manifest, generated facts, schema docs and prose counts. |
| Player/world runtime startup | `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy make smoke CC=clang SMOKE_FRAMES=5 SMOKE_SEED=1` | Boots every TOML level plus the editor in bounded dummy-SDL mode. |
| Replay or event handling | `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy make scripted-smoke CC=clang SMOKE_FRAMES=5 SMOKE_SEEDS="1 7 23"` | Generates deterministic replay inputs and injects movement/jump/pause events across levels. |
| Editor behaviour | `make editor CC=clang` and `./out/super-mango-editor --smoke-test` | Builds the standalone editor and checks headless startup/exit. |
| Memory/UB-sensitive C changes | `make sanitize CC=clang` and, when startup paths changed, `make sanitize-smoke CC=clang` | Runs AddressSanitizer/UBSan over tests and optionally smoke startup. |
| Docs, README, Pages routes | `make docs-drift`, then from `docs/`: `bun run lint`, `bun run build`, `bun run check-site` | Checks generated facts, TOML examples, API/CLI references, Astro compilation and emitted routes/links/metadata/sitemap. Uses the frozen `bun.lock` dependency set. |
| WebAssembly payload | `make web` when local Emscripten is healthy; otherwise trust green GitHub Actions WebAssembly + Pages smoke | CI is authoritative for Emscripten/SDL port-cache issues. |
| Release packaging | `make dist-native` and `make dist-wasm` or the `Build & Release` workflow | Produces native and WebAssembly archives using the same archive layout described in the release checklist. |

## Native Regression Tests

The session suite also runs `tests/simulation_test.c`: 180-step capture/replay
equivalence, inspection pause ownership, moving-platform carry, same-frame hazard
damage, required sprite failure and checkpoint respawn. Stable geometry belongs
in `tests/fixtures/runtime/`; showcase data is still checked separately for
campaign ordering and round-trip validity. Editor regressions cover history
ownership through overflow, undo/redo, branching and clearing.

`make content-inventory` updates generated facts after intentional content changes;
`make asset-budget`/`make docs-drift` verify freshness. The website consumes the
generated JSON instead of maintaining its own counts. Packaging tests inspect
archive contents, editor inclusion, reserve-asset exclusion and license notices.

`make test` builds these 15 native binaries under `out/` and runs each one:

- `level-serializer-test`
- `level-validate-test`
- `runtime-load-test`
- `rail-test`
- `entity-utils-test`
- `collision-test`
- `phase-transition-test`
- `editor-validation-test`
- `gameplay-damage-test`
- `gameplay-config-test`
- `gameplay-score-test`
- `game-overlay-test`
- `game-events-test`
- `session-test`
- `game-checkpoint-test`

It also runs `tests/validate_levels_test.py`. The `web-host-contract` prerequisite
adds `tools/check_web_boot_contract.py`, `tests/web_host_test.cjs`,
`tests/profile_storage_test.cjs`, `tests/touch_controls_test.cjs` and
`tests/package_release_test.py`. These cover static boot wiring, host lifecycle,
storage conflicts, touch ownership and native/WASM archive contracts without a
browser. Native harnesses cover parser/serializer, validation, runtime, editor,
profile, checkpoint, simulation and session behavior.

## Smoke Tests

`make smoke` builds the game and editor, runs root and lab TOML levels for a bounded frame count with dummy video/audio drivers, then renders five editor frames. Use it when startup, asset loading, level data, or resource cleanup changed.

`make scripted-smoke` drives the runtime with generated replay scripts. The runner writes files under `out/replays-smoke/`, then invokes the game with `--replay-script`, `--seed`, and `--smoke-test-frames` to exercise deterministic movement, jumping, and pause/resume paths.

## Documentation Drift Gate

`make docs-drift` checks generated content facts/asset budget, level catalog and overlay snapshots, then runs semantic checks for:

- documented Makefile test targets;
- source file map entries;
- public runtime flags;
- strict v1 TOML examples and player API declarations;
- README / docs workflow summaries;
- level schema and generated level prose counts;
- key constants and `GameState` fields;
- overlay controls and snapshot text;
- WebAssembly authority caveats;
- Pages route metadata and developer-guide context.

If this target fails, update the code-backed docs rather than weakening the check.

### Terminal-overlay documentation workflow

After changing terminal copy or actions, regenerate the artifact, run drift (which includes roadmap quality), then build/check the Pages site:

```sh
make overlay-snapshots
make docs-drift
cd docs
bun run lint
bun run build
bun run check-site
```

Do not hand-edit generated `docs/wiki/overlay-snapshots.md`. CI restores the frozen
Bun lockfile. With restored dependencies and no Bun, the same scripts can use
`npm run`. `check-site` checks the emitted production HTML, including the manual
overview, link fragments, unique descriptions and sitemap coverage; it is not
interactive browser testing.

## WebAssembly and Pages

`make web` produces `out/super-mango.html`, `out/super-mango.js`, `out/super-mango.wasm`, and `out/super-mango.data`. Local Emscripten preflights are useful, but GitHub Actions are the authoritative WebAssembly gate because host Emscripten/SDL port caches can fail before Super Mango code compiles.

After a Pages deploy, smoke the live site with at least:

```sh
python3 - <<'PY'
from urllib.request import Request, urlopen
base = 'https://jonathanperis.github.io/super-mango-editor/'
for path, markers in {
    '': ['Super Mango', 'super-mango.js'],
    'docs/': ['Builder Manual', 'Trace every wire'],
    'super-mango.js': ['Module'],
}.items():
    url = base + path
    data = urlopen(Request(url, headers={'User-Agent': 'super-mango-smoke'}), timeout=20).read().decode('utf-8', 'ignore')
    missing = [m for m in markers if m not in data]
    if missing:
        raise SystemExit(f'{url}: missing {missing}')
    print('ok', url)
PY
```

## Related Pages

- [Build System](../build-system/) — detailed target descriptions and platform setup.
- [Release Checklist](../release-checklist/) — gates before tagged or manual releases.
- [Controls & Input](../controls/) — runtime input, replay, and smoke flags.
