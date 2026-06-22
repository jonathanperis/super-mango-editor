# Build System

<a id="home"></a>

---

## Makefile Overview

The project uses a **GNU Makefile** with explicit per-directory wildcards. New `.c` files in recognized source directories are compiled automatically; new source directories need matching `SRCS` and pattern-rule entries.

```makefile
CC      ?= clang
CFLAGS  = -std=c11 -Wall -Wextra -Wpedantic $(shell sdl2-config --cflags)
LIBS    = $(shell sdl2-config --libs) -lSDL2_image -lSDL2_ttf -lSDL2_mixer -lm
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
          $(SRCDIR)/editor/serializer.c \
          $(SRCDIR)/editor/serializer_emit.c \
          $(SRCDIR)/editor/serializer_io.c \
          $(SRCDIR)/editor/serializer_load.c \
          $(SRCDIR)/editor/serializer_load_climbables.c \
          $(SRCDIR)/editor/serializer_load_collectibles.c \
          $(SRCDIR)/editor/serializer_load_config.c \
          $(SRCDIR)/editor/serializer_load_enemies.c \
          $(SRCDIR)/editor/serializer_load_geometry.c \
          $(SRCDIR)/editor/serializer_load_hazards.c \
          $(SRCDIR)/editor/serializer_load_header.c \
          $(SRCDIR)/editor/serializer_load_layers.c \
          $(SRCDIR)/editor/serializer_load_surfaces.c \
          $(SRCDIR)/editor/serializer_parse.c \
          $(SRCDIR)/editor/serializer_save.c \
          $(SRCDIR)/editor/serializer_types.c \
          vendor/tomlc17/tomlc17.c
OBJS    = $(patsubst %.c,$(OBJDIR)/%.o,$(SRCS))
DEPS    = $(OBJS:.o=.d)
```

### Key Variables

| Variable | Value | Description |
|----------|-------|-------------|
| `CC` | `clang` intended; pass explicitly for parity | C compiler. Use `CC=clang` for CI/local parity; GNU Make may otherwise use its built-in `CC=cc`. |
| `CFLAGS` | see below | Compiler flags |
| `LIBS` | see below | Linker flags |
| `TARGET` | `out/super-mango` | Output binary path |
| `SRCS` | explicit per-directory wildcards plus editor serializer files | Game C sources from recognized source directories, TOML serializer modules, and tomlc17 |
| `OBJDIR` | `out/obj` | Object/dependency root that mirrors source paths |
| `OBJS` | `$(patsubst %.c,$(OBJDIR)/%.o,$(SRCS))` | Object files under `out/obj/...` |
| `DEPS` | `$(OBJS:.o=.d)` | Auto-generated dependency files beside object files under `out/obj/...` |

### Compiler Selection Caveat

For reproducible local results, pass `CC=clang` explicitly:

```sh
make test CC=clang
make smoke CC=clang
```

The Makefile uses `CC ?= clang`, but GNU Make defines a built-in `CC=cc`, so some local invocations resolve to `cc` unless the compiler is overridden on the command line.

### Compiler Flags Explained

| Flag | Meaning |
|------|---------|
| `-std=c11` | Compile as C11 (ISO/IEC 9899:2011) |
| `-Wall` | Enable common warnings |
| `-Wextra` | Enable extra warnings beyond `-Wall` |
| `-Wpedantic` | Strict ISO compliance warnings |
| `-MMD` | Generate `.d` dependency files for each `.o` (tracks header changes) -- passed in compile rule, not in `CFLAGS` |
| `-MP` | Add phony targets for each dependency (prevents errors when headers are deleted) -- passed in compile rule, not in `CFLAGS` |
| `$(shell sdl2-config --cflags)` | SDL2 include paths (`-I/opt/homebrew/include/SDL2`) |

### Linker Flags Explained

| Flag | Meaning |
|------|---------|
| `$(shell sdl2-config --libs)` | SDL2 core library (`-L/opt/homebrew/lib -lSDL2`) |
| `-lSDL2_image` | PNG/JPG texture loading |
| `-lSDL2_ttf` | TrueType font rendering |
| `-lSDL2_mixer` | Audio mixing (WAV, MP3, OGG) |
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

Builds (if out of date) then immediately executes the binary (no CLI flags).

```sh
make run
```

The binary must be run from the **repo root** because asset paths are relative:

```c
IMG_LoadTexture(renderer, "assets/sprites/backgrounds/sky_blue.png");
Mix_LoadWAV("assets/sounds/player/player_jump.wav");
```

### `make run-debug`

Builds (if out of date) then runs the binary with the `--debug` flag, which enables the debug overlay: FPS counter, collision hitbox visualization, and scrolling event log.

```sh
make run-debug
```

### `make run-level LEVEL=path`

Builds (if out of date) then runs the binary with the `--level` flag, loading a specific TOML level file.

```sh
make run-level LEVEL=levels/00_sandbox_01.toml
```

### `make run-level-debug LEVEL=path`

Builds (if out of date) then runs the binary with both `--debug` and `--level` flags, loading a specific TOML level file with the debug overlay enabled.

```sh
make run-level-debug LEVEL=levels/00_sandbox_01.toml
```

### `make editor`

Compiles the standalone level editor into `out/super-mango-editor`. The editor is a separate binary with its own source files in `src/editor/` and the tomlc17 TOML parser from `vendor/tomlc17/`.

```sh
make editor
```

### `make run-editor`

Builds the editor (if out of date) then immediately runs it.

```sh
make run-editor
```

### `make web`

Compiles the game to WebAssembly using the Emscripten SDK (`emcc`). Requires Emscripten to be installed and `emcc` on `PATH`.

```sh
make web
```

Produces `out/super-mango.html`, `.js`, `.wasm`, and `.data` (bundled assets/sounds). SDL2 ports are compiled from source by Emscripten on first build; subsequent builds reuse cached port libraries. Uses a custom shell template from `web/shell.html`.

The target also produces debug boot artifacts (`out/super-mango-debug.html` and companions) for direct debug HTML launches. The docs-site browser debug button uses the normal `super-mango.js` payload and passes `--debug` at boot.

For release confidence, the **GitHub Actions WebAssembly build is authoritative**. Some local/container Emscripten installs can fail before reaching Super Mango code when Emscripten 3.1.58 builds SDL_ttf's HarfBuzz port with newer Clang warnings (`-Wnontrivial-memcall`). If that host-toolchain issue appears locally, treat the CI `make web` + artifact smoke from `build.yml` and the Pages assembly smoke from `deploy.yml` as the trusted WASM verification path.

### `make test`

Builds and runs native regression harnesses for pure logic that does not require opening an SDL window.

```sh
make test
```

Current test binaries (14):

- `out/level-serializer-test`
- `out/level-validate-test`
- `out/runtime-load-test`
- `out/rail-test`
- `out/entity-utils-test`
- `out/collision-test`
- `out/phase-transition-test`
- `out/exporter-test`
- `out/editor-validation-test`
- `out/gameplay-damage-test`
- `out/gameplay-config-test`
- `out/gameplay-score-test`
- `out/game-overlay-test`
- `out/game-events-test`

### `make validate-levels`

Runs the Python TOML validator against every `levels/*.toml` file. It checks parsing, referenced asset paths, `next_phase` links, and array counts against the C `MAX_*` constants.

```sh
make validate-levels
```

### `make smoke`

Builds the game and editor, then runs every `levels/*.toml` with dummy SDL video/audio drivers for a bounded number of frames. It also runs the editor's headless smoke mode.

```sh
make smoke SMOKE_FRAMES=5 SMOKE_SEED=1
```

### `make scripted-smoke`

Builds the game and editor, then runs `tools/run_scripted_smoke.py` against every `levels/*.toml` for each seed in `SMOKE_SEEDS`. The runner writes deterministic replay scripts under `out/replays-smoke/` and passes sanitized replay names to the game with `--replay-script`, injecting SDL key events such as move-right, jump-right, and pause/resume while still using dummy SDL video/audio in CI.

```sh
make scripted-smoke SMOKE_FRAMES=5 SMOKE_SEEDS="1 7 23"
```

### `make sanitize`

Runs `make test` in a separate `out-sanitize/` tree with AddressSanitizer and UndefinedBehaviorSanitizer enabled.

```sh
make sanitize
```

### `make sanitize-smoke`

Builds sanitizer-instrumented native game/editor binaries in `out-sanitize/`, then runs the dummy SDL smoke pass against every TOML level plus the editor smoke mode. This complements `make sanitize` by covering SDL/resource startup paths, not just pure logic harnesses.

```sh
make sanitize-smoke SMOKE_FRAMES=5 SMOKE_SEED=1
```

### `make level-catalog`

Regenerates `docs/wiki/level-catalog.md` from `levels/*.toml` using `tools/generate_level_catalog.py`. Use this after adding levels or changing level metadata/content counts.

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

Builds native distribution archives under `dist/` using `tools/package_release.py`. The archive layout includes the game/editor binaries where applicable, assets, levels, license, and a short run README. CI uses this path for release artifacts.

```sh
make dist-native
```

### `make dist-wasm`

Builds the WebAssembly payload and packages the static browser files into a release archive. The verifier expects the HTML, JavaScript, `.wasm`, `.data`, `README.txt`, and `LICENSE` entries.

```sh
make dist-wasm
```

### `make docs-drift`

Runs the generated level-catalog freshness check, generated overlay-snapshot freshness check, `tools/check_docs_drift.py`, and `tools/check_roadmap_quality.py`. Together they compare Makefile test targets, README/agent/docs command summaries, source-file map entries, layer TOML snippets, workflow docs, runtime flags, key constants, level prose counts, TOML line endings, overlay text snapshots, local Emscripten caveats, and scripted-smoke wiring against the current repository.

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
bun run lint
NODE_ENV=production bun run build
```

### `make clean`

Removes all build artifacts.

```sh
make clean
```

Deletes legacy in-source `.o` / `.d` files from recognized source directories and removes `out/`.

---

## Prerequisites

### macOS (Apple Silicon / Intel)

```sh
# Install Homebrew if needed: https://brew.sh
brew install sdl2 sdl2_image sdl2_ttf sdl2_mixer

# Xcode Command Line Tools (provides clang and make)
xcode-select --install
```

SDL2 libraries are installed to `/opt/homebrew/` on Apple Silicon. `sdl2-config` resolves the correct paths automatically.

### Linux -- Debian / Ubuntu

```sh
sudo apt update
sudo apt install build-essential clang \
    libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl2-mixer-dev
```

### Linux -- Fedora / RHEL / CentOS

```sh
sudo dnf install clang make \
    SDL2-devel SDL2_image-devel SDL2_ttf-devel SDL2_mixer-devel
```

### Linux -- Arch Linux

```sh
sudo pacman -S clang make sdl2 sdl2_image sdl2_ttf sdl2_mixer
```

### Windows (MSYS2)

1. Install [MSYS2](https://www.msys2.org/)
2. Open the **MSYS2 UCRT64** terminal:

```sh
pacman -S mingw-w64-ucrt-x86_64-clang \
          mingw-w64-ucrt-x86_64-make \
          mingw-w64-ucrt-x86_64-SDL2 \
          mingw-w64-ucrt-x86_64-SDL2_image \
          mingw-w64-ucrt-x86_64-SDL2_ttf \
          mingw-w64-ucrt-x86_64-SDL2_mixer
```

3. Build:

```sh
cd /c/path/to/super-mango-editor
make
```

4. SDL2 DLLs must be in the same directory as the binary. Copy them from the MSYS2 prefix.

---

## CI/CD Pipelines

Four GitHub Actions workflows handle automated builds and docs checks:

| Workflow | File | Trigger | Purpose |
|----------|------|---------|---------|
| Build & Release | `build.yml` | Push to `main`, pull requests, `v*` tags, manual | Multi-platform native build, `make test`, `make validate-levels`, `make editor`, native game/editor smoke, Linux scripted smoke, WebAssembly build, WebAssembly artifact/package smoke; GitHub Release creation is limited to `v*` tags and manual dispatches |
| Docs | `docs.yml` | Docs pull requests, manual | `make docs-drift`, `bun install --frozen-lockfile`, `bun run lint`, `bun run build` under `docs/` |
| CodeQL | `codeql.yml` | Push/PR to `main`, weekly | Automated code security and quality analysis |
| Deploy | `deploy.yml` | Successful main Build & Release workflow | Builds docs, copies WebAssembly artifacts, and deploys `docs/out/` to GitHub Pages |

Native smoke uses dummy SDL drivers where supported: `./out/super-mango --level levels/00_sandbox_01.toml --smoke-test-frames 5` and `./out/super-mango-editor --smoke-test`. WebAssembly smoke asserts `out/super-mango.html`, `.js`, `.wasm`, and `.data` exist.

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
