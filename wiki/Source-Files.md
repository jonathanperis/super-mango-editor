# Source Files

← [Home](Home)

---

## File Map

```
src/
├── main.c        Entry point — SDL subsystem lifecycle
├── game.h        Shared constants + GameState struct (included everywhere)
├── game.c        Window, renderer, parallax background, game loop
├── player.h      Player struct + function declarations
├── player.c      Player input, physics, animation, rendering
├── platform.h    Platform struct + MAX_PLATFORMS constant
├── platform.c    One-way platform pillar init and 9-slice rendering
├── water.h       Water struct + animation/scroll constants
├── water.c       Animated water strip: init, scroll, tile render
├── spider.h      Spider struct + patrol/animation constants
├── spider.c      Spider enemy: patrol movement, animation, render
├── fog.h         FogSystem struct + instance pool
├── fog.c         Atmospheric fog overlay: init, slide, spawn, render
├── parallax.h    ParallaxSystem struct + PARALLAX_MAX_LAYERS constant
├── parallax.c    Multi-layer scrolling background: init, tiled render, cleanup
├── coin.h        Coin struct + constants (MAX_COINS, COIN_SCORE, …)
├── coin.c        Coin placement, AABB collection, render
├── vine.h        VineDecor struct + MAX_VINES / VINE_W / VINE_H constants
├── vine.c        Static vine decoration: init and render
├── fish.h        Fish struct + patrol / jump / animation constants
├── fish.c        Jumping fish enemy: patrol, random jump arcs, render
├── hud.h         Hud struct (font + star texture) + HUD constants
└── hud.c         HUD renderer: hearts, lives counter, score text
```

Every `.c` file in `src/` is automatically picked up by the Makefile wildcard — no changes to the build system are needed when adding new source files.

---

## `main.c`

**Role:** Owns the program entry point and every SDL subsystem's lifetime.

### Responsibilities
- Call `SDL_Init`, `IMG_Init`, `TTF_Init`, `Mix_OpenAudio` in order
- Allocate `GameState gs = {0}` on the stack
- Call `game_init` → `game_loop` → `game_cleanup`
- Tear down SDL subsystems in reverse order before returning

### Subsystem Init Order

| Order | Call | Purpose |
|-------|------|---------|
| 1 | `SDL_Init(SDL_INIT_VIDEO \| SDL_INIT_AUDIO)` | Core: window + audio device |
| 2 | `IMG_Init(IMG_INIT_PNG)` | PNG decoder |
| 3 | `TTF_Init()` | FreeType / font support |
| 4 | `Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048)` | Audio mixer |

On failure at any step, all previously-succeeded subsystems are torn down before returning `EXIT_FAILURE`.

### SDL_Init Flags Used

| Flag | Activates |
|------|-----------|
| `SDL_INIT_VIDEO` | Event queue, window system, renderer support |
| `SDL_INIT_AUDIO` | Platform audio device |

### Mix_OpenAudio Parameters

| Parameter | Value | Meaning |
|-----------|-------|---------|
| frequency | `44100` | CD-quality sample rate (Hz) |
| format | `MIX_DEFAULT_FORMAT` | 16-bit signed samples |
| channels | `2` | Stereo |
| chunksize | `2048` | Buffer size (samples) — controls latency |

---

## `game.h`

**Role:** The single shared header. Defines constants and `GameState`. Included by all other `.c` files.

### Constants

See [Constants Reference](Constants-Reference) for full details.

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
```

### Includes

```c
#include "player.h"     // Player struct
#include "platform.h"   // Platform struct + MAX_PLATFORMS
#include "water.h"      // Water struct
#include "fog.h"        // FogSystem struct
#include "spider.h"     // Spider struct + MAX_SPIDERS
#include "coin.h"       // Coin struct + MAX_COINS constant
#include "vine.h"       // VineDecor struct + MAX_VINES constant
#include "fish.h"       // Fish struct + MAX_FISH constant
#include "hud.h"        // Hud struct — HUD display resources
#include "parallax.h"   // ParallaxSystem — multi-layer scrolling background
```

### GameState Fields

| Field | Type | Description |
|-------|------|-------------|
| `window` | `SDL_Window *` | OS window handle |
| `renderer` | `SDL_Renderer *` | GPU drawing context |
| `controller` | `SDL_GameController *` | First connected gamepad; `NULL` when none |
| `parallax` | `ParallaxSystem` | Multi-layer scrolling background (replaces single `background` texture) |
| `floor_tile` | `SDL_Texture *` | Grass tile, 9-slice tiled across `FLOOR_Y` |
| `platform_tex` | `SDL_Texture *` | Shared tile texture for all platform pillars |
| `spider_tex` | `SDL_Texture *` | Shared texture for all spider enemies |
| `fish_tex` | `SDL_Texture *` | Shared texture for all fish enemies |
| `vine_tex` | `SDL_Texture *` | Shared texture for vine decorations |
| `snd_jump` | `Mix_Chunk *` | Jump WAV effect |
| `snd_coin` | `Mix_Chunk *` | WAV chunk played when a coin is collected |
| `snd_hit` | `Mix_Chunk *` | WAV chunk played when the player is hurt |
| `music` | `Mix_Music *` | Streaming background music (OGG) |
| `player` | `Player` | Player data — **by value**, not pointer |
| `platforms` | `Platform[MAX_PLATFORMS]` | One-way pillar definitions |
| `platform_count` | `int` | How many platforms are active |
| `water` | `Water` | Animated water strip at the bottom |
| `fog` | `FogSystem` | Atmospheric fog overlay — topmost layer |
| `spiders` | `Spider[MAX_SPIDERS]` | Ground-patrol enemy instances |
| `spider_count` | `int` | Number of active spiders |
| `coin_tex` | `SDL_Texture *` | Shared texture for all coin collectibles |
| `coins` | `Coin[MAX_COINS]` | Collectible coin instances |
| `coin_count` | `int` | Number of coins placed |
| `fish` | `Fish[MAX_FISH]` | Jumping water enemy instances |
| `fish_count` | `int` | Number of active fish |
| `vines` | `VineDecor[MAX_VINES]` | Static scenery vine instances |
| `vine_count` | `int` | Number of vine decorations placed |
| `hud` | `Hud` | HUD display: hearts, lives, score |
| `hearts` | `int` | Current hit points (0–MAX_HEARTS) |
| `lives` | `int` | Remaining lives; 0 triggers game over |
| `score` | `int` | Cumulative score from collecting coins |
| `coins_for_heart` | `int` | Coins collected toward next heart restore |
| `camera` | `Camera` | Viewport scroll position; updated every frame |
| `running` | `int` | `1` = loop running, `0` = exit |

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

Creates all runtime resources in this order:

1. `SDL_CreateWindow(WINDOW_TITLE, centered, 800×600, SHOWN)` → `gs->window`
2. `SDL_CreateRenderer(window, -1, ACCELERATED | PRESENTVSYNC)` → `gs->renderer`
3. `SDL_RenderSetLogicalSize(renderer, 400, 300)` — enables 2× scaling
4. `parallax_init(&gs->parallax, gs->renderer)` — loads multi-layer background textures (non-fatal per layer)
5. `IMG_LoadTexture(renderer, "assets/Grass_Tileset.png")` → `gs->floor_tile`
6. `IMG_LoadTexture(renderer, "assets/Grass_Oneway.png")` → `gs->platform_tex`
7. `platforms_init(gs->platforms, &gs->platform_count)` — two pillar definitions
8. `water_init(&gs->water, gs->renderer)` — loads Water.png and resets scroll
9. `IMG_LoadTexture(renderer, "assets/Spider_1.png")` → `gs->spider_tex`
10. `spiders_init(gs->spiders, &gs->spider_count)` — two patrol spiders
11. `IMG_LoadTexture(renderer, "assets/Coin.png")` → `gs->coin_tex`
12. `coins_init(gs->coins, &gs->coin_count)` — place initial coins
13. `IMG_LoadTexture(renderer, "assets/Fish_2.png")` → `gs->fish_tex` (non-fatal)
14. `fish_init(gs->fish, &gs->fish_count)` — place fish in water lane
15. `IMG_LoadTexture(renderer, "assets/Vine.png")` → `gs->vine_tex` (non-fatal)
16. `vine_init(gs->vines, &gs->vine_count)` — place vine decorations
17. `Mix_LoadWAV("sounds/jump.wav")` → `gs->snd_jump`
18. `Mix_LoadWAV("sounds/coin.wav")` → `gs->snd_coin` (non-fatal)
19. `Mix_LoadWAV("sounds/hit.wav")` → `gs->snd_hit` (non-fatal)
20. `Mix_LoadMUS("sounds/water-ambience.ogg")` → `gs->music`
21. `Mix_PlayMusic(gs->music, -1)` + `Mix_VolumeMusic(13)` — start music at ~10%
22. `player_init(&gs->player, gs->renderer)`
23. `fog_init(&gs->fog, gs->renderer)` — loads Sky_Background_1/2.png, spawns first wave
24. `hud_init(&gs->hud, gs->renderer)` — load font and heart texture
25. `gs->running = 1`

**Renderer flags:**

| Flag | Effect |
|------|--------|
| `SDL_RENDERER_ACCELERATED` | GPU rendering (not software fallback) |
| `SDL_RENDERER_PRESENTVSYNC` | `SDL_RenderPresent` blocks until vsync — prevents tearing |

### `game_loop(GameState *gs)`

The main loop. Runs until `gs->running == 0`.

```
frame_ms = 1000 / TARGET_FPS   (≈ 16 ms)
prev     = SDL_GetTicks64()

while (gs->running):
  dt = (now - prev) / 1000.0        // delta time in seconds
  prev = now

  // 1. Events
  SDL_PollEvent → SDL_QUIT or SDLK_ESCAPE → gs->running = 0
  SDL_CONTROLLERDEVICEADDED   → SDL_GameControllerOpen → gs->controller
  SDL_CONTROLLERDEVICEREMOVED → SDL_GameControllerClose → gs->controller = NULL
  SDL_CONTROLLERBUTTONDOWN (SDL_CONTROLLER_BUTTON_START) → gs->running = 0

  // 2. Update
  player_handle_input(&gs->player, gs->snd_jump, gs->controller)
  player_update(&gs->player, dt, gs->platforms, gs->platform_count)
  spiders_update(gs->spiders, gs->spider_count, dt)
  // player-spider AABB collision (with 1.5 s invincibility window)
  if hurt_timer > 0: hurt_timer -= dt
  else: for each spider → if SDL_HasIntersection(player_hitbox, spider_rect): hurt_timer = 1.5, snd_hit
  coins_update / coin–player AABB collision: award COIN_SCORE, coins_for_heart++
  if coins_for_heart >= COINS_PER_HEART && hearts < MAX_HEARTS: hearts++, coins_for_heart = 0
  if hearts <= 0: lives--, player_reset(&gs->player), hearts = MAX_HEARTS
  fish_update(gs->fish, gs->fish_count, dt)
  water_update(&gs->water, dt)
  fog_update(&gs->fog, dt)

  // 3. Render
  SDL_RenderClear
  parallax_render(&gs->parallax, gs->renderer, cam_x)
  9-slice floor tiles (Grass_Tileset.png at FLOOR_Y)
  platforms_render(platforms, platform_count, renderer, platform_tex)
  vine_render(vines, vine_count, renderer, vine_tex, cam_x)
  coins_render(coins, coin_count, renderer, coin_tex, cam_x)
  fish_render(fish, fish_count, renderer, fish_tex, cam_x)
  water_render(&water, renderer)
  spiders_render(spiders, spider_count, renderer, spider_tex)
  player_render(&player, renderer)
  fog_render(&fog, renderer)
  hud_render(&hud, renderer, player_tex, hearts, lives, score)
  SDL_RenderPresent

  // 4. Frame cap fallback
  if elapsed < frame_ms: SDL_Delay(frame_ms - elapsed)
```

**Render layer order (painter's algorithm):**

```
[1] Parallax background → [2] Floor tiles → [3] Platforms → [4] Vines →
[5] Coins → [6] Fish → [7] Water → [8] Spiders → [9] Player → [10] Fog → [11] HUD
```

### `game_cleanup(GameState *gs)`

Frees all resources in **reverse init order**:

```
hud_cleanup(&hud)
fog_cleanup(&fog)
player_cleanup
Mix_HaltMusic → Mix_FreeMusic(music) → music = NULL
Mix_FreeChunk(snd_jump)              → snd_jump = NULL
Mix_FreeChunk(snd_coin)              → snd_coin = NULL
Mix_FreeChunk(snd_hit)               → snd_hit = NULL
water_cleanup(&water)
SDL_DestroyTexture(coin_tex)         → coin_tex = NULL
SDL_DestroyTexture(vine_tex)         → vine_tex = NULL
SDL_DestroyTexture(fish_tex)         → fish_tex = NULL
SDL_DestroyTexture(spider_tex)       → spider_tex = NULL
SDL_DestroyTexture(platform_tex)     → platform_tex = NULL
SDL_DestroyTexture(floor_tile)       → floor_tile = NULL
parallax_cleanup(&parallax)
SDL_GameControllerClose(controller)  → controller = NULL  (skipped if NULL)
SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER)
SDL_DestroyRenderer(renderer)        → renderer = NULL
SDL_DestroyWindow(window)            → window = NULL
```

Setting each pointer to `NULL` after freeing ensures that:
- Double-free bugs are silent no-ops (`SDL_Destroy*` on `NULL` is safe)
- Dangling pointers can be detected with a simple `NULL` check

---

## `player.h`

**Role:** Public interface for the player module. Defines the `Player` struct and the `AnimState` enum, and declares the five lifecycle functions.

### `AnimState` Enum

```c
typedef enum {
    ANIM_IDLE = 0,   // Row 0 on the sprite sheet — 4 frames
    ANIM_WALK,       // Row 1                     — 4 frames
    ANIM_JUMP,       // Row 2                     — 2 frames
    ANIM_FALL        // Row 3                     — 1 frame
} AnimState;
```

Values map directly to the sprite sheet **row index** via the `ANIM_ROW[]` table in `player.c`.

### `Player` Struct Fields

| Field | Type | Description |
|-------|------|-------------|
| `x` | `float` | Horizontal position (top-left, logical px) |
| `y` | `float` | Vertical position (top-left, logical px) |
| `vx` | `float` | Horizontal velocity (px/s) |
| `vy` | `float` | Vertical velocity (px/s; negative = up) |
| `speed` | `float` | Max horizontal speed (160 px/s) |
| `w` | `int` | Display width (48 px) |
| `h` | `int` | Display height (48 px) |
| `on_ground` | `int` | `1` if standing on floor, `0` if airborne |
| `anim_state` | `AnimState` | Active animation (IDLE/WALK/JUMP/FALL) |
| `anim_frame_index` | `int` | Current frame within the animation |
| `anim_timer_ms` | `Uint32` | Accumulated ms in the current frame |
| `facing_left` | `int` | `1` = flip sprite horizontally |
| `hurt_timer` | `float` | Seconds of invincibility remaining; `0` = normal |
| `frame` | `SDL_Rect` | Source rect cut from the sprite sheet |
| `texture` | `SDL_Texture *` | GPU handle to `Player.png` |

### Function Declarations

```c
void player_init(Player *player, SDL_Renderer *renderer);
void player_handle_input(Player *player, Mix_Chunk *snd_jump,
                         SDL_GameController *ctrl);
void player_update(Player *player, float dt, const Platform *platforms, int platform_count);
void player_render(Player *player, SDL_Renderer *renderer);
SDL_Rect player_get_hitbox(const Player *player);
void player_reset(Player *player);
void player_cleanup(Player *player);
```

---

## `player.c`

See [Player Module](Player-Module) for a full deep dive into input, physics, and animation.

### Local Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `FRAME_W` | `48` | Width of one sprite frame (pixels) |
| `FRAME_H` | `48` | Height of one sprite frame (pixels) |
| `FLOOR_SINK` | `16` | Pixels the player overlaps the floor visually |

### Animation Tables (static arrays)

```c
static const int ANIM_FRAME_COUNT[4] = { 4,   4,   2,   1   };
static const int ANIM_FRAME_MS[4]    = { 150, 100, 150, 200 };
static const int ANIM_ROW[4]         = { 0,   1,   2,   3   };
```

Indexed by `AnimState` value (0–3).

---

## `platform.h`

**Role:** Defines the `Platform` struct and declares init/render functions for one-way elevated surfaces.

### Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `MAX_PLATFORMS` | `8` | Maximum number of platforms in the game |

### `Platform` Struct Fields

| Field | Type | Description |
|-------|------|-------------|
| `x` | `float` | Left edge in logical pixels |
| `y` | `float` | Top edge (landing surface) in logical pixels |
| `w` | `int` | Width in logical pixels |
| `h` | `int` | Height in logical pixels |

### Function Declarations

```c
void platforms_init(Platform *platforms, int *count);
void platforms_render(const Platform *platforms, int count,
                      SDL_Renderer *renderer, SDL_Texture *tex);
```

---

## `platform.c`

**Role:** Implements one-way platform pillar initialisation and 9-slice rendering.

### Platform Definitions

Two pillar stacks are placed on the floor:

| Platform | X | Width | Height | Top surface Y |
|----------|---|-------|--------|---------------|
| 0 | 80 | 48 | 96 | `FLOOR_Y - 96 = 156` |
| 1 | 256 | 48 | 144 | `FLOOR_Y - 144 = 108` |

### 9-Slice Tile Rendering

Each platform is rendered using the same 9-slice logic as the floor — the 48×48 Grass_Oneway.png tile is split into a 3×3 grid of 16×16 pieces (P = 16). Left/right caps carry the border outline; centre pieces tile seamlessly.

---

## `water.h`

**Role:** Defines the `Water` struct and constants for the animated water strip at the bottom of the screen.

### Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `WATER_FRAMES` | `8` | Total animation frames in `Water.png` |
| `WATER_FRAME_W` | `48` | Full slot width per frame in the sheet (px) |
| `WATER_ART_DX` | `16` | Left offset to art within each slot |
| `WATER_ART_W` | `16` | Width of actual visible art per frame |
| `WATER_ART_Y` | `17` | First visible row in each frame |
| `WATER_ART_H` | `31` | Height of visible art pixels |
| `WATER_PERIOD` | `128` | Pattern repeat distance (`WATER_FRAMES × WATER_ART_W`) |
| `WATER_SCROLL_SPEED` | `40.0f` | Rightward scroll speed (px/s) |

### `Water` Struct Fields

| Field | Type | Description |
|-------|------|-------------|
| `texture` | `SDL_Texture *` | GPU handle for `Water.png` |
| `scroll_x` | `float` | Rightward offset, wraps at `WATER_PERIOD` |

### Function Declarations

```c
void water_init(Water *w, SDL_Renderer *renderer);
void water_update(Water *w, float dt);
void water_render(const Water *w, SDL_Renderer *renderer);
void water_cleanup(Water *w);
```

---

## `water.c`

**Role:** Implements the animated water strip — loading, scrolling, and seamless tile rendering.

### Rendering Strategy

Each tile is cropped from the sheet at `{ frame*48+16, 17, 16, 31 }` — extracting only the 16×31 px of visible art from each 48×64 frame slot. Tiles are placed every 16 px across the full canvas width, cycling through all 8 frames in order. The pattern repeats every 128 px. A continuous rightward scroll at 40 px/s creates a seamless flowing water effect.

---

## `spider.h`

**Role:** Defines the `Spider` struct and constants for ground-patrol spider enemies.

### Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `MAX_SPIDERS` | `4` | Maximum simultaneous spiders |
| `SPIDER_FRAMES` | `4` | Animation frames in `Spider_1.png` |
| `SPIDER_FRAME_W` | `48` | Width of one frame slot in the sheet (px) |
| `SPIDER_ART_Y` | `22` | First visible row in each frame slot |
| `SPIDER_ART_H` | `10` | Height of visible art (rows 22–31) |
| `SPIDER_SPEED` | `50.0f` | Walk speed (px/s) |
| `SPIDER_FRAME_MS` | `150` | Milliseconds per animation frame |

### `Spider` Struct Fields

| Field | Type | Description |
|-------|------|-------------|
| `x` | `float` | Left edge of the 48-px frame slot in logical space |
| `vx` | `float` | Horizontal velocity (px/s); sign encodes direction |
| `patrol_x0` | `float` | Left patrol boundary |
| `patrol_x1` | `float` | Right patrol boundary |
| `frame_index` | `int` | Current animation frame (0–3) |
| `anim_timer_ms` | `Uint32` | Accumulator driving frame advances |

### Function Declarations

```c
void spiders_init(Spider *spiders, int *count);
void spiders_update(Spider *spiders, int count, float dt);
void spiders_render(const Spider *spiders, int count,
                    SDL_Renderer *renderer, SDL_Texture *tex);
```

---

## `spider.c`

**Role:** Implements spider enemy patrol movement, animation, and rendering.

### Spider Definitions

Two spiders patrol the ground floor:

| Spider | Patrol range | Start dir | Frame offset |
|--------|-------------|-----------|-------------|
| 0 | x = 20..190 | Right | 0 |
| 1 | x = 220..370 | Left | 2 |

### Rendering

The source rect crops to the art band `{ frame*48, 22, 48, 10 }`. Spiders are bottom-aligned at `FLOOR_Y` and flipped horizontally via `SDL_RenderCopyEx` when walking left (`vx < 0`).

---

## `fog.h`

**Role:** Defines the `FogSystem` and `FogInstance` structs for the atmospheric fog overlay.

### Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `FOG_TEX_COUNT` | `2` | Number of fog texture assets in rotation |
| `FOG_MAX` | `4` | Maximum concurrent fog instances |

### `FogInstance` Struct Fields

| Field | Type | Description |
|-------|------|-------------|
| `active` | `int` | `1` if currently animating, `0` if free |
| `tex_index` | `int` | Which texture to draw (0 or 1) |
| `x` | `float` | Current horizontal offset (logical px) |
| `duration` | `float` | Total travel time in seconds (2.0–3.0) |
| `elapsed` | `float` | Seconds since spawn |
| `dir` | `int` | `+1` = left→right, `-1` = right→left |
| `spawned_next` | `int` | `1` once the next wave has been triggered |

### `FogSystem` Struct Fields

| Field | Type | Description |
|-------|------|-------------|
| `textures` | `SDL_Texture *[FOG_TEX_COUNT]` | GPU images for the sky assets |
| `instances` | `FogInstance[FOG_MAX]` | Pool of simultaneous fog layers |

### Function Declarations

```c
void fog_init(FogSystem *fog, SDL_Renderer *renderer);
void fog_update(FogSystem *fog, float dt);
void fog_render(FogSystem *fog, SDL_Renderer *renderer);
void fog_cleanup(FogSystem *fog);
```

---

## `fog.c`

**Role:** Implements the atmospheric fog overlay — loading sky textures, spawning semi-transparent sliding layers, and advancing/recycling instances.

### Assets Used

| Texture | File |
|---------|------|
| 0 | `Sky_Background_1.png` |
| 1 | `Sky_Background_2.png` |

Each fog instance picks a random texture, direction, and duration (2–3 seconds) when spawned, then slides across the canvas at a constant speed. A 1-second overlap between consecutive waves ensures continuous coverage. Fog is rendered as the topmost layer, after the player sprite.

---

## `parallax.h`

**Role:** Defines the `ParallaxSystem` and `ParallaxLayer` structs for the multi-layer scrolling background.

### Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `PARALLAX_MAX_LAYERS` | `8` | Maximum number of background layers the system can hold |

### `ParallaxLayer` Struct Fields

| Field | Type | Description |
|-------|------|-------------|
| `texture` | `SDL_Texture *` | GPU image handle; `NULL` if the asset failed to load |
| `tex_w` | `int` | Natural width of the loaded PNG in pixels |
| `tex_h` | `int` | Natural height of the loaded PNG in pixels |
| `speed` | `float` | Parallax fraction: `0.0` = static, `0.5` = half camera speed, `1.0` = full world speed |

### `ParallaxSystem` Struct Fields

| Field | Type | Description |
|-------|------|-------------|
| `layers` | `ParallaxLayer[PARALLAX_MAX_LAYERS]` | All configured background layers |
| `count` | `int` | Number of layers actually configured |

### Function Declarations

```c
void parallax_init(ParallaxSystem *ps, SDL_Renderer *renderer);
void parallax_render(const ParallaxSystem *ps, SDL_Renderer *renderer, int cam_x);
void parallax_cleanup(ParallaxSystem *ps);
```

---

## `parallax.c`

**Role:** Implements multi-layer parallax scrolling — loading layer textures, tiling them horizontally, and scrolling each at a configurable fraction of the camera offset.

Each layer scrolls at `speed × cam_x` pixels. Layers with lower `speed` values appear further away; `speed = 0.0` produces a completely static background. Each layer is tiled seamlessly: `offset = (int)(cam_x × speed) % tex_w`, then drawn from `-offset` rightward to cover `GAME_W`. Missing PNGs print a warning and leave the layer's texture `NULL` so the game continues without that layer.

---

## `coin.h`

**Role:** Defines the `Coin` struct and constants for collectible coin items.

### Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `MAX_COINS` | `24` | Maximum simultaneous coins on screen |
| `COIN_DISPLAY_W` | `16` | Render width in logical pixels |
| `COIN_DISPLAY_H` | `16` | Render height in logical pixels |
| `COIN_SCORE` | `100` | Score awarded per coin collected |
| `COINS_PER_HEART` | `3` | Coins needed to restore one heart |

### `Coin` Struct Fields

| Field | Type | Description |
|-------|------|-------------|
| `x` | `float` | Horizontal position in logical pixels (top-left) |
| `y` | `float` | Vertical position in logical pixels (top-left) |
| `active` | `int` | `1` = visible and collectible, `0` = already collected |

### Function Declarations

```c
void coins_init(Coin *coins, int *count);
void coins_render(const Coin *coins, int count,
                  SDL_Renderer *renderer, SDL_Texture *tex);
```

---

## `coin.c`

**Role:** Implements coin placement, AABB-based collection, and rendering.

Coins are placed on the ground and on platforms at `coins_init`. Each frame, `game_loop` performs an AABB test between the player hitbox and each active coin's `COIN_DISPLAY_W × COIN_DISPLAY_H` rect. On overlap, the coin's `active` flag is cleared, `COIN_SCORE` is added to `gs->score`, and `coins_for_heart` is incremented. When `coins_for_heart` reaches `COINS_PER_HEART` and `hearts < MAX_HEARTS`, one heart is restored.

---

## `vine.h`

**Role:** Defines the `VineDecor` struct and constants for static plant decorations placed on the ground and platform tops.

### Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `MAX_VINES` | `24` | Maximum number of vine decoration instances |
| `VINE_W` | `16` | Sprite width in logical pixels |
| `VINE_H` | `48` | Sprite height in logical pixels |

### `VineDecor` Struct Fields

| Field | Type | Description |
|-------|------|-------------|
| `x` | `float` | Left edge in logical world pixels |
| `y` | `float` | Top edge in logical world pixels |

### Function Declarations

```c
void vine_init(VineDecor *vines, int *count);
void vine_render(const VineDecor *vines, int count,
                 SDL_Renderer *renderer, SDL_Texture *tex, int cam_x);
```

---

## `vine.c`

**Role:** Implements static vine/plant decoration — placement on the ground floor and platform tops, and camera-aware rendering.

Vines are purely visual: there is no update step and no collision. `vine_init` populates the `VineDecor` array with world-space positions on the floor and on select platform tops. `vine_render` blits each vine with a world-to-screen camera offset; vines are drawn after platforms but before coins, placing them behind all moving entities.

---

## `fish.h`

**Role:** Defines the `Fish` struct and constants for the jumping water enemy module.

### Constants

| Constant | Value | Type | Description |
|----------|-------|------|-------------|
| `MAX_FISH` | `4` | `int` | Maximum simultaneous fish instances |
| `FISH_FRAMES` | `2` | `int` | Horizontal frames in `Fish_2.png` (96×48 sheet) |
| `FISH_FRAME_W` | `48` | `int` | Width of one frame slot in the sheet (px) |
| `FISH_FRAME_H` | `48` | `int` | Height of one frame slot in the sheet (px) |
| `FISH_RENDER_W` | `48` | `int` | On-screen render width in logical pixels |
| `FISH_RENDER_H` | `48` | `int` | On-screen render height in logical pixels |
| `FISH_SPEED` | `70.0f` | `float` | Horizontal patrol speed (px/s) |
| `FISH_JUMP_VY` | `-280.0f` | `float` | Upward jump impulse (px/s) |
| `FISH_JUMP_MIN` | `1.4f` | `float` | Minimum seconds before next jump |
| `FISH_JUMP_MAX` | `3.0f` | `float` | Maximum seconds before next jump |
| `FISH_HITBOX_PAD_X` | `8` | `int` | Horizontal inset for fair collision box |
| `FISH_HITBOX_PAD_Y` | `8` | `int` | Vertical inset for fair collision box |
| `FISH_FRAME_MS` | `120` | `int` | Milliseconds per swim animation frame |

### `Fish` Struct Fields

| Field | Type | Description |
|-------|------|-------------|
| `x` | `float` | World-space top-left horizontal position (logical px) |
| `y` | `float` | World-space top-left vertical position (logical px) |
| `vx` | `float` | Horizontal patrol velocity (px/s) |
| `vy` | `float` | Current vertical velocity (px/s; negative = up) |
| `patrol_x0` | `float` | Left patrol boundary in world space |
| `patrol_x1` | `float` | Right patrol boundary in world space |
| `jump_timer` | `float` | Countdown in seconds until the next jump |
| `water_y` | `float` | Resting top-left Y when the fish is submerged |
| `frame_index` | `int` | Current animation frame (0 or 1) |
| `anim_timer_ms` | `Uint32` | Accumulator driving frame advances |

### Function Declarations

```c
void fish_init(Fish *fish, int *count);
void fish_update(Fish *fish, int count, float dt);
void fish_render(const Fish *fish, int count,
                 SDL_Renderer *renderer, SDL_Texture *tex, int cam_x);
SDL_Rect fish_get_hitbox(const Fish *fish);
```

---

## `fish.c`

**Role:** Implements the jumping fish enemy — patrol movement, random jump arcs, animation, and camera-aware rendering.

Fish patrol the water lane horizontally between `patrol_x0` and `patrol_x1`, reversing direction at the boundaries. A countdown timer triggers a vertical jump impulse at a random interval between `FISH_JUMP_MIN` and `FISH_JUMP_MAX` seconds; gravity pulls them back down until they return to `water_y`. The two-frame swim animation advances every `FISH_FRAME_MS` milliseconds. Fish are drawn before the water strip so the water wave art occludes their submerged portion, giving a natural surfacing look. A slightly inset hitbox (`FISH_HITBOX_PAD_X/Y`) is used for player collision tests.

---

## `hud.h`

**Role:** Defines the `Hud` struct and constants for the heads-up display overlay.

### Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `MAX_HEARTS` | `3` | Maximum hearts the player can have |
| `DEFAULT_LIVES` | `3` | Lives the player starts with |
| `HUD_MARGIN` | `4` | Pixel margin from screen edges |
| `HUD_HEART_SIZE` | `12` | Display size of each heart icon (px) |
| `HUD_HEART_GAP` | `2` | Horizontal gap between heart icons (px) |
| `HUD_ICON_SIZE` | `48` | Display size of the player icon (px) |

### `Hud` Struct Fields

| Field | Type | Description |
|-------|------|-------------|
| `font` | `TTF_Font *` | `Round9x13.ttf` loaded at its native bitmap size |
| `star_tex` | `SDL_Texture *` | Heart/health indicator icon (`Stars_Ui.png`) |

### Function Declarations

```c
void hud_init(Hud *hud, SDL_Renderer *renderer);
void hud_render(const Hud *hud, SDL_Renderer *renderer,
                SDL_Texture *player_tex,
                int hearts, int lives, int score);
void hud_cleanup(Hud *hud);
```

---

## `hud.c`

**Role:** Implements the HUD overlay — loading the font and heart icon, and rendering hearts, lives counter, and score text on every frame.

The HUD is always drawn last (layer 11), on top of the fog overlay. Three sections are rendered in the top margin:

- **Left:** `hearts` heart icons drawn using `Stars_Ui.png`, spaced `HUD_HEART_GAP` px apart
- **Centre:** first frame of `Player.png` as a player icon, followed by `x{lives}` text
- **Right:** `SCORE: {score}` text rendered with `Round9x13.ttf`
