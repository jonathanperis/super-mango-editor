# Build System

<a id="home"></a>

On this page: [Makefile overview](#makefile-overview) ·
[Build targets](#build-targets) · [Docs site toolchain](#astro-docs-site-toolchain) ·
[Prerequisites](#prerequisites) · [CI/CD pipelines](#cicd-pipelines) ·
[Adding source files](#adding-new-source-files) · [Output structure](#output-structure)

---

## Makefile Overview

The project uses a **GNU Makefile** with explicit per-directory wildcards. New `.c` files in recognized source directories are compiled automatically; a new source directory needs only a matching `SRCS` wildcard (plus `EDITOR_SRCS` if the editor links it), because one pattern rule compiles every `src/` path into `out/obj/`.

```makefile
CC      ?= clang
CFLAGS  = -std=c11 -Wall -Wextra -Wpedantic -I$(RAYLIB_BUILD)/build/raylib/include
LIBS    = $(RAYLIB_LIB) $(PLATFORM_LIBS)
OUTDIR  = out
OBJDIR  = $(OUTDIR)/obj
TARGET  = $(OUTDIR)/super-mango
SRCDIR  = src
SRCS    = $(wildcard $(SRCDIR)/*.c) \
          $(wildcard $(SRCDIR)/collectibles/*.c) \
          $(wildcard $(SRCDIR)/collision/*.c) \
          $(wildcard $(SRCDIR)/core/*.c) \
          $(wildcard $(SRCDIR)/effects/*.c) \
          $(wildcard $(SRCDIR)/entities/*.c) \
          $(wildcard $(SRCDIR)/hazards/*.c) \
          $(wildcard $(SRCDIR)/input/*.c) \
          $(wildcard $(SRCDIR)/levels/*.c) \
          $(wildcard $(SRCDIR)/player/*.c) \
          $(wildcard $(SRCDIR)/render/*.c) \
          $(wildcard $(SRCDIR)/screens/*.c) \
          $(wildcard $(SRCDIR)/surfaces/*.c) \
          $(wildcard $(SRCDIR)/shared/*.c) \
          vendor/tomlc17/tomlc17.c
OBJS    = $(patsubst %.c,$(OBJDIR)/%.o,$(SRCS))
DEPS    = $(OBJS:.o=.d)
```

This abbreviated source-list example omits platform detection and build-mode flags.
Both applications include `src/shared/serializer_load_checkpoints.c` and the other shared serializer/UI modules.

raylib **6.0** is fetched from the source/checksum pin in `vendor/raylib/manifest.json`.
`tools/build_raylib.py` verifies the archive, applies the documented replacements
in `vendor/raylib/patches.json`, and uses CMake for an out-of-source static build.
The patches release alias converter caches and WAV decoders and avoid miniaudio
null-pointer arithmetic. A macOS-only timing patch gives the existing partial
busy wait an extra 1 ms wakeup margin to reduce sleep overshoot at 60 FPS; it
uses a little more CPU near the deadline, not a full-frame spin. The bootstrap
rejects unfamiliar patch contexts.
Desktop uses bundled GLFW; Web uses the same Emscripten toolchain
as the application. `RAYLIB_BUILD` defaults to `$(OUTDIR)/raylib`; web uses
`$(OUTDIR)/raylib-web`. Keep build modes/toolchains in separate directories.
Use `EXTRA_CFLAGS` and `EXTRA_LDFLAGS` for additional flags.
Set `RAYLIB_ARCHIVE=path/source.tar.gz` to share one downloaded archive across build
directories (CI caches `.cache/raylib/`); it is SHA-256 verified on every use.

Application objects depend on the built raylib library as well as their own
sources/headers. A dependency or patch change therefore recompiles consumers and
relinks executables; preserved upstream header timestamps cannot leave old code
silently linked. `vendor/raylib/README.md` records the source and patch provenance.
raylib itself is rebuilt only when the pin, the patches, `tools/build_raylib.py`
or its options (platform, compiler, build mode, sanitizer, null audio) change.
`build_raylib.py` always re-runs CMake, but CMake leaves `libraylib.a` untouched
when nothing needed recompiling, and the Makefile records the successful run in
`$(RAYLIB_BUILD)/build-done.stamp`; so editing the Makefile or the build script
does not recompile the game.

### Flag stamps

Make only compares timestamps, so it cannot notice on its own that a compiler
or flag changed. One-line stamp files record the settings each part of a tree
was built with, compile and link settings apart:

| Stamp | Records | Rebuilds when it changes |
|-------|---------|--------------------------|
| `$(OBJDIR)/build-flags.txt` | `CC`, `CFLAGS` (with `EXTRA_CFLAGS`), `PROJECT_INCLUDES` | game, editor and tool objects |
| `$(OBJDIR)/test-flags.txt` | `CC`, `TEST_CFLAGS`, `PROJECT_INCLUDES`, every per-object `TEST_OBJ_FLAGS_<source>` extra and the text of the two per-object lookups | test copies of objects |
| `$(OBJDIR)/link-flags.txt` | `CC`, `CFLAGS`, `TEST_CFLAGS`, `LIBS` (with `EXTRA_LDFLAGS`), `EDITOR_LIBS`, `TEST_LIBS`, `MATH_LIBS` | every native program is relinked; no object recompiles |
| `$(OBJDIR)/fuzz-flags.txt` | `CC`, `FUZZ_FLAGS`, `LIBS`, `MATH_LIBS` | the two fuzz replay programs |
| `$(RAYLIB_BUILD)/make-options.txt` | the `build_raylib.py` options | raylib |
| `$(OBJDIR)/web/build-flags.txt` | the Emscripten pin and every Web compile and link flag | Web raylib, objects and pages |

While parsing the Makefile, Make reads each stamp back and rewrites it only
when the text differs. Every output lists its stamp as a prerequisite, so a
changed setting rebuilds the outputs that use it (for example
`make BUILD_MODE=release` after a debug build in the same `OUTDIR` recompiles
everything with `-O2`, while `make EXTRA_LDFLAGS=...` only relinks), and an
unchanged one rebuilds nothing. That holds for edits to the Makefile itself
because recipes carry no literal flags: every flag a compile or link recipe
passes (include paths in `PROJECT_INCLUDES`, `-lm` in `MATH_LIBS`, the per-object
test renames in `TEST_OBJ_FLAGS_<source>`) lives in a variable a stamp records.
A new flag belongs in one of those variables, never in recipe text.
Test sources (`tests/*.c`) and `tools/level_check.c` compile through the same
object rules with `-MMD -MP`, so editing a shared test header such as
`tests/test_paths.h` rebuilds exactly the tests that include it.

`RAYLIB_PLATFORM=memory` selects raylib's software framebuffer and miniaudio's
null backend for explicit headless tests. Use a dedicated `OUTDIR`, such as
`out/headless`; `release` and `dist-native` reject this backend. Linux CI keeps
the desktop GLFW test path under Xvfb/Mesa; the gating macOS/Windows legs use Memory
tests and separately build/package the desktop applications. The non-gating
`Desktop backend` job runs the same tests and smoke on real GLFW/OpenGL on
Windows (a checksum-pinned Mesa llvmpipe `opengl32.dll` beside the binaries)
until it proves stable enough to require. That VM has no sound hardware, so the
job builds with `RAYLIB_AUDIO=null`: the normal desktop raylib backend with
miniaudio's null playback device, in its own `raylib-nullaudio` build
directory. `release` and `dist-native` refuse this test-only variant. Hosted macOS runners offer no OpenGL
pixel format, so the real macOS backend is verified locally instead.

The vendored tomlc17 parser is based on upstream **R260821**. Its exact upstream
commit and retained project-patch inventory are recorded in
[`vendor/tomlc17/README.md`](https://github.com/jonathanperis/super-mango-editor/blob/main/vendor/tomlc17/README.md).

### Key Variables

| Variable | Value | Description |
|----------|-------|-------------|
| `CC` | `clang` | Replaces Make's built-in default (on Windows, MSYS2's `/c/msys64/ucrt64/bin/clang.exe`); explicit compiler overrides remain supported. |
| `CFLAGS` | see below | Compiler flags |
| `LIBS` | see below | Linker flags |
| `TARGET` | `out/super-mango` | Output binary path |
| `SRCS` | explicit per-directory wildcards including shared modules | Runtime, shared serializer/UI and tomlc17 sources |
| `OBJDIR` | `out/obj` | Object/dependency root that mirrors source paths |
| `OBJS` | `$(patsubst %.c,$(OBJDIR)/%.o,$(SRCS))` | Object files under `out/obj/...` |
| `DEPS` | `$(OBJS:.o=.d)` | Auto-generated dependency files beside object files under `out/obj/...` |
| `BUILD_MODE` | `debug` | `debug` (`-g -O0`) or `release` (`-O2` plus hardening); `make debug`/`make release` set it |
| `RAYLIB_PLATFORM` | `native` | `memory` selects the headless test backend (rejected by `release`/`dist-native`) |
| `RAYLIB_AUDIO` | `device` | `null` builds raylib with miniaudio's null playback device in `$(OUTDIR)/raylib-nullaudio` (test-only; rejected by `release`/`dist-native`) |
| `RAYLIB_ARCHIVE` | empty | Optional shared, SHA-256-verified raylib source archive |
| `EXTRA_CFLAGS` / `EXTRA_LDFLAGS` / `EXTRA_WEB_CFLAGS` | empty | Appended flags; CI passes `-Werror` |

The Makefile declares `.DELETE_ON_ERROR`, so a recipe that fails part-way (for
example emcc succeeding and `tools/web_csp.py` then failing) deletes its target
instead of leaving a half-built file that Make would treat as up to date.

### Compiler Selection Caveat

For reproducible local results, pass `CC=clang` explicitly:

```sh
make test CC=clang
make smoke CC=clang
```

The Makefile detects GNU Make's built-in compiler default (`$(origin CC)` is `default`) and replaces it with clang; on Windows that is the MSYS2 UCRT64 Clang at `/c/msys64/ucrt64/bin/clang.exe`. A plain `CC ?= clang` would never apply, because Make always predefines `CC=cc`. A compiler from the command line or the environment takes precedence.

### Builder and build modes

`make builder` builds both executables. `make run-editor` also builds the sibling
game needed for F5 playtest. The default build includes `-g -O0`; `make debug`
isolates it under `out/debug/`, and `make release` builds `-O2` game/editor binaries
under `out/release/`. Release builds on Linux/macOS add `-fstack-protector-strong` and
`-D_FORTIFY_SOURCE=2`; Linux also links PIE with full RELRO (`-pie -Wl,-z,relro,-z,now`)
against a position-independent release raylib. MSYS2/MinGW keeps plain `-O2`.
Their object directories never overlap. Changing flags inside one `OUTDIR` is
safe (the [flag stamps](#flag-stamps) rebuild what changed), but switching back
and forth recompiles each time, so give long-lived flag combinations their own
`OUTDIR`.

`make content-inventory` regenerates public counts and the raw asset inventory;
`make asset-budget` checks freshness and the 4 MiB raw playable-asset budget
(2 MiB of it for generated sounds). `make sounds` regenerates the WAV files
from `tools/gen_sounds.py`; `make docs-drift` runs its `--check` mode.
`make timing-lab` prints a small integration experiment for 30/60/144 Hz that
shows why the game simulates fixed 1/60 s steps instead of a variable frame dt.
Python 3.11+ and Node.js are required for host checks. Use a current Python
(3.12+ recommended) for the raylib bootstrap, which also uses `tarfile` extraction
filters. Linux dialogs use zenity.

### Compiler Flags Explained

| Flag | Meaning |
|------|---------|
| `-std=c11` | Compile as C11 (ISO/IEC 9899:2011) |
| `-Wall` | Enable common warnings |
| `-Wextra` | Enable extra warnings beyond `-Wall` |
| `-Wpedantic` | Strict ISO compliance warnings |
| `-MMD` | Generate `.d` dependency files for each `.o` (tracks header changes) -- passed in compile rule, not in `CFLAGS` |
| `-MP` | Add phony targets for each dependency (prevents errors when headers are deleted) -- passed in compile rule, not in `CFLAGS` |
| `-I$(RAYLIB_BUILD)/build/raylib/include` | Headers from the pinned raylib build |

### Linker Flags Explained

| Flag | Meaning |
|------|---------|
| `$(RAYLIB_LIB)` | Static raylib library: graphics, image/font decoding, input and audio |
| `$(PLATFORM_LIBS)` | Native OpenGL/OS frameworks and system libraries selected by platform |
| `-lm` | Math library (`math.h` functions: `sinf`, `cosf`, `fmodf`, etc.) |

---

## Build Targets

`make help` prints every target below with a one-line description, grouped by
purpose. A target shows up there when its rule line ends in a `## Group: text`
comment, so document a new target by adding one.

### `make` / `make all`

Compiles game source files from the Makefile's source directory list to `.o` objects, then links them into `out/super-mango`.

```sh
make
```

**Steps:**
1. Creates `out/` directory if it does not exist
2. Downloads/checks/patches raylib and builds its static library if needed
3. Compiles each listed source file → `.o`
4. Links all `.o` files and raylib → `out/super-mango`
5. On macOS (`uname -s == Darwin`), ad-hoc code signs the binary with `codesign --force --sign - $@` (required on Apple Silicon to avoid `Killed: 9` errors). On other platforms this step is skipped

### `make run`

Builds (if out of date) then immediately executes the binary with no CLI flags. The native start menu loads the ordered v1 campaign catalog from `levels/campaigns/main.toml`; it displays the selected entry's TOML `name` (or its filename stem when `name` is empty).

```sh
make run
```

Asset and level paths are relative to the process's working folder:

```c
texture_load("assets/sprites/backgrounds/sky_blue.png");
sound_load("assets/sounds/player/player_jump.wav");
```

`make run` starts the game from the repo root, where those paths resolve. Started
from any other folder, the game and editor first change to the folder that holds
`assets/` and `levels/`: the executable's own folder (a release zip) or one or two
folders above it (`out/super-mango`, `out/release/super-mango` in a checkout).
Command-line paths are made absolute before that move, so they keep meaning the
folder they were typed in (`src/shared/asset_root.c`).

### `make run-debug`

Builds (if out of date) then runs the binary with the `--debug` flag, which enables the debug overlay: FPS counter, collision hitbox visualization, and scrolling event log.

```sh
make run-debug
```

### `make run-level LEVEL=path`

Builds (if out of date) then runs the binary with the `--level` flag, loading a specific TOML level file directly and skipping the campaign selector. The supplied valid TOML level does not need to be listed in `levels/campaigns/main.toml`.

```sh
make run-level LEVEL=levels/labs/01_collision.toml
```

### `make run-level-debug LEVEL=path`

Builds (if out of date) then runs the binary with both `--debug` and `--level` flags, loading a specific TOML level file with the debug overlay enabled.

```sh
make run-level-debug LEVEL=levels/labs/01_collision.toml
```

### `make editor`

Compiles the standalone level editor into `out/super-mango-editor`. The editor is a separate binary with its own source files in `src/editor/` and the tomlc17 TOML parser from `vendor/tomlc17/`.

```sh
make editor
```

### `make run-editor`

Builds both game and editor (if out of date), then runs the editor with its sibling game available for playtest.

```sh
make run-editor
```

### `make web`

Compiles the game to WebAssembly using the Emscripten SDK (`emcc`). CI pins **6.0.9**; use that SDK with `emcc` on `PATH` for matching local builds. `make web` first checks `emcc --version` against `EMSCRIPTEN_VERSION` (by default that same pin) and stops with a clear message on a mismatch; pass `EMSCRIPTEN_VERSION=<x.y.z>` to expect another release, or `EMSCRIPTEN_VERSION=` to skip the check.

```sh
make web
```

Produces `out/super-mango.html`, `.js`, `.wasm`, and `.data` (bundled assets/sounds). Each source compiles once into `out/obj/web/` (with `-MMD -MP` header tracking), and the normal and debug pages both link those objects, since they differ only at link time. The pinned raylib Web library is built separately with `emcmake`; the application links it with `USE_GLFW=3`. Uses a custom shell template from `web/shell.html`. Application sources compile with the native warning set (`-Wall -Wextra -Wpedantic`), minus the pedantic empty-declaration diagnostic that Emscripten's documented `EM_JS(...);` form triggers. `EXTRA_WEB_CFLAGS` appends flags (CI passes `-Werror`). After linking, `tools/web_csp.py` pins the minified shell's inline boot script hash into its same-origin Content-Security-Policy (`script-src 'self' 'wasm-unsafe-eval'`, no JavaScript eval); `tools/check_wasm_artifacts.py` re-verifies it.

The Web frame callback returns to Emscripten's animation-frame scheduler rather
than using raylib's blocking FPS limiter. Asyncify is not required by this loop.
Artifact compilation and HTTP delivery checks do not execute browser frames;
startup/render changes also need the browser checks in [Testing](../testing/).

The target also produces debug boot artifacts (`out/super-mango-debug.html` and companions) for direct debug HTML launches. The docs-site browser debug button uses the normal `super-mango.js` payload and passes `--debug` at boot.

For release confidence, the **GitHub Actions WebAssembly build is authoritative**. Report local SDK/cache failures separately from application failures, and require the CI `make web`, artifact checks and Pages assembly checks. JavaScript syntax/WASM validation requires real Node.js; if a local `node` wrapper launches Bun, set `NODE` to the Node executable explicitly.

### `make test`

Builds and runs native regression harnesses, including hidden-window session/editor integration cases. Rendering still requires a working desktop graphics context and audio device. Linux CI provides Xvfb/Mesa and a PulseAudio null sink; no human interaction is required.

```sh
make test
```

Current test binaries (17):

- `out/level-serializer-test`
- `out/level-validate-test`
- `out/runtime-load-test`
- `out/rail-test`
- `out/entity-utils-test`
- `out/collision-test`
- `out/phase-transition-test`
- `out/editor-validation-test`
- `out/gameplay-damage-test`
- `out/gameplay-config-test`
- `out/gameplay-score-test`
- `out/game-overlay-test`
- `out/game-events-test`
- `out/session-test`
- `out/game-checkpoint-test`
- `out/gameplay-mechanics-test`
- `out/editor-ui-test`

`make test` also runs `tests/validate_levels_test.py` and `tests/gen_sounds_test.py`.
Its `web-host-contract` prerequisite runs `tools/check_web_boot_contract.py`, the
Node tests `tests/web_host_test.cjs`, `tests/profile_storage_test.cjs`,
`tests/touch_controls_test.cjs` and `tests/keyboard_scope_test.cjs`, then
`tests/package_release_test.py` and `tests/filter_codeql_sarif_test.py`
(`make web-host-contract` runs them alone). `tests/docs_checks_test.py` runs from
`bun run check-site` instead. The native list above is the complete
`TEST_TARGETS` inventory; profile, simulation and parser-boundary cases are linked
into existing harnesses rather than separate binaries.

`make test` also runs the standalone `parser-allocation-probe` and Python
`parser-encoding-probe` (`tests/parser_validator_test.py`). These focused targets
verify allocation-growth limits and consistent UTF-8/BOM decoding across tools.
`make sanitize` instruments the C probe and runs both alongside the existing
regression suites.

### `make validate-levels`

Builds `out/level-check` (`tools/level_check.c`) and loads every `levels/*.toml` and `levels/labs/*.toml` through the game's own loader and validator, `level_load_toml()`. The rules live in one place, `src/levels/level_validate.c` and `src/shared/serializer_parse.c`: schema, counts, geometry, rails, checkpoints and path shapes. The checker needs no window or GPU; it reuses the game's objects (and so builds raylib once, for its headers).

`tools/validate_levels.py` then makes the checks a single file cannot: referenced assets and `next_phase` targets exist, checked-in levels state `format_version` and a positive `screen_count`, the required v1 `levels/campaigns/main.toml` manifest lists real levels in order as a linear chain, and asset literals in `src/` exist. Its `validate_schema()` only mirrors the type schema so `make docs-drift` can check the manual's partial TOML snippets without a compiler.

```sh
make validate-levels
```

### `make smoke`

Builds the game and editor, then runs every `levels/*.toml` and `levels/labs/*.toml` in a hidden window for a bounded number of frames. The editor smoke mode renders five frames before exiting.

```sh
make smoke SMOKE_FRAMES=5 SMOKE_SEED=1
```

### `make scripted-smoke`

Builds the game and editor, then runs `tools/run_scripted_smoke.py` against every `levels/*.toml` and `levels/labs/*.toml` (the same set as `make smoke`) for each seed in `SMOKE_SEEDS`. The runner writes deterministic replay scripts under `$(OUTDIR)/replays-smoke/`, so `OUTDIR=out/headless` keeps them beside that build, and passes that folder with `--replay-dir` and validated replay names with `--replay-script`, injecting semantic commands and sampled action masks for movement, jumping and pause/resume. It checks observable results and repeats each scenario to verify deterministic state.

```sh
make scripted-smoke SMOKE_FRAMES=5 SMOKE_SEEDS="1 7 23"
```

### `make sanitize`

Builds the game and editor and runs `make test` in a separate `out-sanitize/` tree with AddressSanitizer and UndefinedBehaviorSanitizer enabled, then runs `make fuzz-corpus` in the same tree. Undefined behaviour is fatal (`-fno-sanitize-recover=undefined` plus `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`): UBSan would otherwise print a report and let the test pass.

### `make coverage`

Rebuilds and runs `make test` in `out/coverage/` with clang source-based coverage, then prints an `llvm-cov` per-file summary. Requires clang plus `llvm-profdata`/`llvm-cov` (`xcrun` on macOS). See [Testing](../testing/#coverage).

### `make fuzz-corpus` / `make fuzz`

`fuzz-corpus` replays the level and profile fuzz seeds under ASan/UBSan with a plain driver. `fuzz` runs coverage-guided libFuzzer for `FUZZ_SECONDS` per harness and needs a clang with libFuzzer (`FUZZ_CC`). See [Testing](../testing/#fuzzing).

```sh
make fuzz-corpus FUZZ_MUTATIONS=500
```

### `make sanitize-smoke`

Builds sanitizer-instrumented native game/editor binaries and raylib in `out-sanitize/`, then runs rendered smoke against every TOML level plus the editor. This complements `make sanitize` by covering graphics/resource startup and teardown, not just pure logic harnesses.

```sh
make sanitize-smoke SMOKE_FRAMES=5 SMOKE_SEED=1
```

### `make level-catalog`

Regenerates `docs/wiki/level-catalog.md` from the ordered campaign manifest using `tools/generate_level_catalog.py`. Use this after changing manifest membership/order or listed-level metadata/content counts.

```sh
make level-catalog
```

### `make overlay-snapshots`

Regenerates `docs/wiki/overlay-snapshots.md` from the canonical overlay strings in the runtime. Use this after changing pause, game-over, level-complete, or terminal overlay copy/controls.

```sh
make overlay-snapshots
```

### `make roadmap-quality`

Runs `tools/check_roadmap_quality.py`, the extra semantic guardrail layer that complements `check_docs_drift.py`. It checks things like TOML line-ending policy, overlay snapshot coverage, release/WebAssembly guardrails, and scripted-smoke wiring.

```sh
make roadmap-quality
```

### `make dist-native`

Depends on `release` and `asset-budget`, then packages optimized native builder archives under `dist/`. It refuses `RAYLIB_PLATFORM=memory` and `RAYLIB_AUDIO=null`. Both game and editor are included with playable assets, campaign/lab levels, project and third-party notices, and a run README. `unused/` assets are excluded. CI uses the same packaging path. Archives are reproducible: `tools/package_release.py` sorts the entries, gives files fixed modes (755 for executables, 644 otherwise) and dates every entry from `SOURCE_DATE_EPOCH` (CI sets the commit time), or 1980-01-01 when it is unset, so one commit always packs the same bytes.

```sh
make dist-native
```

### `make dist-wasm`

Depends on `asset-budget` and `make web`, whose HTML outputs are file targets over the Web objects (and through them the sources and headers), `web/` host files, `assets/`, `levels/`, the Web raylib library and the Web flag stamp; emcc only reruns when one of them is newer, so a stale WASM build is never packaged. Archives include HTML/JS/WASM/data files, README and third-party notices.

```sh
make dist-wasm   # runs make web first when its outputs are stale
```

### `make docs-drift`

Runs generated content-inventory/asset-budget, `tools/gen_sounds.py --check`, level-catalog and overlay-snapshot freshness checks, `tools/check_docs_drift.py`, and `tools/check_roadmap_quality.py`. They compare documented test targets, README/guide summaries, source-map entries, campaign coverage, TOML example schemas, player API declarations, runtime flags, constant values, inspector keys, the render order, `make` targets and `src/` paths named in the manual, level prose, workflow references and scripted-smoke wiring. Run the separate built-site check below for emitted links and metadata.

```sh
make docs-drift
```

---

## Astro Docs Site Toolchain

The public GitHub Pages documentation site lives under `docs/` and builds with **Astro 7**. The upgrade keeps the site fully static for GitHub Pages while taking the parts of Astro 7 that fit this repo:

- Astro's Rust `.astro` compiler is now the default compiler, so malformed templates fail during `astro check`/`astro build` instead of being silently corrected.
- Vite 8 and Rolldown are pulled in through Astro 7; the docs site keeps the existing Tailwind Vite plugin and does not depend on Vite internals.
- The site already uses the Sätteri Markdown processor via `markdown.processor: satteri()`, so Markdown pages are rendered through the Rust-backed pipeline.
- Queued rendering is enabled by Astro 7 by default and does not require config.

Astro 7's route caching, CDN cache providers, and `src/fetch.ts` advanced-routing hooks are intentionally **not** configured here because this repo deploys static HTML/assets to GitHub Pages (`output: "static"`) and has no SSR adapter or request-time runtime.

The manual is an Astro content collection: `docs/src/content.config.ts` uses the
`glob` loader over `docs/wiki/*.md`, and its strict schema accepts only optional
`title`/`description` frontmatter. `docs/src/pages/docs/[...slug].astro` loads
pages with `getCollection('docs')` and `render()`, and fails the build when a
page and the `SECTION_ORDER` list in `docs/src/lib/docsSidebar.ts` disagree.
`docs/integrations/home-csp.mjs` adds a hash-pinned Content-Security-Policy to
the home page that hosts the game after the build. The site's three fonts
(Pixelify Sans, Atkinson Hyperlegible, DM Mono) are self-hosted from the
`@fontsource/*` packages **5.3.0**, which `BaseLayout.astro` imports so Astro
bundles the font files; that policy allows styles and fonts only from the site
itself, and `bun run check-site` fails if any page references Google Fonts or
any source under `docs/` or `tools/` contains a Google Fonts URL. The link
preview template `tools/og-image.html` loads Pixelify Sans from
`docs/node_modules` too, so run `bun install` in `docs/` before
`python3 tools/render_og_image.py`. The
SIL Open Font License texts are published from `docs/public/licenses/fonts/`. Optional analytics loads only
on manual pages, never on that home page.

```sh
cd docs
bun install --frozen-lockfile
bun audit
bun run lint
NODE_ENV=production bun run build
bun run check-site
```

When Bun is unavailable but `docs/node_modules` has already been restored, the same package scripts can be checked with npm:

```sh
cd docs
npm run lint && npm run build
npm run check-site
```

Keep Bun and `bun.lock` as the CI dependency contract. The npm fallback runs the declared scripts; it does not replace the locked install step.

Astro requires Node.js 22.12+; CI uses Node **26.9.0** and Bun **1.4.2**. The
supported frontend set is Astro **7.3.6**, `@astrojs/markdown-satteri` **0.4.3**,
`@astrojs/sitemap` **3.7.4**, `@astrojs/check` **0.9.10**, Tailwind CSS and its Vite
plugin **4.3.3**, and TypeScript **6.0.3**. TypeScript **7.0.2** is newer but outside
Astro Check 0.9.10's `^5.0.0 || ^6.0.0` peer range; retain 6.0.3 until supported.
Locked transitive dependencies respect their upstream version constraints.

Development uses `/`; production output uses `/super-mango-editor/`. Astro does not compile or
copy WASM: see the [website maintainer guide](https://github.com/jonathanperis/super-mango-editor/blob/main/docs/README.md)
for local game assembly, page registration and generated-content ownership.

### `make clean`

Removes the selected `OUTDIR`, its `OUTDIR-sanitize` sibling created by `make sanitize`, and `DISTDIR` (default `out/`, `out-sanitize/` and `dist/`). Objects only ever live under `OUTDIR/obj`, so no in-tree `.o` files exist to delete. The docs site output (`docs/out/`) is not covered.

```sh
make clean
```

Override `OUTDIR`/`DISTDIR` only when you intend to remove those specific build outputs.

---

## Prerequisites

### macOS (Apple Silicon / Intel)

```sh
# Install Homebrew if needed: https://brew.sh
brew install cmake python

# Xcode Command Line Tools (provides clang and make)
xcode-select --install
```

The current Python downloads/verifies the pinned raylib archive. The native library
links against macOS frameworks. Rendered tests need an available desktop session;
a hidden window is not a display-less renderer. Report missing display/audio
devices as environment blockers, rather than disabling sanitizer checks.

### Linux -- Debian / Ubuntu

```sh
sudo apt update
sudo apt install build-essential clang cmake python3 libgl1-mesa-dev libx11-dev \
    libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev zenity
```

### Linux -- Fedora / RHEL / CentOS

```sh
sudo dnf install clang make cmake python3 mesa-libGL-devel libX11-devel \
    libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel zenity
```

### Linux -- Arch Linux

```sh
sudo pacman -S clang make cmake python mesa libx11 libxrandr libxinerama libxcursor libxi zenity
```

### Windows (MSYS2)

1. Install [MSYS2](https://www.msys2.org/)
2. Open the **MSYS2 UCRT64** terminal:

```sh
pacman -S make mingw-w64-ucrt-x86_64-clang mingw-w64-ucrt-x86_64-gcc \
          mingw-w64-ucrt-x86_64-python mingw-w64-ucrt-x86_64-cmake \
          mingw-w64-ucrt-x86_64-make
```

3. Build:

```sh
cd /c/path/to/super-mango-editor
make
```

4. Run through `make run` / `make run-editor`, which puts the UCRT64 DLL directory on `PATH`, or ensure runtime DLLs are discoverable when launching directly. Current Windows release packaging bundles runtime DLLs. CI builds and tests UCRT64 with both GCC (`CC=gcc`, release archives) and Clang (`CC=clang`).

---

## CI/CD Pipelines

Three workflow files handle automated builds, docs checks and analysis; GitHub Pages publishing is part of `build.yml`:

| Workflow | File | Trigger | Purpose |
|----------|------|---------|---------|
| Build & Release | `build.yml` | Push to `main`, pull requests, `v*` tags, manual | Always-on `Docs drift` job (a required check; see the [release checklist](../release-checklist/)); native game/editor tests, smoke and packaging (Windows with GCC); a separate `Windows x86_64 clang -Werror (rolling MSYS2 toolchain)` job builds and tests with Clang outside the release `needs`; a parallel `Sanitizers (Linux x86_64)` job runs `make sanitize` and `make sanitize-smoke` (ASan/UBSan/LeakSanitizer, undefined behaviour fatal); Linux scripted smoke; level validation on every native OS; WASM build/artifact/package checks; a separate `Desktop backend` job runs Windows (Mesa llvmpipe) tests, smoke and scripted smoke on real GLFW/OpenGL (hosted macOS runners have no OpenGL pixel format); a `Coverage (Linux x86_64)` job runs `make coverage` on the Memory backend with a line-coverage floor and posts the per-file table to the job summary; on `main` pushes a `Release freshness` job warns when `main` is more than 50 commits past the latest `v*` tag. Superseded PR runs are cancelled; main/tag runs never are. Releases only on `v*` tags or manual dispatch on `main` (tagged `v1.0.<commit count>`) |
| Docs | `docs.yml` | Relevant pull requests, weekly, manual | Frozen Bun install, lint, `bun audit`, build and `bun run check-site`; filters include root docs, source, content and workflows. `make docs-drift` is left to the always-on `Docs drift` job, and on `main` the `Pages build` job lints, builds and checks the site, so neither runs twice. The weekly run audits the lockfile against new advisories. Superseded PR runs are cancelled |
| CodeQL | `codeql.yml` | Push/PR to `main`, weekly, manual | C/C++ (built), GitHub Actions, Python and JavaScript/TypeScript (no build) security-and-quality analysis; superseded PR runs are cancelled. The C/C++ job analyzes without uploading, drops code-quality (non-security) results located in third-party code (`out/` raylib headers, `vendor/`) with `tools/filter_codeql_sarif.py`, then uploads the SARIF with `upload-sarif` |
| Pages | `build.yml` jobs `pages-build` → `pages-deploy` | Main push/manual run on main, after `Docs drift`, the build matrix and `Sanitizers (Linux x86_64)` pass | Lints, builds and checks docs at the run's commit with read-only permissions, adds the same run's WASM artifact, HTTP-smokes the assembly; a separate job with only Pages/OIDC permissions deploys `docs/out/`. Keeping both in one workflow avoids a `workflow_run` trust boundary (no artifact from another run is consumed) |

Every job sets `timeout-minutes`. Every native leg (Clang and GCC) passes
`EXTRA_CFLAGS=-Werror` and the WebAssembly leg `EXTRA_WEB_CFLAGS=-Werror`.
The release job needs the legs that ship an archive (`build`), the
`Sanitizers (Linux x86_64)` job (which used to be part of the Linux leg) and
`provenance`, which itself waits for `build` and the sanitizers so it never
attests archives a failing gate blocks; the Windows Clang, `Desktop backend`
and `Coverage (Linux x86_64)` jobs report separately and never block a release.

**Toolchain drift under `-Werror`.** Linux/macOS use the runner image's
compilers and the WebAssembly leg pins Emscripten 6.0.9, but Windows uses
MSYS2, a rolling distribution: `setup-msys2` runs with `update: true` and
`pacman` installs the current GCC/Clang. MSYS2 cannot reliably install older
package versions, so these are not pinned. A new compiler release can add a
warning and fail a Windows leg with no source change. Each Windows job prints
`--version` and `pacman -Q` output in its "Record ... toolchain versions" step;
when a Windows-only `-Werror` failure appears, compare that output with the last
green run before changing code. A Clang-only failure shows up in the job named
`Windows x86_64 clang -Werror (rolling MSYS2 toolchain)` and does not block
releases; a GCC failure in `Build (Windows x86_64)` does, because that leg ships
the Windows archive.
GCC's `-Wformat-truncation` is stricter than Clang's: label code formats long
paths through `editor_path_for_display()` and checks `snprintf` results where
truncation would change behaviour. Build jobs use Python 3.12 from
`actions/setup-python` on Linux/macOS (MSYS2 supplies Python on Windows) and
restore the pinned raylib source archive from `actions/cache` via
`RAYLIB_ARCHIVE`; the archive is SHA-256 verified on every use. Every native
build leg (Linux, macOS and Windows) caches its compiled debug raylib trees
(`out/raylib`, plus `out/headless/raylib` for the Memory-backend tests), and the
sanitizers job caches its sanitizer tree; the release tree that ships is never
cached. CMake records absolute paths and per-compiler settings, so the key must
match exactly: it covers the pin, patches, `Makefile`, `tools/build_raylib.py`,
runner image, workspace path and the leg's compiler (`MANGO_CC`) and CMake
versions. `make` still runs `build_raylib.py` on a restored
tree, which re-verifies the archive and re-runs CMake; CMake then rebuilds
anything its own dependency tracking finds stale.

Renovate (`renovate.json`, on top of the shared `jonathanperis/.github` preset)
watches the pins that no package manager owns, through regex `customManagers`:
the Emscripten SDK (the setup-emsdk `version` in `build.yml`, `EMSCRIPTEN_VERSION`
in the Makefile and every version quoted in the docs, grouped into one pull
request; `make docs-drift` fails if they disagree), Mesa for the `Desktop
backend` job, the raylib tag and commit in `vendor/raylib/manifest.json`, and
the vendored tomlc17 release. None of them automerge. Mesa and raylib bumps
also need their new SHA-256 entered by hand (CI fails closed until then), and
tomlc17 only appears on the Dependency Dashboard, because re-importing it means
re-applying the project patches listed in `vendor/tomlc17/README.md`. Pins
inside a workflow or the Makefile carry a `# renovate: datasource=... depName=...`
comment on the line above.

The repository restricts third-party Actions to an allowlist and requires full
commit-SHA pins. A renamed or transferred Action can resolve through the GitHub
API yet be rejected before any job starts if its new repository name is absent
from that allowlist. The canonical `emscripten-core/setup-emsdk@*` entry is allowed
alongside the existing integrations. Coordinate future owner/name changes with
the repository settings rather than relaxing SHA pinning.

Native rendered smoke uses `./out/super-mango --level levels/00_sandbox_01.toml --smoke-test-frames 5` and `./out/super-mango-editor --smoke-test`. Linux CI supplies a virtual display and audio sink. WebAssembly checks validate both normal/debug HTML/JS/WASM/data sets, JavaScript syntax, module compilation, host interfaces and archive contents.

---

## Adding New Source Files

Because the Makefile uses per-subdirectory wildcards, any new `.c` file placed in `src/` or a recognized source subdirectory is compiled automatically on the next `make` invocation. A new source directory needs a `SRCS` wildcard; the shared pattern rule and `make clean` already cover everything under `out/`.

```sh
# Example: adding an entity in a subdirectory
touch src/entities/new_enemy.c src/entities/new_enemy.h
make   # new_enemy.c is compiled automatically
```

See the [Entity Walkthrough](../entity-walkthrough/) for the full new-entity workflow.

---

## Output Structure

After a successful build:

```
out/
├── super-mango                          ← the game binary
├── super-mango-editor                   ← the editor binary (make editor)
├── raylib/                              ← verified source, applied patches and CMake build
│   ├── make-options.txt                 ← raylib flag stamp
│   └── build-done.stamp                 ← last successful build_raylib.py run
├── raylib-web/                          ← Emscripten raylib build (make web)
└── obj/
    ├── build-flags.txt                  ← flag stamp for game, editor and tool objects
    ├── test-flags.txt                   ← flag stamp for test objects
    ├── link-flags.txt                   ← flag stamp for linking native programs
    ├── fuzz-flags.txt                   ← flag stamp for the fuzz replay programs
    ├── src/                             ← game/editor objects mirror source paths
    │   ├── core/*.o / *.d
    │   ├── editor/*.o / *.d
    │   ├── player/*.o / *.d
    │   └── ...
    ├── tests/                           ← test-flag copies, same mirrored paths (make test)
    │   └── tests/*.o / *.d              ← the test sources themselves
    ├── tools/level_check.o / .d         ← level checker (make validate-levels)
    ├── web/                             ← emcc objects and Web flag stamp (make web)
    └── vendor/tomlc17/tomlc17.o / .d
```
