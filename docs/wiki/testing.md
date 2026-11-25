# Testing & Smoke Matrix

<a id="home"></a>

---

Use this page to choose the smallest useful verification set for a change. Run commands from the repository root unless a row says otherwise.

## Quick Matrix

| Changed area | Run locally | Why |
|--------------|-------------|-----|
| C runtime, gameplay, collision, score, overlays, sessions | `make test CC=clang` | Builds and runs 18 native regression binaries, a parser allocation probe, and Python/Node host checks (levels, generated sounds, coverage baseline, clang-tidy report, parser encoding, web host, release packaging, CodeQL SARIF filter). |
| Level TOML, campaign manifest, or level schema | `make validate-levels` and `make docs-drift` | Loads root and lab levels through the C loader and validator, then checks assets, the v1 campaign manifest, generated facts, schema docs and prose counts. |
| Player/world runtime startup | `make smoke CC=clang SMOKE_FRAMES=5 SMOKE_SEED=1` | Renders every TOML level plus the editor in bounded hidden windows. |
| Replay or event handling | `make scripted-smoke CC=clang SMOKE_FRAMES=5 SMOKE_SEEDS="1 7 23"` | Generates deterministic commands/action masks and checks movement/jump/pause results. |
| Editor behaviour | `make editor CC=clang` and `./out/super-mango-editor --smoke-test` | Builds the editor and renders five hidden-window frames; a desktop context is still required. |
| Suspected C defects | `make tidy` | clang-tidy (static analyzer, `bugprone-*`, a few CERT C rules) over `src/` and `tools/`; report only. |
| Memory/UB-sensitive C changes | `make sanitize CC=clang` and, when startup paths changed, `make sanitize-smoke CC=clang` | Runs AddressSanitizer/UBSan over tests and optionally smoke startup. |
| Docs, README, Pages routes | `make docs-drift`, then from `docs/`: `bun run lint`, `bun run build`, `bun run check-site` | Checks generated facts and sounds, TOML examples, API/CLI references, Astro compilation and emitted routes/links/metadata/sitemap/CSP (`make docs-drift` also runs the checkers' unit tests in `tests/docs_drift_unit_test.py`; `check-site` runs `tests/docs_checks_test.py` against the built site). Uses the frozen `bun.lock` dependency set. |
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

`make test` builds these 18 native binaries under `out/` and runs each one:

- `level-serializer-test`
- `level-validate-test`
- `runtime-load-test`
- `rail-test`
- `entity-utils-test`
- `collision-test`
- `phase-transition-test`
- `editor-validation-test`
- `editor-undo-recovery-test`
- `gameplay-damage-test`
- `gameplay-config-test`
- `gameplay-score-test`
- `game-overlay-test`
- `game-events-test`
- `session-test`
- `game-checkpoint-test`
- `gameplay-mechanics-test`
- `editor-ui-test`

It also runs the Python tests of the build and docs tools:
`tests/validate_levels_test.py`, `tests/gen_sounds_test.py`,
`tests/fuzz_dict_test.py`, `tests/coverage_baseline_test.py`,
`tests/clang_tidy_report_test.py`, `tests/doctor_test.py`,
`tests/build_raylib_test.py` (the raylib source unpacking),
`tests/gen_compile_commands_test.py` (the compile database),
`tests/roadmap_quality_test.py` and `tests/import_raygui_icons_test.py`, and the
parser encoding probe `tests/parser_validator_test.py`. (`make validate-levels`
runs one more, `tests/schema_parity_test.py`, which compares the level schema in
`tools/validate_levels.py` with the C table that `level-check --dump-schema` prints.) The
`web-host-contract` prerequisite adds `tools/check_web_boot_contract.py`, six
Node tests (`tests/boot_test.cjs`, `tests/web_host_test.cjs`,
`tests/profile_storage_test.cjs`, `tests/touch_controls_test.cjs`,
`tests/keyboard_scope_test.cjs`, `tests/audio_unlock_test.cjs`) and
`tests/web_boot_contract_test.py`, `tests/web_shell_test.py`,
`tests/web_debug_page_test.py`, `tests/wasm_artifacts_test.py`,
`tests/package_release_test.py` and `tests/filter_codeql_sarif_test.py`. These
cover the shared boot script (`web/boot.js`: one start of `main()`, versioned
file addresses, a failed download, a lost WebGL context), the rule that neither
page carries boot logic of its own, host lifecycle, storage conflicts, touch
ownership, canvas keyboard scoping, keyboard audio unlock, the standalone
page's shell and derived debug page, the WebAssembly artifact checker,
native/WASM archive contracts and the CodeQL SARIF filter without a browser. Native harnesses cover parser/serializer, validation, runtime, editor,
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
to the respawn point. Linked into the same binary, `tests/creature_update_test.c`
and `tests/player_climb_test.c` call `birds_update`, `bird_variant_update`,
`spiders_update` and the climbing code (`player/player_climb.c`, the climbing
half of `player_handle_input`) on hand-made structs with no level loaded:
patrol turns, wing and leg frame rates, the sine-wave hitbox, floor-gap turns,
grab zones, the 80 px/s climb, the top clamp and every way of letting go. It checks parallax scroll factors and wrapping
(`parallax_layer_offset`) and renders one frame in every overlay state (play,
pause, game over, completion with and without a next level, a failed next
level, settings open). It also replays
`--replay-script` scripts (from a `--replay-dir` under the test output) and
rejects malformed ones. `editor-ui-test` drives the editor with input events
(palette picks, place, select, drag, delete, undo, wheel zoom/pan, property
and config panel clicks, box and Shift selection acting on the group, resize
handles and patrol ranges, the Entities list, layer rows moving, arrow-key
nudges, Ctrl+D, the snap toggle, Alt+click cycling, text-field caret and Tab,
Up/Down stepping number fields, validation messages that select the problem,
the Campaign view, Playtest from here, a level dropped on the window, the
toolbar's tooltips and toggles, and the mouse cursor's shapes) and checks the
resulting document and undo history. `editor-undo-recovery-test` calls the
safety nets directly on a bare `EditorState`: undo and redo of every kind of
step (`editor_undo_apply.c`, `undo.c`), edits taken back when the history cannot
record them, the `UNDO_MAX` cap, multi-selection copy/paste/cut with its offset
and clamping, refused pastes, and recovery snapshots (naming, recovering a
crashed editor's copy by id, the picker, and never offering a copy another
running editor owns, with a child process held on a gate pipe standing in for
that editor). `editor-ui-test` also reads the canvas back to check the floor still reaches both
edges at every zoom and scroll. It and `editor-validation-test` edit a campaign in a
scratch game folder, made and entered through `tests/test_folders.h`, so those
cases run on Windows too. On POSIX it also covers the playtest process status
and the native file pickers through stand-in `osascript`/`zenity` scripts, including a picker that fails and the in-window path prompt that then takes its place. The in-window prompts are answered with queued events (keys, clicks, closing the window), on every system; closing the window during a prompt must cancel it and still quit afterwards (asking about unsaved changes first), and a prompt opened mid-frame must leave the frame's target and clip as it found them. They write scratch files
under the build's own `OUTDIR` (`TEST_OUT` in `tests/test_paths.h`), so
`make test`, `make sanitize` and `make coverage` can run at the same time in one
checkout. The cases in those two programs are grouped under `tests/editor_ui/`
and `tests/editor_validation/` and included back into `tests/editor_ui_test.c`
and `tests/editor_validation_test.c`. They are not extra binaries. Test objects are also built with `-DMANGO_TESTING`, which turns on the
`*_test_set_*` seams (canned answers for native dialogs, injected save failures,
fake controller input); the game and editor binaries are built without it, so
they do not contain those functions.

`level-serializer-test` also covers the Windows save that stops halfway (the
old file already moved away, the new one not yet in place). No test can make
the real `ReplaceFileW` stop between its two steps, so a seam
(`serializer_test_set_half_replace`) reports that failure in its place and
removes the old file, as the real failure would. Everything after that is
production code: on the Windows CI legs the real `MoveFileExW` fallback and its
retry loop run, and on macOS/Linux, where `rename()` cannot stop halfway, the
seam leads into the same loop around `rename()`. The cases check that the first
move finishes the save, that the last of the five tries can still finish it,
and that when no try works the save reports "temporary file kept", names that
file and leaves the new level in it (also through `serializer_install_temp`,
which the profile and the Campaign view use). A try the test refuses never
waits, so none of this depends on the clock.

`level-serializer-test` also links `tests/serializer_io_test.c`, which takes
`src/shared/serializer_io.c` through what a file system can do to a save: a
folder where a level was expected, a file exactly one byte too big for its
reader, a NUL byte in the middle of a level, a stale temporary file left by a
crashed save of the same process id (stepped over, never reused), a target
folder that does not exist, a name too long for the buffer (refused, never
cut), a create-only save whose target already exists (refused by `link()` and,
on a drive without hard links, by the `O_EXCL` copy), a dangling symlink, and a
durable save of a bare file name. `session-test` links
`tests/platform_paths_test.c` for the edges of `src/shared/platform.c` and
`src/shared/asset_root.c`: text copies that would split a UTF-8 character,
control bytes in a writable path, `preference_path()` with no `HOME` (and, on
Linux, an empty or overlong `XDG_DATA_HOME`), a plain file where a preference
folder must go, executable paths in Windows spelling, and the asset root found
(or, from a build three folders deep, not found) from the test program's own
folder. `editor-undo-recovery-test` reads twenty damaged recovery `.meta` files
(cut short, a second line, unknown versions, ids and timestamps that are zero,
not numbers or past 64 bits, a source path that is not hex or holds a NUL) and
offers only the three well-formed ones, and refuses to recover a snapshot that no
longer loads, leaving the open document as it was.

The session harness compiles the production display boundary's Web path against
test-owned platform calls. It verifies that visible and hidden Web windows leave
raylib's blocking FPS limiter disabled. This guards the non-Asyncify browser loop
without substituting for a real browser smoke check.
The artifact check also rejects an `emscripten_sleep` dependency in the emitted
glue: this catches native-only calls such as raylib's Web `WindowShouldClose`
before a non-Asyncify build is deployed.

Two extra standalone probes accompany the 18 regression binaries:
`make parser-allocation-probe` checks parser buffer-growth limits without huge
allocations, and `make parser-encoding-probe` checks all Python level readers.
Both run under `make test`; `make sanitize` instruments the C probe as well.
Level readers accept one leading UTF-8 BOM and preserve raw line endings so
invalid CR-only documents remain rejected.

## The Test Harness

The native tests use no test framework: `tests/check.h` is the whole of it,
about a hundred lines meant to be read. `CHECK(condition)` prints the file,
the line and the condition's text, sets the case's `failed` flag and jumps to
its `done:` label, where the case frees what it opened on both paths (C has no
exceptions or destructors, so clean-up has to live in one place).
`CHECK_GOTO(condition, label)` does the same for code that cleans up at
another label. `test_run_cases()` runs a table of named cases and gives every
program built on it (`editor-ui-test`, `editor-validation-test`,
`editor-undo-recovery-test`, `session-test`, `gameplay-mechanics-test`) the
same command line:

```sh
out/headless/gameplay-mechanics-test --list                   # case names
out/headless/gameplay-mechanics-test --case parallax_scrolls_and_wraps
out/headless/editor-ui-test --shuffle                         # random order, seed printed
out/headless/editor-ui-test --shuffle=1234                    # that order again
MANGO_TEST_SHUFFLE=random make test OUTDIR=out/headless RAYLIB_PLATFORM=memory
```

The cases of one program share one process: one window, the input queues,
file-scope statics, the random streams. A case that leaves any of that
changed can make a later case pass or fail only because of the table's order.
Shuffling exposes that, and two leaks turned up when it was added: the
editor's palette remembers its open categories in file-scope state, so
`palette_shows_what_place_will_add` failed whenever a case that opened the
Hazards category ran first (`editor-ui-test` now resets the palette and the
Level Config foldouts after every case, with `palette_reset_for_test()`); and
the gameplay random streams are seeded only in `main()`, so a replay's exact
state depended on which cases ran before it (`mechanics_open_level()` now seeds
them as a fresh run would). The fix for a leak is a reset in the runner's
`after_each` hook or in the case, never putting the cases back in an order
that happens to work. Fifty shuffled runs of the five programs (ten seeds
each) pass.

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

The report is also saved as `out/coverage/summary.txt`. Two kinds of number
are gated: lines (was each line run?) and branches (did each condition go both
ways?). Branches are the stricter of the two: `if (!fp) return -1;` counts as a
covered line as soon as it runs, but its branch is only covered once a test
makes `fp` NULL, which is exactly the error path a line count lets slip.

`tools/check_coverage_baseline.py` (tested by `tests/coverage_baseline_test.py`)
checks three things, each empty (report only) by default:

| Variable | CI | Fails the run when |
|----------|----|--------------------|
| `COVERAGE_MIN` / `COVERAGE_BRANCH_MIN` | 90 / 75 | the TOTAL line / branch coverage is below this percentage |
| `COVERAGE_TOLERANCE` | 2 | a file's line or branch coverage is more than this many points below its numbers in `tools/coverage_baseline.txt` |
| `COVERAGE_NEW_FILE_MIN_LINES` / `_BRANCHES` | 80 / 60 | a file the baseline does not list yet (new, or split out of another) is below this |

The `Coverage (Linux x86_64)` CI job runs `make coverage` on the headless
Memory backend with those values and publishes the per-file table (lines and
branches, against the baseline) in the job summary and as the
`coverage-summary` artifact. The totals' floors sit a little under the measured
totals and exist to catch a large accidental drop; the per-file check is what
notices one file losing its tests while the total barely moves. A removed
file is listed and never fails.

The baseline only moves on purpose, and it ratchets:

- **After adding tests**, record the gain so the next pull request cannot give
  it back unnoticed. `make coverage-ratchet` raises each number that
  improved, adds new files and drops deleted ones, and never lowers anything.
  The check also says how many files are above their baseline, as a reminder.
- **After an intended drop** (tests moved to another file, a file split),
  `make coverage-baseline` rewrites every number, lower ones included. Say why
  in the pull request.

Both use the CI configuration, which the baseline describes (Linux, clang, the
Memory backend); on macOS run them in the `ubuntu:24.04` container, since
another platform's counts differ slightly:

```sh
make coverage-ratchet CC=clang OUTDIR=out/headless RAYLIB_PLATFORM=memory
make coverage-baseline CC=clang OUTDIR=out/headless RAYLIB_PLATFORM=memory
```

When branch gating arrived, the native tests covered 94.29% of
lines and 76.00% of branches. New cases aimed at the files with
the weakest branch coverage, and at what goes wrong there in real use (damaged
files, failing file systems, odd paths) rather than at the count, brought that
to 94.51% and 76.86%:

| File | Branches before | Branches after |
|------|----------------:|---------------:|
| `src/shared/platform.c` | 55.4% | 90.5% |
| `src/shared/asset_root.c` | 58.1% | 83.9% |
| `src/shared/serializer_io.c` | 64.5% | 79.3% |
| `src/editor/editor_recovery.c` | 62.2% | 65.9% |
| `src/collision/game_collision.c` | 76.7% | 86.7% |

Coverage only measures what the tests execute, not whether they would notice
a mistake there; [mutation testing](#mutation-testing) asks that.

## Render Snapshots

Almost every line in `src/render/` runs under the tests, but running a draw
call says nothing about what it drew: a sprite in the wrong place, colour or
layer passes them all. Three cases compare whole frames with recorded ones:

| Snapshot | Case | What is drawn |
|----------|------|---------------|
| `hud` | `hud_frame_matches_its_snapshot` (`gameplay-mechanics-test`) | the HUD alone: two hearts, four lives, a score, a checkpoint label |
| `level_hazards` | `level_frame_matches_its_snapshot` (`gameplay-mechanics-test`) | `tests/fixtures/runtime/hazards.toml` after a scripted second of running and a jump |
| `editor_side_panels` | `side_panels_match_their_snapshot` (`editor-ui-test`) | the Level Config, palette and properties panels with a spike block selected |

Each case reads the window back (`LoadImageFromScreen`), hashes the RGBA
pixels (FNV-1a, 64 bit) and compares the hash with its line in
`tests/snapshots/render_snapshots.txt`; the recorded frames are the PNGs beside
it, to look at in a review. The helpers are in `tests/render_snapshot.c`. When
a frame changes the case fails with the snapshot's name, how many pixels
differ from the recorded PNG, the largest colour change and where the first
changed pixel is, and writes the new frame to `OUTDIR/snapshots/<name>.png`. If
the change is intended:

```sh
make render-snapshots-update
```

re-records the hashes and copies the new frames, compressed, into
`tests/snapshots/`; commit them with the change.

Only raylib's Memory backend is compared. It is a software renderer, so the
same code computes every pixel on every system, while a GPU driver (Mesa on the
Linux desktop leg, a laptop's driver) may round an edge its own way; on a
desktop build the cases still draw and read back, and say they did not
compare. The hashes are exact, not a tolerance: the same frames came out bit
for bit on macOS arm64, Linux arm64 (GCC and Clang) and Linux x86_64 (GCC),
and CI checks them on Linux, macOS and Windows (GCC and Clang) on every pull
request. The frames avoid what could differ: nothing in them depends on the
clock or on what an earlier case did to the random streams, and the game
builds with `-ffp-contract=off`, so no compiler fuses a multiply and an add on
one CPU and not on another. raylib's rasterizer builds with its compiler's
defaults; should one platform ever round an edge differently, the failure's
pixel count and largest colour change will show it, and the fix is a
tolerance in `snapshot_check()`, not a separate recording per platform.

## Cross-Platform Determinism

The replay checks in `game_replay_test.c` and `simulation_test.c` and the
scripted smoke compare two runs inside one process, so they cannot notice a
compiler or a C library computing a float differently. One committed replay
does: `tests/fixtures/determinism/jump-right.replay`, fifteen seconds of
running, jumping and turning back through `tests/fixtures/determinism/sandbox.toml`
(a frozen copy of the sandbox level, with every enemy, hazard and surface
kind, nine lives so the whole replay is played, and no next level). The case
`committed_replay_reaches_the_recorded_state` (`gameplay-mechanics-test`) plays
it for 900 frames with seed 7 through the same injection, event handling and
fixed-step loop as `game_frame`, then hashes the state field by field (the
player, hearts, lives, score, camera, step count and every moving entity;
floats by their exact bits, no pointers or padding) and compares it with
`tests/fixtures/determinism/expected_state.txt`. A mismatch prints both
states in words (position, hearts, lives, score) next to the hashes. After an
intended change to the simulation, `make replay-state-update` records the new
state.

It runs on every native CI leg: Linux (GCC on the desktop backend, Clang in the
coverage and sanitizer jobs), macOS arm64, and Windows (GCC, and Clang in the
extras job). WebAssembly is not covered. Its tests would have to run the
simulation under Node, but the Web build links raylib's Web backend, which
needs a browser's WebGL context to start (`game_init` loads textures), and is
built with `-s ENVIRONMENT=web`. Covering it would take a second Emscripten
build of raylib on the Memory backend plus a Node-targeted build of the test
program, a new raylib variant for the CI cache and several minutes per run; the
browser check after each deploy (`Live site check`) runs the real game but
cannot replay a script.

## Fuzzing

## Fuzzing

Six harnesses feed untrusted text to the parsers the game and editor use
(POSIX only):

- `tests/fuzz_level_parse.c` writes the input to a temporary `.toml` file and
  calls `level_load_toml()` (tomlc17, schema, loaders, `level_validate_runtime()`).
  An accepted level must save, reload and save again to identical bytes.
- `tests/fuzz_editor_roundtrip.c` checks the editor's promise that a saved
  level opens unchanged. An accepted level is saved as a new file and as a
  crash-recovery copy, and each reloaded `LevelDef` must equal the first one
  byte for byte, which also catches a field that is loaded but never saved.
  The recovery copy's destination path (the level's name) must read back
  unless it holds a control byte. Its own seeds are in
  `tests/fuzz/corpus/editor/`.
- `tests/fuzz_profile_decode.c` calls `game_profile_decode()`. An accepted
  profile must encode, decode and encode again to identical text.
- `tests/fuzz_campaign_experiment.c` loads campaign manifests and experiment
  captures.
- `tests/fuzz_replay_script.c` writes the input as a `--replay-script` file and
  loads it with `game_replay_load()`. A refused script must leave no events
  behind; an accepted one must hold 1 to 4096 events whose frames never go
  backwards, written back in canonical form must load to the same events, and
  played back every event must fire at its own frame. Words for libFuzzer are
  in `tests/fuzz/replay.dict`.
- `tests/fuzz_bindings.c` reads the input as twelve key and button numbers and
  as the text of a profile's `keys = ...` line. Settings that
  `game_settings_valid()` accepts must bind no key or button twice (not even
  two numbers that reach the same physical key), translate there and back,
  and survive a save and load; a profile with bindings the validator refuses
  must not load with them (the decoder may only put a debug-inspector key back
  on its default).

Each defines the libFuzzer entry point `LLVMFuzzerTestOneInput`.

| Command | What it does |
|---------|--------------|
| `make fuzz-corpus` | Builds every harness with `tests/fuzz_replay_main.c` under ASan/UBSan and replays the seeds: `levels/`, `levels/labs/`, `tests/fixtures/serializer_v1/`, `tests/fixtures/runtime/`, `tests/fuzz/corpus/level/` and `tests/fuzz/corpus/profile/`, for the editor harness the level seeds plus `tests/fuzz/corpus/editor/`, and `tests/fuzz/corpus/<harness>/` for the others. `make sanitize` runs it too, and CI's sanitizer job adds `FUZZ_MUTATIONS=50`. `FUZZ_MUTATIONS=N` adds N blind byte mutations per seed: this is slower and weaker than libFuzzer, but needs no extra toolchain. |
| `make fuzz` | Coverage-guided libFuzzer run for `FUZZ_SECONDS` (default 60) per harness. New inputs go to `out/fuzz/<harness>/` (`level`, `editor`, `profile`, `campaign`, `replay`, `bindings`), and crashes to `out/fuzz/*-crash-*`. Apple clang has no libFuzzer, so use `make fuzz FUZZ_CC=$(brew --prefix llvm)/bin/clang` on macOS. `.github/workflows/fuzz.yml` runs this weekly and on demand; it is not a required check. |
| `make fuzz-merge` | After `make fuzz`: replaces each `out/fuzz/<harness>/` corpus with the inputs libFuzzer's `-merge=1` keeps (only those that add coverage). The weekly workflow runs it after every successful search, then keeps the merged corpus in the Actions cache and uploads it as the `fuzz-corpus` artifact (90 days); a run with no cache entry downloads the newest such artifact (`gh run download`), so an evicted cache does not send the search back to the seeds. |

Replay a crash with `out/fuzz-level-replay <file>` (or `out/fuzz-editor-replay`, `out/fuzz-profile-replay`, `out/fuzz-campaign-replay`, `out/fuzz-replay-replay`, `out/fuzz-bindings-replay`).
If an input exposed a real bug, add it to `tests/fuzz/corpus/` after the fix.

The committed corpus is what every pull request replays, so the search's
finds belong there, not only in the Actions cache and artifact, which expire.
`make fuzz-corpus-export` (after `make fuzz`) runs libFuzzer's `-merge=1` with
the committed folder first, so it copies into `tests/fuzz/corpus/<harness>/`
only the inputs that reach code the committed seeds do not, each named by its
SHA-1 and cut to `FUZZ_EXPORT_MAX_LEN` (4096) bytes. Check what it added with
`git status tests/fuzz/corpus` and commit it. The weekly workflow runs the same
export and uploads only the new files as the `fuzz-corpus-new` artifact (and
lists them in its summary); it cannot push, so to keep them, unpack that
artifact into `tests/fuzz/corpus/` and commit. On macOS, where Apple clang has
no libFuzzer, run the search and the export in the `ubuntu:24.04` container.

The first export, after 90 to 100 seconds of search per harness in that
container, took the committed corpus from 45 hand-written seeds to about 1,400
inputs (325 KB): level 3 to 349, editor 4 to 314, profile 12 to 291, campaign
3 to 195, bindings 15 to 198 and replay 15 to 21. Replaying all of them with
50 blind mutations each, as the sanitizer job does, takes about half a minute.
The bindings search also found a wrong assumption in its own harness: a
controller paddle is a valid binding with no button behind it, which the
settings screen reports as unavailable; the harness now checks that report
instead.

## Mutation Testing

Coverage says which lines the tests run; it cannot say whether a test would
notice if one of them were wrong. Mutation testing asks that directly.
`tools/mutate.py` (`make mutate`) makes one small change at a time, the kind
a real slip would make, rebuilds, and runs the test programs that exercise
the file:

| Operator | Change |
|----------|--------|
| boundary | `<` ↔ `<=`, `>` ↔ `>=` |
| equality | `==` ↔ `!=` |
| logic | `&&` ↔ `\|\|` |
| arithmetic | binary `+` ↔ `-` |
| constant | an integer `N` becomes `N + 1` |

Comments, strings and preprocessor lines are left alone. A mutant is
*killed* when a test fails, crashes or hangs, and *survives* when every test
still passes. The kill rate is killed / (killed + survived); a mutant that
does not compile says nothing about the tests and is counted apart.

A survivor is one of two things. It may be a gap: some behaviour no test
looks at, worth a test that would have caught the slip. Or it may be an
*equivalent mutant*, a change the program cannot observe: `return 1` turned
into `return 2` in a function whose result is only used as true or false, a
sound volume of 128 turned into 129, `i < count` turned into `i <= count` where
the element past the end is always inactive. Only a person can tell the two
apart, so the report lists every survivor with its line, and the useful
question for each is "what test would fail if this line were wrong?"

```sh
make mutate                                    # src/collision/ and serializer_parse*.c
make mutate MUTATE_ARGS="--max 50 --seed 3"    # a random sample, repeatable
make mutate MUTATE_ARGS="--target src/core/game_score.c --report out/mutation.md"
```

Each mutant rebuilds a few objects in `out/mutate/` (Memory backend) and
reruns the programs one at a time, quickest first, stopping at the first
failure; a survivor costs a full run, so a whole file takes minutes. The
source file is always put back, even after Ctrl+C. `mutation.yml` runs it
weekly (and on demand, with an optional sample size) and publishes the kill
rate and the survivors in its job summary and the `mutation-report` artifact;
it is informational and fails only when the unmutated code does not build or
pass.

The first run covered all 132 mutants of `src/collision/` (kill rate 44.7%)
and a fixed sample of 150 of the 531 in `src/shared/serializer_parse.c`
(`--max 150 --seed 1`, 62.8%). Among the survivors were real gaps, and new
cases now kill them:

- **Retry after game over** (`src/collision/collision_damage.c`): no test
  checked that a retry that arrives while the game is not over does nothing,
  that a level's `initial_lives = 1` gives one life (and 0 the default), or
  that the pause, the level timer, the start score and the Continue flags
  start over. `retry_starts_the_level_afresh` (`gameplay-damage-test`) does.
- **Which boxes hurt** (`src/collision/game_collision.c`): a spike row's
  last tile, a blue flame waiting below its gap or switched off, collisions
  after game over or level completion, and the damage-source table's bounds
  were unchecked. `damage_comes_only_from_live_boxes`
  (`gameplay-mechanics-test`) checks each with `game_collide()` alone.
- **Array limits** (`src/shared/serializer_parse.c`): a level with one coin
  too many was refused, but nothing loaded one with exactly `MAX_COINS`, so
  `>` against the limit could become `>=`. `rejects_oversized_arrays` now
  loads the full level too.

With those cases `src/collision/` went from 44.7% to 58.3% (59 to 77 of 132
killed). The serializer sample stayed level (62.8%, then 62.2%): its one real
gap is now killed, and the rest of the difference is noise. A mutant that
makes the parser read past an array (`i < size` turned into `i <= size`, or a
type check turned off) is undefined behaviour, so whether a test fails
depends on what memory happens to hold; such mutants can flip between runs.
Compare kill rates with a margin of a few mutants, or look at the survivor
lists.

Most of what still survives is equivalent: truth values written as `1`,
sound volumes, loop bounds whose extra element is inactive, and guards
against values tomlc17 never produces (a negative string length). A few are
loop bounds that would read one element past an array; a sanitizer build
would catch those every time, the plain mutation build only sometimes.

## Static Analysis

`make tidy` runs clang-tidy over the project's own C files (`src/` and
`tools/`) using the checks in `.clang-tidy`: the Clang static analyzer
(`clang-analyzer-*`), `bugprone-*` and a few CERT C rules. The file lists the
checks it turns off and why (most fire on correct code here, such as
`#define _POSIX_C_SOURCE` or every `(x, y)` parameter pair). Findings in
`vendor/` and raylib's headers are never reported. `tools/run_clang_tidy.py`
reads `compile_commands.json`, prints each finding once and exits 0;
`--strict` makes any finding fail, and `tests/clang_tidy_report_test.py`
covers its filtering. The `clang-tidy (informational)` job in
`.github/workflows/static-analysis.yml` runs it on every pull request and lists
the findings in its job summary; it is not a required check, so read the
findings as leads to triage, not as errors.

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

`make smoke` builds the game and editor, runs root and lab TOML levels for a bounded frame count, then renders five editor frames. Desktop hidden windows still need graphics and audio devices. Linux CI supplies Xvfb/Mesa and a PulseAudio null sink. It starts Xvfb with `-noreset`, and so should you when you run the tests under your own Xvfb: without it the X server resets itself each time its last client disconnects, the tests close and reopen their window many times, and a window opened during such a reset fails with `X11: Failed to open display`. The gating macOS/Windows legs use a separate Memory/software-rendered test build while building and packaging normal GLFW desktop executables. The Mesa half of the non-gating `Windows x86_64 extras (clang -Werror, Mesa desktop backend)` job runs the real GLFW/OpenGL tests and smoke on Windows only, with Mesa llvmpipe and `RAYLIB_AUDIO=null`; hosted macOS runners have no OpenGL pixel format.

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
normalizes raylib 6.2 Memory's bottom-origin/BGRA framebuffer readback. Software
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
- every backticked `src/...` path in the manual, README, `.specs/project/` and `.specs/features/` exists (a `*` pattern must match at least one file);
- backticked `name()` functions on the learning pages are defined in `src/`, `tests/` or `labs/`;
- constant values in [Constants Reference](../constants-reference/) and in `#define` snippets match the source;
- the [debug inspector key table](../controls/#debug-inspector-keys) matches `src/core/game_inspector.c`;
- the [render order table](../architecture/#render-order-back-to-front) matches the `s_render_layers` table in `src/render/game_render.c`, row for row, and nothing is drawn outside that table;
- the [update order table](../architecture/#update-order-one-fixed-step) matches the `s_update_steps` table in `src/core/game_update.c`, row for row;
- every "N native binaries" claim (also spelled "N-native-binary") in the manual, README, `.specs/project/` and `.specs/features/` matches `TEST_TARGETS`, and `STATE.md` lists each test binary;
- every constant the docs place in a particular `src/` header (written as the constant name, "in", then the header path) is really defined there, and so are the camera constants;
- every function the docs place in a particular `src/` file (written as `name()`, or a list such as `a()` and `b()`, then "in" or "(" and the file path) is defined or declared in that file;
- the README's workflow trigger summary matches each workflow's `on:` block, and no page credits the drift check to the Docs workflow (only `build.yml` runs it);
- the README prerequisites name Node.js (`make test` runs Node tests), and the README, `docs/README.md` and the Build System page quote the Node version CI uses (the `.node-version` file, which every workflow reads) in the form `renovate.json`'s Node manager rewrites, so a Node bump updates all four together;
- every quoted raylib version (`raylib 6.2`, `raylib **6.2**`, the README's tech-stack row) in the manual, the READMEs, `PRODUCT.md`, `THIRD_PARTY_NOTICES.md` and `.specs/` matches `vendor/raylib/manifest.json`;
- `docs/bun.lock` keeps Tailwind CSS and its Vite plugin on one major.minor line, and the docs toolchain paragraphs name the site's packages without quoting versions (the lock is the record, so a dependency bump needs no docs edit);
- every `tests/*.cjs` and `tests/*_test.py` file is run by a `make` target (`tests/docs_drift_unit_test.py` by `make docs-drift` itself; `tests/docs_checks_test.py` by `bun run check-site`), every such file the Makefile names exists, the README and `STATE.md` quote the right number of Node tests, and `STATE.md`, this page and the Build System page name each of them;
- level, screen and lab counts in `PRODUCT.md`, the README and `.specs/project/` match `docs/src/generated/project.json` and `levels/`;
- every render layer count ("N render layers", "N-layer order") in the manual, README, `PRODUCT.md` and `.specs/project/` matches the rows of the [render order table](../architecture/#render-order-back-to-front), numbered 1..N;
- the Emscripten version is the same in the Makefile, `build.yml` and the files the check lists in `EMSCRIPTEN_MENTIONS` (the READMEs, the Build System page, `vendor/raylib/README.md` and `web/keyboard-scope.js`). Dated reports such as `docs/AUDIT_IMPLEMENTATION.md` keep the version they were written against.
- `VERSION` is `MAJOR.MINOR.PATCH`, and `build.yml` tags `v` plus that file instead of `v1.0.<commit count>`. The [release checklist](../release-checklist/) is the rule for which number comes next.

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

After every Pages deploy, Build & Release's `Live site check` job loads the
published game in headless Chromium with `tools/live-check/check_live_site.mjs`.
It waits up to five minutes for the page to link this commit's assets
(`super-mango.js?v=<commit>`), then fails when the canvas never reaches its
running label, the loading line stays visible, the console logs an error, the
page throws, a request fails, or the game stops while ArrowRight is held.
Three messages every healthy load prints are allowed by their exact text: audio
waiting for a user gesture, the `ScriptProcessorNode` deprecation (see
[Build System](../build-system/), "Web audio") and SwiftShader's ReadPixels
stall. Screenshots and `report.json` are uploaded as the `live-site-check`
artifact. The job is not a required check: the site is already live when it
runs, so a failure means fix forward. Run it locally with real Node.js:

```sh
cd tools/live-check
npm ci && npx playwright install chromium
node check_live_site.mjs --expect-version "$(git rev-parse HEAD)" --out ../../out/live-check
```

Without `--expect-version` it accepts whatever version the page serves;
`--url` checks another copy of the page, and `--timeout-seconds` shortens the
wait. Exit status 0 is a pass, 1 a failed check, 2 bad arguments, and 3 means
the page kept serving another commit until the deadline. A later `main` run
can deploy while this one waits, so the job counts 3 as a pass only when the
GitHub compare API says the served commit is newer than this run's (that run
checks its own deploy); an older or unrelated version still fails.

The pages around the game can also be smoked with plain HTTP:

```sh
python3 - <<'PY'
from urllib.request import Request, urlopen
base = 'https://super-mango-editor.jonathanperis.tech/'
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
