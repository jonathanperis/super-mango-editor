# Architecture

<a id="home"></a>

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
  │     │
  │     │   ── Load all textures (engine resources) ──
  │     ├── parallax_init(&gs.parallax, gs.renderer)  (multi-layer background)
  │     ├── IMG_LoadTexture → gs.floor_tile        (grass_tileset.png — fatal)
  │     ├── IMG_LoadTexture → gs.platform_tex      (platform.png — fatal)
  │     ├── water_init(&gs.water, gs.renderer)      (water.png)
  │     ├── IMG_LoadTexture → gs.spider_tex        (spider.png — fatal)
  │     ├── IMG_LoadTexture → gs.jumping_spider_tex (jumping_spider.png — fatal)
  │     ├── IMG_LoadTexture → gs.bird_tex          (bird.png — fatal)
  │     ├── IMG_LoadTexture → gs.faster_bird_tex   (faster_bird.png — fatal)
  │     ├── IMG_LoadTexture → gs.fish_tex          (fish.png — fatal)
  │     ├── IMG_LoadTexture → gs.coin_tex          (coin.png — fatal)
  │     ├── IMG_LoadTexture → gs.bouncepad_medium_tex (bouncepad_medium.png — fatal)
  │     ├── IMG_LoadTexture → gs.vine_tex          (vine.png — non-fatal)
  │     ├── IMG_LoadTexture → gs.ladder_tex        (ladder.png — non-fatal)
  │     ├── IMG_LoadTexture → gs.rope_tex          (rope.png — non-fatal)
  │     ├── IMG_LoadTexture → gs.bouncepad_small_tex  (bouncepad_small.png — non-fatal)
  │     ├── IMG_LoadTexture → gs.bouncepad_high_tex   (bouncepad_high.png — non-fatal)
  │     ├── IMG_LoadTexture → gs.rail_tex          (rail.png — non-fatal)
  │     ├── IMG_LoadTexture → gs.spike_block_tex   (spike_block.png — non-fatal)
  │     ├── IMG_LoadTexture → gs.float_platform_tex (float_platform.png — non-fatal)
  │     ├── IMG_LoadTexture → gs.bridge_tex        (bridge.png — non-fatal)
  │     ├── IMG_LoadTexture → gs.yellow_star_tex   (yellow_star.png — non-fatal)
  │     ├── IMG_LoadTexture → gs.axe_trap_tex      (axe_trap.png — non-fatal)
  │     ├── IMG_LoadTexture → gs.circular_saw_tex  (circular_saw.png — non-fatal)
  │     ├── IMG_LoadTexture → gs.blue_flame_tex    (blue_flame.png — non-fatal)
  │     ├── IMG_LoadTexture → gs.faster_fish_tex   (faster_fish.png — non-fatal)
  │     ├── IMG_LoadTexture → gs.spike_tex         (spike.png — non-fatal)
  │     ├── IMG_LoadTexture → gs.spike_platform_tex (spike_platform.png — non-fatal)
  │     │
  │     │   ── Load all sound effects ──
  │     ├── Mix_LoadWAV     → gs.snd_spring        (bouncepad.wav — non-fatal)
  │     ├── Mix_LoadWAV     → gs.snd_axe           (axe_trap.wav — non-fatal)
  │     ├── Mix_LoadWAV     → gs.snd_flap          (bird.wav — non-fatal)
  │     ├── Mix_LoadWAV     → gs.snd_spider_attack (spider.wav — non-fatal)
  │     ├── Mix_LoadWAV     → gs.snd_dive          (fish.wav — non-fatal)
  │     ├── Mix_LoadWAV     → gs.snd_jump          (player_jump.wav — fatal)
  │     ├── Mix_LoadWAV     → gs.snd_coin          (coin.wav — non-fatal)
  │     ├── Mix_LoadWAV     → gs.snd_hit           (player_hit.wav — non-fatal)
  │     ├── Mix_LoadMUS     → gs.music             (game_music.wav — fatal)
  │     ├── Mix_PlayMusic(-1)                      (loop forever, 10% volume)
  │     │
  │     │   ── Initialise game objects ──
  │     ├── player_init(&gs.player, gs.renderer)
  │     ├── fog_init(&gs.fog, gs.renderer)         (fog_background_1.png, fog_background_2.png)
  │     ├── hud_init(&gs.hud, gs.renderer)
  │     ├── if (debug_mode) debug_init(&gs.debug)
  │     ├── level_load(&gs, level_path)             (TOML level file → entity inits, sea gaps, backgrounds, music)
  │     ├── hearts/lives/score/score_life_next initialisation
  │     ├── SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) — lazy init, non-fatal
  │     └── scan joysticks for first connected gamepad
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
        ├── Mix_FreeChunk (snd_spring)
        ├── Mix_FreeChunk (snd_axe)
        ├── Mix_FreeChunk (snd_flap)
        ├── Mix_FreeChunk (snd_spider_attack)
        ├── Mix_FreeChunk (snd_dive)
        ├── water_cleanup
        ├── SDL_DestroyTexture (blue_flame_tex)
        ├── SDL_DestroyTexture (axe_trap_tex)
        ├── SDL_DestroyTexture (circular_saw_tex)
        ├── SDL_DestroyTexture (spike_platform_tex)
        ├── SDL_DestroyTexture (spike_tex)
        ├── SDL_DestroyTexture (spike_block_tex)
        ├── SDL_DestroyTexture (bridge_tex)
        ├── SDL_DestroyTexture (float_platform_tex)
        ├── SDL_DestroyTexture (rail_tex)
        ├── SDL_DestroyTexture (bouncepad_high_tex)
        ├── SDL_DestroyTexture (bouncepad_medium_tex)
        ├── SDL_DestroyTexture (bouncepad_small_tex)
        ├── SDL_DestroyTexture (rope_tex)
        ├── SDL_DestroyTexture (ladder_tex)
        ├── SDL_DestroyTexture (vine_tex)
        ├── last_star_cleanup
        ├── SDL_DestroyTexture (yellow_star_tex)
        ├── SDL_DestroyTexture (coin_tex)
        ├── SDL_DestroyTexture (faster_fish_tex)
        ├── SDL_DestroyTexture (fish_tex)
        ├── SDL_DestroyTexture (faster_bird_tex)
        ├── SDL_DestroyTexture (bird_tex)
        ├── SDL_DestroyTexture (jumping_spider_tex)
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
  3. Update       — player_handle_input → player_update (incl. bouncepad, float-platform, bridge landing)
                    → bouncepad response (animation + spring sound)
                    → spiders_update → jumping_spiders_update → birds_update → faster_birds_update
                    → fish_update → faster_fish_update → spike_blocks_update → spikes_update
                    → spike_platforms_update → circular_saws_update → axe_traps_update
                    → blue_flames_update → float_platforms_update → bridges_update
                    → spider collision → jumping_spider collision → bird collision → faster_bird collision
                    → fish collision → faster_fish collision → spike_block collision (+ push impulse)
                    → spike collision → spike_platform collision → circular_saw collision
                    → axe_trap collision → blue_flame collision
                    → sea gap fall detection (instant death)
                    → coin–player collision → yellow_star–player collision → last_star–player collision
                    → heart/lives/score_life_next logic
                    → water_update → fog_update → bouncepads_update (small, medium, high)
                    → debug_update (if --debug)
  4. Render       — clear → parallax background → platforms → floor tiles
                    → float platforms → spike rows → spike platforms → bridges
                    → bouncepads (medium, small, high) → rails
                    → vines → ladders → ropes → coins → yellow stars → last star
                    → blue flames → fish → faster fish → water
                    → spike blocks → axe traps → circular saws
                    → spiders → jumping spiders → birds → faster birds
                    → player → fog → hud
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
| 1 | Background | 7 layers from `assets/` (parallax_sky.png, parallax_clouds_bg.png, parallax_glacial_mountains.png, parallax_clouds_mg_1/2/3.png, parallax_cloud_lonely.png) tiled horizontally, each scrolling at a different speed fraction of `cam_x` |
| 2 | Platforms | `platform.png` 9-slice tiled pillar stacks (drawn before floor so pillars sink into ground) |
| 3 | Floor | `grass_tileset.png` 9-slice tiled across world width at `FLOOR_Y`, with sea-gap openings |
| 4 | Float platforms | `float_platform.png` 3-slice hovering surfaces (static, crumble, rail modes) |
| 5 | Spike rows | `spike.png` ground-level spike strips on the floor surface |
| 6 | Spike platforms | `spike_platform.png` elevated spike hazard surfaces |
| 7 | Bridges | `bridge.png` tiled crumble walkways |
| 8 | Bouncepads (medium) | `bouncepad_medium.png` standard-launch spring pads |
| 9 | Bouncepads (small) | `bouncepad_small.png` low-launch spring pads |
| 10 | Bouncepads (high) | `bouncepad_high.png` high-launch spring pads |
| 11 | Rails | `rail.png` bitmask tile tracks for spike blocks and float platforms |
| 12 | Vines | `vine.png` climbable plant decorations hanging from platforms |
| 13 | Ladders | `ladder.png` climbable ladder structures |
| 14 | Ropes | `rope.png` climbable rope segments |
| 15 | Coins | `coin.png` collectible sprites drawn on top of platforms |
| 16 | Yellow stars | `yellow_star.png` collectible star pickups |
| 17 | Last star | end-of-level star collectible (uses HUD star sprite) |
| 18 | Blue flames | `blue_flame.png` animated flame hazards erupting from sea gaps |
| 19 | Fish | `fish.png` animated jumping enemies, drawn before water for submerged look |
| 20 | Faster fish | `faster_fish.png` fast aggressive jumping fish enemies |
| 21 | Water | `water.png` animated scrolling strip at the bottom |
| 22 | Spike blocks | `spike_block.png` rotating rail-riding hazards |
| 23 | Axe traps | `axe_trap.png` swinging axe hazards |
| 24 | Circular saws | `circular_saw.png` spinning blade hazards |
| 25 | Spiders | `spider.png` animated ground patrol enemies |
| 26 | Jumping spiders | `jumping_spider.png` animated jumping patrol enemies |
| 27 | Birds | `bird.png` slow sine-wave sky patrol enemies |
| 28 | Faster birds | `faster_bird.png` fast aggressive sky patrol enemies |
| 29 | Player | Animated sprite sheet, drawn on top of environment |
| 30 | Fog | `fog_background_1.png` / `fog_background_2.png` semi-transparent sliding overlay |
| 31 | HUD | `hud_render`: hearts, lives, score -- always drawn on top |
| 32 | Debug | `debug_render`: FPS counter, collision boxes, event log — when `--debug` active |

> **Note:** Additional foreground layers (fog, water overlays) can be added per-level via `[foreground_layers]` in the TOML file, adding up to 8 extra layers above the HUD. The base 32 layers are always present.

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

`SDL_RenderSetLogicalSize(renderer, 400, 300)` makes SDL scale this canvas **2x** to fill the 800x600 OS window automatically, giving the chunky pixel-art look with no changes to game logic.

---

## GameState Struct

Defined in `game.h`. The **single container** for every runtime resource.

```c
typedef struct {
    SDL_Window         *window;      // OS window handle
    SDL_Renderer       *renderer;    // GPU drawing context
    SDL_GameController *controller;  // first connected gamepad; NULL = none
    ParallaxSystem      parallax;    // multi-layer scrolling background

    SDL_Texture   *floor_tile;       // grass_tileset.png (GPU)
    SDL_Texture   *platform_tex;     // Shared tile for platform pillars (GPU)

    SDL_Texture   *spider_tex;       // Shared texture for all spiders (GPU)
    Spider         spiders[MAX_SPIDERS];
    int            spider_count;

    SDL_Texture   *jumping_spider_tex;  // Shared texture for jumping spiders (GPU)
    JumpingSpider  jumping_spiders[MAX_JUMPING_SPIDERS];
    int            jumping_spider_count;

    SDL_Texture   *bird_tex;         // Shared texture for Bird enemies (GPU)
    Bird           birds[MAX_BIRDS];
    int            bird_count;

    SDL_Texture   *faster_bird_tex;  // Shared texture for FasterBird (GPU)
    FasterBird     faster_birds[MAX_FASTER_BIRDS];
    int            faster_bird_count;

    SDL_Texture   *fish_tex;         // Shared texture for all fish enemies (GPU)
    Fish           fish[MAX_FISH];
    int            fish_count;

    SDL_Texture   *faster_fish_tex;  // Shared texture for faster fish enemies (GPU)
    FasterFish     faster_fish[MAX_FASTER_FISH];
    int            faster_fish_count;

    SDL_Texture   *coin_tex;         // Shared texture for all coin collectibles (GPU)
    Coin           coins[MAX_COINS];
    int            coin_count;

    SDL_Texture   *yellow_star_tex;  // Shared texture for yellow star pickups (GPU)
    YellowStar     yellow_stars[MAX_YELLOW_STARS];
    int            yellow_star_count;

    LastStar       last_star;        // Special end-of-level star collectible

    SDL_Texture   *vine_tex;         // Shared texture for vine decorations (GPU)
    VineDecor      vines[MAX_VINES];
    int            vine_count;

    SDL_Texture   *ladder_tex;       // Shared texture for ladders (GPU)
    LadderDecor    ladders[MAX_LADDERS];
    int            ladder_count;

    SDL_Texture   *rope_tex;         // Shared texture for ropes (GPU)
    RopeDecor      ropes[MAX_ROPES];
    int            rope_count;

    SDL_Texture   *bouncepad_small_tex;    // Shared texture for small bouncepads (GPU)
    Bouncepad      bouncepads_small[MAX_BOUNCEPADS_SMALL];
    int            bouncepad_small_count;

    SDL_Texture   *bouncepad_medium_tex;   // Shared texture for medium bouncepads (GPU)
    Bouncepad      bouncepads_medium[MAX_BOUNCEPADS_MEDIUM];
    int            bouncepad_medium_count;

    SDL_Texture   *bouncepad_high_tex;     // Shared texture for high bouncepads (GPU)
    Bouncepad      bouncepads_high[MAX_BOUNCEPADS_HIGH];
    int            bouncepad_high_count;

    SDL_Texture   *rail_tex;         // Shared texture for all rail tiles (GPU)
    Rail           rails[MAX_RAILS];
    int            rail_count;

    SDL_Texture   *spike_block_tex;  // Shared texture for spike blocks (GPU)
    SpikeBlock     spike_blocks[MAX_SPIKE_BLOCKS];
    int            spike_block_count;

    SDL_Texture   *spike_tex;        // Shared texture for ground spikes (GPU)
    SpikeRow       spike_rows[MAX_SPIKE_ROWS];
    int            spike_row_count;

    SDL_Texture   *spike_platform_tex;  // Shared texture for spike platforms (GPU)
    SpikePlatform  spike_platforms[MAX_SPIKE_PLATFORMS];
    int            spike_platform_count;

    SDL_Texture   *circular_saw_tex;    // Shared texture for circular saws (GPU)
    CircularSaw    circular_saws[MAX_CIRCULAR_SAWS];
    int            circular_saw_count;

    SDL_Texture   *axe_trap_tex;        // Shared texture for axe traps (GPU)
    AxeTrap        axe_traps[MAX_AXE_TRAPS];
    int            axe_trap_count;

    SDL_Texture   *blue_flame_tex;      // Shared texture for blue flames (GPU)
    BlueFlame      blue_flames[MAX_BLUE_FLAMES];
    int            blue_flame_count;

    SDL_Texture   *float_platform_tex;  // float_platform.png 3-slice (GPU)
    FloatPlatform  float_platforms[MAX_FLOAT_PLATFORMS];
    int            float_platform_count;

    SDL_Texture   *bridge_tex;       // bridge.png tile (GPU)
    Bridge         bridges[MAX_BRIDGES];
    int            bridge_count;

    Mix_Chunk     *snd_jump;         // Player jump sound effect (WAV)
    Mix_Chunk     *snd_coin;         // Coin collect sound effect (WAV)
    Mix_Chunk     *snd_hit;          // Player hurt sound effect (WAV)
    Mix_Chunk     *snd_spring;       // Bouncepad spring sound effect (WAV)
    Mix_Chunk     *snd_axe;          // Axe trap swing sound effect (WAV)
    Mix_Chunk     *snd_flap;         // Bird flap sound effect (WAV)
    Mix_Chunk     *snd_spider_attack;// Spider attack sound effect (WAV)
    Mix_Chunk     *snd_dive;         // Fish dive sound effect (WAV)
    Mix_Music     *music;            // Background music stream (WAV)

    Player         player;           // Player data — stored by value
    Platform       platforms[MAX_PLATFORMS];
    int            platform_count;
    Water          water;            // Animated water strip at the bottom
    FogSystem      fog;              // Atmospheric fog overlay — topmost layer

    int            sea_gaps[MAX_SEA_GAPS];
    int            sea_gap_count;

    Hud            hud;              // HUD display: hearts, lives, score
    int            hearts;           // Current hit points (0-MAX_HEARTS)
    int            lives;            // Remaining lives; <0 triggers game over
    int            score;            // Cumulative score from collecting coins/stars
    int            score_life_next;  // Score threshold for next bonus life

    Camera         camera;           // Viewport scroll position; updated every frame
    int            running;          // Loop flag: 1 = keep going, 0 = quit
    int            paused;           // 1 = window lost focus; physics/music frozen
    int            debug_mode;       // 1 = debug overlays active (--debug flag)
    DebugOverlay   debug;            // FPS counter, collision vis, event log

    // ---- Loop state (persists across frames for emscripten callback) ----
    Uint64         loop_prev_ticks;  // timestamp of previous frame
    int            fp_prev_riding;   // float platform player stood on last frame
} GameState;
```

**Key design decisions:**

- `Player` is **embedded by value**, not a pointer. This avoids a heap allocation and keeps the struct self-contained. The same applies to `Platform`, `Water`, `FogSystem`, and all entity arrays.
- Every pointer is set to `NULL` after freeing, making accidental double-frees safe.
- Initialised with `GameState gs = {0}` so every field starts as `0` / `NULL`.

---

## Error Handling Strategy

| Situation | Action |
|-----------|--------|
| SDL subsystem init failure (in `main`) | `fprintf(stderr, ...)` → clean up already-inited subsystems → `return EXIT_FAILURE` |
| Resource load failure (in `game_init`) | `fprintf(stderr, ...)` → destroy already-created resources → `exit(EXIT_FAILURE)` |
| Sound load failure (non-fatal pattern) | `fprintf(stderr, ...)` then continue -- play is guarded by `if (snd_jump)` |
| Optional texture load failure (non-fatal) | `fprintf(stderr, ...)` then continue -- render is guarded by `if (texture)` |

All SDL error strings are retrieved with `SDL_GetError()`, `IMG_GetError()`, or `Mix_GetError()` and printed to `stderr`.
