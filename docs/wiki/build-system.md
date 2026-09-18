# Build System

<a id="home"></a>

---

## Makefile Overview

The project uses a **GNU Makefile** with explicit per-directory wildcards. New `.c` files in recognized source directories are compiled automatically; new source directories need matching `SRCS` and pattern-rule entries.

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
`tools/build_raylib.py` verifies the archive and uses CMake for an out-of-source
static build. Desktop uses bundled GLFW; Web uses the same Emscripten toolchain
as the application. `RAYLIB_BUILD` defaults to `$(OUTDIR)/raylib`; web uses
`$(OUTDIR)/raylib-web`. Keep build modes/toolchains in separate directories.
Use `EXTRA_CFLAGS` and `EXTRA_LDFLAGS` for additional flags.

`RAYLIB_PLATFORM=memory` selects raylib's software framebuffer and miniaudio's
null backend for explicit headless tests. Use a dedicated `OUTDIR`, such as
`out/headless`; `release` and `dist-native` reject this backend. Linux CI keeps
the desktop GLFW test path under Xvfb/Mesa; macOS/Windows CI uses Memory tests
and separately builds/packages the desktop applications.

The vendored tomlc17 parser is based on upstream **R260821**. Its exact upstream
commit and retained project-patch inventory are recorded in
[`vendor/tomlc17/README.md`](https://github.com/jonathanperis/super-mango-editor/blob/main/vendor/tomlc17/README.md).

### Key Variables

| Variable | Value | Description |
|----------|-------|-------------|
| `CC` | `clang` | Replaces Make's built-in default; explicit compiler overrides remain supported. |
| `CFLAGS` | see below | Compiler flags |
| `LIBS` | see below | Linker flags |
| `TARGET` | `out/super-mango` | Output binary path |
| `SRCS` | explicit per-directory wildcards including shared modules | Runtime, shared serializer/UI and tomlc17 sources |
| `OBJDIR` | `out/obj` | Object/dependency root that mirrors source paths |
| `OBJS` | `$(patsubst %.c,$(OBJDIR)/%.o,$(SRCS))` | Object files under `out/obj/...` |
| `DEPS` | `$(OBJS:.o=.d)` | Auto-generated dependency files beside object files under `out/obj/...` |

### Compiler Selection Caveat

For reproducible local results, pass `CC=clang` explicitly:

```sh
make test CC=clang
make smoke CC=clang
```

The Makefile detects GNU Make's built-in compiler default and replaces it with clang. An explicit command-line compiler takes precedence.

### Builder and build modes

`make builder` builds both executables. `make run-editor` also builds the sibling
game needed for F5 playtest. The default build includes `-g -O0`; `make debug`
isolates it under `out/debug/`, and `make release` builds `-O2` game/editor binaries
under `out/release/`. Their object directories never overlap. For additional
custom flag combinations, choose a separate `OUTDIR` to avoid reusing old objects.

`make content-inventory` regenerates public counts and the raw asset inventory;
`make asset-budget` checks freshness and the 40 MiB raw playable-asset budget.
`make timing-lab` prints a small integration experiment for 30/60/144 Hz.
Python 3.11+ and Node.js are required for host checks. Linux dialogs use zenity.

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

### `make` / `make all`

Compiles game source files from the Makefile's source directory list to `.o` objects, then links them into `out/super-mango`.

```sh
make
```

**Steps:**
1. Creates `out/` directory if it does not exist
2. Compiles each listed source file → `.o`
3. Links all `.o` files → `out/super-mango`
4. On macOS (`uname -s == Darwin`), ad-hoc code signs the binary with `codesign --force --sign - $@` (required on Apple Silicon to avoid `Killed: 9` errors). On other platforms this step is skipped

### `make run`

Builds (if out of date) then immediately executes the binary with no CLI flags. The native start menu loads the ordered v1 campaign catalog from `levels/campaigns/main.toml`; it displays the selected entry's TOML `name` (or its filename stem when `name` is empty).

```sh
make run
```

The binary must be run from the **repo root** because asset paths are relative:

```c
texture_load("assets/sprites/backgrounds/sky_blue.png");
sound_load("assets/sounds/player/player_jump.wav");
```

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

Compiles the game to WebAssembly using the Emscripten SDK (`emcc`). CI pins **6.0.9**; use that SDK with `emcc` on `PATH` for matching local builds.

```sh
make web
```

Produces `out/super-mango.html`, `.js`, `.wasm`, and `.data` (bundled assets/sounds). The pinned raylib Web library is built separately with `emcmake`; the application links it with `USE_GLFW=3`. Uses a custom shell template from `web/shell.html`.

The target also produces debug boot artifacts (`out/super-mango-debug.html` and companions) for direct debug HTML launches. The docs-site browser debug button uses the normal `super-mango.js` payload and passes `--debug` at boot.

For release confidence, the **GitHub Actions WebAssembly build is authoritative**. Report local SDK/cache failures separately from application failures, and require the CI `make web`, artifact checks and Pages assembly checks. JavaScript syntax/WASM validation requires real Node.js; if a local `node` wrapper launches Bun, set `NODE` to the Node executable explicitly.

### `make test`

Builds and runs native regression harnesses, including hidden-window session/editor integration cases. Rendering still requires a working desktop graphics context and audio device. Linux CI provides Xvfb/Mesa and a PulseAudio null sink; no human interaction is required.

```sh
make test
```

Current test binaries (15):

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

`make test` also runs `tests/validate_levels_test.py`. Its `web-host-contract`
prerequisite runs the static boot check, JavaScript host/profile-storage/touch
contracts and Python release-archive tests. The native list above is the complete
`TEST_TARGETS` inventory; profile, simulation and parser-boundary cases are linked
into existing harnesses rather than separate binaries.

`make test` also runs the standalone `parser-allocation-probe` and Python
`parser-encoding-probe`. These focused targets verify allocation-growth limits
and consistent UTF-8/BOM decoding across tools. `make sanitize` instruments the C
probe and runs both alongside the existing regression suites.

### `make validate-levels`

Runs the Python TOML validator against `levels/*.toml`, `levels/labs/*.toml` and the required v1 `levels/campaigns/main.toml` manifest. It checks schema, referenced asset paths, `next_phase` links, counts and geometry against C limits, checkpoints, manifest membership/order and the linear campaign chain.

```sh
make validate-levels
```

### `make smoke`

Builds the game and editor, then runs every `levels/*.toml` and `levels/labs/*.toml` in a hidden window for a bounded number of frames. The editor smoke mode renders five frames before exiting.

```sh
make smoke SMOKE_FRAMES=5 SMOKE_SEED=1
```

### `make scripted-smoke`

Builds the game and editor, then runs `tools/run_scripted_smoke.py` against every `levels/*.toml` for each seed in `SMOKE_SEEDS`. The runner writes deterministic replay scripts under `out/replays-smoke/` and passes validated replay names with `--replay-script`, injecting semantic commands and sampled action masks for movement, jumping and pause/resume. It checks observable results and repeats each scenario to verify deterministic state.

```sh
make scripted-smoke SMOKE_FRAMES=5 SMOKE_SEEDS="1 7 23"
```

### `make sanitize`

Runs `make test` in a separate `out-sanitize/` tree with AddressSanitizer and UndefinedBehaviorSanitizer enabled.

```sh
make sanitize
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

Builds optimized native builder archives under `dist/`. Both game and editor are included with playable assets, campaign/lab levels, project and third-party notices, and a run README. `unused/` assets are excluded. CI uses the same packaging path.

```sh
make dist-native
```

### `make dist-wasm`

Packages an existing verified WebAssembly build without invoking emcc again. Missing normal/debug outputs are errors. Archives include HTML/JS/WASM/data files, README and third-party notices.

```sh
make web
make dist-wasm
```

### `make docs-drift`

Runs generated content-inventory, level-catalog and overlay-snapshot freshness checks, `tools/check_docs_drift.py`, and `tools/check_roadmap_quality.py`. They compare documented test targets, README/guide summaries, source-map entries, campaign coverage, TOML example schemas, player API declarations, runtime flags, selected constants, level prose, workflow references and scripted-smoke wiring. Run the separate built-site check below for emitted links and metadata.

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
supported frontend set is Astro **7.3.3**, `@astrojs/markdown-satteri` **0.4.1**,
`@astrojs/sitemap` **3.7.4**, `@astrojs/check` **0.9.10**, Tailwind CSS and its Vite
plugin **4.3.3**, and TypeScript **6.0.3**. TypeScript **7.0.2** is newer but outside
Astro Check 0.9.10's `^5.0.0 || ^6.0.0` peer range; retain 6.0.3 until supported.
Locked transitive dependencies respect their upstream version constraints.

Development uses `/`; production output uses `/super-mango-editor/`. Astro does not compile or
copy WASM: see the [website maintainer guide](https://github.com/jonathanperis/super-mango-editor/blob/main/docs/README.md)
for local game assembly, page registration and generated-content ownership.

### `make clean`

Removes the selected `OUTDIR` and `DISTDIR` (default `out/` and `dist/`) and legacy in-source objects. Separate sanitizer and docs output directories are not covered by the default invocation.

```sh
make clean
```

Override `OUTDIR`/`DISTDIR` only when you intend to remove those specific build outputs.

---

## Prerequisites

### macOS (Apple Silicon / Intel)

```sh
# Install Homebrew if needed: https://brew.sh
brew install cmake

# Xcode Command Line Tools (provides clang and make)
xcode-select --install
```

Python 3.11+ downloads/verifies the pinned raylib archive. The native library
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

4. Run through `make run` / `make run-editor`, which puts the UCRT64 DLL directory on `PATH`, or ensure runtime DLLs are discoverable when launching directly. Current Windows release packaging bundles runtime DLLs. CI uses GCC (`CC=gcc`) in UCRT64; the local Clang commands above are also supported.

---

## CI/CD Pipelines

Four GitHub Actions workflows handle automated builds and docs checks:

| Workflow | File | Trigger | Purpose |
|----------|------|---------|---------|
| Build & Release | `build.yml` | Push to `main`, pull requests, `v*` tags, manual | Native game/editor tests, smoke and packaging; Linux sanitizers/scripted smoke; Linux/macOS level validation; WASM build/artifact/package checks. Releases only on `v*` tags or manual dispatch on `main` |
| Docs | `docs.yml` | Relevant pull requests, manual | `make docs-drift`, frozen Bun install, lint, `bun audit`, build and `bun run check-site`; filters include root docs, source, content and workflows |
| CodeQL | `codeql.yml` | Push/PR to `main`, weekly, manual | C/C++ and GitHub Actions security-and-quality analysis |
| Deploy | `deploy.yml` | Successful same-repository main push/manual Build & Release run | Builds/checks docs from the run's exact commit, copies matching WASM, HTTP-smokes the assembly and deploys `docs/out/` |

The repository restricts third-party Actions to an allowlist and requires full
commit-SHA pins. A renamed or transferred Action can resolve through the GitHub
API yet be rejected before any job starts if its new repository name is absent
from that allowlist. The canonical `emscripten-core/setup-emsdk@*` entry is allowed
alongside the existing integrations. Coordinate future owner/name changes with
the repository settings rather than relaxing SHA pinning.

Native rendered smoke uses `./out/super-mango --level levels/00_sandbox_01.toml --smoke-test-frames 5` and `./out/super-mango-editor --smoke-test`. Linux CI supplies a virtual display and audio sink. WebAssembly checks validate both normal/debug HTML/JS/WASM/data sets, JavaScript syntax, module compilation, host interfaces and archive contents.

---

## Adding New Source Files

Because the Makefile uses per-subdirectory wildcards, any new `.c` file placed in `src/` or a recognized source subdirectory is compiled automatically on the next `make` invocation. New source directories require adding a wildcard, compile rule, and clean entry.

```sh
# Example: adding an entity in a subdirectory
touch src/entities/new_enemy.c src/entities/new_enemy.h
make   # new_enemy.c is compiled automatically
```

See [Developer Guide](../developer-guide/) for the full new-entity workflow.

---

## Output Structure

After a successful build:

```
out/
├── super-mango                          ← the game binary
├── super-mango-editor                   ← the editor binary (make editor)
└── obj/
    ├── src/                             ← game/editor objects mirror source paths
    │   ├── core/*.o / *.d
    │   ├── editor/*.o / *.d
    │   ├── player/*.o / *.d
    │   └── ...
    └── vendor/tomlc17/tomlc17.o / .d
```
