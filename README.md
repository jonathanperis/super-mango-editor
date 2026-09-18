# super-mango-editor

> C11 + raylib platformer, visual level editor, and hands-on game-development school — playable in the browser via WebAssembly.

[![Build Check](https://github.com/jonathanperis/super-mango-editor/actions/workflows/build.yml/badge.svg?event=pull_request)](https://github.com/jonathanperis/super-mango-editor/actions/workflows/build.yml) [![Main Build](https://github.com/jonathanperis/super-mango-editor/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/jonathanperis/super-mango-editor/actions/workflows/build.yml) [![CodeQL](https://github.com/jonathanperis/super-mango-editor/actions/workflows/codeql.yml/badge.svg)](https://github.com/jonathanperis/super-mango-editor/actions/workflows/codeql.yml) [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

**[Play →](https://jonathanperis.github.io/super-mango-editor/)** | **[Learn →](https://jonathanperis.github.io/super-mango-editor/docs/learning-path/)** | **[Builder Manual →](https://jonathanperis.github.io/super-mango-editor/docs/)** | **[Releases →](https://github.com/jonathanperis/super-mango-editor/releases)**

---

## About

Super Mango is a C11/raylib platformer and sandbox school: play the game, inspect a running simulation, edit TOML worlds, and study the code that connects them. The campaign is Creator's Playground → Volcanic Depths 1 → Volcanic Depths 2. Six separate mechanics levels in `levels/labs/` support an eight-lab learning track. The standalone editor saves the same TOML data that the runtime loads. Rendering uses a 400×300 logical canvas, normally scaled to an 800×600 window. Native builds target macOS, Linux and Windows; Emscripten supplies browser play.

**Start learning:** [Sandbox School](docs/wiki/learning-path.md) · [Mechanics Museum](docs/wiki/mechanics-museum.md) · [Entity Walkthrough](docs/wiki/entity-walkthrough.md).

## Tech Stack

| Technology | Version | Purpose |
|-----------|---------|---------|
| C | C11 | Language standard (`clang -std=c11`) |
| raylib | 6.0, pinned source/checksum | Window, graphics, input, PNG/TrueType loading and audio; bundled GLFW on desktop |
| CMake | Available platform version | Out-of-source raylib dependency build; Make remains the application entry point |
| tomlc17 | R260821 + project patches | TOML v1.1 parser; [upstream provenance and patch inventory](vendor/tomlc17/README.md) |
| Emscripten | 6.0.9 in CI | WebAssembly compilation for browser play |

## Features

- 2D side-scrolling platformer with dynamic multi-screen TOML worlds, from the 4-screen sandbox to longer volcanic stages
- 32 render layers drawn back-to-front: parallax background, platforms, floor, enemies, player, fog, HUD, debug overlay
- Delta-time physics with explicit numerical-integration tradeoffs; `make timing-lab` compares variable and fixed steps
- Six enemy types: spiders, jumping spiders, birds, faster birds, fish, faster fish
- Seven hazard types: spike rows, spike blocks, spike platforms, circular saws, axe traps, blue flames, fire flames
- Collectibles: coins (100 pts each, bonus life by score threshold), star yellow, star green, star red health pickups, end-of-level last star
- Climbable vines, ladders, and ropes; three bouncepad variants (small, medium, high)
- TOML-only level workflow: `levels/campaigns/main.toml` orders the three campaign stages; `--level path/to/level.toml` directly opens a campaign, museum or custom level
- Authored `[[checkpoints]]` records give a level explicit respawn positions; no records preserves legacy automatic screen-boundary respawns
- Pause, game-over, and end-of-level overlays: terminal action rows use Up/Down or D-pad to select, Enter/Space/Start (A also confirms) to confirm, and Esc/Back (B also exits) to exit
- Completion actions: Next Level when `next_phase` exists, Replay, Level Select, Exit; game-over actions: Retry, Level Select, Exit
- Campaign-driven native start menu and level select; HUD (hearts/lives/score), lives system, invincibility blink on damage
- Browser Replay stores the current TOML path in session storage, tears down the active WebAssembly session, reloads the page, and boots that level again
- Keyboard, hot-plug gamepad and browser touch controls; F1 settings with remapping, audio, dead-zone, native scale, high-contrast and reduced-motion options
- Local settings and per-level best results; `--continue` opens the last played stage, while debug/smoke/playtest sessions remain profile-isolated
- Debug inspector: FPS/frame interval, memory, hitboxes, velocity/state/contact display, freeze/step/slow motion, live movement tuning and explicit experiment capture/replay
- Builds natively on macOS, Linux, and Windows; WebAssembly build via Emscripten

## Level Editor

Super Mango includes a standalone visual level editor built with C11 and raylib. The editor lets you create and edit levels with a point-and-click interface, then save and load TOML files used directly by the game.

Editor features:

- Scrollable canvas with zoom, grid snapping, and select/place/delete tools
- Entity palette with world geometry and game objects, including authored checkpoint markers: platforms, enemies, hazards, collectibles, and surfaces
- Per-entity property editing (position, size, speed, animation, behavior)
- TOML serialization (save/load `.toml` level files)
- Undo/redo history, copy/paste, recent files, autosave, and dirty-state indicators
- Validation status for the active level; validation errors block save and playtest
- Native file dialogs
- Bounded rendered smoke mode for CI (`--smoke-test`)

Build and run the editor:

```sh
make editor       # build the editor binary into out/
make run-editor   # build and run the editor
```

## Development

Start with [Sandbox School](docs/wiki/learning-path.md), then use the [Developer Guide](docs/wiki/developer-guide.md) for conventions and ownership. [Level Design](docs/wiki/level-design.md) documents TOML; [Assets](docs/wiki/assets.md) covers sprite tools. [Asset Inventory](docs/wiki/asset-inventory.md) records the raw bundle budget and [Asset Provenance](docs/wiki/asset-provenance.md) distinguishes code/media licenses.

## Getting Started

### Prerequisites

A C11-compatible compiler (`clang` or `gcc`), `make`, CMake and Python 3.11+.
The first build downloads and verifies the pinned raylib 6.0 source archive;
no system raylib installation is required. See [dependency provenance](vendor/raylib/README.md).

**macOS:**

```sh
brew install cmake
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

**WebAssembly:** Install the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) at **6.0.9** and ensure `emcc` and `emcmake` are on `PATH`. The same toolchain builds raylib's Web backend and both game variants. GitHub CI is the authoritative WASM release verification; local toolchain failures must be reported separately from application failures.

**Rendered tests:** desktop hidden windows still require a graphics context and
audio device. Linux CI uses Xvfb/Mesa and a PulseAudio null sink. On display-less
hosts, use the explicit test backend: `make test OUTDIR=out/headless RAYLIB_PLATFORM=memory`.
It renders in memory with null audio; OS resize/clipboard/device behavior remains
a desktop check. Release packaging rejects the Memory backend.

### Quick Start

Full verification also needs Python 3.11+ and Node.js. Docs development requires
Node.js 22.12+ and Bun with the frozen `docs/bun.lock` dependency set (CI pins
Node **26.9.0** and Bun **1.4.2**); see
[website maintenance](docs/README.md).

```sh
make CC=clang                         # build the game binary into out/
make run CC=clang                     # build and run
make run-debug CC=clang               # build and run with debug overlay
make run-level CC=clang LEVEL=levels/labs/01_collision.toml      # run a focused lab
make run-level-debug CC=clang LEVEL=levels/labs/01_collision.toml # inspect a lab
make builder CC=clang                 # build game and editor together
make debug CC=clang                   # -g -O0 binaries in out/debug/
make release CC=clang                 # -O2 binaries in out/release/
make timing-lab                       # quantitative timestep experiment
make editor CC=clang                  # build the level editor
make run-editor CC=clang              # build and run the level editor
make test CC=clang                    # 15 native regression tests (binaries) plus Python/JavaScript host checks
make validate-levels                  # validate campaign, root levels and levels/labs/
make web                              # build to WebAssembly (requires Emscripten)
make clean                            # remove default out/ and dist/ build artifacts
```

> The Makefile replaces GNU Make's built-in `CC=cc` with clang; explicit `CC=gcc`
> remains supported. After moving source files between directories, use a fresh
> `OUTDIR` or clean your old build's generated dependencies.

In debug mode: **F2** freezes, **F3** steps once, **F4** changes speed, **F6** selects
a movement property, **-/+** tunes it and **F7** resets it. **F8** restarts/records;
**F9** exports a capture; **F10** cycles inspected entities. Replay with `--level PATH --experiment CAPTURE.toml`.
Debug/playtest sessions do not touch personal profiles. See the museum guide for
capture limits and pause ownership.

Or just **[play in your browser](https://jonathanperis.github.io/super-mango-editor/)** -- no build required. Full project documentation is available at the **[docs site](https://jonathanperis.github.io/super-mango-editor/docs/)**.

Useful docs routes:

- **[Controls & Input](https://jonathanperis.github.io/super-mango-editor/docs/controls/)** — keyboard, gamepad, browser/WASM, replay, smoke, and runtime flag reference.
- **[Testing & Smoke Matrix](https://jonathanperis.github.io/super-mango-editor/docs/testing/)** — which local/CI checks to run for each kind of change.
- **[Level Design — TOML Reference](https://jonathanperis.github.io/super-mango-editor/docs/level-design/)** — full level schema, including optional `[physics]` tuning.
- **[Release Checklist](https://jonathanperis.github.io/super-mango-editor/docs/release-checklist/)** — source, docs, WebAssembly, archive, CI, and Pages gates before shipping.

### Release Downloads

Browse [published releases](https://github.com/jonathanperis/super-mango-editor/releases) and check each release's asset list. Older releases predate the current builder packaging; use `make builder` for the current editor and learning labs. The website tracks successful `main` builds independently of tagged releases.

Releases built by the current workflow (a `v*` tag or manual dispatch on `main`) contain native builder archives with both `super-mango` and `super-mango-editor`, playable assets, campaign/lab levels, and third-party notices. Run from the extracted folder. raylib is linked statically; native OS graphics/audio support remains required. Linux dialogs need zenity. Windows bundles both executables' required non-system runtime DLLs and available package notices. `unused/` assets stay in the source checkout. WebAssembly archives contain both normal/debug HTML/JS/WASM/data outputs; serve them with a static HTTP server. Build with `make web`, then package those verified outputs with `make dist-wasm`.

## Project Structure

```
super-mango-editor/
├── Makefile                          Application build, pinned raylib bootstrap, ad-hoc codesign
├── levels/                           TOML level definitions
│   ├── labs/                        Six focused learning levels
│   ├── 00_sandbox_01.toml           Creator's Playground; first campaign level
│   ├── 01_lugio_01.toml             Level data loaded at runtime
│   ├── 02_lugio_02.toml             Level data loaded at runtime
│   └── campaigns/main.toml           v1 ordered campaign manifest for the native selector
├── src/                              C source files and headers
│   ├── main.c                        CLI entry point; AppSession owns raylib lifetime
│   ├── game.h                        Shared GameState/constants declarations
│   ├── collectibles/                  Pickup items
│   │   ├── coin.h / .c               Coin (100 pts; bonus life at score threshold)
│   │   ├── star_yellow.h / .c        Yellow star health pickup
│   │   ├── star_green.h / .c         Green star health pickup
│   │   ├── star_red.h / .c           Red star health pickup
│   │   └── last_star.h / .c          End-of-level star
│   ├── collision/                     Gameplay collision and damage passes
│   ├── core/                          Runtime lifecycle, window/timing/resources, update, camera, checkpoint, completion, overlay, actor/hazard helpers
│   │   ├── debug.h / .c              Debug overlay (FPS, CPU, memory, hitboxes, event log)
│   │   ├── game_lifecycle.c          game_init / game_cleanup orchestration
│   │   ├── game_loop.c               Main native/WebAssembly frame loop
│   │   ├── game_update.h / .c        Top-level update orchestration
│   │   └── game_* helpers            Window, resources, timing, camera, checkpoint, overlay, actors, hazards, surfaces
│   ├── editor/                        Standalone visual level editor
│   │   ├── editor_main.c             Editor entry point
│   │   ├── editor.h / .c             Editor state and high-level glue
│   │   ├── canvas/palette/properties/tools/ui modules
│   │   ├── editor_frame/events/chrome/panels/layout/textures modules
│   │   ├── editor_files/session/playtest/clipboard/validation modules
│   │   ├── shared serializer/UI consumers
│   │   ├── file_dialog.h / .c        Native file dialogs
│   │   └── undo*.h / .c              Undo/redo history and operation application
│   ├── shared/                        TOML serializer, atomic UTF-8 I/O, shared UI
│   ├── effects/                       Visual effects
│   │   ├── fog.h / .c                Fog overlay
│   │   ├── parallax.h / .c           Multi-layer scrolling background
│   │   └── water.h / .c              Animated water strip
│   ├── entities/                      Enemies
│   │   ├── spider.h / .c             Spider (ground patrol)
│   │   ├── jumping_spider.h / .c     Jumping spider
│   │   ├── bird.h / .c               Bird (sine-wave sky patrol)
│   │   ├── faster_bird.h / .c        Fast bird
│   │   ├── fish.h / .c               Fish (jumping water patrol)
│   │   └── faster_fish.h / .c        Fast fish
│   ├── hazards/                       Damaging obstacles
│   │   ├── spike.h / .c              Ground spike rows
│   │   ├── spike_block.h / .c        Rail-riding spike
│   │   ├── spike_platform.h / .c     Elevated spike
│   │   ├── circular_saw.h / .c       Rotating saw
│   │   ├── axe_trap.h / .c           Swinging axe
│   │   └── blue_flame.h / .c         Blue flame / fire flame
│   ├── input/                         raylib device sampling, semantic commands, browser bridge and replay injection
│   ├── levels/                        Level system
│   │   ├── level.h                    Shared level definitions (LevelDef struct)
│   │   ├── level_loader.h / .c       TOML level loading and switching
│   │   ├── level_path/resources/session/physics helpers
│   │   ├── phase_transition.h / .c   next_phase resolution and progress helpers
│   │   ├── level_validate.c          LevelDef count validation
│   ├── player/                        Player module split into lifecycle, input, motion, jump, climb, surface, and animation files
│   ├── render/                        `game_render` frame order and `render_overlay` foreground/overlay helpers
│   ├── screens/                       Game screens
│   │   ├── start_menu.h / .c         Start menu
│   │   └── hud.h / .c                HUD (hearts, lives, score)
│   └── surfaces/                      Traversable objects
│       ├── platform.h / .c           One-way platform pillars (9-slice)
│       ├── float_platform.h / .c     Hovering platforms (static/crumble/rail)
│       ├── bridge.h / .c             Crumble walkways
│       ├── bouncepad.h / .c          Bouncepad base + 3 variants (small/medium/high)
│       ├── rail.h / .c               Rail path system
│       ├── vine.h / .c               Climbable vine
│       ├── ladder.h / .c             Climbable ladder
│       └── rope.h / .c               Climbable rope
├── assets/                            All game assets
│   ├── sprites/                       PNG sprites and tilesets
│   │   ├── backgrounds/              Parallax background layers
│   │   ├── foregrounds/              Fog and foreground overlays
│   │   ├── collectibles/             Coins, stars
│   │   ├── entities/                 Enemy sprite sheets
│   │   ├── hazards/                  Hazard sprite sheets
│   │   ├── levels/                   Floor tiles and level-specific assets
│   │   ├── player/                   Player sprite sheet
│   │   ├── screens/                  Menu and HUD sprites
│   │   ├── surfaces/                 Platforms, bridges, vines, ladders, ropes
│   │   └── unused/                   Reserve assets from asset pack
│   ├── sounds/                        WAV sound effects
│   │   ├── collectibles/             Pickup sounds
│   │   ├── entities/                 Enemy sounds
│   │   ├── hazards/                  Hazard sounds
│   │   ├── levels/                   Level music and ambient
│   │   ├── player/                   Player action sounds
│   │   ├── screens/                  Menu sounds
│   │   ├── surfaces/                 Surface interaction sounds
│   │   └── unused/                   Reserve sound files
│   └── fonts/                         TrueType fonts
│       └── round9x13.ttf            Debug overlay font
├── vendor/                            Vendored third-party libraries
│   └── tomlc17/                      TOML v1.1 parser (tomlc17.c/.h)
├── tests/                             Native regression test harnesses
├── docs/                              Astro GitHub Pages documentation site
├── web/                               Emscripten shell template
└── .github/workflows/                 CI/CD pipelines
    ├── build.yml                      Build checks (PRs/main) + tagged/manual releases
    ├── codeql.yml                     Code security analysis
    ├── docs.yml                       Docs lint/build checks
    └── deploy.yml                     GitHub Pages deployment
```

## Project Documents

| File | Purpose |
|------|---------|
| `PRODUCT.md` | Product direction, player promise, and feature framing. |
| `DESIGN.md` | Visual/UX design notes for the arcade-cabinet presentation. |
| `docs/wiki/developer-guide.md` | Coding conventions, entity integration, resource ownership, and verification. |
| `docs/README.md` | Website setup, content ownership, generated facts and deployment. |
| `docs/AUDIT.md` | Dated documentation audit, verification and enhancement follow-ups. |
| `CODEOWNERS` | Review ownership hints for GitHub. |

These files complement the public GH Pages manual. If they disagree with code, update the docs and source-backed checks together.

## CI/CD

Four GitHub Actions workflows:

| Workflow | File | Trigger | Purpose |
|----------|------|---------|---------|
| Build & Release | `build.yml` | Push to `main`, pull requests, `v*` tags, manual | Linux x86_64, macOS arm64, Windows x86_64 and WebAssembly builds; releases only for `v*` tags or manual dispatch on `main` |
| Docs | `docs.yml` | Relevant pull requests, manual | Source/content drift, frozen Bun install, Astro lint/build, dependency audit and built-site checks |
| CodeQL | `codeql.yml` | Push/PR to `main`, weekly, manual | C/C++ and GitHub Actions security-and-quality analysis |
| Deploy Pages | `deploy.yml` | Successful same-repository main push/manual Build & Release run | Builds/checks docs at the artifact's commit, copies matching WebAssembly files and deploys Pages |

The native matrix builds desktop game/editor binaries and archives. Linux runs GLFW tests/rendered smoke with a virtual display and audio sink; macOS/Windows run the same logical/resource suite and rendered smoke using a separate Memory test build. Additional checks include desktop sanitizers and scripted replay smoke on Linux, plus `make validate-levels` on Linux/macOS. The WebAssembly leg builds and checks normal/debug artifacts and their archive. Releases upload assets to a draft before publishing it. Docs and Pages gates validate the matching source/content and WebAssembly artifacts.

## License

Project source: MIT — see [LICENSE](LICENSE). Third-party code and media have
separate terms; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
