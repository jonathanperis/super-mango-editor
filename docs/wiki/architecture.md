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
  │     ├── IMG_LoadTexture → gs.textures.floor_tile (grass_tileset.png)
  │     ├── IMG_LoadTexture → gs.textures.platform   (grass_platform.png)
  │     ├── water_init(&gs.water, gs.renderer)      (water.png)
  │     ├── IMG_LoadTexture → gs.textures.*         (entities, hazards, collectibles, surfaces)
  │     ├── level resource reload → parallax/floor/water/fog/music from active LevelDef
  │     │
  │     │   ── Load all sound effects ──
  │     ├── Mix_LoadWAV     → gs.audio.*           (jump, coin, hit, spring, axe, flap, spider_attack, dive)
  │     ├── Mix_LoadMUS     → gs.audio.music       (from active LevelDef music_path)
  │     ├── Mix_PlayMusic(gs.audio.music, -1)      (loop forever at level volume)
  │     │
  │     │   ── Initialise game objects ──
  │     ├── player_init(&gs.player, gs.renderer)
  │     ├── fog_init(&gs.fog, gs.renderer)         (level fog layers, e.g. fog_1.png/fog_2.png)
  │     ├── hud_init(&gs.hud, gs.renderer)
  │     ├── if (debug_mode) debug_init(&gs.debug)
  │     ├── level_load_toml(level_path, &def)       (staged TOML parse → runtime validation → cleanup → caller assignment)
  │     ├── level_load(&gs, &def)                   (apply LevelDef to runtime GameState)
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
        ├── Mix_HaltMusic + Mix_FreeMusic (gs.audio.music)
        ├── FREE_CHUNK(gs.audio.*)
        ├── water_cleanup
        ├── DESTROY_TEX(gs.textures.*)
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
  2. Events       — SDL_PollEvent (quit window / pause and overlay controls)
                    SDL_CONTROLLERDEVICEADDED   — opens a newly plugged-in controller
                    SDL_CONTROLLERDEVICEREMOVED — closes and NULLs gs->controller when unplugged
                    SDL_CONTROLLERBUTTONDOWN (START) — pauses/resumes, restarts game-over, or advances completion overlay
  3. Update       — player_handle_input → player_update (incl. bouncepad, float-platform, bridge landing)
                    → bouncepad response (animation + spring sound)
                    → spiders_update → jumping_spiders_update → birds_update → faster_birds_update
                    → fish_update → faster_fish_update → spike_blocks_update → spikes_update
                    → spike_platforms_update → circular_saws_update → axe_traps_update
                    → blue_flames_update → float_platforms_update → bridges_update
                    → spider collision → jumping_spider collision → bird collision → faster_bird collision
                    → fish collision → faster_fish collision → spike_block collision (+ push impulse)
                    → spike collision → spike_platform collision → circular_saw collision
                    → axe_trap collision → blue_flame collision → fire_flame collision
                    → sea gap fall detection (instant death)
                    → coin-player collision → star-player collision → last_star-player collision
                    → completion summary snapshot / next_phase pending state when last_star is collected
                    → heart/lives/score_life_next logic
                    → water_update → fog_update → bouncepads_update (small, medium, high)
                    → debug_update (if --debug)
  4. Render       — clear → parallax background → platforms → floor tiles
                    → float platforms → spike rows → spike platforms → bridges
                    → bouncepads (medium, small, high) → rails
                    → vines → ladders → ropes → coins → yellow stars → last star
                    → blue flames → fire flames → fish → faster fish → water
                    → spike blocks → axe traps → circular saws
                    → spiders → jumping spiders → birds → faster birds
                    → player → fog → hud
                    → debug overlay (if --debug) → pause/game-over/completion overlay → present
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
| 1 | Background | Per-level `background_layers` from `assets/sprites/backgrounds/`, tiled horizontally with each layer's configured scroll speed |
| 2 | Platforms | active level platform tile, 9-slice tiled pillar stacks (drawn before floor so pillars sink into ground) |
| 3 | Floor | active level floor tile tiled across world width at `FLOOR_Y`, with floor-gap openings |
| 4 | Float platforms | `float_platform.png` 3-slice hovering surfaces (static, crumble, rail modes) |
| 5 | Spike rows | `spike.png` ground-level spike strips on the floor surface |
| 6 | Spike platforms | `spike_platform.png` elevated spike hazard surfaces |
| 7 | Bridges | `bridge.png` tiled crumble walkways |
| 8 | Bouncepads (medium) | `bouncepad_medium.png` standard-launch spring pads |
| 9 | Bouncepads (small) | `bouncepad_small.png` low-launch spring pads |
| 10 | Bouncepads (high) | `bouncepad_high.png` high-launch spring pads |
| 11 | Rails | `rail.png` bitmask tile tracks for spike blocks and float platforms |
| 12 | Vines | `vine_green.png` / `vine_brown.png` climbable plant decorations hanging from platforms |
| 13 | Ladders | `ladder.png` climbable ladder structures |
| 14 | Ropes | `rope.png` climbable rope segments |
| 15 | Coins | `coin.png` collectible sprites drawn on top of platforms |
| 16 | Yellow stars | `star_yellow.png` collectible star pickups |
| 17 | Last star | end-of-level star collectible (uses HUD star sprite) |
| 18 | Blue/fire flames | `blue_flame.png` / `fire_flame.png` animated flame hazards erupting from floor gaps |
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
| 30 | Fog | Per-level `fog_layers` from `assets/sprites/foregrounds/` (for example `fog_1.png`, `fog_2.png`, `fog_fire_1.png`, `fog_fire_2.png`, `smoke.png`) |
| 31 | HUD | `hud_render`: hearts, lives, score -- always drawn on top |
| 32 | Debug | `debug_render`: FPS counter, collision boxes, event log — when `--debug` active |

> **Note:** Additional foreground layers (fog, water overlays) can be added per-level via `[foreground_layers]` in the TOML file, adding up to 8 extra layers above the HUD. The base 32 layers are always present.

### Level Completion Flow

Collecting `last_star` calls `game_complete_level()`. The game snapshots elapsed time, coins collected, total coins, and the resolved `next_phase` path (if any), then shows a completion overlay. While the overlay is active, gameplay update pauses; pressing Enter or Start calls `game_load_next_phase()` when `next_phase` is configured, otherwise it exits the run.

### Pause Overlay Flow

During active gameplay, Esc or controller Start toggles the player pause reason through the overlay helper in `src/core/game_overlay.c`. Paused frames keep rendering the last camera position, skip gameplay updates, pause music, and draw a semi-transparent pause overlay with resume hints. Enter, Space, Esc, or controller Start resumes gameplay. Window focus loss uses a separate focus pause reason, so regaining focus does not clear an intentional player pause. Completion and game-over overlays take priority over pause.

### Game-Over Flow

When lethal damage consumes the final life, `apply_damage()` sets `gs->game_over` and returns without resetting the level. The shared overlay helper reports `GAME_OVERLAY_GAME_OVER`, so the loop blocks gameplay updates and rendering draws a game-over overlay with the final score. Enter, Space, or controller Start confirms restart through `game_restart_after_game_over()`, which restores level-defined lives/hearts, resets score and bonus-life threshold, and reloads the current level. Esc exits the game-over overlay.

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
    SDL_Window         *window;
    SDL_Renderer       *renderer;
    SDL_GameController *controller;
    TextureResources    textures;  /* all owned SDL_Texture pointers */
    AudioResources      audio;     /* all Mix_Chunk plus Mix_Music */

    ParallaxSystem parallax;
    Player         player;
    Platform       platforms[MAX_PLATFORMS];
    Water          water;
    FogSystem      fog;
    /* fixed-size arrays + counts for every enemy, hazard, collectible, surface */

    Hud     hud;
    Camera  camera;
    int     hearts, lives, score, score_life_next;
    int     level_complete;
    float   level_elapsed;
    int     level_coin_total;
    int     completion_coins_collected, completion_coin_total;
    float   completion_elapsed;
    int     completion_pending_next_phase;
    char    completion_next_phase[256];
    char    level_path[256];

    LevelRuntime  runtime;
    GameRules     rules;
    GameLoopState loop;
    DebugOverlay  debug;
} GameState;
```

**Key design decisions:**

- Textures are grouped in `TextureResources` (`gs->textures.*`) and audio in `AudioResources` (`gs->audio.*`) so cleanup can be centralized.
- `Player` is **embedded by value**, not a pointer. This avoids a heap allocation and keeps the struct self-contained. The same applies to `Platform`, `Water`, `FogSystem`, and all entity arrays.
- Every pointer is set to `NULL` after freeing, making accidental double-frees safe.
- Initialised with `GameState gs = {0}` so every field starts as `0` / `NULL`.

---

## Error Handling Strategy

| Situation | Action |
|-----------|--------|
| SDL subsystem init failure (in `main`) | `fprintf(stderr, ...)` → clean up already-inited subsystems → `return EXIT_FAILURE` |
| Resource load failure (in `game_init`) | `fprintf(stderr, ...)` → destroy already-created resources → `exit(EXIT_FAILURE)` |
| Sound load failure (non-fatal pattern) | `fprintf(stderr, ...)` then continue -- play is guarded by `if (gs->audio.<name>)` |
| Optional texture load failure (non-fatal) | `fprintf(stderr, ...)` then continue -- render is guarded by `if (texture)` |

All SDL error strings are retrieved with `SDL_GetError()`, `IMG_GetError()`, or `Mix_GetError()` and printed to `stderr`.
