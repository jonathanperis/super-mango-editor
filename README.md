# super-mango-game

> 2D side-scrolling platformer written in C using SDL2 -- browser-playable via WebAssembly

[![Build Check](https://github.com/jonathanperis/super-mango-game/actions/workflows/build.yml/badge.svg?event=pull_request)](https://github.com/jonathanperis/super-mango-game/actions/workflows/build.yml) [![Main Release](https://github.com/jonathanperis/super-mango-game/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/jonathanperis/super-mango-game/actions/workflows/build.yml) [![CodeQL](https://github.com/jonathanperis/super-mango-game/actions/workflows/codeql.yml/badge.svg)](https://github.com/jonathanperis/super-mango-game/actions/workflows/codeql.yml) [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

**[Live demo →](https://jonathanperis.github.io/super-mango-game/)** | **[Documentation →](https://jonathanperis.github.io/super-mango-game/docs/)**

---

## About

Super Mango is a 2D side-scrolling platformer built in C11 with SDL2, designed as an educational project with well-commented source code that can be read as a learning resource for C + SDL2 game development. The game features a multi-screen forest stage with parallax backgrounds, one-way platforms, floating platforms, crumble bridges, floor gaps, collectible coins, climbable vines/ladders/ropes, six enemy types, seven hazard types, bouncepads, animated water, fog overlays, a start menu, and an HUD with hearts, lives, and score. Levels are defined in TOML and loaded at runtime, with a standalone visual level editor for creating and editing levels. It renders at a 400x300 logical resolution scaled 2x to an 800x600 window for a chunky pixel-art look, with frame-rate-independent movement via delta-time physics. The project builds natively on macOS, Linux, and Windows, and compiles to WebAssembly via Emscripten for browser play.

## Tech Stack

| Technology | Version | Purpose |
|-----------|---------|---------|
| C | C11 | Language standard (`clang -std=c11`) |
| SDL2 | 2.x | Window, renderer, input, events, timing |
| SDL2_image | 2.x | PNG texture loading |
| SDL2_ttf | 2.x | TrueType font rendering |
| SDL2_mixer | 2.x | Sound effects and music |
| tomlc17 | vendored | TOML v1.1 parser for level definitions |
| Emscripten | latest | WebAssembly compilation for browser play |

## Features

- 2D side-scrolling platformer with a multi-screen forest stage (1600px world, 4 screens wide)
- 32 render layers drawn back-to-front: parallax background, platforms, floor, enemies, player, fog, HUD, debug overlay
- Delta-time physics for frame-rate-independent movement at 60 FPS (VSync + manual fallback)
- Six enemy types: spiders, jumping spiders, birds, faster birds, fish, faster fish
- Seven hazard types: spike rows, spike blocks, spike platforms, circular saws, axe traps, blue flames, fire flames
- Collectibles: coins (100 pts each, 3 coins restore a heart), star yellow, star green, star red health pickups, end-of-level last star
- Climbable vines, ladders, and ropes; three bouncepad variants (small, medium, high)
- TOML-based level format with runtime level loading (`--level path/to/level.toml`)
- Start menu, HUD (hearts/lives/score), lives system, invincibility blink on damage
- Keyboard and gamepad (hot-plug) controls
- Debug overlay (`--debug` flag): FPS counter, CPU frame time, memory usage, collision hitbox visualization, scrolling event log
- Builds natively on macOS, Linux, and Windows; WebAssembly build via Emscripten

## Level Editor

Super Mango includes a standalone visual level editor built with C11 and SDL2. The editor lets you create and edit levels with a point-and-click interface, then save them as TOML files or export them as C source for embedding.

Editor features:

- Scrollable canvas with zoom, grid snapping, and multi-select
- Entity palette with all game objects: platforms, enemies, hazards, collectibles, surfaces, effects
- Per-entity property editing (position, size, speed, animation, behavior)
- TOML serialization (save/load `.toml` level files)
- C code exporter (generates `.c`/`.h` files for compiled-in levels)
- Undo/redo history
- Native file dialogs (macOS)

Build and run the editor:

```sh
make editor       # build the editor binary into out/
make run-editor   # build and run the editor
```

## Development Crew

Super Mango is developed with the help of four specialized [Claude Code](https://docs.anthropic.com/en/docs/claude-code) agents, each owning a distinct part of the project. Call them by name with slash commands in any Claude Code session inside this repository.

| Agent | Command | Role | Owns |
|-------|---------|------|------|
| **Bosser** | `/bosser-engineer` | Chief Engineer | C source, SDL2 engine, editor, Makefile, architecture, bug fixes |
| **Lugio** | `/lugio-creator` | Level Builder | TOML level files, entity placement, theming, difficulty balancing |
| **Goobma** | `/goobma-designer` | Pixel Art Designer | Sprite assets, palette remapping, frame layout analysis |
| **Warro** | `/warro-inscriber` | Documentation Inscriber | README, CLAUDE.md, wiki, GitHub Pages docs, cross-referencing |

### When to call whom

| You want to... | Call |
|----------------|------|
| Add a new enemy, surface, or hazard type | `/bosser-engineer` |
| Fix a bug or refactor engine code | `/bosser-engineer` |
| Create a new level or redesign an existing one | `/lugio-creator` |
| Design a new sprite or create a theme variant | `/goobma-designer` |
| Update documentation or audit for accuracy | `/warro-inscriber` |

### How to get the best results

- **Be specific about what you want.** "Add a spider that jumps higher" gives Bosser a clear target. "Make the game better" does not.
- **One agent at a time.** Each agent stays in their lane. If you ask Lugio to fix a bug, he'll tell you to call Bosser. That's by design.
- **Bosser delegates.** If you're unsure who to call, start with `/bosser-engineer` -- he'll route the work to the right crew member or handle it himself.
- **Lugio needs a theme.** When requesting a level, tell him the theme (forest, volcanic, sky), difficulty (easy/medium/hard), and length (number of screens). He'll ask if you don't.
- **Goobma needs a reference.** When requesting a sprite, point him at an existing asset in the same category. He matches dimensions, palette, and style automatically.
- **Warro verifies against code.** He reads source files and runs analysis tools before writing a single word. If the docs say one thing and the code says another, Warro trusts the code.

## Getting Started

### Prerequisites

A C11-compatible compiler (`clang` or `gcc`), `make`, and the SDL2 development libraries.

**macOS:**

```sh
brew install sdl2 sdl2_image sdl2_ttf sdl2_mixer
xcode-select --install   # provides clang and make
```

**Linux (Debian/Ubuntu):**

```sh
sudo apt update
sudo apt install build-essential clang \
    libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl2-mixer-dev
```

**Windows (MSYS2 UCRT64):**

```sh
pacman -S mingw-w64-ucrt-x86_64-clang \
          mingw-w64-ucrt-x86_64-make \
          mingw-w64-ucrt-x86_64-SDL2 \
          mingw-w64-ucrt-x86_64-SDL2_image \
          mingw-w64-ucrt-x86_64-SDL2_ttf \
          mingw-w64-ucrt-x86_64-SDL2_mixer
```

**WebAssembly:** Install the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) and ensure `emcc` is on `PATH`.

### Quick Start

```sh
make                                  # build the game binary into out/
make run                              # build and run
make run-debug                        # build and run with debug overlay
make run-level LEVEL=levels/sandbox_00.toml         # run a specific TOML level
make run-level-debug LEVEL=levels/sandbox_00.toml   # run a level with debug overlay
make editor                           # build the level editor
make run-editor                       # build and run the level editor
make web                              # build to WebAssembly (requires Emscripten)
make clean                            # remove all build artifacts
```

Or just **[play in your browser](https://jonathanperis.github.io/super-mango-game/)** -- no build required. Full project documentation is available at the **[docs site](https://jonathanperis.github.io/super-mango-game/docs/)**.

## Project Structure

```
super-mango-game/
├── Makefile                          Build system (clang, sdl2-config, ad-hoc codesign)
├── levels/                           TOML level definitions
│   └── sandbox_00.toml              Level data loaded at runtime
├── src/                              46 C source files + 48 headers
│   ├── main.c                        Entry point: SDL init/teardown
│   ├── game.h / game.c               GameState struct, window, renderer, game loop
│   ├── collectibles/                  Pickup items
│   │   ├── coin.h / .c               Coin (100 pts, 3 restore a heart)
│   │   ├── star_yellow.h / .c        Yellow star health pickup
│   │   └── last_star.h / .c          End-of-level star
│   ├── core/                          Shared utilities
│   │   ├── debug.h / .c              Debug overlay (FPS, CPU, memory, hitboxes, event log)
│   │   └── entity_utils.h / .c       Shared entity helper functions
│   ├── editor/                        Standalone visual level editor
│   │   ├── editor_main.c             Editor entry point
│   │   ├── editor.h / .c             Editor state, main loop, event handling
│   │   ├── canvas.h / .c             Scrollable canvas with zoom and grid
│   │   ├── palette.h / .c            Entity palette panel
│   │   ├── properties.h / .c         Per-entity property editing
│   │   ├── tools.h / .c              Selection, placement, and manipulation tools
│   │   ├── ui.h / .c                 Immediate-mode UI widgets
│   │   ├── serializer.h / .c         TOML save/load
│   │   ├── exporter.h / .c           C code export (.c/.h generation)
│   │   ├── file_dialog.h / .c        Native file dialogs
│   │   └── undo.h / .c               Undo/redo history
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
│   ├── levels/                        Level system
│   │   ├── level.h                    Shared level definitions (LevelDef struct)
│   │   ├── level_loader.h / .c       TOML level loading and switching
│   │   └── exported/                  Auto-generated C level data
│   │       └── sandbox_00.h / .c     Compiled-in level (exported from editor)
│   ├── player/                        Player module
│   │   └── player.h / .c             Input, physics, animation, render
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
├── docs/                              GitHub Pages shell (index.html)
├── web/                               Emscripten shell template
└── .github/workflows/                 CI/CD pipelines
    ├── build.yml                      Build check (PRs) + release + Pages deploy (main)
    ├── codeql.yml                     Code security analysis
    └── deploy.yml                     GitHub Pages deployment
```

## CI/CD

Three GitHub Actions workflows:

| Workflow | File | Trigger | Purpose |
|----------|------|---------|---------|
| Build & Release | `build.yml` | Push to `main`, pull requests | Multi-platform build (Linux x86_64, macOS arm64, Windows x86_64, WebAssembly); on main push: GitHub Release creation |
| CodeQL | `codeql.yml` | Push/PR to `main`, weekly | Automated code security and quality analysis |
| Deploy Pages | `deploy.yml` | Push to `main`, manual | Deploys the WebAssembly build to GitHub Pages for browser play |

All workflows install SDL2 dependencies per platform and compile with the project Makefile. The Build & Release workflow creates versioned GitHub Releases. The Deploy Pages workflow publishes the docs/ directory to GitHub Pages.

## License

MIT -- see [LICENSE](LICENSE)
