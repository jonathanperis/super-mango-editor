# Architecture

← [Home](Home)

---

## Overview

Super Mango follows a classic **init → loop → cleanup** pattern. A single `GameState` struct is the owner of every resource in the game and is passed by pointer to every function that needs to read or modify it.

---

## Startup Sequence

```
main()
  ├── SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)
  ├── IMG_Init(IMG_INIT_PNG)
  ├── TTF_Init()
  ├── Mix_OpenAudio(44100, stereo, 2048 buffer)
  │
  ├── game_init(&gs)
  │     ├── SDL_CreateWindow  → gs.window
  │     ├── SDL_CreateRenderer → gs.renderer
  │     ├── SDL_RenderSetLogicalSize(GAME_W, GAME_H)
  │     ├── parallax_init(&gs.parallax, gs.renderer)  (multi-layer background)
  │     ├── IMG_LoadTexture → gs.floor_tile    (Grass_Tileset.png)
  │     ├── IMG_LoadTexture → gs.platform_tex  (Grass_Oneway.png)
  │     ├── platforms_init(gs.platforms, &gs.platform_count)
  │     ├── water_init(&gs.water, gs.renderer)  (Water.png)
  │     ├── IMG_LoadTexture → gs.spider_tex    (Spider_1.png)
  │     ├── spiders_init(gs.spiders, &gs.spider_count)
  │     ├── IMG_LoadTexture → gs.fish_tex      (Fish_2.png)
  │     ├── fish_init(gs.fish, &gs.fish_count)
  │     ├── IMG_LoadTexture → gs.coin_tex      (Coin.png)
  │     ├── coins_init(gs.coins, &gs.coin_count)
  │     ├── IMG_LoadTexture → gs.vine_tex      (Vine.png — non-fatal)
  │     ├── vine_init(gs.vines, &gs.vine_count)
  │     ├── IMG_LoadTexture → gs.bouncepad_tex (Bouncepad_Wood.png)
  │     ├── bouncepads_init(gs.bouncepads, &gs.bouncepad_count)
  │     ├── Mix_LoadWAV     → gs.snd_spring    (spring-boing.mp3 — non-fatal)
  │     ├── IMG_LoadTexture → gs.rail_tex      (Rails.png — non-fatal)
  │     ├── rail_init(gs.rails, &gs.rail_count)
  │     ├── IMG_LoadTexture → gs.spike_block_tex (Spike_Block.png — non-fatal)
  │     ├── spike_blocks_init(gs.spike_blocks, &gs.spike_block_count, gs.rails)
  │     ├── Mix_LoadWAV     → gs.snd_jump      (jump.wav)
  │     ├── Mix_LoadWAV     → gs.snd_coin      (coin.wav — non-fatal)
  │     ├── Mix_LoadWAV     → gs.snd_hit       (hit.wav — non-fatal)
  │     ├── Mix_LoadMUS     → gs.music         (water-ambience.ogg)
  │     ├── Mix_PlayMusic(-1)                  (loop forever, 10% volume)
  │     ├── player_init(&gs.player, gs.renderer)
  │     ├── fog_init(&gs.fog, gs.renderer)     (Sky_Background_1/2.png)
  │     ├── hud_init(&gs.hud, gs.renderer)
  │     └── if (debug_mode) debug_init(&gs.debug)
  │
  ├── game_loop(&gs)          ← see Game Loop section below
  │
  └── game_cleanup(&gs)       ← reverse init order
        ├── SDL_GameControllerClose(gs->controller)  ← if non-NULL
        ├── SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER)
        ├── hud_cleanup
        ├── fog_cleanup
        ├── player_cleanup
        ├── Mix_HaltMusic + Mix_FreeMusic
        ├── Mix_FreeChunk (snd_jump)
        ├── Mix_FreeChunk (snd_coin)
        ├── Mix_FreeChunk (snd_hit)
        ├── water_cleanup
        ├── SDL_DestroyTexture (spike_block_tex)
        ├── SDL_DestroyTexture (rail_tex)
        ├── SDL_DestroyTexture (vine_tex)
        ├── Mix_FreeChunk (snd_spring)
        ├── SDL_DestroyTexture (coin_tex)
        ├── SDL_DestroyTexture (fish_tex)
        ├── SDL_DestroyTexture (spider_tex)
        ├── SDL_DestroyTexture (platform_tex)
        ├── SDL_DestroyTexture (floor_tile)
        ├── parallax_cleanup
        ├── SDL_DestroyRenderer
        └── SDL_DestroyWindow
  │
  ├── Mix_CloseAudio
  ├── TTF_Quit
  ├── IMG_Quit
  └── SDL_Quit
```

---

## Game Loop

The loop runs at **60 FPS**, capped via VSync + a manual `SDL_Delay` fallback. Each frame has four distinct phases:

```
while (gs.running) {
  1. Delta Time   — measure ms since last frame → dt (seconds)
  2. Events       — SDL_PollEvent (quit / ESC key)
                    SDL_CONTROLLERDEVICEADDED   — opens a newly plugged-in controller
                    SDL_CONTROLLERDEVICEREMOVED — closes and NULLs gs->controller when unplugged
                    SDL_CONTROLLERBUTTONDOWN (START) — sets gs->running = 0 to quit
  3. Update       — player_handle_input → player_update (incl. bouncepad landing detection)
                    → bouncepad response (animation + spring sound)
                    → spiders_update → fish_update → spike_blocks_update
                    → spider collision → fish collision → spike_block collision (+ push impulse)
                    → coin–player collision → heart/lives logic
                    → water_update → fog_update → bouncepads_update
                    → debug_update (if --debug)
  4. Render       — clear → parallax background → floor tiles → platforms
                    → bouncepads → rails → vines → coins → fish → water
                    → spike blocks → spiders → player → fog → hud
                    → debug overlay (if --debug) → present
}
```

### Delta Time

```c
Uint64 now = SDL_GetTicks64();
float  dt  = (float)(now - prev) / 1000.0f;
prev = now;
```

All velocities are expressed in **pixels per second**. Multiplying by `dt` (seconds) gives the correct displacement per frame regardless of the actual frame rate.

### Render Order (back to front)

| Layer | What | How |
|-------|------|-----|
| 1 | Background | 6 layers from `assets/Parallax/` tiled horizontally, each scrolling at a different speed fraction of `cam_x` |
| 2 | Floor | `Grass_Tileset.png` 9-slice tiled across `GAME_W` at `FLOOR_Y` |
| 3 | Platforms | `Grass_Oneway.png` 9-slice tiled pillar stacks |
| 4 | Bouncepads | `Bouncepad_Wood.png` spring pads on floor and platforms |
| 5 | Rails | `Rails.png` bitmask tile tracks for spike blocks |
| 6 | Vines | `Vine.png` static scenery on ground floor and platform tops |
| 7 | Coins | `Coin.png` collectible sprites drawn on top of platforms |
| 8 | Fish | `Fish_2.png` animated jumping enemies, drawn before water for submerged look |
| 9 | Water | `Water.png` animated scrolling strip at the bottom |
| 10 | Spike blocks | `Spike_Block.png` rotating rail-riding hazards |
| 11 | Spiders | `Spider_1.png` animated patrol enemies on the ground |
| 12 | Player | Animated sprite sheet, drawn on top of environment |
| 13 | Fog | `Sky_Background_1/2.png` semi-transparent sliding overlay |
| 14 | HUD | `hud_render`: hearts, lives, score — always drawn on top |
| 15 | Debug | `debug_render`: FPS counter, collision boxes, event log — when `--debug` active |

---

## Coordinate System

SDL's Y-axis increases **downward**. The origin (0, 0) is at the **top-left** of the logical canvas.

```
(0,0) ──────────────────► x  (GAME_W = 400)
  │
  │   LOGICAL CANVAS (400 × 300)
  │
  ▼
  y
(GAME_H = 300)
              ┌──────────────────────────────────────────┐
              │ ←──────── GAME_W (400 px) ─────────────► │
  FLOOR_Y ──►│▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│
  (300-48=252)│          Grass Tileset (48px tall)        │
              └──────────────────────────────────────────┘
```

`SDL_RenderSetLogicalSize(renderer, 400, 300)` makes SDL scale this canvas **2×** to fill the 800×600 OS window automatically, giving the chunky pixel-art look with no changes to game logic.

---

## GameState Struct

Defined in `game.h`. The **single container** for every runtime resource.

```c
typedef struct {
    SDL_Window         *window;      // OS window handle
    SDL_Renderer       *renderer;    // GPU drawing context
    SDL_GameController *controller;  // first connected gamepad; NULL = none
    ParallaxSystem      parallax;    // multi-layer scrolling background
    SDL_Texture   *floor_tile;  // Grass tile image (GPU)
    SDL_Texture   *platform_tex; // Shared tile for platform pillars (GPU)
    SDL_Texture   *spider_tex;  // Shared texture for all spiders (GPU)
    SDL_Texture   *fish_tex;    // Shared texture for all fish enemies (GPU)
    SDL_Texture   *vine_tex;    // Shared texture for vine decorations (GPU)
    Mix_Chunk    *snd_jump;    // Jump sound effect (WAV)
    Mix_Chunk    *snd_coin;    // Coin collect sound effect (WAV)
    Mix_Chunk    *snd_hit;     // Player hurt sound effect (WAV)
    Mix_Music    *music;       // Background music stream (OGG)
    Player        player;      // Player data — stored by value
    Platform      platforms[MAX_PLATFORMS]; // One-way pillar definitions
    int           platform_count;          // How many platforms are active
    Water         water;       // Animated water strip at the bottom
    FogSystem     fog;         // Atmospheric fog overlay — topmost layer
    Spider        spiders[MAX_SPIDERS]; // Ground-patrol enemy instances
    int           spider_count;         // Number of active spiders
    SDL_Texture  *coin_tex;    // Shared texture for all coin collectibles
    Coin          coins[MAX_COINS]; // Collectible coin instances
    int           coin_count;       // Number of coins placed
    Fish          fish[MAX_FISH];   // Jumping water enemy instances
    int           fish_count;
    VineDecor     vines[MAX_VINES]; // Static scenery vine instances
    int           vine_count;
    SDL_Texture  *bouncepad_tex;              // Shared texture for all bouncepads (GPU)
    Bouncepad     bouncepads[MAX_BOUNCEPADS]; // Spring launch pad instances
    int           bouncepad_count;            // Number of bouncepads placed
    Mix_Chunk    *snd_spring;  // Bouncepad spring-boing sound effect (MP3)
    SDL_Texture  *rail_tex;    // Shared texture for all rail tiles (GPU)
    Rail          rails[MAX_RAILS];       // Level rail path definitions
    int           rail_count;             // Number of active rail paths
    SDL_Texture  *spike_block_tex;        // Shared texture for spike blocks (GPU)
    SpikeBlock    spike_blocks[MAX_SPIKE_BLOCKS]; // Rail-riding hazard instances
    int           spike_block_count;              // Number of active spike blocks
    Hud           hud;         // HUD display: hearts, lives, score
    int           hearts;      // Current hit points (0–MAX_HEARTS)
    int           lives;       // Remaining lives; 0 triggers game over
    int           score;       // Cumulative score from collecting coins
    int           coins_for_heart; // Coins collected toward next heart restore
    Camera        camera;      // Viewport scroll position; updated every frame
    int           running;     // Loop flag: 1 = keep going, 0 = quit
    int           paused;      // 1 = window lost focus; physics/music frozen
    int           debug_mode;  // 1 = debug overlays active (--debug flag)
    DebugOverlay  debug;       // FPS counter, collision vis, event log
} GameState;
```

**Key design decisions:**

- `Player` is **embedded by value**, not a pointer. This avoids a heap allocation and keeps the struct self-contained. The same applies to `Platform`, `Water`, `FogSystem`, and `Spider` arrays.
- Every pointer is set to `NULL` after freeing, making accidental double-frees safe.
- Initialised with `GameState gs = {0}` so every field starts as `0` / `NULL`.

---

## Error Handling Strategy

| Situation | Action |
|-----------|--------|
| SDL subsystem init failure (in `main`) | `fprintf(stderr, ...)` → clean up already-inited subsystems → `return EXIT_FAILURE` |
| Resource load failure (in `game_init`) | `fprintf(stderr, ...)` → destroy already-created resources → `exit(EXIT_FAILURE)` |
| Sound load failure (non-fatal pattern) | `fprintf(stderr, ...)` then continue — play is guarded by `if (snd_jump)` |

All SDL error strings are retrieved with `SDL_GetError()`, `IMG_GetError()`, or `Mix_GetError()` and printed to `stderr`.
