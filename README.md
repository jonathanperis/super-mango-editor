# Super Mango

A 2D platformer game written in C using SDL2, built for learning purposes.

## About

Super Mango is a 2D platformer where a player character runs, jumps, and climbs through a multi-screen forest stage packed with one-way platforms, floating platforms, crumble bridges, sea gaps, collectible coins, climbable vines, ladders, ropes, patrolling enemies (spiders, jumping spiders, birds, faster birds, fish, faster fish), spring-loaded bouncepads (small, medium, high), rail-riding spike blocks, ground spikes, spike platforms, axe traps, circular saws, blue flames, yellow star health pickups, an end-of-level last star, animated water, atmospheric fog, a start menu, and a scrolling parallax background. The game renders at a 400x300 logical resolution scaled 2x to an 800x600 OS window, giving a chunky pixel-art look. Movement is smooth and frame-rate independent thanks to delta-time physics. The project is intentionally minimal and well-commented so the source code can be read as a learning resource for C + SDL2 game development.

### Current Features

- **Start menu** — Title screen with logo (`start_menu_logo.png`) displayed before gameplay begins
- **Parallax background** — Multi-layer scrolling sky/mountain/cloud PNGs (`parallax_sky.png`, `parallax_clouds_bg.png`, `parallax_glacial_mountains.png`, `parallax_clouds_mg_1.png`, `parallax_clouds_mg_2.png`, `parallax_clouds_mg_3.png`, `parallax_cloud_lonely.png`) scroll at increasing speeds to create a sense of depth as the camera follows the player
- **Scrolling camera** — Smooth lerp-follow camera with directional look-ahead; the level is 1 600 logical pixels wide (4 screens), clamped so the canvas never shows beyond the world boundaries
- **Player** — 5-state animated character (idle/walk/jump/fall/climb) with gravity, floor collision, one-way platform landing, and vine/ladder/rope climbing
- **One-way platforms** — Pillar stacks built from 9-slice tiled grass blocks; the player can jump through from below and land on top
- **Float platforms** — Hovering surfaces with three behaviour modes: static (fixed position), crumble (falls after the player stands for 0.75 s), and rail (follows a rail path)
- **Crumble bridges** — Tiled brick walkways (16x16 per brick) that cascade-fall outward from the player's feet after a short delay
- **Sea gaps** — Holes in the ground floor that expose the water below; falling in costs all hearts (instant death)
- **Animated water** — Seamless scrolling water strip at the bottom of the screen using cropped sprite frames
- **Spider enemies** — Ground-patrol spiders with 3-frame walk animation that reverse at patrol boundaries and respect sea gaps; touching a spider grants 1.5 s of invincibility and triggers a blinking sprite effect
- **Jumping spider enemies** — A faster spider variant that periodically jumps in short arcs to clear sea gaps, making it harder to avoid
- **Bird enemies** — Slow sine-wave sky patrol birds (`bird.png`) with lazy curved flight paths
- **Faster bird enemies** — Aggressive fast sky patrol birds (`faster_bird.png`) with tighter, more erratic sine-wave curves and quicker wing animation
- **Fish enemies** — Jumping water enemies that patrol the bottom lane, leap out of the water on random arcs, and use AABB collision with the player
- **Faster fish enemies** — A speedier fish variant with more aggressive jump arcs and quicker patrol movement
- **Blue flame hazards** — Animated flame obstacles that erupt from sea gaps, cycling through rise/flip/fall phases, damaging the player on contact
- **Spike rows** — Ground-level spike strips placed on the floor surface that damage the player on contact
- **Spike platforms** — Elevated spike hazards mounted on platforms
- **Axe traps** — Swinging axe hazards with animated rotation that damage the player on contact
- **Circular saws** — Spinning blade hazards with continuous rotation that damage the player on contact
- **Fog overlay** — Semi-transparent sky layers (`fog_background_1.png`, `fog_background_2.png`) that slide across the screen for an atmospheric mist effect
- **Coins** — Collectible items placed on the ground and platforms; AABB pickup awards 100 points each; every 3 coins restores one heart
- **Yellow stars** — Health pickup items that restore one heart when collected
- **Last star** — End-of-level collectible star that completes the stage
- **Climbable vines** — Plant sprites placed on platforms; the player can grab a vine with UP, climb up/down, drift horizontally, and dismount with a jump
- **Ladders** — Climbable vertical structures that the player can grab and ascend/descend
- **Ropes** — Climbable hanging ropes that the player can grab and climb
- **HUD overlay** — Top-of-screen display showing heart icons (hit points), coin icon + coin counter, player icon + lives counter, and a score readout
- **Lives/Hearts system** — The player has hearts (hit points, max 3) and lives (remaining tries, starts at 3); enemy collision drains a heart; reaching 0 hearts costs a life; falling into a sea gap costs all hearts instantly
- **Bouncepads (small)** — Small spring launch pads (`bouncepad_small.png`) with lower launch height
- **Bouncepads (medium)** — Standard spring launch pads (`bouncepad_medium.png`) with 3-frame squash/release animation; landing on one launches the player upward and plays a bouncepad sound
- **Bouncepads (high)** — Tall spring launch pads (`bouncepad_high.png`) with maximum launch height
- **Spike blocks** — Rail-riding rotating hazards (16x16 scaled to 24x24, 360 deg/s spin); collision pushes the player away and deals damage
- **Rails** — Tile-based rail paths (closed loops and open lines) that spike blocks and float platforms travel along; built from a 4x4 bitmask tileset
- **Debug overlay** — `--debug` flag enables an FPS counter, collision hitbox visualization, and a scrolling event log
- **Audio** — Jump sound, coin pickup sound, hurt sound, bouncepad spring sound, bird flap sound, fish dive sound, spider attack sound, axe trap sound, and looping ambient background music

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

# Build and run in sandbox mode
make run-sandbox

# Build and run in sandbox mode with debug overlay
make run-sandbox-debug

# Build to WebAssembly (requires Emscripten SDK)
make web

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
| `W` / `↑` | Grab vine/ladder/rope / climb up |
| `S` / `↓` | Climb down |
| `ESC` | Quit |
| Close window | Quit |

### Gamepad (optional)

Gamepad support is **hot-plug**: a controller can be connected or disconnected while the game is running.

| Input | Action |
|---|---|
| D-Pad `←` / `→` | Move left / right |
| D-Pad `↑` / `↓` | Grab vine/ladder/rope / climb up / climb down |
| Left analog stick (X-axis) | Move left / right (dead-zone: 8000 / 32767) |
| Left analog stick (Y-axis) | Climb up / down on vine/ladder/rope |
| `A` button (Cross on PlayStation) | Jump |
| `Start` button | Quit |

## Project Structure

```
super-mango-game/
├── Makefile               <- Build system (clang, sdl2-config, ad-hoc codesign)
├── assets/                <- PNG sprites and TTF font
│   ├── player.png
│   ├── spider.png
│   ├── jumping_spider.png
│   ├── bird.png
│   ├── faster_bird.png
│   ├── fish.png
│   ├── faster_fish.png
│   ├── coin.png
│   ├── yellow_star.png
│   ├── last_star.png
│   ├── spike.png
│   ├── spike_block.png
│   ├── spike_platform.png
│   ├── circular_saw.png
│   ├── axe_trap.png
│   ├── blue_flame.png
│   ├── platform.png
│   ├── float_platform.png
│   ├── bridge.png
│   ├── bouncepad_small.png
│   ├── bouncepad_medium.png
│   ├── bouncepad_high.png
│   ├── vine.png
│   ├── ladder.png
│   ├── rope.png
│   ├── rail.png
│   ├── water.png
│   ├── grass_tileset.png
│   ├── fog_background_1.png
│   ├── fog_background_2.png
│   ├── hud_coins.png
│   ├── start_menu_logo.png
│   ├── parallax_sky.png
│   ├── parallax_clouds_bg.png
│   ├── parallax_glacial_mountains.png
│   ├── parallax_clouds_mg_1.png
│   ├── parallax_clouds_mg_2.png
│   ├── parallax_clouds_mg_3.png
│   ├── parallax_cloud_lonely.png
│   ├── round9x13.ttf
│   └── unused/            <- Unused asset files from the asset pack
├── sounds/                <- WAV sound effects and music
│   ├── player_jump.wav
│   ├── player_hit.wav
│   ├── coin.wav
│   ├── bouncepad.wav
│   ├── bird.wav
│   ├── fish.wav
│   ├── spider.wav
│   ├── axe_trap.wav
│   ├── game_music.wav
│   └── unused/            <- Unused sound files
└── src/
    ├── main.c             <- Entry point: SDL init/teardown
    ├── game.h             <- GameState struct and constants
    ├── game.c             <- Window, renderer, textures, sound, game loop
    ├── player.h           <- Player struct declaration
    ├── player.c           <- Player init, input, physics, render
    ├── platform.h         <- Platform struct + MAX_PLATFORMS constant
    ├── platform.c         <- One-way platform init and 9-slice render
    ├── water.h            <- Water struct + animation constants
    ├── water.c            <- Animated water strip: init, scroll, render
    ├── fog.h              <- FogSystem struct + instance pool
    ├── fog.c              <- Fog overlay: init, slide, spawn, render
    ├── parallax.h         <- ParallaxSystem struct + PARALLAX_MAX_LAYERS constant
    ├── parallax.c         <- Multi-layer scrolling background: init, tiled render, cleanup
    ├── hud.h              <- Hud struct (font + star texture) + HUD constants
    ├── hud.c              <- HUD renderer: hearts, lives counter, score text
    ├── debug.h            <- DebugOverlay struct + log/FPS constants
    ├── debug.c            <- Debug overlay: FPS counter, collision boxes, scrolling event log
    ├── sandbox.h          <- Sandbox struct + level layout constants
    ├── sandbox.c          <- Level sandbox: entity placement and layout
    ├── start_menu.h       <- StartMenu struct + logo texture
    ├── start_menu.c       <- Start menu screen: init, input, render
    ├── coin.h             <- Coin struct + constants (MAX_COINS, COIN_SCORE, ...)
    ├── coin.c             <- Coin placement, AABB collection, render
    ├── yellow_star.h      <- YellowStar struct + health pickup constants
    ├── yellow_star.c      <- Yellow star health pickup: init, collection, render
    ├── last_star.h        <- LastStar struct + end-of-level constants
    ├── last_star.c        <- End-of-level star: init, collection, render
    ├── spider.h           <- Spider struct + patrol constants
    ├── spider.c           <- Spider enemy patrol, animation, render
    ├── jumping_spider.h   <- JumpingSpider struct + jump/patrol constants
    ├── jumping_spider.c   <- Jumping spider enemy: patrol, jump arcs, sea-gap awareness
    ├── bird.h             <- Bird struct + sine-wave patrol constants
    ├── bird.c             <- Slow bird enemy: sine-wave sky patrol, animation, render
    ├── faster_bird.h      <- FasterBird struct + aggressive patrol constants
    ├── faster_bird.c      <- Fast bird enemy: tighter sine-wave, faster animation
    ├── fish.h             <- Fish struct + patrol / jump / animation constants
    ├── fish.c             <- Jumping fish enemy: patrol, random jump arcs, render
    ├── faster_fish.h      <- FasterFish struct + aggressive patrol constants
    ├── faster_fish.c      <- Faster fish enemy: quicker patrol, aggressive jumps
    ├── blue_flame.h       <- BlueFlame struct + animation constants
    ├── blue_flame.c       <- Blue flame hazard: animation, collision, render
    ├── spike.h            <- Spike struct + ground spike constants
    ├── spike.c            <- Ground spike rows: init, collision, render
    ├── spike_block.h      <- SpikeBlock struct + speed/push constants
    ├── spike_block.c      <- Spike block rail traversal, free-fall, push collision, render
    ├── spike_platform.h   <- SpikePlatform struct + platform spike constants
    ├── spike_platform.c   <- Spike platform hazard: init, collision, render
    ├── circular_saw.h     <- CircularSaw struct + rotation constants
    ├── circular_saw.c     <- Circular saw hazard: rotation, collision, render
    ├── axe_trap.h         <- AxeTrap struct + swing constants
    ├── axe_trap.c         <- Axe trap hazard: swing animation, collision, render
    ├── bouncepad.h        <- Bouncepad struct + constants (MAX_BOUNCEPADS, BOUNCEPAD_VY, ...)
    ├── bouncepad.c        <- Bouncepad base: init, 3-frame release animation, render
    ├── bouncepad_small.h  <- BouncePadSmall variant constants
    ├── bouncepad_small.c  <- Small bouncepad: lower launch height
    ├── bouncepad_medium.h <- BouncePadMedium variant constants
    ├── bouncepad_medium.c <- Medium bouncepad: standard launch height
    ├── bouncepad_high.h   <- BouncePadHigh variant constants
    ├── bouncepad_high.c   <- High bouncepad: maximum launch height
    ├── float_platform.h   <- FloatPlatform struct + mode enum + crumble/rail constants
    ├── float_platform.c   <- Hovering platform: static, crumble, and rail behaviours
    ├── bridge.h           <- Bridge/BridgeBrick structs + cascade-fall constants
    ├── bridge.c           <- Tiled crumble walkway: init, cascade-fall, render
    ├── rail.h             <- Rail/RailTile structs + bitmask constants (MAX_RAILS, MAX_RAIL_TILES, ...)
    ├── rail.c             <- Rail path building, bitmask tile rendering, position interpolation
    ├── vine.h             <- VineDecor struct + MAX_VINES / VINE_W / VINE_H constants
    ├── vine.c             <- Climbable vine: init, render, grab-zone for climbing
    ├── ladder.h           <- Ladder struct + climbable ladder constants
    ├── ladder.c           <- Climbable ladder: init, render, grab-zone for climbing
    ├── rope.h             <- Rope struct + climbable rope constants
    └── rope.c             <- Climbable rope: init, render, grab-zone for climbing
```

## Architecture

The game follows a simple **init -> loop -> cleanup** pattern:

```
main()
  └── SDL/audio/image/font init
       └── game_init()      — create window, renderer, load textures, sounds, entities
            └── game_loop() — event poll -> update -> render (60 FPS)
                 └── game_cleanup() — destroy all resources in reverse order
  └── SDL subsystem teardown
```

### Render Order (back to front)

| Layer | What |
|-------|------|
| 1 | Parallax background (7 layers: `parallax_sky.png` through `parallax_cloud_lonely.png`, rendered back-to-front via `parallax_render`) |
| 2 | Platforms (9-slice tiled `grass_tileset.png` pillars -- drawn before floor so pillars sink into ground) |
| 3 | Floor (9-slice tiled `grass_tileset.png` with sea-gap openings) |
| 4 | Float platforms (`float_platform.png` 3-slice hovering surfaces -- static, crumble, rail modes) |
| 5 | Spike rows (`spike.png` ground-level spike strips) |
| 6 | Spike platforms (`spike_platform.png` elevated spike hazards) |
| 7 | Bridges (`bridge.png` tiled crumble walkways) |
| 8 | Bouncepads medium (`bouncepad_medium.png` standard spring pads) |
| 9 | Bouncepads small (`bouncepad_small.png` lower spring pads) |
| 10 | Bouncepads high (`bouncepad_high.png` tall spring pads) |
| 11 | Rails (`rail.png` bitmask tile tracks) |
| 12 | Vines (`vine.png` climbable plants on platforms) |
| 13 | Ladders (`ladder.png` climbable vertical structures) |
| 14 | Ropes (`rope.png` climbable hanging ropes) |
| 15 | Coins (`coin.png` collectibles) |
| 16 | Yellow stars (`yellow_star.png` health pickups) |
| 17 | Last star (end-of-level star using HUD star sprite) |
| 18 | Blue flames (`blue_flame.png` animated flame hazards erupting from sea gaps) |
| 19 | Fish (`fish.png` jumping water enemies, drawn before water for submerged look) |
| 20 | Faster fish (`faster_fish.png` aggressive jumping water enemies) |
| 21 | Water (animated `water.png` strip) |
| 22 | Spike blocks (`spike_block.png` rotating rail-riding hazards) |
| 23 | Axe traps (`axe_trap.png` swinging axe hazards) |
| 24 | Circular saws (`circular_saw.png` spinning blade hazards) |
| 25 | Spiders (animated `spider.png` ground patrol enemies) |
| 26 | Jumping spiders (animated `jumping_spider.png` jumping patrol enemies) |
| 27 | Birds (animated `bird.png` slow sine-wave sky patrol) |
| 28 | Faster birds (animated `faster_bird.png` fast aggressive sky patrol) |
| 29 | Player (animated `player.png` sprite) |
| 30 | Fog (semi-transparent `fog_background_1.png` / `fog_background_2.png` sliding layers) |
| 31 | HUD (`hud_render`: hearts, lives, score -- always drawn on top) |
| 32 | Debug overlay (FPS counter, collision boxes, event log -- when `--debug` active) |

### Delta Time

Movement uses **delta time** (`dt`): the number of seconds elapsed since the last frame. Multiplying velocity by `dt` makes movement speed consistent regardless of frame rate -- so the player moves at 160 px/s whether the game runs at 30 FPS or 120 FPS.

### Frame Cap

VSync is requested from the GPU via `SDL_RENDERER_PRESENTVSYNC`. A manual `SDL_Delay` fallback ensures the loop never spins faster than 60 FPS even if VSync is unavailable.

## Assets

All sprites are PNG files in `assets/`. The project ships with a full set of tiles, enemies, hazards, UI elements, and backgrounds. `assets/round9x13.ttf` is available for on-screen text rendering. Parallax background layers use a `parallax_` prefix and are stored at the `assets/` root (not in a subfolder). Unused assets from the original asset pack are stored in `assets/unused/`.

## Sounds

All sound effects and music are WAV files in `sounds/`. The game uses 9 audio files: `player_jump.wav`, `player_hit.wav`, `coin.wav`, `bouncepad.wav`, `bird.wav`, `fish.wav`, `spider.wav`, `axe_trap.wav`, and `game_music.wav`. Unused sound files are stored in `sounds/unused/`.

## Credits

### Art Assets

All sprites, tilesets, and backgrounds are from the **Super Mango 2D Pixel Art Platformer Asset Pack (16x16)** by **Juho** on itch.io:

- [https://juhosprite.itch.io/super-mango-2d-pixelart-platformer-asset-pack16x16](https://juhosprite.itch.io/super-mango-2d-pixelart-platformer-asset-pack16x16)

### Game Reference

This game is based on the Mario-style platformer by **JSLegendDev**, built with Kaboom.js. The game design, mechanics, and sound effects were originally created for that project:

- [https://github.com/JSLegendDev/Mario-Game-Kaboom.js](https://github.com/JSLegendDev/Mario-Game-Kaboom.js)

## License

MIT -- see [LICENSE](LICENSE).
