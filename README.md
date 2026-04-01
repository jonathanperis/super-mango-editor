# super-mango-game

> 2D side-scrolling platformer written in C using SDL2 -- browser-playable via WebAssembly

[![Build Check](https://github.com/jonathanperis/super-mango-game/actions/workflows/build-check.yml/badge.svg)](https://github.com/jonathanperis/super-mango-game/actions/workflows/build-check.yml) [![Main Release](https://github.com/jonathanperis/super-mango-game/actions/workflows/main-release.yml/badge.svg)](https://github.com/jonathanperis/super-mango-game/actions/workflows/main-release.yml) [![CodeQL](https://github.com/jonathanperis/super-mango-game/actions/workflows/codeql.yml/badge.svg)](https://github.com/jonathanperis/super-mango-game/actions/workflows/codeql.yml) [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

**[Play in browser →](https://jonathanperis.github.io/super-mango-game/)**

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

Or just **[play in your browser](https://jonathanperis.github.io/super-mango-game/)** -- no build required.

## Project Structure

```
super-mango-game/
├── Makefile                  Build system (clang, sdl2-config, ad-hoc codesign)
├── src/                      36 C source files + headers
│   ├── main.c                Entry point: SDL init/teardown
│   ├── game.h / game.c       GameState struct, window, renderer, game loop
│   ├── player.h / player.c   Player input, physics, animation, render
│   ├── sandbox.h / sandbox.c Level layout and entity placement
│   ├── start_menu.h / .c     Start menu screen
│   ├── platform.h / .c       One-way platform pillars (9-slice)
│   ├── float_platform.h / .c Hovering platforms (static/crumble/rail)
│   ├── bridge.h / .c         Crumble walkways
│   ├── spider.h / .c         Spider enemy (ground patrol)
│   ├── jumping_spider.h / .c Jumping spider enemy
│   ├── bird.h / .c           Bird enemy (sine-wave sky patrol)
│   ├── faster_bird.h / .c    Fast bird enemy
│   ├── fish.h / .c           Fish enemy (jumping water patrol)
│   ├── faster_fish.h / .c    Fast fish enemy
│   ├── coin.h / .c           Coin collectible
│   ├── yellow_star.h / .c    Health pickup
│   ├── last_star.h / .c      End-of-level star
│   ├── blue_flame.h / .c     Blue flame hazard
│   ├── spike.h / .c          Ground spike rows
│   ├── spike_block.h / .c    Rail-riding spike hazard
│   ├── spike_platform.h / .c Elevated spike hazard
│   ├── circular_saw.h / .c   Rotating saw hazard
│   ├── axe_trap.h / .c       Swinging axe hazard
│   ├── bouncepad*.h / .c     Bouncepad variants (small/medium/high)
│   ├── rail.h / .c           Rail path system
│   ├── vine.h / .c           Climbable vine
│   ├── ladder.h / .c         Climbable ladder
│   ├── rope.h / .c           Climbable rope
│   ├── water.h / .c          Animated water strip
│   ├── fog.h / .c            Fog overlay
│   ├── parallax.h / .c       Multi-layer scrolling background
│   ├── hud.h / .c            HUD (hearts, lives, score)
│   └── debug.h / .c          Debug overlay (FPS, hitboxes, event log)
├── assets/                   PNG sprites, tilesets, TTF font
│   └── unused/               Reserve assets from asset pack
├── sounds/                   WAV sound effects and music
│   └── unused/               Reserve sound files
├── docs/                     GitHub Pages shell (index.html)
├── web/                      Emscripten shell template
└── .github/workflows/        CI/CD pipelines
    ├── build-check.yml       PR build verification (Linux/macOS/Windows/WASM)
    ├── main-release.yml      Release + GitHub Pages deployment on push to main
    └── codeql.yml            Code security analysis
```

## CI/CD

Three GitHub Actions workflows:

| Workflow | File | Trigger | Purpose |
|----------|------|---------|---------|
| Build Check | `build-check.yml` | Pull requests | Multi-platform build verification (Linux x86_64, macOS arm64, Windows x86_64, WebAssembly) with artifact uploads |
| Main Release | `main-release.yml` | Push to `main` | Multi-platform build, GitHub Release creation with platform binaries, GitHub Pages deployment of WebAssembly build |
| CodeQL | `codeql.yml` | Push/PR to `main`, weekly | Automated code security and quality analysis |

All workflows install SDL2 dependencies per platform and compile with the project Makefile. The Main Release workflow additionally creates versioned GitHub Releases and deploys the WebAssembly build to GitHub Pages for browser play.

## License

MIT -- see [LICENSE](LICENSE)
