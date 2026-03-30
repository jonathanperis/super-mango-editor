# Source Files

← [Home](home)

---

## File Map

```
src/
├── main.c              Entry point — SDL subsystem lifecycle
├── game.h              Shared constants + GameState struct (included everywhere)
├── game.c              Window, renderer, textures, sounds, game loop
├── player.h / .c       Player input, physics, animation, rendering
├── sandbox.h / .c      Level layout and entity placement
├── start_menu.h / .c   Start menu screen with logo
├── platform.h / .c     One-way platform pillar init and 9-slice rendering
├── float_platform.h/.c Hovering platform: static, crumble, and rail behaviours
├── bridge.h / .c       Tiled crumble walkway: init, cascade-fall, render
├── water.h / .c        Animated water strip: init, scroll, tile render
├── fog.h / .c          Atmospheric fog overlay: init, slide, spawn, render
├── parallax.h / .c     Multi-layer scrolling background: init, tiled render, cleanup
├── hud.h / .c          HUD renderer: hearts, lives counter, score text
├── debug.h / .c        Debug overlay: FPS counter, collision hitboxes, event log
├── coin.h / .c         Coin collectible: placement, AABB collection, render
├── yellow_star.h / .c  Yellow star health pickup
├── last_star.h / .c    End-of-level star collectible
├── spider.h / .c       Spider enemy: ground patrol, animation, render
├── jumping_spider.h/.c Jumping spider: patrol, jump arcs, sea-gap awareness
├── bird.h / .c         Slow bird enemy: sine-wave sky patrol, animation
├── faster_bird.h / .c  Fast bird enemy: tighter sine-wave, faster animation
├── fish.h / .c         Fish enemy: patrol, random jump arcs, render
├── faster_fish.h / .c  Fast fish enemy: higher jumps, faster patrol
├── blue_flame.h / .c   Blue flame hazard: erupts from sea gaps, rise/flip/fall cycle
├── spike.h / .c        Static ground spike hazard rows
├── spike_block.h / .c  Rail-riding rotating spike hazard
├── spike_platform.h/.c Elevated spike surface hazard
├── circular_saw.h / .c Fast rotating patrol saw hazard
├── axe_trap.h / .c     Swinging/spinning axe hazard
├── bouncepad.h / .c    Shared bouncepad mechanics (squash/release animation)
├── bouncepad_small.h/.c Green bouncepad (small jump)
├── bouncepad_medium.h/.c Wood bouncepad (medium jump)
├── bouncepad_high.h/.c Red bouncepad (high jump)
├── rail.h / .c         Rail path builder, bitmask tile render, position interpolation
├── vine.h / .c         Climbable vine decoration
├── ladder.h / .c       Climbable ladder decoration
└── rope.h / .c         Climbable rope decoration
```

Every `.c` file in `src/` is automatically picked up by the Makefile wildcard — no changes to the build system are needed when adding new source files.

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

See [Constants Reference](constants_reference) for full details.

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
#include "player.h"          // Player struct
#include "platform.h"        // Platform struct + MAX_PLATFORMS
#include "water.h"           // Water struct
#include "fog.h"             // FogSystem struct
#include "spider.h"          // Spider struct + MAX_SPIDERS
#include "fish.h"            // Fish struct + MAX_FISH
#include "coin.h"            // Coin struct + MAX_COINS
#include "vine.h"            // VineDecor struct + MAX_VINES
#include "bouncepad.h"       // Bouncepad struct (shared mechanics)
#include "bouncepad_small.h" // Small bouncepad
#include "bouncepad_medium.h"// Medium bouncepad
#include "bouncepad_high.h"  // High bouncepad
#include "hud.h"             // Hud struct
#include "parallax.h"        // ParallaxSystem
#include "rail.h"            // Rail, RailTile
#include "spike_block.h"     // SpikeBlock
#include "float_platform.h"  // FloatPlatform
#include "bridge.h"          // Bridge
#include "jumping_spider.h"  // JumpingSpider
#include "bird.h"            // Bird
#include "faster_bird.h"     // FasterBird
#include "yellow_star.h"     // YellowStar
#include "axe_trap.h"        // AxeTrap
#include "circular_saw.h"    // CircularSaw
#include "blue_flame.h"      // BlueFlame
#include "ladder.h"          // LadderDecor
#include "rope.h"            // RopeDecor
#include "faster_fish.h"     // FasterFish
#include "last_star.h"       // LastStar
#include "spike.h"           // SpikeRow
#include "spike_platform.h"  // SpikePlatform
#include "debug.h"           // DebugOverlay
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

60 FPS loop: delta time → events → update → render. See [Architecture](architecture) for the full render order.

### `game_cleanup(GameState *gs)`

Frees all resources in reverse init order.

---

## `player.h` / `player.c`

**Role:** Player character lifecycle. See [Player Module](player_module) for the deep dive.

**Key functions:** `player_init`, `player_handle_input`, `player_update`, `player_render`, `player_get_hitbox`, `player_reset`, `player_cleanup`

---

## `sandbox.h` / `sandbox.c`

**Role:** Level layout and entity placement. Separates content (WHERE entities are placed) from engine logic (HOW they behave).

**Key functions:**
- `sandbox_load_level(GameState *gs)` — place all entities, define sea gaps, set up the level (called once from `game_init`)
- `sandbox_reset_level(GameState *gs, int *fp_prev_riding)` — reset all entities on player death

---

## `start_menu.h` / `start_menu.c`

**Role:** Start menu screen with centred title text and `start_menu_logo.png` logo.

**Key functions:** `start_menu_init`, `start_menu_loop`, `start_menu_cleanup`

---

## Enemy Modules

### `spider.h` / `spider.c`
Ground-patrol spider enemy with 3-frame walk animation. Reverses at patrol boundaries and respects sea gaps. Asset: `spider.png`.

### `jumping_spider.h` / `jumping_spider.c`
Faster spider variant that periodically jumps in short arcs to clear sea gaps. Asset: `jumping_spider.png`.

### `bird.h` / `bird.c`
Slow sine-wave sky patrol bird. Asset: `bird.png`.

### `faster_bird.h` / `faster_bird.c`
Fast aggressive sine-wave sky patrol bird with tighter curves and quicker wing animation. Asset: `faster_bird.png`.

### `fish.h` / `fish.c`
Jumping water enemy that patrols the bottom lane and leaps on random arcs. Asset: `fish.png`.

### `faster_fish.h` / `faster_fish.c`
Fast fish variant with higher jumps and faster patrol speed. Asset: `faster_fish.png`.

---

## Hazard Modules

### `blue_flame.h` / `blue_flame.c`
Erupting fire hazard from sea gaps. Cycles through waiting → rising → flipping (180 degree rotation at apex) → falling. 2-frame animation. Asset: `blue_flame.png`.

### `spike.h` / `spike.c`
Static ground spike rows placed along the floor. Asset: `spike.png`.

### `spike_block.h` / `spike_block.c`
Rail-riding rotating hazard (360 degrees/s spin). Travels along rail paths, pushes player on collision. Asset: `spike_block.png`.

### `spike_platform.h` / `spike_platform.c`
Elevated spike surface hazard. 3-slice rendered. Asset: `spike_platform.png`.

### `circular_saw.h` / `circular_saw.c`
Fast rotating patrol saw hazard (720 degrees/s). Patrols horizontally. Asset: `circular_saw.png`.

### `axe_trap.h` / `axe_trap.c`
Swinging pendulum or spinning axe hazard. Two behaviour modes: swing (60 degree amplitude) and spin (180 degrees/s). Asset: `axe_trap.png`.

---

## Collectible Modules

### `coin.h` / `coin.c`
Gold coin collectible. AABB pickup awards 100 points. Every 3 coins restores one heart. Asset: `coin.png`.

### `yellow_star.h` / `yellow_star.c`
Yellow star health pickup that restores hearts. Asset: `yellow_star.png`.

### `last_star.h` / `last_star.c`
End-of-level star collectible. Asset: `last_star.png`.

---

## Platform Modules

### `platform.h` / `platform.c`
One-way pillar platforms built from 9-slice tiled grass blocks. Player can jump through from below and land on top. Asset: `platform.png`.

### `float_platform.h` / `float_platform.c`
Hovering surfaces with three modes: static, crumble (falls after 0.75s), and rail (follows a rail path). 3-slice rendered. Asset: `float_platform.png`.

### `bridge.h` / `bridge.c`
Tiled crumble walkway. Bricks cascade-fall outward from the player's feet after a short delay. Asset: `bridge.png`.

### `bouncepad.h` / `bouncepad.c`
Shared bouncepad mechanics: squash/release 3-frame animation.

### `bouncepad_small.h` / `bouncepad_small.c`
Green bouncepad — small jump height. Asset: `bouncepad_small.png`.

### `bouncepad_medium.h` / `bouncepad_medium.c`
Wood bouncepad — medium jump height. Asset: `bouncepad_medium.png`.

### `bouncepad_high.h` / `bouncepad_high.c`
Red bouncepad — high jump height. Asset: `bouncepad_high.png`.

---

## Decoration Modules

### `vine.h` / `vine.c`
Climbable vine decoration. Player can grab, climb up/down, drift horizontally, and dismount with a jump. Asset: `vine.png`.

### `ladder.h` / `ladder.c`
Climbable ladder decoration. Same climbing mechanics as vines. Asset: `ladder.png`.

### `rope.h` / `rope.c`
Climbable rope decoration. Same climbing mechanics as vines. Asset: `rope.png`.

---

## Environment Modules

### `water.h` / `water.c`
Animated scrolling water strip at the bottom of the screen. 8 frames tiled seamlessly. Asset: `water.png`.

### `fog.h` / `fog.c`
Atmospheric fog overlay. Two semi-transparent sky layers slide across the screen with random direction, duration, and fade-in/out. Assets: `fog_background_1.png`, `fog_background_2.png`.

### `parallax.h` / `parallax.c`
Multi-layer scrolling background. 7 layers tiled horizontally, each at a different scroll speed. Assets: `parallax_sky.png`, `parallax_clouds_bg.png`, `parallax_glacial_mountains.png`, `parallax_clouds_mg_1/2/3.png`, `parallax_cloud_lonely.png`.

---

## System Modules

### `rail.h` / `rail.c`
Rail path system. Builds closed-loop and open-line rail paths from tile definitions. 4x4 bitmask tileset for rendering. Used by spike blocks and float platforms. Asset: `rail.png`.

### `hud.h` / `hud.c`
HUD renderer. Draws heart icons (health), player icon + lives counter, coin icon + score. Assets: `yellow_star.png` (hearts), `hud_coins.png` (coin icon), `player.png` (lives icon), `round9x13.ttf` (font).

### `debug.h` / `debug.c`
Debug overlay (activated with `--debug` flag). FPS counter, collision hitbox visualization for all entities, and a scrolling event log.
