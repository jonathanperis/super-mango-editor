# super-mango-game

> 2D side-scrolling platformer written in C using SDL2 -- browser-playable via WebAssembly

[![Build Check](https://github.com/jonathanperis/super-mango-game/actions/workflows/build.yml/badge.svg?event=pull_request)](https://github.com/jonathanperis/super-mango-game/actions/workflows/build.yml) [![Main Release](https://github.com/jonathanperis/super-mango-game/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/jonathanperis/super-mango-game/actions/workflows/build.yml) [![CodeQL](https://github.com/jonathanperis/super-mango-game/actions/workflows/codeql.yml/badge.svg)](https://github.com/jonathanperis/super-mango-game/actions/workflows/codeql.yml) [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

**[Live demo →](https://jonathanperis.github.io/super-mango-game/)** | **[Documentation →](https://jonathanperis.github.io/super-mango-game/docs/)**

---

## About

Super Mango is a 2D side-scrolling platformer built in C11 with SDL2, designed as an educational project with well-commented source code that can be read as a learning resource for C + SDL2 game development. The game features a multi-screen forest stage with parallax backgrounds, one-way platforms, floating platforms, crumble bridges, sea gaps, collectible coins, climbable vines/ladders/ropes, six enemy types, six hazard types, bouncepads, animated water, fog overlays, a start menu, and an HUD with hearts, lives, and score. It renders at a 400x300 logical resolution scaled 2x to an 800x600 window for a chunky pixel-art look, with frame-rate-independent movement via delta-time physics. The project builds natively on macOS, Linux, and Windows, and compiles to WebAssembly via Emscripten for browser play.

## Tech Stack

| Technology | Version | Purpose |
|-----------|---------|---------|
| C | C11 | Language standard (`clang -std=c11`) |
| SDL2 | 2.x | Window, renderer, input, events, timing |
| SDL2_image | 2.x | PNG texture loading |
| SDL2_ttf | 2.x | TrueType font rendering |
| SDL2_mixer | 2.x | Sound effects and music |
| Emscripten | latest | WebAssembly compilation for browser play |

## Features

- 2D side-scrolling platformer with a multi-screen forest stage (1600px world, 4 screens wide)
- 32 render layers drawn back-to-front: parallax background, platforms, floor, enemies, player, fog, HUD, debug overlay
- Delta-time physics for frame-rate-independent movement at 60 FPS (VSync + manual fallback)
- Six enemy types: spiders, jumping spiders, birds, faster birds, fish, faster fish
- Six hazard types: spike rows, spike blocks, spike platforms, circular saws, axe traps, blue flames
- Collectibles: coins (100 pts each, 3 coins restore a heart), yellow star health pickups, end-of-level last star
- Climbable vines, ladders, and ropes; three bouncepad variants (small, medium, high)
- Start menu, HUD (hearts/lives/score), lives system, invincibility blink on damage
- Keyboard and gamepad (hot-plug) controls
- Debug overlay (`--debug` flag): FPS counter, collision hitbox visualization, scrolling event log
- Builds natively on macOS, Linux, and Windows; WebAssembly build via Emscripten

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
make          # build the binary into out/
make run      # build and run
make run-debug    # build and run with debug overlay
make web      # build to WebAssembly (requires Emscripten)
make clean    # remove all build artifacts
```

Or just **[play in your browser](https://jonathanperis.github.io/super-mango-game/)** -- no build required. Full project documentation is available at the **[docs site](https://jonathanperis.github.io/super-mango-game/docs/)**.

## Project Structure

```
super-mango-game/
├── Makefile                          Build system (clang, sdl2-config, ad-hoc codesign)
├── src/                              39 C source files + headers
│   ├── main.c                        Entry point: SDL init/teardown
│   ├── game.h / game.c               GameState struct, window, renderer, game loop
│   ├── collectibles/                  Pickup items
│   │   ├── coin.h / .c               Coin (100 pts, 3 restore a heart)
│   │   ├── yellow_star.h / .c        Health pickup
│   │   └── last_star.h / .c          End-of-level star
│   ├── core/                          Shared utilities
│   │   ├── debug.h / .c              Debug overlay (FPS, hitboxes, event log)
│   │   └── entity_utils.h / .c       Shared entity helper functions
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
│   │   └── blue_flame.h / .c         Blue flame
│   ├── levels/                        Level system
│   │   ├── level.h                    Shared level definitions
│   │   ├── level_01.h / .c           Level 1 data
│   │   └── level_loader.h / .c       Level loading and switching
│   ├── player/                        Player module
│   │   └── player.h / .c             Input, physics, animation, render
│   ├── screens/                       Game screens
│   │   ├── start_menu.h / .c         Start menu
│   │   ├── sandbox.h / .c            Level layout and entity placement
│   │   └── hud.h / .c                HUD (hearts, lives, score)
│   └── surfaces/                      Traversable objects
│       ├── platform.h / .c           One-way platform pillars (9-slice)
│       ├── float_platform.h / .c     Hovering platforms (static/crumble/rail)
│       ├── bridge.h / .c             Crumble walkways
│       ├── bouncepad*.h / .c         Bouncepad base + 3 variants (small/medium/high)
│       ├── rail.h / .c               Rail path system
│       ├── vine.h / .c               Climbable vine
│       ├── ladder.h / .c             Climbable ladder
│       └── rope.h / .c               Climbable rope
├── assets/                            PNG sprites, tilesets, TTF font
│   └── unused/                        Reserve assets from asset pack
├── sounds/                            WAV sound effects and music
│   └── unused/                        Reserve sound files
├── docs/                              GitHub Pages shell (index.html)
├── web/                               Emscripten shell template
└── .github/workflows/                 CI/CD pipelines
    ├── build.yml                      Build check (PRs) + release + Pages deploy (main)
    ├── codeql.yml                     Code security analysis
    └── deploy-docs.yml                Documentation generation from wiki
```

## CI/CD

Three GitHub Actions workflows:

| Workflow | File | Trigger | Purpose |
|----------|------|---------|---------|
| Build & Release | `build.yml` | Push to `main`, pull requests | Multi-platform build (Linux x86_64, macOS arm64, Windows x86_64, WebAssembly); on main push: GitHub Release creation + Pages deployment |
| CodeQL | `codeql.yml` | Push/PR to `main`, weekly | Automated code security and quality analysis |
| Deploy Docs | `deploy-docs.yml` | Push to `main`, wiki changes, manual | Generates documentation from wiki and creates update PR |

All workflows install SDL2 dependencies per platform and compile with the project Makefile. The Build & Release workflow creates versioned GitHub Releases and deploys the WebAssembly build to GitHub Pages for browser play.

## License

MIT -- see [LICENSE](LICENSE)
