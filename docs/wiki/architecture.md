# Architecture

<a id="home"></a>

---

## Overview

Super Mango follows a classic **init → loop → cleanup** pattern. `AppSession` owns the application runtime and one active screen (start menu or game); `GameState` owns resources and state for the active game screen. The session consumes explicit routes after a screen frame, so native and WebAssembly builds share one transition model.

---

## Startup Sequence

```
main()
  ├── SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)
  ├── IMG_Init(IMG_INIT_PNG)
  ├── TTF_Init()
  ├── Mix_OpenAudio(44100, stereo, 2048 buffer)
  └── session_create(config)
       ├── initialise AppSession runtime and controller ownership
       │   └── native controller setup is deferred until a stable screen frame;
       │       WebAssembly initializes it synchronously
       ├── no `--level` → start_menu_create()
       └── `--level` / `--sandbox` → session_open_game() → game_init(gs)
            ├── create active game window and renderer
            ├── load textures, audio, TOML level data, player, HUD, effects, and entities
            └── arm release latch and repair browser keyboard state

session_run(session)
  ├── native: while active, call session_frame(session)
  └── WebAssembly: register one Emscripten callback for session_frame(session)
       ├── start menu frame → Play or Exit route
       └── game_frame(gs) → Next Level, Replay, Level Select, or Exit route
            ├── Next Level: load resolved phase in current GameState
            ├── native Replay: replace active game with same TOML path
            ├── browser Replay: persist path, cancel callback, clean up, reload
            └── Level Select: close game only, open start menu

session_destroy(session) / browser terminal cleanup
  ├── close active menu or game screen
  ├── join and close the AppSession-owned controller subsystem
  ├── Mix_CloseAudio → TTF_Quit → IMG_Quit → SDL_Quit
  └── free AppSession
```

---

## App Session and Game Frame

`AppSession` is the sole production loop owner. It frames the active menu or game, then consumes that screen's route only after the frame. Active-game frames run at **60 FPS**, capped via VSync plus a manual `SDL_Delay` fallback:

```
session_frame(session) {
  if (session->screen == APP_SCREEN_MENU) {
    start_menu_frame(menu);
    apply MenuRoute;
  } else if (session->screen == APP_SCREEN_GAME) {
    game_frame(gs);
    apply GameRoute;
  }
}

game_frame(gs) {
  1. Delta Time   — measure ms since last frame → dt (seconds)
  2. Events       — SDL_PollEvent (quit window / pause and overlay controls)
                     SDL_CONTROLLERDEVICEADDED   — opens a newly plugged-in controller
                     SDL_CONTROLLERDEVICEREMOVED — closes and NULLs gs->controller when unplugged
                     terminal: Up/Down or D-pad selects; Enter/Space/Start (or A) confirms
                     terminal: Esc/Back (or B) exits; Start toggles active-game pause
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

Velocities are expressed in **pixels per second**. Multiplication by `dt` gives a displacement, but discrete acceleration and collision sampling still introduce timestep-dependent error. `make timing-lab` demonstrates this distinction. Replay uses recorded steps; the inspector can freeze, single-step, slow and tune a simulation without overriding focus/settings/terminal blockers.

During an active game update, authored checkpoints are sampled after player movement and before lethal collision handling. Legacy screen-boundary checkpoint sampling runs only when the active level has no authored records.

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

> **Note:** Per-level visual layers are split by role: `background_layers` feed the parallax renderer, `foreground_layers` select the water/lava foreground strip texture, and `fog_layers` feed the atmospheric fog system. Fog renders before the HUD so hearts/lives/score remain legible.

### Level Completion and Terminal Actions

Collecting `last_star` calls `game_complete_level()`. The game snapshots elapsed time, coins collected, total coins, and the resolved `next_phase` path (if any), then shows a completion overlay. While it is active, gameplay update pauses. Its action list is **Next Level**, **Replay**, **Level Select**, **Exit** when a phase is pending; otherwise it is **Replay**, **Level Select**, **Exit**. Up/Down or D-pad moves the focused row with wraparound. Enter/Space/Start confirms it (controller A also confirms). Esc/Back exits immediately (controller B is equivalent).

Next Level uses `game_load_next_phase()` without replacing the game screen. If loading fails, the completion overlay remains visible and keeps its current focus. Level Select closes the game screen and opens the start menu in the same `AppSession`. Native Replay closes the game screen and opens the same TOML path in a new `GameState`; Browser Replay persists that path in session storage, cancels the Emscripten callback, tears down once, reloads, and then boots the stored level.

### Pause Overlay Flow

During active gameplay, Esc or controller Start toggles the player pause reason through the overlay helper in `src/core/game_overlay.c`. Paused frames keep rendering the last camera position, skip gameplay updates, pause music, and draw a semi-transparent pause overlay with resume hints. Enter, Space, Esc, or controller Start resumes gameplay. Window focus loss uses a separate focus pause reason, so regaining focus does not clear an intentional player pause. Completion and game-over overlays take priority over pause.

### Game-Over Flow

When lethal damage consumes the final life, `apply_damage()` sets `gs->game_over` and returns without resetting the level. The shared overlay helper reports `GAME_OVERLAY_GAME_OVER`, so the loop blocks gameplay updates and rendering draws a game-over overlay with the final score. Its terminal action list is **Retry**, **Level Select**, **Exit**. Retry calls `game_restart_after_game_over()` in place: it restores level-defined lives/hearts, resets score and bonus-life threshold, reloads the current level, resumes music, and clears its input latch after held controls are released. Level Select and Exit follow the session routes above.

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

Defined in `game.h`. The **single container** for active-game resources; `AppSession` owns app-wide runtime state and the active screen.

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
    int     running;
    int     game_over;
    int     paused;
    unsigned int pause_reasons;
    float   respawn_x, respawn_y;
    int     checkpoint_index;
    Uint32  checkpoint_feedback_until;
    int     legacy_checkpoint_screen;
    int     debug_mode;
    int     smoke_test_frames;
    char    level_path[GAME_LEVEL_PATH_MAX];
    void   *level_def;      /* owned active LevelDef backing storage */

    LevelRuntime        runtime;
    GameRules           rules;
    GameLoopState       loop;
    GameCompletionState completion;
    DebugOverlay        debug;
} GameState;
```

**Key design decisions:**

- Textures are grouped in `TextureResources` (`gs->textures.*`) and audio in `AudioResources` (`gs->audio.*`) so cleanup can be centralized.
- `Player` is **embedded by value**, not a pointer. This avoids a heap allocation and keeps the struct self-contained. The same applies to `Platform`, `Water`, `FogSystem`, and all entity arrays.
- Owning pointers are cleared after release. Borrowed pointers and aliases still require correct lifetime handling.
- Initialised with `GameState gs = {0}` so every field starts as `0` / `NULL`.
- `checkpoint_x` is no longer a `GameState` field. The resolved respawn state is `respawn_x`, `respawn_y`, and `checkpoint_index`; `legacy_checkpoint_screen` is used only when the active level has no authored records.

### Authored Checkpoint Flow

`LevelDef` owns optional immutable `CheckpointPlacement { x, y }` records. Each active frame samples authored records after player movement and before lethal collisions. The furthest record with `x <= player.x` becomes the resolved respawn point, so a death in the same frame preserves a crossed checkpoint. The runtime never regresses to an earlier record.

Authored records disable automatic screen-boundary checkpoints for that level. A level with no records preserves the legacy boundary behavior. Retry, replay, and successful next-phase loads reset to the effective start of their respective level; a failed next-phase load retains the active level and its resolved checkpoint. The HUD shows a brief checkpoint notice, then the active `CP n`; a respawn displays `RESPAWN CP n`.

---

## Error Handling Strategy

| Situation | Action |
|-----------|--------|
| SDL subsystem init failure (in `main`) | `fprintf(stderr, ...)` → clean up already-inited subsystems → `return EXIT_FAILURE` |
| Resource load failure (in `game_init`) | `fprintf(stderr, ...)` → clean up partially-created `GameState` resources → return `-1`; the top-level runner returns `EXIT_FAILURE` |
| Sound load failure (non-fatal pattern) | `fprintf(stderr, ...)` then continue -- play is guarded by `if (gs->audio.<name>)` |
| Missing gameplay-critical shared sprite | Reject the level before replacing active level state; identify the required asset path |
| Optional presentation texture load failure | Warn and preserve the documented visual fallback |

All SDL error strings are retrieved with `SDL_GetError()`, `IMG_GetError()`, or `Mix_GetError()` and printed to `stderr`.
