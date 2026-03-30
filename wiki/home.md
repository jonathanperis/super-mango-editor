# Super Mango — Wiki

> A 2D pixel art platformer written in **C11 + SDL2**, targeting macOS (Apple Silicon) but buildable on Linux and Windows.

Super Mango is an intentionally minimal, heavily-commented game project designed as a **learning resource** for C + SDL2 game development. The player runs, jumps, and climbs through a multi-layer parallax mountain stage with one-way platforms, floating platforms (static/crumble/rail), crumble bridges, sea gaps, collectible coins, yellow stars, a last star goal, climbable vines, ladders, and ropes, animated water, six enemy types (spiders, jumping spiders, birds, faster birds, fish, faster fish), six hazard types (spike blocks, spike rows, spike platforms, circular saws, axe traps, blue flames), three bouncepad variants (small, medium, high), and atmospheric fog, with smooth delta-time physics, a scrolling camera, sprite sheet animation, a start menu, an HUD with hearts and score, a sandbox mode, and an optional debug overlay.

---

## Navigation

| Page | Description |
|------|-------------|
| [Architecture](architecture) | Game loop, init/loop/cleanup pattern, GameState container |
| [Source Files](source_files) | Detailed reference for every `.c` / `.h` file |
| [Player Module](player_module) | Input, physics, animation — deep dive into `player.c` |
| [Build System](build_system) | Makefile, compiler flags, build targets, prerequisites |
| [Assets](assets) | All sprite sheets and tile sets in `assets/` |
| [Sounds](sounds) | All audio files in `sounds/` |
| [Constants Reference](constants_reference) | Every `#define` in `game.h` and `player.c` explained |
| [Developer Guide](developer_guide) | How to add new entities, sound effects, and features |

---

## Quick Start

```sh
# macOS — install dependencies
brew install sdl2 sdl2_image sdl2_ttf sdl2_mixer

# Build and run
make run
```

See [Build System](build_system) for Linux and Windows instructions.

---

## Project At a Glance

| Item | Detail |
|------|--------|
| Language | C11 |
| Compiler | `clang` (default), `gcc` compatible |
| Window size | 800 x 600 px (OS window) |
| Logical canvas | 400 x 300 px (2x pixel scale) |
| Target FPS | 60 |
| Audio | 44100 Hz, stereo, 16-bit |
| Libraries | SDL2, SDL2_image, SDL2_ttf, SDL2_mixer |

---

## Repository Structure

```
super-mango-game/
├── Makefile                ← Build system (ad-hoc codesign on macOS)
├── .claude/
│   └── scripts/
│       └── analyze_sprite.py  ← Sprite sheet analysis tool
├── assets/                 ← PNG sprite sheets, tilesets, font
│   └── unused/             ← Reserve assets not loaded by the game
├── sounds/                 ← .wav sound effects and music
│   └── unused/             ← Reserve sounds not loaded by the game
└── src/
    ├── main.c              ← SDL subsystem init/teardown, entry point
    ├── game.h              ← GameState struct + shared constants
    ├── game.c              ← Window, renderer, background, game loop
    ├── player.h/c          ← Player input, physics, animation, render
    ├── platform.h/c        ← One-way platform init and 9-slice render
    ├── water.h/c           ← Animated water strip: init, scroll, render
    ├── fog.h/c             ← Fog overlay: init, slide, spawn, render
    ├── parallax.h/c        ← Multi-layer scrolling background: init, tiled render, cleanup
    ├── hud.h/c             ← HUD renderer: hearts, lives counter, score text
    ├── debug.h/c           ← Debug overlay: FPS counter, collision hitboxes, event log
    ├── sandbox.h/c         ← Sandbox mode for testing entities
    ├── start_menu.h/c      ← Start menu screen with logo
    ├── coin.h/c            ← Coin collectible: placement, AABB collection, render
    ├── yellow_star.h/c     ← Yellow star collectible
    ├── last_star.h/c       ← Last star goal collectible
    ├── spider.h/c          ← Spider enemy: ground patrol, animation, render
    ├── jumping_spider.h/c  ← Jumping spider enemy: patrol, jump arcs, sea-gap awareness
    ├── bird.h/c            ← Slow bird enemy: sine-wave sky patrol, animation, render
    ├── faster_bird.h/c     ← Fast bird enemy: tighter sine-wave, faster animation
    ├── fish.h/c            ← Fish enemy: jumping water patrol, swim animation
    ├── faster_fish.h/c     ← Faster fish enemy: aggressive water patrol
    ├── blue_flame.h/c      ← Blue flame hazard
    ├── spike.h/c           ← Spike row hazard (floor/ceiling)
    ├── spike_block.h/c     ← Rail-riding spike block hazard: traversal, free-fall, collision
    ├── spike_platform.h/c  ← Spiked platform hazard
    ├── circular_saw.h/c    ← Rotating circular saw hazard
    ├── axe_trap.h/c        ← Swinging axe trap hazard
    ├── bouncepad.h/c       ← Shared bouncepad logic
    ├── bouncepad_small.h/c ← Small bouncepad (low launch)
    ├── bouncepad_medium.h/c← Medium bouncepad (standard launch)
    ├── bouncepad_high.h/c  ← High bouncepad (max launch)
    ├── float_platform.h/c  ← Hovering platform: static, crumble, and rail behaviours
    ├── bridge.h/c          ← Tiled crumble walkway: init, cascade-fall, render
    ├── rail.h/c            ← Rail path builder, bitmask tile render, position interpolation
    ├── vine.h/c            ← Climbable vine: init, render, grab-zone for climbing
    ├── ladder.h/c          ← Climbable ladder
    └── rope.h/c            ← Climbable rope
```
