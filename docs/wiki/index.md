<!-- markdownlint-disable MD051 -- fragment anchors resolve on the combined /docs/ page -->
# super-mango-editor

> 2D side-scrolling platformer written in C using SDL2 -- browser-playable via WebAssembly

Super Mango is a 2D platformer built in C11 with SDL2, designed as an educational project with well-commented source code for learning C + SDL2 game development. The game features a multi-screen forest stage with parallax backgrounds, enemies, hazards, collectibles, and delta-time physics, building natively on macOS/Linux/Windows and as WebAssembly for browser play.

---

## Quick Links

### Engine & Code

| Page | Description |
|------|-------------|
| [Architecture](#architecture) | Game loop, init/loop/cleanup pattern, GameState container, 32-layer render order |
| [Source Files](#source-files) | Module-by-module reference for every `.c` / `.h` file |
| [Player Module](#player-module) | Input, physics, animation — deep dive into `player.c` |
| [Constants Reference](#constants-reference) | Every `#define` in `game.h` and entity headers explained |

### Content & Assets

| Page | Description |
|------|-------------|
| [Entities & Hazards](#entities-and-hazards) | All 6 enemy types and 7 hazard types: behaviour, constants, TOML placement |
| [Collectibles & Surfaces](#collectibles-and-surfaces) | Coins, stars, bouncepads, rails, float platforms, climbable surfaces |
| [Assets](#assets) | All sprite sheets, tilesets, and fonts in `assets/` |
| [Sounds](#sounds) | All audio files in `assets/sounds/` |

### Building & Contributing

| Page | Description |
|------|-------------|
| [Build System](#build-system) | Makefile, compiler flags, build targets, prerequisites for all platforms |
| [Level Design — TOML Reference](#level-design) | Full TOML schema for every entity type; minimum level template |
| [Level Editor](#level-editor) | Visual editor: canvas, palette, properties, undo, play-test, file I/O |
| [Developer Guide](#developer-guide) | Coding conventions, adding new entities, sound effects workflow |

---

## Key Features

- 2D side-scrolling platformer with dynamic multi-screen worlds (configurable via `screen_count`)
- 32 render layers drawn back-to-front with per-level configurable parallax backgrounds
- Delta-time physics for frame-rate-independent movement at 60 FPS
- Six enemy types (spider, jumping spider, bird, faster bird, fish, faster fish)
- Seven hazard types (spike, spike block, spike platform, circular saw, axe trap, blue flame, fire flame)
- Five collectible types (coin, star yellow/green/red, last star)
- Climbable vines, ladders, ropes; three bouncepad tiers (small/medium/high); crumble bridges; float platforms (static/crumble/rail-riding)
- TOML-based level format with per-level music, backgrounds, and floor tileset
- Standalone visual level editor with undo, copy/paste, and play-test integration
- Start menu, HUD (hearts/lives/score), lives system, debug overlay (`--debug`)
- Keyboard and gamepad (hot-plug) controls
- Builds natively on macOS, Linux, Windows; WebAssembly via Emscripten

**[Play in browser →](https://jonathanperis.github.io/super-mango-editor/)**

---

## Project at a Glance

| Item | Detail |
|------|--------|
| Language | C11 |
| Compiler | `clang` (default), `gcc` compatible |
| Window size | 800 × 600 px (OS window) |
| Logical canvas | 400 × 300 px (2× pixel scale) |
| Target FPS | 60 |
| Audio | 44100 Hz, stereo, 16-bit |
| Libraries | SDL2, SDL2_image, SDL2_ttf, SDL2_mixer, tomlc17 (TOML parser) |
| Level format | TOML (`.toml` files in `levels/`) |

---

## Quick Start

```sh
# macOS — install dependencies
brew install sdl2 sdl2_image sdl2_ttf sdl2_mixer

# Build and run the game
make run

# Build and run the level editor
make run-editor

# Run a specific level file
make run-level LEVEL=levels/00_sandbox_01.toml

# Build for WebAssembly
make web
```

See [Build System](#build-system) for Linux and Windows instructions.
