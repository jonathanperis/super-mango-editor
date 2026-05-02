# Source Files

<a id="home"></a>

---

## File Map

```
src/
├── main.c                        Entry point -- SDL subsystem lifecycle
├── game.h                        Shared constants + GameState struct (included everywhere)
├── game.c                        Window, renderer, textures, sounds, game loop
├── collectibles/
│   ├── coin.h / .c               Coin collectible: placement, AABB collection, render
│   ├── yellow_star.h / .c        Yellow star health pickup
│   └── last_star.h / .c          End-of-level star collectible
├── core/
│   ├── debug.h / .c              Debug overlay: FPS counter, collision hitboxes, event log
│   └── entity_utils.h / .c      Shared entity helper functions
├── effects/
│   ├── fog.h / .c                Atmospheric fog overlay: init, slide, spawn, render
│   ├── parallax.h / .c           Multi-layer scrolling background: init, tiled render, cleanup
│   └── water.h / .c              Animated water strip: init, scroll, tile render
├── entities/
│   ├── spider.h / .c             Spider enemy: ground patrol, animation, render
│   ├── jumping_spider.h / .c     Jumping spider: patrol, jump arcs, sea-gap awareness
│   ├── bird.h / .c               Slow bird enemy: sine-wave sky patrol, animation
│   ├── faster_bird.h / .c        Fast bird enemy: tighter sine-wave, faster animation
│   ├── fish.h / .c               Fish enemy: patrol, random jump arcs, render
│   └── faster_fish.h / .c        Fast fish enemy: higher jumps, faster patrol
├── hazards/
│   ├── spike.h / .c              Static ground spike hazard rows
│   ├── spike_block.h / .c        Rail-riding rotating spike hazard
│   ├── spike_platform.h / .c     Elevated spike surface hazard
│   ├── circular_saw.h / .c       Fast rotating patrol saw hazard
│   ├── axe_trap.h / .c           Swinging/spinning axe hazard
│   └── blue_flame.h / .c         Blue flame hazard: erupts from sea gaps, rise/flip/fall cycle
├── levels/
│   ├── level.h                   Shared level definitions
│   ├── level_01.h / .c           Level 1 data
│   └── level_loader.h / .c       Level loading and switching
├── player/
│   └── player.h / .c             Player input, physics, animation, rendering
├── screens/
│   ├── start_menu.h / .c         Start menu screen with logo
│   ├── sandbox.h / .c            Level layout and entity placement
│   └── hud.h / .c                HUD renderer: hearts, lives counter, score text
└── surfaces/
    ├── platform.h / .c           One-way platform pillar init and 9-slice rendering
    ├── float_platform.h / .c     Hovering platform: static, crumble, and rail behaviours
    ├── bridge.h / .c             Tiled crumble walkway: init, cascade-fall, render
    ├── bouncepad.h / .c          Shared bouncepad mechanics (squash/release animation)
    ├── bouncepad_small.h / .c    Green bouncepad (small jump)
    ├── bouncepad_medium.h / .c   Wood bouncepad (medium jump)
    ├── bouncepad_high.h / .c     Red bouncepad (high jump)
    ├── rail.h / .c               Rail path builder, bitmask tile render, position interpolation
    ├── vine.h / .c               Climbable vine decoration
    ├── ladder.h / .c             Climbable ladder decoration
    └── rope.h / .c               Climbable rope decoration
```

Every `.c` file in `src/` and its subdirectories is automatically picked up by the Makefile wildcards -- no changes to the build system are needed when adding new source files.

---

## `main.c`

**Role:** Owns the program entry point and every SDL subsystem's lifetime.

### Responsibilities

- Parse CLI flags: `--debug` and `--sandbox`
- Call `SDL_Init`, `IMG_Init`, `TTF_Init`, `Mix_OpenAudio` in order
- Route to start menu or sandbox (game) mode
- Tear down SDL subsystems in reverse order before returning

### Subsystem Init Order

| Order | Call | Purpose |
|-------|------|---------|
| 1 | `SDL_Init(SDL_INIT_VIDEO \| SDL_INIT_AUDIO)` | Core: window + audio device |
| 2 | `IMG_Init(IMG_INIT_PNG)` | PNG decoder |
| 3 | `TTF_Init()` | FreeType / font support |
| 4 | `Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048)` | Audio mixer |

On failure at any step, all previously-succeeded subsystems are torn down before returning `EXIT_FAILURE`.

---

## `game.h`

**Role:** The single shared header. Defines constants and `GameState`. Included by all other `.c` files.

### Constants

See [Constants Reference](#constants-reference) for full details.

```c
#define WINDOW_TITLE  "Super Mango"
#define WINDOW_W      800
#define WINDOW_H      600
#define TARGET_FPS    60
#define GAME_W        400
#define GAME_H        300
#define TILE_SIZE     48
#define FLOOR_Y       (GAME_H - TILE_SIZE)   // = 252
#define GRAVITY       800.0f
#define WORLD_W       1600
#define CAM_LOOKAHEAD  40
#define CAM_SMOOTHING  8.0f
#define CAM_SNAP_THRESHOLD  0.5f
#define SEA_GAP_W     32
#define MAX_SEA_GAPS  8
```

### Includes

```c
#include "player/player.h"              // Player struct
#include "surfaces/platform.h"          // Platform struct + MAX_PLATFORMS
#include "effects/water.h"              // Water struct
#include "effects/fog.h"                // FogSystem struct
#include "entities/spider.h"            // Spider struct + MAX_SPIDERS
#include "entities/fish.h"              // Fish struct + MAX_FISH
#include "collectibles/coin.h"          // Coin struct + MAX_COINS
#include "surfaces/vine.h"              // VineDecor struct + MAX_VINES
#include "surfaces/bouncepad.h"         // Bouncepad struct (shared mechanics)
#include "surfaces/bouncepad_small.h"   // Small bouncepad
#include "surfaces/bouncepad_medium.h"  // Medium bouncepad
#include "surfaces/bouncepad_high.h"    // High bouncepad
#include "screens/hud.h"                // Hud struct
#include "effects/parallax.h"           // ParallaxSystem
#include "surfaces/rail.h"              // Rail, RailTile
#include "hazards/spike_block.h"        // SpikeBlock
#include "surfaces/float_platform.h"    // FloatPlatform
#include "surfaces/bridge.h"            // Bridge
#include "entities/jumping_spider.h"    // JumpingSpider
#include "entities/bird.h"              // Bird
#include "entities/faster_bird.h"       // FasterBird
#include "collectibles/yellow_star.h"   // YellowStar
#include "hazards/axe_trap.h"           // AxeTrap
#include "hazards/circular_saw.h"       // CircularSaw
#include "hazards/blue_flame.h"         // BlueFlame
#include "surfaces/ladder.h"            // LadderDecor
#include "surfaces/rope.h"              // RopeDecor
#include "entities/faster_fish.h"       // FasterFish
#include "collectibles/last_star.h"     // LastStar
#include "hazards/spike.h"              // SpikeRow
#include "hazards/spike_platform.h"     // SpikePlatform
#include "core/debug.h"                 // DebugOverlay
```

### Function Declarations

```c
void game_init(GameState *gs);
void game_loop(GameState *gs);
void game_cleanup(GameState *gs);
```

---

## `game.c`

**Role:** Implements the three game lifecycle functions.

### `game_init(GameState *gs)`

Creates all runtime resources:

1. Window + renderer + logical size (400x300)
2. Parallax background (7 layers)
3. Floor tile (`grass_tileset.png`) and platform tile (`platform.png`)
4. Entity textures: `spider.png`, `jumping_spider.png`, `bird.png`, `faster_bird.png`, `fish.png`, `faster_fish.png`, `coin.png`, `bouncepad_medium.png`, `bouncepad_small.png`, `bouncepad_high.png`, `vine.png`, `ladder.png`, `rope.png`, `rail.png`, `spike_block.png`, `float_platform.png`, `bridge.png`, `yellow_star.png`, `axe_trap.png`, `circular_saw.png`, `blue_flame.png`, `spike.png`, `spike_platform.png`
5. Sound effects: `bouncepad.wav`, `axe_trap.wav`, `bird.wav`, `spider.wav`, `fish.wav`, `player_jump.wav`, `coin.wav`, `player_hit.wav`
6. Background music: `game_music.wav` (via `Mix_LoadMUS`)
7. Entity init: player, water, fog, HUD, debug, level (via sandbox)
8. Gamepad controller init

### `game_loop(GameState *gs)`

60 FPS loop: delta time -> events -> update -> render. See [Architecture](architecture) for the full render order.

### `game_cleanup(GameState *gs)`

Frees all resources in reverse init order.

---

## `player/player.h` / `player/player.c`

**Role:** Player character lifecycle. See [Player Module](#player-module) for the deep dive.

**Key functions:** `player_init`, `player_handle_input`, `player_update`, `player_render`, `player_get_hitbox`, `player_reset`, `player_cleanup`

---

## `screens/sandbox.h` / `screens/sandbox.c`

**Role:** Level layout and entity placement. Separates content (WHERE entities are placed) from engine logic (HOW they behave).

**Key functions:**
- `sandbox_load_level(GameState *gs)` -- place all entities, define sea gaps, set up the level (called once from `game_init`)
- `sandbox_reset_level(GameState *gs, int *fp_prev_riding)` -- reset all entities on player death

---

## `screens/start_menu.h` / `screens/start_menu.c`

**Role:** Start menu screen with centred title text and `start_menu_logo.png` logo.

**Key functions:** `start_menu_init`, `start_menu_loop`, `start_menu_cleanup`

---

## Enemy Modules (`entities/`)

### `entities/spider.h` / `entities/spider.c`

Ground-patrol spider enemy with 3-frame walk animation. Reverses at patrol boundaries and respects sea gaps. Asset: `spider.png`.

### `entities/jumping_spider.h` / `entities/jumping_spider.c`

Faster spider variant that periodically jumps in short arcs to clear sea gaps. Asset: `jumping_spider.png`.

### `entities/bird.h` / `entities/bird.c`

Slow sine-wave sky patrol bird. Asset: `bird.png`.

### `entities/faster_bird.h` / `entities/faster_bird.c`

Fast aggressive sine-wave sky patrol bird with tighter curves and quicker wing animation. Asset: `faster_bird.png`.

### `entities/fish.h` / `entities/fish.c`

Jumping water enemy that patrols the bottom lane and leaps on random arcs. Asset: `fish.png`.

### `entities/faster_fish.h` / `entities/faster_fish.c`

Fast fish variant with higher jumps and faster patrol speed. Asset: `faster_fish.png`.

---

## Hazard Modules (`hazards/`)

### `hazards/blue_flame.h` / `hazards/blue_flame.c`

Erupting fire hazard from sea gaps. Cycles through waiting -> rising -> flipping (180 degree rotation at apex) -> falling. 2-frame animation. Asset: `blue_flame.png`.

### `hazards/spike.h` / `hazards/spike.c`

Static ground spike rows placed along the floor. Asset: `spike.png`.

### `hazards/spike_block.h` / `hazards/spike_block.c`

Rail-riding rotating hazard (360 degrees/s spin). Travels along rail paths, pushes player on collision. Asset: `spike_block.png`.

### `hazards/spike_platform.h` / `hazards/spike_platform.c`

Elevated spike surface hazard. 3-slice rendered. Asset: `spike_platform.png`.

### `hazards/circular_saw.h` / `hazards/circular_saw.c`

Fast rotating patrol saw hazard (720 degrees/s). Patrols horizontally. Asset: `circular_saw.png`.

### `hazards/axe_trap.h` / `hazards/axe_trap.c`

Swinging pendulum or spinning axe hazard. Two behaviour modes: swing (60 degree amplitude) and spin (180 degrees/s). Asset: `axe_trap.png`.

---

## Collectible Modules (`collectibles/`)

### `collectibles/coin.h` / `collectibles/coin.c`

Gold coin collectible. AABB pickup awards 100 points. Every 3 coins restores one heart. Asset: `coin.png`.

### `collectibles/yellow_star.h` / `collectibles/yellow_star.c`

Yellow star health pickup that restores hearts. Asset: `yellow_star.png`.

### `collectibles/last_star.h` / `collectibles/last_star.c`

End-of-level star collectible. Asset: `last_star.png`.

---

## Surface Modules (`surfaces/`)

### `surfaces/platform.h` / `surfaces/platform.c`

One-way pillar platforms built from 9-slice tiled grass blocks. Player can jump through from below and land on top. Asset: `platform.png`.

### `surfaces/float_platform.h` / `surfaces/float_platform.c`

Hovering surfaces with three modes: static, crumble (falls after 0.75s), and rail (follows a rail path). 3-slice rendered. Asset: `float_platform.png`.

### `surfaces/bridge.h` / `surfaces/bridge.c`

Tiled crumble walkway. Bricks cascade-fall outward from the player's feet after a short delay. Asset: `bridge.png`.

### `surfaces/bouncepad.h` / `surfaces/bouncepad.c`

Shared bouncepad mechanics: squash/release 3-frame animation.

### `surfaces/bouncepad_small.h` / `surfaces/bouncepad_small.c`

Green bouncepad -- small jump height. Asset: `bouncepad_small.png`.

### `surfaces/bouncepad_medium.h` / `surfaces/bouncepad_medium.c`

Wood bouncepad -- medium jump height. Asset: `bouncepad_medium.png`.

### `surfaces/bouncepad_high.h` / `surfaces/bouncepad_high.c`

Red bouncepad -- high jump height. Asset: `bouncepad_high.png`.

### `surfaces/rail.h` / `surfaces/rail.c`

Rail path system. Builds closed-loop and open-line rail paths from tile definitions. 4x4 bitmask tileset for rendering. Used by spike blocks and float platforms. Asset: `rail.png`.

### `surfaces/vine.h` / `surfaces/vine.c`

Climbable vine decoration. Player can grab, climb up/down, drift horizontally, and dismount with a jump. Asset: `vine.png`.

### `surfaces/ladder.h` / `surfaces/ladder.c`

Climbable ladder decoration. Same climbing mechanics as vines. Asset: `ladder.png`.

### `surfaces/rope.h` / `surfaces/rope.c`

Climbable rope decoration. Same climbing mechanics as vines. Asset: `rope.png`.

---

## Environment Modules (`effects/`)

### `effects/water.h` / `effects/water.c`

Animated scrolling water strip at the bottom of the screen. 8 frames tiled seamlessly. Asset: `water.png`.

### `effects/fog.h` / `effects/fog.c`

Atmospheric fog overlay. Two semi-transparent sky layers slide across the screen with random direction, duration, and fade-in/out. Assets: `fog_background_1.png`, `fog_background_2.png`.

### `effects/parallax.h` / `effects/parallax.c`

Multi-layer scrolling background. 7 layers tiled horizontally, each at a different scroll speed. Assets: `parallax_sky.png`, `parallax_clouds_bg.png`, `parallax_glacial_mountains.png`, `parallax_clouds_mg_1/2/3.png`, `parallax_cloud_lonely.png`.

---

## System Modules

### `screens/hud.h` / `screens/hud.c`

HUD renderer. Draws heart icons (health), player icon + lives counter, coin icon + score. Assets: `yellow_star.png` (hearts), `hud_coins.png` (coin icon), `player.png` (lives icon), `round9x13.ttf` (font).

### `core/debug.h` / `core/debug.c`

Debug overlay (activated with `--debug` flag). FPS counter, collision hitbox visualization for all entities, and a scrolling event log.

### `core/entity_utils.h` / `core/entity_utils.c`

Shared entity helper functions used across multiple modules.

### `levels/level.h`

Shared level definitions and constants.

### `levels/level_01.h` / `levels/level_01.c`

Level 1 data definitions.

### `levels/level_loader.h` / `levels/level_loader.c`

Level loading and switching system.
