# super-mango-editor

> C11 + raylib platformer, visual level editor, and hands-on game-development school — playable in the browser via WebAssembly.

[![Build Check](https://github.com/jonathanperis/super-mango-editor/actions/workflows/build.yml/badge.svg?event=pull_request)](https://github.com/jonathanperis/super-mango-editor/actions/workflows/build.yml) [![Main Build](https://github.com/jonathanperis/super-mango-editor/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/jonathanperis/super-mango-editor/actions/workflows/build.yml) [![CodeQL](https://github.com/jonathanperis/super-mango-editor/actions/workflows/codeql.yml/badge.svg)](https://github.com/jonathanperis/super-mango-editor/actions/workflows/codeql.yml) [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

**[Play →](https://jonathanperis.github.io/super-mango-editor/)** | **[Learn →](https://jonathanperis.github.io/super-mango-editor/docs/learning-path/)** | **[Builder Manual →](https://jonathanperis.github.io/super-mango-editor/docs/)** | **[Releases →](https://github.com/jonathanperis/super-mango-editor/releases)**

---

## About

Super Mango is a C11/raylib platformer and a place to learn C by taking a real
game apart: play it, freeze and step the running simulation, edit its TOML
worlds, and read the code that connects them. The campaign is Creator's
Playground → Volcanic Depths 1 → Volcanic Depths 2. Six small mechanics levels
in `levels/labs/` go with an eight-lab learning track. The standalone editor
saves the same TOML files the game loads. Native builds run on macOS, Linux and
Windows; Emscripten builds the browser version.

**Start learning:** [Sandbox School](https://jonathanperis.github.io/super-mango-editor/docs/learning-path/) · [Debugging C](https://jonathanperis.github.io/super-mango-editor/docs/debugging-c/) · [C in This Codebase](https://jonathanperis.github.io/super-mango-editor/docs/c-concepts/) · [Mechanics Museum](https://jonathanperis.github.io/super-mango-editor/docs/mechanics-museum/) · [Entity Walkthrough](https://jonathanperis.github.io/super-mango-editor/docs/entity-walkthrough/)

The source is part of the lesson. Explicit update/draw steps, simple loops and
comments about units, ownership and library calls are intentional. We favour
code a learner can trace over a shorter version that hides a step.

## Tech Stack

| Technology | Version | Purpose |
|-----------|---------|---------|
| C | C11 | Language standard (`clang -std=c11`) |
| raylib | 6.0, pinned source/checksum | Window, graphics, input, PNG loading, built-in text font and audio; bundled GLFW on desktop |
| CMake | Available platform version | Out-of-source raylib dependency build; Make remains the application entry point |
| tomlc17 | R260821 + project patches | TOML v1.1 parser; [upstream provenance and patch inventory](vendor/tomlc17/README.md) |
| Emscripten | 6.0.9 in CI | WebAssembly compilation for browser play |

## What is in it

- A side-scrolling platformer with multi-screen TOML worlds, 32 render layers and fixed 1/60 s physics steps (`make timing-lab` shows why)
- Six enemy types, seven hazard types, coins (score and bonus lives), health stars and an end-of-level star
- Vines, ladders, ropes, three bouncepad sizes, crumbling bridges and float platforms
- Authored `[[checkpoints]]`, or automatic screen-boundary respawns for levels without them
- Pause, game-over and completion overlays: Up/Down or D-pad selects, Enter/Space/Start confirms (A also confirms), Esc/Back exits (B also exits)
- Keyboard, hot-plug gamepad and browser touch controls; F1 settings with remapping, audio, dead zone, window scale, high contrast and reduced motion
- A debug inspector to freeze, step, slow down and tune the simulation, and to record and replay experiments
- A standalone level editor with undo/redo, copy/paste, validation that blocks bad saves, autosave and one-key playtesting

The [manual](https://jonathanperis.github.io/super-mango-editor/docs/) covers each of these in detail.

## Getting Started

### Prerequisites

A C11 compiler (`clang` or `gcc`), `make`, CMake and Python 3 (3.12+
recommended). The first build downloads and verifies the pinned raylib 6.0
source; no system raylib is needed. See [dependency provenance](vendor/raylib/README.md).

**macOS:**

```sh
brew install cmake python
xcode-select --install   # provides clang and make
```

**Linux (Debian/Ubuntu):**

```sh
sudo apt update
sudo apt install build-essential clang cmake python3 libgl1-mesa-dev libx11-dev \
    libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev zenity
```

**Windows (MSYS2 UCRT64):**

```sh
pacman -S make mingw-w64-ucrt-x86_64-clang mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-python \
          mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-make
```

**WebAssembly:** install the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) at **6.0.9** with `emcc` and `emcmake` on `PATH`. GitHub CI is the authoritative WASM release verification; report local toolchain failures separately from application failures.

The docs tools need Python 3.11+; the docs site needs Node.js 22.12+ and Bun
(see [website maintenance](docs/README.md)). On a machine without a display or
sound card, `make test OUTDIR=out/headless RAYLIB_PLATFORM=memory` runs the
tests on raylib's in-memory backend.

### Quick Start

```sh
make run CC=clang                     # build and play
make run-editor CC=clang              # build and open the level editor
make run-level-debug CC=clang LEVEL=levels/labs/01_collision.toml  # a lab, with the inspector
make debug CC=clang                   # -g -O0 game and editor in out/debug/ for lldb/gdb
make test CC=clang                    # 16 native regression tests (binaries) plus Python/JavaScript host checks
make sanitize CC=clang                # the tests again under AddressSanitizer/UBSan
make docs-drift                       # check the manual against the code
make help                             # list every target with a one-line description
```

The [Build System](https://jonathanperis.github.io/super-mango-editor/docs/build-system/) page lists every target and flag, and
[Testing](https://jonathanperis.github.io/super-mango-editor/docs/testing/) says which checks to run for which change. The debug
inspector keys are in [Controls](https://jonathanperis.github.io/super-mango-editor/docs/controls/#debug-inspector-keys); debug and
playtest runs never touch your saved profile.

Or just **[play in your browser](https://jonathanperis.github.io/super-mango-editor/)**, no build required.

### Release Downloads

[Releases](https://github.com/jonathanperis/super-mango-editor/releases) built by
the current workflow contain a native archive per platform with both
`super-mango` and `super-mango-editor`, the assets, campaign and lab levels, and
third-party notices; run them from the extracted folder. A WebAssembly archive
holds the normal and debug browser builds; serve it with any static HTTP server.
Older releases predate this packaging. The website follows successful `main`
builds, not tags. Details are in the [Release Checklist](https://jonathanperis.github.io/super-mango-editor/docs/release-checklist/).

## Project Structure

```
super-mango-editor/
├── src/          The game and editor in C: core/, player/, entities/, hazards/,
│                 collectibles/, surfaces/, levels/, render/, input/, screens/,
│                 effects/, collision/, shared/ (used by both), editor/
├── levels/       TOML levels, labs/ for the learning levels, campaigns/main.toml
├── assets/       Sprites and the WAV files generated by tools/gen_sounds.py
├── labs/         Small standalone C files with planted bugs for Debugging C
├── tests/        Native, fuzz, Python and Node tests and their fixtures
├── tools/        Build, validation, generator, packaging and docs-check scripts
├── vendor/       Pinned raylib source/checksum and the tomlc17 parser
├── web/          Emscripten shell, touch controls and keyboard scoping
├── docs/         The Astro website; manual pages in docs/wiki/
└── .github/      CI workflows and SECURITY_TRIAGE.md
```

[Source Files](https://jonathanperis.github.io/super-mango-editor/docs/source-files/) describes every `.c` and `.h` file.

## Project Documents

| File | Purpose |
|------|---------|
| `PRODUCT.md` | Product direction, player promise, and feature framing. |
| `DESIGN.md` | How the website looks: built from the game's own sprites, palette and fonts. |
| `docs/wiki/` | The manual, published on the [docs site](https://jonathanperis.github.io/super-mango-editor/docs/). |
| `docs/README.md` | Website setup, content ownership, generated facts and deployment. |
| `docs/AUDIT.md`, `docs/AUDIT_IMPLEMENTATION.md` | Historical, dated audit reports (not current status). |
| `SECURITY.md` | Vulnerability reporting, scope and the web build's shared-origin model. |
| `THIRD_PARTY_NOTICES.md` | Third-party code and media notices. |
| `CODEOWNERS` | Review ownership hints for GitHub. |

If any of these disagree with the code, the code wins: update the docs and the
checks that guard them together.

## CI/CD

Three workflows run on pull requests and on `main`:

- **`build.yml`** builds and tests the game and editor on Linux, macOS and
  Windows with warnings as errors, renders every level briefly, and builds the
  WebAssembly version. It also runs the sanitizers (with the fuzz seeds) and
  scripted replay smoke on Linux. An always-on `Docs drift` job runs
  `make docs-drift`. On `main` it publishes the website; on a `v*` tag
  or a manual release run it adds checksums and build provenance and publishes
  the release archives.
- **`docs.yml`** checks the manual against the code, then lints, builds and
  link-checks the website.
- **`codeql.yml`** runs CodeQL security and quality analysis on the C code, the
  workflows, the Python tools and the JavaScript/TypeScript.

The [Build System](https://jonathanperis.github.io/super-mango-editor/docs/build-system/#cicd-pipelines) page has the job-by-job detail.

## License

Project source: MIT — see [LICENSE](LICENSE). Third-party code and media have
separate terms; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
