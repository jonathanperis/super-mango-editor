# Testing & Smoke Matrix

<a id="home"></a>

---

Use this page to choose the smallest useful verification set for a change. Run commands from the repository root unless a row says otherwise.

## Quick Matrix

| Changed area | Run locally | Why |
|--------------|-------------|-----|
| C runtime, gameplay, collision, score, overlays | `make test CC=clang` | Builds and runs the 14 native regression harnesses. |
| Level TOML or level schema | `make validate-levels` and `make docs-drift` | Validates every `levels/*.toml`, generated catalog freshness, schema docs, and prose counts. |
| Player/world runtime startup | `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy make smoke CC=clang SMOKE_FRAMES=5 SMOKE_SEED=1` | Boots every TOML level plus the editor in bounded dummy-SDL mode. |
| Replay or event handling | `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy make scripted-smoke CC=clang SMOKE_FRAMES=5 SMOKE_SEEDS="1 7 23"` | Generates deterministic replay inputs and injects movement/jump/pause events across levels. |
| Editor behaviour | `make editor CC=clang` and `./out/super-mango-editor --smoke-test` | Builds the standalone editor and checks headless startup/exit. |
| Memory/UB-sensitive C changes | `make sanitize CC=clang` and, when startup paths changed, `make sanitize-smoke CC=clang` | Runs AddressSanitizer/UBSan over tests and optionally smoke startup. |
| Docs, README, Pages routes | `make docs-drift`, then `cd docs && bun run lint && NODE_ENV=production bun run build` | Checks source-backed doc facts, Astro validation, and static Pages output. |
| WebAssembly payload | `make web` when local Emscripten is healthy; otherwise trust green GitHub Actions WebAssembly + Pages smoke | CI is authoritative for Emscripten/SDL port-cache issues. |
| Release packaging | `make dist-native` and `make dist-wasm` or the `Build & Release` workflow | Produces native and WebAssembly archives using the same archive layout described in the release checklist. |

## Native Regression Tests

`make test` builds these 14 harnesses under `out/` and runs each one:

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
- `gameplay-score-test`
- `game-overlay-test`
- `game-events-test`

These tests intentionally avoid opening a normal SDL window. They cover parser/serializer behaviour, validation, level loading, rail math, shared entity helpers, collision/damage, phase transitions, editor validation/export, score/life rules, overlays, and event handling.

## Smoke Tests

`make smoke` builds the game and editor, then runs each `levels/*.toml` for a bounded frame count with dummy video/audio drivers. Use it when startup, asset loading, level data, or resource cleanup changed.

`make scripted-smoke` drives the runtime with generated replay scripts. The runner writes files under `out/replays-smoke/`, then invokes the game with `--replay-script`, `--seed`, and `--smoke-test-frames` to exercise deterministic movement, jumping, and pause/resume paths.

## Documentation Drift Gate

`make docs-drift` is the source-backed documentation gate. It verifies generated level catalog and overlay snapshots, then runs semantic checks for:

- documented Makefile test targets;
- source file map entries;
- public runtime flags;
- README / docs workflow summaries;
- level schema and generated level prose counts;
- key constants and `GameState` fields;
- overlay controls and snapshot text;
- WebAssembly authority caveats;
- Pages route metadata and agent-context docs.

If this target fails, update the code-backed docs rather than weakening the check.

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
