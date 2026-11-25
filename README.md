# super-mango-editor

> C11 + raylib platformer, visual level editor, and hands-on game-development school — playable in the browser via WebAssembly.

[![Build Check](https://github.com/jonathanperis/super-mango-editor/actions/workflows/build.yml/badge.svg?event=pull_request)](https://github.com/jonathanperis/super-mango-editor/actions/workflows/build.yml) [![Main Build](https://github.com/jonathanperis/super-mango-editor/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/jonathanperis/super-mango-editor/actions/workflows/build.yml) [![CodeQL](https://github.com/jonathanperis/super-mango-editor/actions/workflows/codeql.yml/badge.svg)](https://github.com/jonathanperis/super-mango-editor/actions/workflows/codeql.yml) [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

**[Play →](https://super-mango-editor.jonathanperis.tech/)** | **[Learn →](https://super-mango-editor.jonathanperis.tech/docs/learning-path/)** | **[Builder Manual →](https://super-mango-editor.jonathanperis.tech/docs/)** | **[Releases →](https://github.com/jonathanperis/super-mango-editor/releases)**

---

## About

Super Mango is a C11/raylib platformer and a place to learn C by taking a real
game apart: play it, freeze and step the running simulation, edit its TOML
worlds, and read the code that connects them. The campaign is Creator's
Playground → Volcanic Depths 1 → Volcanic Depths 2. Six small mechanics levels
in `levels/labs/` go with an eight-lab learning track. The standalone editor
saves the same TOML files the game loads. Native builds run on macOS, Linux and
Windows; Emscripten builds the browser version.

**Start learning:** [Sandbox School](https://super-mango-editor.jonathanperis.tech/docs/learning-path/) · [Debugging C](https://super-mango-editor.jonathanperis.tech/docs/debugging-c/) · [C in This Codebase](https://super-mango-editor.jonathanperis.tech/docs/c-concepts/) · [Mechanics Museum](https://super-mango-editor.jonathanperis.tech/docs/mechanics-museum/) · [Entity Walkthrough](https://super-mango-editor.jonathanperis.tech/docs/entity-walkthrough/)

The source is part of the lesson. Explicit update/draw steps, simple loops and
comments about units, ownership and library calls are intentional. I favour
code a learner can trace over a shorter version that hides a step.

## Tech Stack

| Technology | Version | Purpose |
|-----------|---------|---------|
| C | C11 | Language standard (`clang -std=c11`) |
| raylib | 6.2, pinned source/checksum | Window, graphics, input, PNG loading, built-in text font and audio; bundled GLFW on desktop |
| CMake | Available platform version | Out-of-source raylib dependency build; Make remains the application entry point |
| tomlc17 | R260821 + project patches | TOML v1.1 parser; [upstream provenance and patch inventory](vendor/tomlc17/README.md) |
| Emscripten | 6.0.13 in CI | WebAssembly compilation for browser play |

## What is in it

A side-scrolling platformer with multi-screen TOML worlds, enemies, hazards,
climbable surfaces and authored checkpoints; menus and settings that work with
a keyboard, a gamepad or touch; a debug inspector that freezes, steps and
replays the simulation; and a standalone level editor that saves the same
files the game loads. The [manual's overview](https://super-mango-editor.jonathanperis.tech/docs/)
has the full feature list, and each feature has its own page there.

## Getting Started

### Prerequisites

A C11 compiler (`clang` or `gcc`), `make`, CMake, Python 3 (3.12+
recommended) and Node.js 22.12+ (CI uses Node 26.9.0), which `make test` needs for
its six JavaScript host-contract tests. The first build downloads and verifies
the pinned raylib 6.2 source; no system raylib is needed. See [dependency provenance](vendor/raylib/README.md).

**macOS:**

```sh
brew install cmake python node
xcode-select --install   # provides clang and make
```

**Linux (Debian/Ubuntu):**

```sh
sudo apt update
sudo apt install build-essential clang cmake python3 libgl1-mesa-dev libx11-dev \
    libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev zenity
# Node.js 22.12+: distribution packages are often older; see https://nodejs.org/
```

**Windows (MSYS2 UCRT64):**

```sh
pacman -S make mingw-w64-ucrt-x86_64-clang mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-python \
          mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-make
# plus Node.js 22.12+ from https://nodejs.org/ on PATH
```

**WebAssembly:** install the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) at **6.0.13** with `emcc` and `emcmake` on `PATH`. GitHub CI is the authoritative WASM release verification; report local toolchain failures separately from application failures.

`make doctor` reports which of these are missing before the first build.
The docs tools need Python 3.11+; the docs site also needs Bun
(see [website maintenance](docs/README.md)). On a machine without a display or
sound card, `make test OUTDIR=out/headless RAYLIB_PLATFORM=memory` runs the
tests on raylib's in-memory backend.

### Quick Start

```sh
make run                              # build and play
make run-editor                       # build and open the level editor
make run-level-debug LEVEL=levels/labs/01_collision.toml  # a lab, with the inspector
make debug                            # -g -O0 game and editor in out/debug/ for lldb/gdb
make test                             # 18 native regression tests (binaries) plus Python/JavaScript host checks
make sanitize                         # the tests again under AddressSanitizer/UBSan
make docs-drift                       # check the manual against the code
make help                             # list every target with a one-line description
```

The Makefile builds with clang by default; add `CC=gcc` to any of these to use
GCC instead. The [Build System](https://super-mango-editor.jonathanperis.tech/docs/build-system/) page lists every target and flag, and
[Testing](https://super-mango-editor.jonathanperis.tech/docs/testing/) says which checks to run for which change. The debug
inspector keys are in [Controls](https://super-mango-editor.jonathanperis.tech/docs/controls/#debug-inspector-keys); debug and
playtest runs never touch your saved profile.

Or just **[play in your browser](https://super-mango-editor.jonathanperis.tech/)**, no build required.

### Release Downloads

[Releases](https://github.com/jonathanperis/super-mango-editor/releases) built by
the current workflow contain a native archive per platform with both
`super-mango` and `super-mango-editor`, the assets, campaign and lab levels, and
third-party notices; run them from the extracted folder. A WebAssembly archive
holds the normal and debug browser builds; serve it with any static HTTP server.
Older releases predate this packaging. The website follows successful `main`
builds, not tags. Details are in the [Release Checklist](https://super-mango-editor.jonathanperis.tech/docs/release-checklist/).

## Project Structure

```
super-mango-editor/
├── src/          The game and editor in C: core/, player/, entities/, hazards/,
│                 collectibles/, surfaces/, levels/, render/, input/, screens/,
│                 effects/, collision/, shared/ (used by both), editor/
├── levels/       TOML levels, labs/ for the learning levels, campaigns/main.toml
├── assets/       Sprites and the QOA sounds generated by tools/gen_sounds.py
├── labs/         Small standalone C files with planted bugs for Debugging C
├── tests/        Native, fuzz, Python and Node tests and their fixtures
├── tools/        Build, validation, generator, packaging and docs-check scripts
├── vendor/       Pinned raylib source/checksum, its patches and the SDL gamepad
│                 database (raylib/gamecontrollerdb/), raygui's icon set
│                 (raygui/raygui_icons.h) and the tomlc17 parser
├── web/          Emscripten shell (shell.html), the boot script both pages share
│                 (boot.js, debug-boot.js), the browser audio unlock
│                 (audio-unlock.js), touch controls and keyboard scoping
├── docs/         The Astro website; manual pages in docs/wiki/
└── .github/      CI workflows and SECURITY_TRIAGE.md
```

[Source Files](https://super-mango-editor.jonathanperis.tech/docs/source-files/) describes every `.c` and `.h` file.

## Project Documents

| File | Purpose |
|------|---------|
| `PRODUCT.md` | Product direction, player promise, and feature framing. |
| `DESIGN.md` | How the website looks: built from the game's own sprites, palette and fonts. |
| `docs/wiki/` | The manual, published on the [docs site](https://super-mango-editor.jonathanperis.tech/docs/). |
| `docs/README.md` | Website setup, content ownership, generated facts and deployment. |
| `docs/archive/`, `.specs/archive/` | Historical, dated documents: two audit reports, the old arcade-cabinet website plan and the SDL2-era working specification. Not current status; each folder's README says what they are. |
| `SECURITY.md` | Vulnerability reporting, scope and the web build's shared-origin model. |
| `THIRD_PARTY_NOTICES.md` | Third-party code and media notices. |
| `CODEOWNERS` | Review ownership hints for GitHub. |
| `CONTRIBUTING.md` | Branch shape, which checks are required, and the headless test command. |
| `CHANGELOG.md` | The longer history of each larger piece of work; `.specs/project/STATE.md` keeps one line per item. |

If any of these disagree with the code, the code wins: update the docs and the
checks that guard them together.

## CI/CD

`build.yml` and `codeql.yml` run on every pull request and every push to
`main`; `docs.yml`, `fuzz.yml` and `static-analysis.yml` run on narrower
triggers. `build.yml` builds and tests the game and editor on Linux, macOS,
Windows and WebAssembly, and on `main` publishes the website. The
[Build System](https://super-mango-editor.jonathanperis.tech/docs/build-system/#cicd-pipelines)
page is the one place that lists every workflow, its triggers and its jobs,
and [CONTRIBUTING.md](CONTRIBUTING.md) lists the checks a pull request needs.

## License

Project source: MIT — see [LICENSE](LICENSE). Third-party code and media have
separate terms; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
