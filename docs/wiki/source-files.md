# Source Files

<a id="home"></a>

---

## File Map

```
src/
├── main.c                        Entry point -- SDL subsystem lifecycle
├── game.h                        Shared constants + GameState struct (included everywhere)
├── collectibles/
│   ├── coin.h / .c               Coin collectible: placement, AABB collection, render
│   ├── star_yellow.h / .c        Yellow star health pickup
│   ├── star_green.h / .c         Green star health pickup
│   ├── star_red.h / .c           Red star health pickup
│   └── last_star.h / .c          End-of-level star collectible
├── collision/
│   ├── collision_damage.h / .c   Damage checks against hazards and enemies
│   ├── floor_gap_collision.h / .c Sea-gap fall/death detection
│   └── game_collision.h / .c     Gameplay collision passes and pickups
├── core/
│   ├── debug.h / .c              Debug overlay: FPS/CPU/memory, hitboxes, event log
│   ├── entity_utils.h / .c       Shared entity helper functions
│   ├── game_state.h / .c         GameState reset helpers
│   ├── game_window.h / .c        Window/renderer setup helpers
│   ├── game_timing.h / .c        Frame timing helpers
│   ├── game_lifecycle.c          `game_init` / `game_cleanup` implementation
│   ├── game_loop.c               Main native/WASM frame loop
│   ├── game_update.h / .c        Top-level update orchestration
│   ├── game_player_step.h / .c   Player update/collision step wrapper
│   ├── game_actors.h / .c        Enemy update/render helpers
│   ├── game_hazards.h / .c       Hazard update/render helpers
│   ├── game_bouncepads.h / .c    Bouncepad update/render helpers
│   ├── game_float_platforms.h / .c Float-platform update helpers
│   ├── game_bridges.h / .c       Bridge update helpers
│   ├── game_checkpoint.h / .c    Checkpoint/respawn helpers
│   ├── game_camera.h / .c        Camera follow/lookahead helpers
│   ├── game_resources.h / .c     Texture/audio/level resource loading
│   ├── game_overlay.h / .c       Canonical pause/game-over/completion overlay state
│   └── game_completion.h / .c    Last-star completion and next-phase flow
├── editor/
│   ├── editor_main.c             Standalone editor entry point
│   ├── editor.h / .c             Editor state, events, render loop
│   ├── canvas.h / .c             Scrollable zoomable editing canvas
│   ├── palette.h / .c            Entity palette
│   ├── properties.h / .c         Per-entity property editing
│   ├── tools.h / .c              Selection and placement tools
│   ├── ui.h / .c                 Immediate-mode editor widgets
│   ├── entity_meta.h / .c        Palette/display metadata for entity types
│   ├── editor_frame.h / .c       Per-frame editor orchestration
│   ├── editor_events.h / .c      SDL event dispatch
│   ├── editor_chrome.h / .c      Toolbar/status/panel chrome
│   ├── editor_panels.h / .c      Palette/properties panel rendering
│   ├── editor_layout.h / .c      Editor layout metrics
│   ├── editor_textures.h / .c    Editor texture loading/cleanup
│   ├── editor_files.h / .c       Open/save/recent file workflows
│   ├── editor_session.h / .c     Session/autosave state
│   ├── editor_playtest.h / .c    Launch playtest from editor
│   ├── editor_clipboard.h / .c   Copy/paste support
│   ├── editor_validation.h / .c  Level validation report helpers
│   ├── editor_undo_apply.h / .c  Undo operation application
│   ├── serializer.h / .c         TOML save/load public API anchor
│   ├── serializer_emit.h / .c    TOML emission helpers
│   ├── serializer_io.h / .c      File I/O helpers for serializer
│   ├── serializer_load.c         `level_load_toml` staged parse orchestration
│   ├── serializer_load_header.h / .c        TOML header/meta and floor-gap parsing
│   ├── serializer_load_geometry.h / .c      Rails and platforms parsing
│   ├── serializer_load_collectibles.h / .c  Coin, star, last-star, next-phase parsing
│   ├── serializer_load_enemies.h / .c       Enemy placement parsing
│   ├── serializer_load_hazards.h / .c       Hazard placement parsing
│   ├── serializer_load_surfaces.h / .c      Surface placement parsing
│   ├── serializer_load_climbables.h / .c    Vine, ladder, rope parsing
│   ├── serializer_load_layers.h / .c        Background/fog/foreground layer parsing
│   ├── serializer_load_config.h / .c        Optional rule/config parsing
│   ├── serializer_parse.h / .c  Shared TOML parse utilities
│   ├── serializer_save.h / .c   TOML save implementation
│   ├── serializer_types.h / .c  Enum/string conversion helpers
│   ├── exporter.h / .c           Generated C level export
│   ├── file_dialog.h / .c        Native file dialogs
│   └── undo.h / .c               Undo/redo history
├── effects/
│   ├── fog.h / .c                Atmospheric fog overlay: init, slide, spawn, render
│   ├── game_effects.h / .c       Per-level effect reload/cleanup helpers
│   ├── parallax.h / .c           Multi-layer scrolling background: init, tiled render, cleanup
│   └── water.h / .c              Animated water strip: init, scroll, tile render
├── entities/
│   ├── bird_variant.h / .c       Shared bird/faster-bird sine-wave helpers
│   ├── spider.h / .c             Spider enemy: ground patrol, animation, render
│   ├── jumping_spider.h / .c     Jumping spider: patrol, jump arcs, floor-gap awareness
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
│   └── blue_flame.h / .c         Blue/fire flame hazards: rise/flip/fall cycle
├── input/
│   ├── game_input.h / .c         SDL keyboard/gamepad input helpers
│   ├── game_events.h / .c        SDL event handling
│   └── game_web_input.h / .c     Browser/WebAssembly input bridge
├── levels/
│   ├── level.h                   Shared level definitions
│   ├── level_loader.h / .c       TOML level loading and switching
│   ├── level_path.h / .c         Level path normalization and directory helpers
│   ├── level_physics.h / .c      Level physics override/default helpers
│   ├── level_resources.h / .c    Per-level resource reload wrappers
│   ├── level_session.h / .c      Owned active LevelDef session storage
│   ├── phase_transition.h / .c   next_phase resolution and progress helpers
│   ├── level_validate.c          LevelDef count validation
│   └── exported/                 Generated C level exports
├── player/
│   ├── player.h / .c             Public API + high-level glue
│   ├── player_internal.h         Private frame/hitbox/coyote constants
│   ├── player_lifecycle.c        Init/render/hitbox/reset/cleanup/default physics
│   ├── player_input.c            Keyboard/gamepad/climb input sampling
│   ├── player_motion.h / .c      Horizontal acceleration/friction
│   ├── player_jump.h / .c        Jump buffering, coyote time, jump cut
│   ├── player_climb.h / .c       Vine/ladder/rope grab and climb helpers
│   ├── player_surfaces.h / .c    Surface collision helpers
│   └── player_animation.h / .c   Animation state/frame selection
├── render/
│   ├── game_render.h / .c        Frame render order and layer drawing
│   └── render_overlay.c          Foreground/overlay render helpers
├── screens/
│   ├── start_menu.h / .c         Start menu screen with logo
│   └── hud.h / .c                HUD renderer: hearts, lives counter, score text
└── surfaces/
    ├── platform.h / .c           One-way platform pillar init and 9-slice rendering
    ├── float_platform.h / .c     Hovering platform: static, crumble, and rail behaviours
    ├── bridge.h / .c             Tiled crumble walkway: init, cascade-fall, render
    ├── bouncepad.h / .c          Shared bouncepad mechanics (squash/release animation)
    ├── bouncepad_small.h         Green bouncepad placement helper
    ├── bouncepad_medium.h        Wood bouncepad placement helper
    ├── bouncepad_high.h          Red bouncepad placement helper
    ├── rail.h / .c               Rail path builder, bitmask tile render, position interpolation
    ├── vine.h / .c               Climbable vine decoration
    ├── ladder.h / .c             Climbable ladder decoration
    └── rope.h / .c               Climbable rope decoration
```

New `.c` files in `src/` or recognized source subdirectories are picked up by Makefile wildcards. New source directories need Makefile wildcard, compile-rule, and clean-rule entries.

---

## `main.c`

**Role:** Owns the program entry point and every SDL subsystem's lifetime.

### Responsibilities

- Parse CLI flags: `--debug`, `--sandbox`, `--level <path>`, `--smoke-test-frames N`, and `--seed N`
- Call `SDL_Init`, `IMG_Init`, `TTF_Init`, `Mix_OpenAudio` in order
- Route to start menu, sandbox, or direct TOML level mode
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
#define FLOOR_GAP_W         32
#define MAX_FLOOR_GAPS      16
#define CAM_LOOKAHEAD_VX_FACTOR  0.20f
#define CAM_LOOKAHEAD_MAX  50.0f
#define CAM_SMOOTHING      8.0f
#define CAM_SNAP_THRESHOLD 0.5f
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
#include "collectibles/star_yellow.h"   // StarYellow
#include "collectibles/star_green.h"    // StarGreen
#include "collectibles/star_red.h"      // StarRed
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
int  game_load_next_phase(GameState *gs);
void game_complete_level(GameState *gs);
```

---

## Runtime Core (`core/game_lifecycle.c`, `core/game_loop.c`, `core/game_resources.c`)

**Role:** Runtime lifecycle and game-loop implementation lives under `src/core/`. `game_lifecycle.c` owns `game_init` / `game_cleanup`, `game_loop.c` owns `game_loop`, and resource loading/reloading lives in `game_resources.c`.

### `game_init(GameState *gs)`

Creates all runtime resources:

1. Window + renderer + logical size (400x300)
2. Shared textures for player, entities, hazards, collectibles, surfaces, HUD, and debug overlay
3. Sound effects for player actions, pickups, entities, hazards, and surface interactions
4. TOML level load from `--level` or the default `levels/00_sandbox_01.toml`
5. Level-wide resources: parallax, floor/platform tiles, foreground strip, fog, water, and music
6. Entity init: player, water, fog, HUD, debug, and level contents
7. Gamepad controller init

### `game_loop(GameState *gs)`

60 FPS loop: delta time -> events -> update -> render. See [Architecture](architecture) for the full render order.

### `game_cleanup(GameState *gs)`

Frees all resources in reverse init order.

---

## Player Module (`player/`)

**Role:** Player character lifecycle, input, horizontal motion, jumps, climbables, surface collision, animation, rendering, hitbox, and reset. Public declarations live in `player.h`; private constants live in `player_internal.h`; implementation is split across focused `.c` files. See [Player Module](#player-module-player) for the deep dive.

**Key functions:** `player_init`, `player_apply_default_physics`, `player_handle_input`, `player_update`, `player_render`, `player_get_hitbox`, `player_reset`, `player_cleanup`

---

## `levels/level.h`, `levels/level_loader.c`, `levels/level_physics.c`, `levels/level_validate.c`, `levels/phase_transition.c`

**Role:** Level schema, TOML loading, physics override application, phase switching, and count validation.

**Key functions:**
- `level_load(GameState *gs, const LevelDef *def)` -- copy a parsed level definition into runtime `GameState`
- `level_reset(GameState *gs, const LevelDef *def)` -- restore mutable level state after death/retry
- `level_load_toml(const char *path, LevelDef *def)` -- parse TOML into staging storage, run runtime validation, free TOML data, then assign the validated `LevelDef` to the caller
- `level_apply_player_physics(Player *player, const LevelDef *def)` -- reset player movement tunables to engine defaults, then apply non-negative level overrides
- `level_validate_counts(const LevelDef *level, char *err, size_t err_sz)` -- reject out-of-range array counts
- `phase_has_next`, `phase_next_path`, `phase_resolve_path` -- resolve level-completion next-phase paths

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

Erupting flame hazard from floor gaps. Cycles through waiting -> rising -> flipping (180 degree rotation at apex) -> falling. Blue and fire visual variants use `blue_flame.png` and `fire_flame.png`.

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

### `collectibles/star_yellow.h` / `collectibles/star_yellow.c`

Yellow star health pickup that restores hearts. Asset: `star_yellow.png`.

### `collectibles/star_green.h` / `collectibles/star_green.c`

Green star health pickup. Asset: `star_green.png`.

### `collectibles/star_red.h` / `collectibles/star_red.c`

Red star health pickup. Asset: `star_red.png`.

### `collectibles/last_star.h` / `collectibles/last_star.c`

End-of-level star collectible. Asset: `last_star.png`.

---

## Surface Modules (`surfaces/`)

### `surfaces/platform.h` / `surfaces/platform.c`

One-way pillar platforms built from 9-slice platform tiles. Player can jump through from below and land on top. Default asset: `grass_platform.png`.

### `surfaces/float_platform.h` / `surfaces/float_platform.c`

Hovering surfaces with three modes: static, crumble (falls after 0.75s), and rail (follows a rail path). 3-slice rendered. Asset: `float_platform.png`.

### `surfaces/bridge.h` / `surfaces/bridge.c`

Tiled crumble walkway. Bricks cascade-fall outward from the player's feet after a short delay. Asset: `bridge.png`.

### `surfaces/bouncepad.h` / `surfaces/bouncepad.c`

Shared bouncepad mechanics: squash/release 3-frame animation.

### `surfaces/bouncepad_small.h`

Green bouncepad -- small jump height. Asset: `bouncepad_small.png`.

### `surfaces/bouncepad_medium.h`

Wood bouncepad -- medium jump height. Asset: `bouncepad_medium.png`.

### `surfaces/bouncepad_high.h`

Red bouncepad -- high jump height. Asset: `bouncepad_high.png`.

### `surfaces/rail.h` / `surfaces/rail.c`

Rail path system. Builds closed-loop and open-line rail paths from tile definitions. 4x4 bitmask tileset for rendering. Used by spike blocks and float platforms. Asset: `rail.png`.

### `surfaces/vine.h` / `surfaces/vine.c`

Climbable vine decoration. Player can grab, climb up/down, drift horizontally, and dismount with a jump. Assets: `vine_green.png`, `vine_brown.png`.

### `surfaces/ladder.h` / `surfaces/ladder.c`

Climbable ladder decoration. Same climbing mechanics as vines. Asset: `ladder.png`.

### `surfaces/rope.h` / `surfaces/rope.c`

Climbable rope decoration. Same climbing mechanics as vines. Asset: `rope.png`.

---

## Environment Modules (`effects/`)

### `effects/water.h` / `effects/water.c`

Animated scrolling water strip at the bottom of the screen. 8 frames tiled seamlessly. Asset: `water.png`.

### `effects/fog.h` / `effects/fog.c`

Atmospheric fog overlay. Semi-transparent foreground layers slide across the screen with random direction, duration, and fade-in/out. Default assets: `assets/sprites/foregrounds/fog_1.png`, `fog_2.png`; volcanic levels can use `fog_fire_1.png`, `fog_fire_2.png`, and `smoke.png`.

### `effects/parallax.h` / `effects/parallax.c`

Multi-layer scrolling background configured per TOML level. Current assets include blue-sky/cloud/glacial layers plus volcanic sky, mountains, and smoke layers under `assets/sprites/backgrounds/`.

---

## System Modules

### `screens/hud.h` / `screens/hud.c`

HUD renderer. Draws heart icons (health), player icon + lives counter, coin icon + score. Assets: `star_yellow.png` (hearts), `hud_coins.png` (coin icon), `player.png` (lives icon), `round9x13.ttf` (font).

### `core/debug.h` / `core/debug.c`

Debug overlay (activated with `--debug` flag). FPS counter, collision hitbox visualization for all entities, and a scrolling event log.

### `core/entity_utils.h` / `core/entity_utils.c`

Shared entity helper functions used across multiple modules.

### `levels/level.h`

Shared level definitions and constants.

### `levels/level_loader.h` / `levels/level_loader.c`

TOML level loading and switching system.

### `levels/level_physics.h` / `levels/level_physics.c`

Shared physics override/default helpers for player movement and camera lookahead.

### `levels/level_validate.c`

Bounds validation for `LevelDef` counts before runtime data is copied into `GameState`.
