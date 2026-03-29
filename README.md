# Super Mango

A 2D platformer game written in C using SDL2, built for learning purposes.

## About

Super Mango is a 2D platformer where a player character runs and jumps through a forest stage with one-way platforms, animated water, patrolling spider enemies, and atmospheric fog. The game renders at a 400×300 logical resolution scaled 2× to an 800×600 OS window, giving a chunky pixel-art look. Movement is smooth and frame-rate independent thanks to delta-time physics. The project is intentionally minimal and well-commented so the source code can be read as a learning resource for C + SDL2 game development.

### Current Features

- **Parallax background** — Multi-layer scrolling sky/mountain/cloud PNGs from `assets/Parallax/` scroll at increasing speeds to create a sense of depth as the camera follows the player
- **Scrolling camera** — smooth lerp-follow camera with directional look-ahead; the level is 1 600 logical pixels wide (4 screens), clamped so the canvas never shows beyond the world boundaries
- **Player** — 4-state animated character (idle/walk/jump/fall) with gravity, floor collision, and one-way platform landing
- **One-way platforms** — Pillar stacks built from 9-slice tiled grass blocks; the player can jump through from below and land on top
- **Animated water** — Seamless scrolling water strip at the bottom of the screen using cropped sprite frames
- **Spider enemies** — Ground-patrol spiders with 4-frame walk animation that reverse at patrol boundaries; touching a spider grants 1.5 s of invincibility and triggers a blinking sprite effect
- **Fog overlay** — Semi-transparent sky layers that slide across the screen for an atmospheric mist effect
- **Coins** — Collectible items placed on the ground and platforms; AABB pickup awards 100 points each; every 3 coins restores one heart
- **Vine decorations** — Static plant sprites placed on the ground and platform tops for visual variety; purely scenery with no collision
- **Fish enemies** — Jumping water enemies that patrol the bottom lane, leap out of the water on random arcs, and use AABB collision with the player
- **HUD overlay** — Top-of-screen display showing heart icons (hit points), player icon + lives counter, and a score readout
- **Lives/Hearts system** — The player has hearts (hit points, max 3) and lives (remaining tries, starts at 3); spider collision drains a heart; reaching 0 hearts costs a life
- **Audio** — Jump sound, coin pickup sound, hurt sound effect, and looping ambient background music

## Prerequisites

You need the following installed on your system:

- **clang** or **gcc** (any C11-compatible compiler)
- **make**
- **SDL2** — window, renderer, input, audio
- **SDL2_image** — loading PNG textures
- **SDL2_ttf** — loading TrueType fonts
- **SDL2_mixer** — audio mixing

### macOS

Install via [Homebrew](https://brew.sh):

```sh
brew install sdl2 sdl2_image sdl2_ttf sdl2_mixer
```

The compiler (`clang`) and `make` are included with Xcode Command Line Tools:

```sh
xcode-select --install
```

### Linux (Debian / Ubuntu)

```sh
sudo apt update
sudo apt install build-essential clang \
    libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl2-mixer-dev
```

On **Fedora / RHEL / CentOS**:

```sh
sudo dnf install clang make \
    SDL2-devel SDL2_image-devel SDL2_ttf-devel SDL2_mixer-devel
```

On **Arch Linux**:

```sh
sudo pacman -S clang make sdl2 sdl2_image sdl2_ttf sdl2_mixer
```

### Windows

The recommended approach on Windows is to use **MSYS2**, which gives you a Unix-like shell, `make`, and the MinGW-w64 compiler toolchain.

1. Download and install [MSYS2](https://www.msys2.org/).

2. Open the **MSYS2 UCRT64** terminal and install the dependencies:

```sh
pacman -S mingw-w64-ucrt-x86_64-clang \
          mingw-w64-ucrt-x86_64-make \
          mingw-w64-ucrt-x86_64-SDL2 \
          mingw-w64-ucrt-x86_64-SDL2_image \
          mingw-w64-ucrt-x86_64-SDL2_ttf \
          mingw-w64-ucrt-x86_64-SDL2_mixer
```

3. In the same UCRT64 terminal, navigate to the project and build:

```sh
cd /c/path/to/super-mango-game
make
```

4. The SDL2 DLLs must be in the same directory as the binary to run it. Copy them from the MSYS2 prefix:

```sh
cp /ucrt64/bin/SDL2*.dll out/
```

Then run `./out/super-mango.exe` from the terminal, or double-click `out/super-mango.exe` in Explorer.

## Building

```sh
# Build the binary into out/
make

# Build and run immediately
make run

# Remove all build artifacts
make clean
```

The compiled binary is placed at `out/super-mango`.

## Controls

### Keyboard

| Key | Action |
|---|---|
| `Space` | Jump |
| `A` / `←` | Move left |
| `D` / `→` | Move right |
| `ESC` | Quit |
| Close window | Quit |

### Gamepad (optional)

Gamepad support is **hot-plug**: a controller can be connected or disconnected while the game is running.

| Input | Action |
|---|---|
| D-Pad `←` / `→` | Move left / right |
| Left analog stick (X-axis) | Move left / right (dead-zone: 8000 / 32767) |
| `A` button (Cross on PlayStation) | Jump |
| `Start` button | Quit |

## Project Structure

```
super-mango-game/
├── Makefile               ← Build system (clang, sdl2-config, ad-hoc codesign)
├── assets/                ← PNG sprites and TTF font
│   ├── Player.png
│   ├── Forest_Background_0.png
│   ├── Grass_Tileset.png
│   ├── Grass_Oneway.png
│   ├── Water.png
│   ├── Spider_1.png
│   ├── Sky_Background_1.png
│   ├── Sky_Background_2.png
│   ├── Vine.png
│   ├── Fish_2.png
│   ├── Round9x13.ttf
│   └── ... (more sprites for future use)
├── sounds/                ← WAV/OGG sound effects and music
│   ├── jump.wav
│   ├── water-ambience.ogg
│   └── ... (more sounds for future use)
└── src/
    ├── main.c             ← Entry point: SDL init/teardown
    ├── game.h             ← GameState struct and constants
    ├── game.c             ← Window, renderer, textures, sound, game loop
    ├── player.h           ← Player struct declaration
    ├── player.c           ← Player init, input, physics, render
    ├── platform.h         ← Platform struct + MAX_PLATFORMS constant
    ├── platform.c         ← One-way platform init and 9-slice render
    ├── water.h            ← Water struct + animation constants
    ├── water.c            ← Animated water strip: init, scroll, render
    ├── spider.h           ← Spider struct + patrol constants
    ├── spider.c           ← Spider enemy patrol, animation, render
    ├── fog.h              ← FogSystem struct + instance pool
    ├── fog.c              ← Fog overlay: init, slide, spawn, render
    ├── parallax.h         ← ParallaxSystem struct + PARALLAX_MAX_LAYERS constant
    ├── parallax.c         ← Multi-layer scrolling background: init, tiled render, cleanup
    ├── coin.h             ← Coin struct + constants (MAX_COINS, COIN_SCORE, …)
    ├── coin.c             ← Coin placement, AABB collection, render
    ├── vine.h             ← VineDecor struct + MAX_VINES / VINE_W / VINE_H constants
    ├── vine.c             ← Static vine decoration: init and render
    ├── fish.h             ← Fish struct + patrol / jump / animation constants
    ├── fish.c             ← Jumping fish enemy: patrol, random jump arcs, render
    ├── hud.h              ← Hud struct (font + star texture) + HUD constants
    └── hud.c              ← HUD renderer: hearts, lives counter, score text
```

## Architecture

The game follows a simple **init → loop → cleanup** pattern:

```
main()
  └── SDL/audio/image/font init
       └── game_init()      — create window, renderer, load textures, sounds, entities
            └── game_loop() — event poll → update → render (60 FPS)
                 └── game_cleanup() — destroy all resources in reverse order
  └── SDL subsystem teardown
```

### Render Order (back to front)

| Layer | What |
|-------|------|
| 1 | Parallax background (6 layers from `assets/Parallax/`, rendered back-to-front via `parallax_render`) |
| 2 | Floor (9-slice tiled Grass_Tileset.png) |
| 3 | Platforms (9-slice tiled Grass_Oneway.png pillars) |
| 4 | Vines (static Vine.png scenery on ground and platform tops) |
| 5 | Coins (animated Coin.png collectibles) |
| 6 | Fish (animated Fish_2.png jumping water enemies, drawn before water) |
| 7 | Water (animated Water.png strip) |
| 8 | Spiders (animated Spider_1.png patrol enemies) |
| 9 | Player (animated Player.png sprite) |
| 10 | Fog (semi-transparent Sky_Background sliding layers) |
| 11 | HUD (`hud_render`: hearts, lives, score — always drawn on top) |

### Delta Time

Movement uses **delta time** (`dt`): the number of seconds elapsed since the last frame. Multiplying velocity by `dt` makes movement speed consistent regardless of frame rate — so the player moves at 160 px/s whether the game runs at 30 FPS or 120 FPS.

### Frame Cap

VSync is requested from the GPU via `SDL_RENDERER_PRESENTVSYNC`. A manual `SDL_Delay` fallback ensures the loop never spins faster than 60 FPS even if VSync is unavailable.

## Assets

All sprites are PNG files in `assets/`. The project ships with a full set of tiles, enemies, UI elements, and backgrounds ready for future development. `assets/Round9x13.ttf` is available for on-screen text rendering.

## Credits

### Art Assets

All sprites, tilesets, and backgrounds are from the **Super Mango 2D Pixel Art Platformer Asset Pack (16×16)** by **Juho** on itch.io:

- [https://juhosprite.itch.io/super-mango-2d-pixelart-platformer-asset-pack16x16](https://juhosprite.itch.io/super-mango-2d-pixelart-platformer-asset-pack16x16)

### Game Reference

This game is based on the Mario-style platformer by **JSLegendDev**, built with Kaboom.js. The game design, mechanics, and sound effects were originally created for that project:

- [https://github.com/JSLegendDev/Mario-Game-Kaboom.js](https://github.com/JSLegendDev/Mario-Game-Kaboom.js)

## License

MIT — see [LICENSE](LICENSE).
