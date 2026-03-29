# Super Mango — Wiki

> A 2D pixel art platformer written in **C11 + SDL2**, targeting macOS (Apple Silicon) but buildable on Linux and Windows.

Super Mango is an intentionally minimal, heavily-commented game project designed as a **learning resource** for C + SDL2 game development. The player runs and jumps through a forest stage with one-way platforms, animated water, patrolling spider enemies, and atmospheric fog, with smooth delta-time physics and sprite sheet animation.

---

## Navigation

| Page | Description |
|------|-------------|
| [Architecture](Architecture) | Game loop, init→loop→cleanup pattern, GameState container |
| [Source Files](Source-Files) | Detailed reference for every `.c` / `.h` file |
| [Player Module](Player-Module) | Input, physics, animation — deep dive into `player.c` |
| [Build System](Build-System) | Makefile, compiler flags, build targets, prerequisites |
| [Assets](Assets) | All sprite sheets and tile sets in `assets/` |
| [Sounds](Sounds) | All audio files in `sounds/` |
| [Constants Reference](Constants-Reference) | Every `#define` in `game.h` and `player.c` explained |
| [Developer Guide](Developer-Guide) | How to add new entities, sound effects, and features |

---

## Quick Start

```sh
# macOS — install dependencies
brew install sdl2 sdl2_image sdl2_ttf sdl2_mixer

# Build and run
make run
```

See [Build System](Build-System) for Linux and Windows instructions.

---

## Project At a Glance

| Item | Detail |
|------|--------|
| Language | C11 |
| Compiler | `clang` (default), `gcc` compatible |
| Window size | 800 × 600 px (OS window) |
| Logical canvas | 400 × 300 px (2× pixel scale) |
| Target FPS | 60 |
| Audio | 44100 Hz, stereo, 16-bit |
| Libraries | SDL2, SDL2_image, SDL2_ttf, SDL2_mixer |

---

## Repository Structure

```
super-mango-game/
├── Makefile                ← Build system (ad-hoc codesign on macOS)
├── assets/                 ← PNG sprite sheets, tilesets, font
├── sounds/                 ← .wav / .ogg sound effects and music
└── src/
    ├── main.c              ← SDL subsystem init/teardown, entry point
    ├── game.h              ← GameState struct + shared constants
    ├── game.c              ← Window, renderer, background, game loop
    ├── player.h            ← Player struct + function declarations
    ├── player.c            ← Player input, physics, animation, render
    ├── platform.h          ← Platform struct + MAX_PLATFORMS constant
    ├── platform.c          ← One-way platform init and 9-slice render
    ├── water.h             ← Water struct + animation constants
    ├── water.c             ← Animated water strip: init, scroll, render
    ├── spider.h            ← Spider struct + patrol constants
    ├── spider.c            ← Spider enemy patrol, animation, render
    ├── fog.h               ← FogSystem struct + instance pool
    ├── fog.c               ← Fog overlay: init, slide, spawn, render
    ├── parallax.h          ← ParallaxSystem struct + PARALLAX_MAX_LAYERS constant
    ├── parallax.c          ← Multi-layer scrolling background: init, tiled render, cleanup
    ├── coin.h              ← Coin struct + constants (MAX_COINS, COIN_SCORE, …)
    ├── coin.c              ← Coin placement, AABB collection, render
    ├── hud.h               ← Hud struct (font + star texture) + HUD constants
    └── hud.c               ← HUD renderer: hearts, lives counter, score text
```
