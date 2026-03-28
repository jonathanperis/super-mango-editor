# Source Files

← [Home](Home)

---

## File Map

```
src/
├── main.c        Entry point — SDL subsystem lifecycle
├── game.h        Shared constants + GameState struct (included everywhere)
├── game.c        Window, renderer, background rendering, game loop
├── player.h      Player struct + function declarations
├── player.c      Player input, physics, animation, rendering
├── platform.h    Platform struct + MAX_PLATFORMS constant
├── platform.c    One-way platform pillar init and 9-slice rendering
├── water.h       Water struct + animation/scroll constants
├── water.c       Animated water strip: init, scroll, tile render
├── spider.h      Spider struct + patrol/animation constants
├── spider.c      Spider enemy: patrol movement, animation, render
├── fog.h         FogSystem struct + instance pool
└── fog.c         Atmospheric fog overlay: init, slide, spawn, render
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
```

### Includes

```c
#include "player.h"     // Player struct
#include "platform.h"   // Platform struct + MAX_PLATFORMS
#include "water.h"      // Water struct
#include "fog.h"        // FogSystem struct
#include "spider.h"     // Spider struct + MAX_SPIDERS
```

### GameState Fields

| Field | Type | Description |
|-------|------|-------------|
| `window` | `SDL_Window *` | OS window handle |
| `renderer` | `SDL_Renderer *` | GPU drawing context |
| `background` | `SDL_Texture *` | Forest background, uploaded to GPU |
| `floor_tile` | `SDL_Texture *` | Grass tile, 9-slice tiled across `FLOOR_Y` |
| `platform_tex` | `SDL_Texture *` | Shared tile texture for all platform pillars |
| `spider_tex` | `SDL_Texture *` | Shared texture for all spider enemies |
| `snd_jump` | `Mix_Chunk *` | Jump WAV effect |
| `music` | `Mix_Music *` | Streaming background music (OGG) |
| `player` | `Player` | Player data — **by value**, not pointer |
| `platforms` | `Platform[MAX_PLATFORMS]` | One-way pillar definitions |
| `platform_count` | `int` | How many platforms are active |
| `water` | `Water` | Animated water strip at the bottom |
| `fog` | `FogSystem` | Atmospheric fog overlay — topmost layer |
| `spiders` | `Spider[MAX_SPIDERS]` | Ground-patrol enemy instances |
| `spider_count` | `int` | Number of active spiders |
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
4. `IMG_LoadTexture(renderer, "assets/Forest_Background_0.png")` → `gs->background`
5. `IMG_LoadTexture(renderer, "assets/Grass_Tileset.png")` → `gs->floor_tile`
6. `IMG_LoadTexture(renderer, "assets/Grass_Oneway.png")` → `gs->platform_tex`
7. `platforms_init(gs->platforms, &gs->platform_count)` — two pillar definitions
8. `water_init(&gs->water, gs->renderer)` — loads Water.png and resets scroll
9. `IMG_LoadTexture(renderer, "assets/Spider_1.png")` → `gs->spider_tex`
10. `spiders_init(gs->spiders, &gs->spider_count)` — two patrol spiders
11. `Mix_LoadWAV("sounds/jump.wav")` → `gs->snd_jump`
12. `Mix_LoadMUS("sounds/water-ambience.ogg")` → `gs->music`
13. `Mix_PlayMusic(gs->music, -1)` + `Mix_VolumeMusic(13)` — start music at ~10%
14. `player_init(&gs->player, gs->renderer)`
15. `fog_init(&gs->fog, gs->renderer)` — loads Sky_Background_1/2.png, spawns first wave
16. `gs->running = 1`

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

  // 2. Update
  player_handle_input(&gs->player, gs->snd_jump)
  player_update(&gs->player, dt, gs->platforms, gs->platform_count)
  spiders_update(gs->spiders, gs->spider_count, dt)
  // player-spider AABB collision (with 1.5 s invincibility window)
  if hurt_timer > 0: hurt_timer -= dt
  else: for each spider → if SDL_HasIntersection(player_hitbox, spider_rect): hurt_timer = 1.5
  water_update(&gs->water, dt)
  fog_update(&gs->fog, dt)

  // 3. Render
  SDL_RenderClear
  SDL_RenderCopy(background, fullscreen)
  9-slice floor tiles (Grass_Tileset.png at FLOOR_Y)
  platforms_render(platforms, platform_count, renderer, platform_tex)
  water_render(&water, renderer)
  spiders_render(spiders, spider_count, renderer, spider_tex)
  player_render(&player, renderer)
  fog_render(&fog, renderer)
  SDL_RenderPresent

  // 4. Frame cap fallback
  if elapsed < frame_ms: SDL_Delay(frame_ms - elapsed)
```

**Render layer order (painter's algorithm):**

```
[1] Background → [2] Floor tiles → [3] Platforms → [4] Water →
[5] Spiders → [6] Player → [7] Fog
```

### `game_cleanup(GameState *gs)`

Frees all resources in **reverse init order**:

```
fog_cleanup(&fog)
player_cleanup
Mix_HaltMusic → Mix_FreeMusic(music) → music = NULL
Mix_FreeChunk(snd_jump)              → snd_jump = NULL
water_cleanup(&water)
SDL_DestroyTexture(spider_tex)       → spider_tex = NULL
SDL_DestroyTexture(platform_tex)     → platform_tex = NULL
SDL_DestroyTexture(floor_tile)       → floor_tile = NULL
SDL_DestroyTexture(background)       → background = NULL
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
void player_handle_input(Player *player, Mix_Chunk *snd_jump);
void player_update(Player *player, float dt, const Platform *platforms, int platform_count);
void player_render(Player *player, SDL_Renderer *renderer);
SDL_Rect player_get_hitbox(const Player *player);
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
