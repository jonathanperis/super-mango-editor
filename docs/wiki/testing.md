# Testing & Smoke Matrix

<a id="home"></a>

---

Use this page to choose the smallest useful verification set for a change. Run commands from the repository root unless a row says otherwise.

## Quick Matrix

| Changed area | Run locally | Why |
|--------------|-------------|-----|
| C runtime, gameplay, collision, score, overlays, sessions | `make test CC=clang` | Builds and runs 17 native regression binaries, a parser allocation probe, and Python/Node host checks (levels, generated sounds, parser encoding, web host, release packaging, CodeQL SARIF filter). |
| Level TOML, campaign manifest, or level schema | `make validate-levels` and `make docs-drift` | Loads root and lab levels through the C loader and validator, then checks assets, the v1 campaign manifest, generated facts, schema docs and prose counts. |
| Player/world runtime startup | `make smoke CC=clang SMOKE_FRAMES=5 SMOKE_SEED=1` | Renders every TOML level plus the editor in bounded hidden windows. |
| Replay or event handling | `make scripted-smoke CC=clang SMOKE_FRAMES=5 SMOKE_SEEDS="1 7 23"` | Generates deterministic commands/action masks and checks movement/jump/pause results. |
| Editor behaviour | `make editor CC=clang` and `./out/super-mango-editor --smoke-test` | Builds the editor and renders five hidden-window frames; a desktop context is still required. |
| Memory/UB-sensitive C changes | `make sanitize CC=clang` and, when startup paths changed, `make sanitize-smoke CC=clang` | Runs AddressSanitizer/UBSan over tests and optionally smoke startup. |
| Docs, README, Pages routes | `make docs-drift`, then from `docs/`: `bun run lint`, `bun run build`, `bun run check-site` | Checks generated facts and sounds, TOML examples, API/CLI references, Astro compilation and emitted routes/links/metadata/sitemap/CSP (`check-site` also runs `tests/docs_checks_test.py`). Uses the frozen `bun.lock` dependency set. |
| WebAssembly payload | `make web`, artifact checks, green WebAssembly + Pages assembly checks, and actual browser startup for runtime changes | Build raylib and the application with the same pinned SDK. HTTP/module checks do not execute the browser frame loop. |
| Release packaging | `make dist-native` and `make dist-wasm` or the `Build & Release` workflow | Produces native and WebAssembly archives using the same archive layout described in the release checklist. |

## Native Regression Tests

The session suite also runs `tests/simulation_test.c`: 180-step capture/replay
equivalence, the inspector never overriding a real pause, moving-platform
carry (sideways, and down and up a RECT rail), same-frame hazard damage,
required sprite failure, checkpoint respawn, floor-gap walls, health-star
healing up to the heart cap, and jump buffering, coyote time and short hops.
Stable geometry belongs
in `tests/fixtures/runtime/`; showcase data is still checked separately for
campaign ordering and round-trip validity. Editor regressions cover history
ownership through overflow, undo/redo, branching and clearing.

`make content-inventory` updates generated facts after intentional content changes;
`make asset-budget`/`make docs-drift` verify freshness. The website consumes the
generated JSON instead of maintaining its own counts. Packaging tests inspect
archive contents, editor inclusion, reserve-asset exclusion and license notices.

`make test` builds these 17 native binaries under `out/` and runs each one:

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
- `gameplay-mechanics-test`
- `editor-ui-test`

It also runs `tests/validate_levels_test.py` and `tests/gen_sounds_test.py`. The
`web-host-contract` prerequisite adds `tools/check_web_boot_contract.py`,
`tests/web_host_test.cjs`, `tests/profile_storage_test.cjs`,
`tests/touch_controls_test.cjs`, `tests/keyboard_scope_test.cjs`,
`tests/package_release_test.py` and `tests/filter_codeql_sarif_test.py`. These
cover static boot wiring, host lifecycle, storage conflicts, touch ownership,
canvas keyboard scoping, native/WASM archive contracts and the CodeQL SARIF
filter without a browser. Native harnesses cover parser/serializer, validation, runtime, editor,
profile, checkpoint, simulation and session behavior. The session, profile and
mechanics cases include the Continue point (its codec, migration and round trip, and
refusing a point the level no longer fits), the time-trial ghost (codec, paths,
keeping the fastest run, racing it), Level Select's best results, `--start-x` /
`--start-checkpoint` start points, the asset root found from the executable's
folder, Replay reusing the session's shared sprites and sounds, and Replay
replacing the game in place under the browser-style callback loop.
`gameplay-mechanics-test` loads small levels from `tests/fixtures/runtime/`
(climbing, hazards, creatures) and steps them through `game_update_active`:
climbing on and off ladders, ropes and vines; the blue flame's eruption timing
and damage; axe swings; spike-block rails (loop, detach, end cap); knockback
throwing a climber off; jumping spiders, a spider keeping its authored speed
through every turn, fish and faster-fish leaps, bird patrols, bridge crumbling
(and not crumbling under a player on another surface) and the camera jumping
to the respawn point. It checks parallax scroll factors and wrapping
(`parallax_layer_offset`) and renders one frame in every overlay state (play,
pause, game over, completion with and without a next level, a failed next
level, settings open). It also replays
`--replay-script` scripts (from a `--replay-dir` under the test output) and
rejects malformed ones. `editor-ui-test` drives the editor with input events
(palette picks, place, select, drag, delete, undo, wheel zoom/pan, property
and config panel clicks, box and Shift selection acting on the group, arrow-key
nudges, Ctrl+D, the snap toggle, Alt+click cycling, text-field caret and Tab,
validation messages that select the problem, the Campaign view and Playtest
from here) and checks the resulting document and undo history. It and `editor-validation-test` edit a campaign in a
scratch game folder, made and entered through `tests/test_folders.h`, so those
cases run on Windows too. On POSIX it also covers the playtest process status
and the native file pickers through stand-in `osascript`/`zenity` scripts. They write scratch files
under the build's own `OUTDIR` (`TEST_OUT` in `tests/test_paths.h`), so
`make test`, `make sanitize` and `make coverage` can run at the same time in one
checkout. Test objects are also built with `-DMANGO_TESTING`, which turns on the
`*_test_set_*` seams (canned answers for native dialogs, injected save failures,
fake controller input); the game and editor binaries are built without it, so
they do not contain those functions.

The session harness compiles the production display boundary's Web path against
test-owned platform calls. It verifies that visible and hidden Web windows leave
raylib's blocking FPS limiter disabled. This guards the non-Asyncify browser loop
without substituting for a real browser smoke check.
The artifact check also rejects an `emscripten_sleep` dependency in the emitted
glue: this catches native-only calls such as raylib's Web `WindowShouldClose`
before a non-Asyncify build is deployed.

Two extra standalone probes accompany the 17 regression binaries:
`make parser-allocation-probe` checks parser buffer-growth limits without huge
allocations, and `make parser-encoding-probe` checks all Python level readers.
Both run under `make test`; `make sanitize` instruments the C probe as well.
Level readers accept one leading UTF-8 BOM and preserve raw line endings so
invalid CR-only documents remain rejected.

## Coverage

`make coverage CC=clang` rebuilds the native suite in `out/coverage/` with
clang source-based coverage (`-fprofile-instr-generate -fcoverage-mapping`),
runs `make test` there, merges the profiles with `llvm-profdata` and prints an
`llvm-cov report` per source file. `vendor/`, `tests/` and `out/` are excluded. macOS
uses `xcrun llvm-profdata`/`xcrun llvm-cov`; Linux uses `llvm-profdata`/`llvm-cov`
from `PATH`. Override them with `LLVM_PROFDATA=` / `LLVM_COV=`. For a
line-by-line view of one file:

```sh
xcrun llvm-cov show out/coverage/level-serializer-test \
    -instr-profile=out/coverage/tests.profdata src/levels/level_ref.c
```

The report is also saved as `out/coverage/summary.txt`. `COVERAGE_MIN=<percent>`
fails the run when the TOTAL line coverage drops below that floor; it is empty
(report only) by default. The `Coverage (Linux x86_64)` CI job runs
`make coverage` on the headless Memory backend with a floor set a little below
the measured total (91.2% of lines when the floor was set), and publishes the
per-file table in the job summary.

Coverage only measures what the tests execute. Use it to find untested
branches; the CI floor only catches a large, accidental drop.

## Fuzzing

Two harnesses feed untrusted text to the parsers the game uses
(POSIX only):

- `tests/fuzz_level_parse.c` writes the input to a temporary `.toml` file and
  calls `level_load_toml()` (tomlc17, schema, loaders, `level_validate_runtime()`).
  An accepted level must save, reload and save again to identical bytes.
- `tests/fuzz_profile_decode.c` calls `game_profile_decode()`. An accepted
  profile must encode, decode and encode again to identical text.

Both define the libFuzzer entry point `LLVMFuzzerTestOneInput`.

| Command | What it does |
|---------|--------------|
| `make fuzz-corpus` | Builds both harnesses with `tests/fuzz_replay_main.c` under ASan/UBSan and replays the seeds: `levels/`, `levels/labs/`, `tests/fixtures/serializer_v1/`, `tests/fixtures/runtime/`, `tests/fuzz/corpus/level/` and `tests/fuzz/corpus/profile/`. `make sanitize` runs it too. `FUZZ_MUTATIONS=N` adds N blind byte mutations per seed: this is slower and weaker than libFuzzer, but needs no extra toolchain. |
| `make fuzz` | Coverage-guided libFuzzer run for `FUZZ_SECONDS` (default 60) per harness. New inputs go to `out/fuzz/level/` and `out/fuzz/profile/`, and crashes to `out/fuzz/*-crash-*`. Apple clang has no libFuzzer, so use `make fuzz FUZZ_CC=$(brew --prefix llvm)/bin/clang` on macOS. |

Replay a crash with `out/fuzz-level-replay <file>` (or `out/fuzz-profile-replay`).
If an input exposed a real bug, add it to `tests/fuzz/corpus/` after the fix.
Keep that corpus small and hand-reviewed.

## Browser Startup Check

After frame-loop, graphics, audio or host-boot changes, serve the freshly built
normal/debug WASM outputs over HTTP and exercise both the site host and standalone
shells in an isolated browser session. Confirm rendered game frames and response
to game controls, then inspect console/runtime errors. A successful download,
`WebAssembly.compile`, `callMain` return or momentary “running” label is not enough:
an abort can occur on the next animation frame.

For agent-driven testing, obtain explicit browser-interaction permission first.
Keep evidence task-owned and distinguish startup/input results from untested
audible output or physical-device behavior.

## Smoke Tests

`make smoke` builds the game and editor, runs root and lab TOML levels for a bounded frame count, then renders five editor frames. Desktop hidden windows still need graphics and audio devices. Linux CI supplies Xvfb/Mesa and a PulseAudio null sink. The gating macOS/Windows legs use a separate Memory/software-rendered test build while building and packaging normal GLFW desktop executables. The non-gating `Desktop backend` job runs the real GLFW/OpenGL tests and smoke on Windows only, with Mesa llvmpipe and `RAYLIB_AUDIO=null`; hosted macOS runners have no OpenGL pixel format.

On a desktop with a display but no sound device, `make test OUTDIR=out/nullaudio RAYLIB_AUDIO=null` keeps the real graphics backend and swaps in miniaudio's null playback device. Like Memory, it is test-only: `release` and `dist-native` refuse it.

For display-less local verification:

```sh
make test OUTDIR=out/headless RAYLIB_PLATFORM=memory
make sanitize OUTDIR=out/headless RAYLIB_PLATFORM=memory
make smoke OUTDIR=out/headless RAYLIB_PLATFORM=memory
```

Memory tests render assets and exercise lifecycle/model/input-injection contracts
with null audio. They verify requested window sizes at the API boundary; only the
desktop suite additionally checks actual OS resize. The context-lifetime test
normalizes raylib 6.0 Memory's bottom-origin/BGRA framebuffer readback. Software
tests do not prove GPU presentation, clipboard/dialog behavior, audible output
or physical input devices. They are explicit builds, never a runtime fallback.

For comment/layout-only teaching changes, compare executable tokens and review
the explanations against their callers before rebuilding. Do not change gameplay
expectations to make a readability pass look green. Saved-binding translations,
units and owner/borrower contracts still need their existing regression checks.

`make scripted-smoke` drives the runtime with generated replay scripts. The runner writes files under `$(OUTDIR)/replays-smoke/` (`out/replays-smoke/` by default), then invokes the game with `--replay-dir` pointing there, `--replay-script`, `--seed`, and `--smoke-test-frames` to exercise deterministic movement, jumping, and pause/resume paths. Each subprocess has a 30-second limit; use the default five-frame scenarios for slow sanitizer/software-renderer runs.

## Documentation Drift Gate

`make docs-drift` checks generated content facts/asset budget, generated sounds (`tools/gen_sounds.py --check`), level catalog and overlay snapshots, then runs semantic checks for:

- documented Makefile test targets;
- source file map entries;
- public runtime flags;
- strict v1 TOML examples and player API declarations;
- README / docs workflow summaries;
- level schema and generated level prose counts;
- key constants and `GameState` fields;
- overlay controls and snapshot text;
- WebAssembly authority caveats;
- Pages route metadata and developer-guide context;
- every `make <target>` the manual or README mentions exists in the Makefile;
- every backticked `src/...` path exists;
- backticked `name()` functions on the learning pages are defined in `src/`, `tests/` or `labs/`;
- constant values in [Constants Reference](../constants-reference/) and in `#define` snippets match the source;
- the [debug inspector key table](../controls/#debug-inspector-keys) matches `src/core/game_inspector.c`;
- the [render order table](../architecture/#render-order-back-to-front) matches the call order in `src/render/game_render.c`;
- every "N native binaries" claim in the manual, README and `.specs/project/` matches `TEST_TARGETS`, and `STATE.md` lists each test binary;
- every constant the docs place in a particular `src/` header (written as the constant name, "in", then the header path) is really defined there, and so are the camera constants;
- every function the docs place in a particular `src/` file (written as `name()`, or a list such as `a()` and `b()`, then "in" or "(" and the file path) is defined or declared in that file;
- the README's workflow trigger summary matches each workflow's `on:` block, and no page credits the drift check to the Docs workflow (only `build.yml` runs it);
- the README prerequisites name Node.js (`make test` runs Node tests) with the Node version CI uses;
- level, screen and lab counts in `PRODUCT.md`, the README and `.specs/project/` match `docs/src/generated/project.json` and `levels/`;
- every render layer count ("N render layers", "N-layer order") in the manual, README, `PRODUCT.md` and `.specs/project/` matches the rows of the [render order table](../architecture/#render-order-back-to-front), numbered 1..N;
- the Emscripten version is the same in the Makefile, `build.yml` and the files the check lists in `EMSCRIPTEN_MENTIONS` (the READMEs, the Build System page, `vendor/raylib/README.md` and `web/keyboard-scope.js`). Dated reports such as `docs/AUDIT_IMPLEMENTATION.md` keep the version they were written against.

Each failure names the page and line to fix.

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

`make web` produces `out/super-mango.html`, `out/super-mango.js`, `out/super-mango.wasm`, and `out/super-mango.data`, plus the debug variants. Local Emscripten preflights are useful; GitHub Actions remain the authoritative WebAssembly gate. Use real Node.js for artifact syntax/module checks, including `NODE=/path/to/node` when another runtime shadows `node` on PATH.

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
