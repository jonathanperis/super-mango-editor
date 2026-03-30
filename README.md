# Super Mango

A 2D platformer game written in C using SDL2, built for learning purposes.

## About

Super Mango is a 2D platformer where a player character runs, jumps, and climbs through a multi-screen forest stage packed with one-way platforms, floating platforms, crumble bridges, sea gaps, collectible coins, climbable vines, patrolling enemies (spiders, jumping spiders, birds, faster birds, fish), spring-loaded bouncepads, rail-riding spike blocks, animated water, and atmospheric fog. The game renders at a 400×300 logical resolution scaled 2× to an 800×600 OS window, giving a chunky pixel-art look. Movement is smooth and frame-rate independent thanks to delta-time physics. The project is intentionally minimal and well-commented so the source code can be read as a learning resource for C + SDL2 game development.

### Current Features

- **Parallax background** — Multi-layer scrolling sky/mountain/cloud PNGs from `assets/parallax/` scroll at increasing speeds to create a sense of depth as the camera follows the player
- **Scrolling camera** — Smooth lerp-follow camera with directional look-ahead; the level is 1 600 logical pixels wide (4 screens), clamped so the canvas never shows beyond the world boundaries
- **Player** — 5-state animated character (idle/walk/jump/fall/climb) with gravity, floor collision, one-way platform landing, and vine climbing
- **One-way platforms** — Pillar stacks built from 9-slice tiled grass blocks; the player can jump through from below and land on top
- **Float platforms** — Hovering surfaces with three behaviour modes: static (fixed position), crumble (falls after the player stands for 0.75 s), and rail (follows a rail path)
- **Crumble bridges** — Tiled brick walkways (16×16 per brick) that cascade-fall outward from the player's feet after a short delay
- **Sea gaps** — Holes in the ground floor that expose the water below; falling in costs all hearts (instant death)
- **Animated water** — Seamless scrolling water strip at the bottom of the screen using cropped sprite frames
- **Spider enemies** — Ground-patrol spiders with 3-frame walk animation that reverse at patrol boundaries and respect sea gaps; touching a spider grants 1.5 s of invincibility and triggers a blinking sprite effect
- **Jumping spider enemies** — A faster spider variant that periodically jumps in short arcs to clear sea gaps, making it harder to avoid
- **Bird enemies** — Slow sine-wave sky patrol birds (Bird_2.png) with lazy curved flight paths
- **Faster bird enemies** — Aggressive fast sky patrol birds (Bird_1.png) with tighter, more erratic sine-wave curves and quicker wing animation
- **Fish enemies** — Jumping water enemies that patrol the bottom lane, leap out of the water on random arcs, and use AABB collision with the player
- **Fog overlay** — Semi-transparent sky layers that slide across the screen for an atmospheric mist effect
- **Coins** — Collectible items placed on the ground and platforms; AABB pickup awards 100 points each; every 3 coins restores one heart
- **Climbable vines** — Plant sprites placed on platforms; the player can grab a vine with UP, climb up/down, drift horizontally, and dismount with a jump
- **HUD overlay** — Top-of-screen display showing heart icons (hit points), player icon + lives counter, and a score readout
- **Lives/Hearts system** — The player has hearts (hit points, max 3) and lives (remaining tries, starts at 3); enemy collision drains a heart; reaching 0 hearts costs a life; falling into a sea gap costs all hearts instantly
- **Bouncepads** — Wooden spring launch pads with 3-frame squash/release animation; landing on one launches the player upward and plays a bouncepad sound
- **Spike blocks** — Rail-riding rotating hazards (16×16 scaled to 24×24, 360°/s spin); collision pushes the player away and deals damage
- **Rails** — Tile-based rail paths (closed loops and open lines) that spike blocks and float platforms travel along; built from a 4×4 bitmask tileset
- **Debug overlay** — `--debug` flag enables an FPS counter, collision hitbox visualization, and a scrolling event log
- **Audio** — Jump sound, coin pickup sound, hurt sound effect, bouncepad spring sound, and looping ambient background music

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

# Build and run with debug overlay (FPS, collision boxes, event log)
make run-debug

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
| `W` / `↑` | Grab vine / climb up |
| `S` / `↓` | Climb down |
| `ESC` | Quit |
| Close window | Quit |

### Gamepad (optional)

Gamepad support is **hot-plug**: a controller can be connected or disconnected while the game is running.

| Input | Action |
|---|---|
| D-Pad `←` / `→` | Move left / right |
| D-Pad `↑` / `↓` | Grab vine / climb up / climb down |
| Left analog stick (X-axis) | Move left / right (dead-zone: 8000 / 32767) |
| Left analog stick (Y-axis) | Climb up / down on vine |
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
    ├── vine.c             ← Climbable vine: init, render, grab-zone for climbing
    ├── fish.h             ← Fish struct + patrol / jump / animation constants
    ├── fish.c             ← Jumping fish enemy: patrol, random jump arcs, render
    ├── hud.h              ← Hud struct (font + star texture) + HUD constants
    ├── hud.c              ← HUD renderer: hearts, lives counter, score text
    ├── bouncepad.h        ← Bouncepad struct + constants (MAX_BOUNCEPADS, BOUNCEPAD_VY, …)
    ├── bouncepad.c        ← Bouncepad init, 3-frame release animation, render
    ├── rail.h             ← Rail/RailTile structs + bitmask constants (MAX_RAILS, MAX_RAIL_TILES, …)
    ├── rail.c             ← Rail path building, bitmask tile rendering, position interpolation
    ├── spike_block.h      ← SpikeBlock struct + speed/push constants
    ├── spike_block.c      ← Spike block rail traversal, free-fall, push collision, render
    ├── float_platform.h   ← FloatPlatform struct + mode enum + crumble/rail constants
    ├── float_platform.c   ← Hovering platform: static, crumble, and rail behaviours
    ├── bridge.h           ← Bridge/BridgeBrick structs + cascade-fall constants
    ├── bridge.c           ← Tiled crumble walkway: init, cascade-fall, render
    ├── jumping_spider.h   ← JumpingSpider struct + jump/patrol constants
    ├── jumping_spider.c   ← Jumping spider enemy: patrol, jump arcs, sea-gap awareness
    ├── bird.h             ← Bird struct + sine-wave patrol constants
    ├── bird.c             ← Slow bird enemy: sine-wave sky patrol, animation, render
    ├── faster_bird.h      ← FasterBird struct + aggressive patrol constants
    ├── faster_bird.c      ← Fast bird enemy: tighter sine-wave, faster animation
    ├── debug.h            ← DebugOverlay struct + log/FPS constants
    └── debug.c            ← Debug overlay: FPS counter, collision boxes, scrolling event log
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
| 1 | Parallax background (7 layers from `assets/parallax/`, rendered back-to-front via `parallax_render`) |
| 2 | Platforms (9-slice tiled Grass_Oneway.png pillars — drawn before floor so pillars sink into ground) |
| 3 | Floor (9-slice tiled Grass_Tileset.png with sea-gap openings) |
| 4 | Float platforms (Platform.png 3-slice hovering surfaces — static, crumble, rail modes) |
| 5 | Bridges (Bridge.png tiled crumble walkways) |
| 6 | Bouncepads (Bouncepad_Wood.png spring pads) |
| 7 | Rails (Rails.png bitmask tile tracks) |
| 8 | Vines (Vine.png climbable plants on platforms) |
| 9 | Coins (Coin.png collectibles) |
| 10 | Fish (animated Fish_2.png jumping water enemies, drawn before water for submerged look) |
| 11 | Water (animated Water.png strip) |
| 12 | Spike blocks (Spike_Block.png rotating rail-riding hazards) |
| 13 | Spiders (animated Spider_1.png ground patrol enemies) |
| 14 | Jumping spiders (animated Spider_2.png jumping patrol enemies) |
| 15 | Birds (animated Bird_2.png slow sine-wave sky patrol) |
| 16 | Faster birds (animated Bird_1.png fast aggressive sky patrol) |
| 17 | Player (animated Player.png sprite) |
| 18 | Fog (semi-transparent Sky_Background sliding layers) |
| 19 | HUD (`hud_render`: hearts, lives, score — always drawn on top) |
| 20 | Debug overlay (FPS counter, collision boxes, event log — when `--debug` active) |

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
