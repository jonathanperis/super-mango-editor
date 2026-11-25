# Troubleshooting

<a id="home"></a>

---

Failures that come from the machine rather than the code, gathered in one
place. Each has the symptom you see, the cause, and the fix. Run
`make doctor` first: it names most missing tools and packages before the
first build ([Build System](../build-system/#make-doctor)).

---

## No window on a headless Mac

**Symptom.** On a Mac with no window server (a CI runner, an SSH session, an
agent's shell), the game, the editor or `make test` stop at start-up because
raylib cannot create a window.

**Cause.** The desktop build opens a real GLFW/OpenGL window, and hosted
macOS runners and remote shells offer no OpenGL pixel format.

**Fix.** Run the tests on raylib's in-memory backend, in an `OUTDIR` of its
own:

```sh
make test OUTDIR=out/headless RAYLIB_PLATFORM=memory
```

This is test-only; `release` and `dist-native` refuse it. A real macOS window
can only be checked at the machine
([Release Checklist](../release-checklist/)).

## No audio device

**Symptom.** The game refuses to start, or `make doctor` warns "No playback
device found". Common on Linux servers, containers and CI.

**Cause.** The game requires an audio device to start (`audio_open()` in
`session_create`).

**Fix.** For tests, use `RAYLIB_PLATFORM=memory` (no window and no audio) or
`RAYLIB_AUDIO=null` (the real desktop window with miniaudio's null playback
device). Both are test-only. On Linux, a PulseAudio null sink is also enough
to make the device exist.

## `X11: Failed to open display` under Xvfb

**Symptom.** Under your own Xvfb, tests fail part-way with
`X11: Failed to open display`.

**Cause.** Without `-noreset`, the X server resets every time its last client
disconnects. The tests close and reopen their window many times, and a window
opened during a reset fails.

**Fix.** Start Xvfb with `-noreset`, as Linux CI does
([Testing](../testing/)).

## No file picker in the editor on Linux

**Symptom.** **Open** or **Save As** shows a box inside the editor asking for
a path instead of a system file dialog.

**Cause.** On Linux the editor's file picker is `zenity` (or `kdialog` when
zenity is missing). With neither, or when the picker fails, the editor falls
back to its in-window prompt.

**Fix.** Nothing is broken: type or paste the path. For the system dialog,
install zenity (`sudo apt install zenity` on Debian/Ubuntu). The game and the
tests do not need it ([Level Editor](../level-editor/)).

## `make fuzz` fails with Apple clang

**Symptom.** `make fuzz` fails to link on macOS.

**Cause.** Apple's clang ships without libFuzzer.

**Fix.** Use Homebrew's LLVM for the fuzz build:

```sh
make fuzz FUZZ_CC=$(brew --prefix llvm)/bin/clang
```

`make fuzz-corpus`, which replays the seeds with a plain driver, works with
any clang ([Testing](../testing/#fuzzing)).

## Wrong Emscripten version for `make web`

**Symptom.** `make web` stops with
`web: expected Emscripten <version> (the CI pin), found: ...`.

**Cause.** The Makefile checks `emcc` against `EMSCRIPTEN_VERSION`, the
version CI builds with.

**Fix.** Activate that SDK (`emsdk install <version>` and
`emsdk activate <version>`), or pass `EMSCRIPTEN_VERSION=<version>` to expect
another release, or `EMSCRIPTEN_VERSION=` (empty) to skip the check. CI's
WebAssembly build stays the one that counts for a release
([Build System](../build-system/#make-web)).

## `node` is really Bun

**Symptom.** The JavaScript host tests in `make test`, or the syntax and
WebAssembly checks in `make web`, fail or behave oddly although `node` is on
`PATH`.

**Cause.** Some setups put a `node` wrapper on `PATH` that launches Bun. These
checks need real Node.js.

**Fix.** Point the Makefile at the real executable:

```sh
make test NODE=/path/to/real/node
```

## Builds keep recompiling

**Symptom.** Switching between two sets of flags rebuilds everything each
time.

**Cause.** Flag stamps rebuild what a changed setting affects, inside one
`OUTDIR`.

**Fix.** Give each long-lived combination its own `OUTDIR`, as the headless
test command above does ([Build System](../build-system/#flag-stamps)).
