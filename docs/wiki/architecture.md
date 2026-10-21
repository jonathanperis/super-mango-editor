# Architecture

<a id="home"></a>

---

## Overview

Super Mango follows a classic **init → loop → cleanup** pattern. `AppSession` owns the application runtime and one active screen (start menu or game); `GameState` owns resources and state for the active game screen. The session consumes explicit routes after a screen frame, so native and WebAssembly builds share one transition model.

---

## Startup Sequence

```
main()
  └── session_create(config)
       ├── initialise one raylib window/context, semantic input and audio device
       │   └── device polling and platform teardown stay on the application thread
       ├── no `--level` → start_menu_create(), its font, logo and confirm
       │   sound acquired from the session's asset cache
       └── `--level` / `--sandbox` → session_open_game()
            ├── first game only: load the shared sprites and sounds (GameAssets)
            └── game_init(gs), with a copy of those GameAssets and a pointer
                 │   to the session's asset cache
                 ├── create a screen-owned logical render target
                 ├── load TOML level data and entities; acquire the player
                 │   sheet, HUD icon and font and the level's own files
                 │   (background, floor, water, fog, tiles, music) from the cache
                 ├── trim the cache: unload what the previous level left idle
                 └── arm release latch and repair browser keyboard state

session_run(session)
  ├── native: while active, call session_frame(session)
  └── WebAssembly: register one Emscripten callback for session_frame(session)
       ├── start menu frame → Play or Exit route
       └── game_frame(gs) → Next Level, Replay, Level Select, or Exit route
            ├── Next Level: load resolved phase in current GameState; only
            │   the files the new level adds are read
            ├── Replay (native and browser): close game, open same path in place,
            │   reusing the session's GameAssets and every cached file
            └── Level Select: close game only, open start menu; the level's
                files stay in the cache, idle, until another level opens

session_destroy(session) / browser terminal cleanup
  ├── close active menu or game screen
  ├── release the session's hold on the menu's files, clear the asset
  │   cache, then unload the shared GameAssets, each once
  ├── clear input handlers and sound voices after screen assets are released
  ├── CloseAudioDevice → CloseWindow
  └── free AppSession
```

---

## App Session and Game Frame

`AppSession` is the sole production loop owner. It samples physical input, pumps the music stream, frames the active screen, then consumes its route. raylib's `EndDrawing` owns presentation and event polling. Native visible windows use its **60 FPS** limiter; hidden smoke runs are uncapped and retain fixed simulation steps. Each click on the native close button is reported once, as an Exit request; it replaces a Play, Continue, Next Level, Replay or Level Select route chosen earlier in the same frame (a fatal error still wins), so a close is never lost to a screen change.

On Web, Emscripten's animation-frame callback owns scheduling. `display_open`
sets raylib's target FPS to zero so `EndDrawing` returns to the browser instead
of sleeping on its main thread. Browser cadence follows animation frames; the
fixed-step accumulator turns whatever rate the browser delivers (60, 120 or
144 Hz) into 60 simulation steps per second. This does not require Asyncify or
change fixed-step replay/smoke simulation. The input
adapter also skips `WindowShouldClose` on Web: there is no native close button,
and raylib's Web implementation calls `emscripten_sleep`, which aborts without
Asyncify. In-game exit remains an application route handled by the host.

On native macOS, the pinned dependency wakes its partial-busy sleep 1 ms earlier
so the existing short busy wait can meet the same frame deadline despite sleep
coalescing. This does not change simulation time or add another limiter. The
debug counter rounds **measured** FPS to the nearest integer; genuine slow frames
remain visible. A 60 FPS target cannot prevent stalls caused by the OS or work
that exceeds the frame budget.

There are two distinct notions of input. GLFW callbacks capture **ordered
commands** (typing, clicks, pause) during raylib's end-of-frame poll; the next
frame drains them. **Held state** answers whether movement remains pressed.
`input_collect` adds focus/gamepad changes without polling the OS again. The
editor drains text before command boundaries, so typing followed by Ctrl+S in
one frame saves the edited value instead of invoking shortcuts for those letters.

The drawing sequence is `BeginDrawing` → `BeginTextureMode` → screen draw calls
→ `display_present`. The final helper ends texture mode, copies the logical
image to the window and calls `EndDrawing` once. The window is a process resource;
a render target is a screen-owned off-screen image, not another OS window.

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
  1. Time         — real seconds since last frame (GetTime, clamped to 0.25 s)
  2. Events       — drain project-owned InputEvent commands (quit / pause / overlays)
                     INPUT_PAD_ADDED / INPUT_PAD_REMOVED — update selected gamepad index
                     terminal: Up/Down or D-pad selects; Enter/Space/Start (or A) confirms
                     terminal: Esc/Back (or B) exits; Start toggles active-game pause
   3. Update       — accumulator turns real time into 0..5 fixed 1/60 s steps; pause/settings/
                     terminal state blocks them; each step runs, in order:
                     → game_player_step (sampled input, motion, surface landing, bounce response)
                     → authored checkpoint sampling → lethal floor-gap detection
                     → the s_update_steps table, row by row (see Update Order below):
                       enemies → spike blocks → float platforms/rider carry → bridges
                       → hazards → game_collide (enemy/hazard damage, coins/stars,
                       completion) → legacy checkpoints → water, fog, bouncepad animation
                     → camera
                     A lost life or game over skips the remaining rows; the camera still moves.
  4. Render       — clear → the s_render_layers table, row by row (see Render Order below):
                    parallax background → platforms → floor tiles
                    → float platforms → spike rows → spike platforms → bridges
                    → bouncepads (medium, small, high) → rails
                     → vines → ladders → ropes → coins → yellow/green/red stars → last star
                    → blue flames → fire flames → fish → faster fish → water
                    → spike blocks → axe traps → circular saws
                    → spiders → jumping spiders → birds → faster birds
                    → player → fog → hud
                     → debug overlay/inspector → pause/game-over/completion → settings → present
}
```

### Fixed Time Step

```c
accumulator += frame_seconds;              /* real time, clamped to 0.25 s  */
while (accumulator >= GAME_FIXED_STEP && steps < GAME_MAX_STEPS_PER_FRAME) {
    game_update_active(gs, GAME_FIXED_STEP, cam_x);   /* always 1/60 s    */
    accumulator -= GAME_FIXED_STEP;
}
render();                                  /* every frame, even 0 steps    */
```

Velocities are expressed in **pixels per second**, and every simulation step
multiplies them by the same `dt` of exactly `1 / TARGET_FPS`. Real frame time
(raylib's `GetTime()`, a double in seconds) only decides *how many* steps run
before the next picture (`src/core/game_timing.c`):

- **Frame-rate-independent results.** Discrete integration still has error
  (`make timing-lab`), but it is the same error at 30, 60 or 144 Hz, so jump
  heights and arcs no longer depend on the display.
- **Bounded movement per step.** One step moves at most speed × 1/60 s, so a
  slow frame cannot carry the player past a collision test.
- **Replays behave like live play.** Live play, smoke tests, scripted replays
  and captured experiments run the very same steps.

A 60 Hz display runs one step per frame; a 120/144 Hz display (the browser
follows `requestAnimationFrame`) runs zero steps on some frames and just
redraws. So that those redraws are not the same picture shown twice and then a
jump (judder), the frame draws the player and the camera between the last two
steps: `game_timing_note_step` records both before each step, and
`game_timing_render_view` blends them by the accumulator's share of a step,
so the picture runs up to one step (16.7 ms) behind the simulation but moves
every frame. It is drawing only: the renderer gets a copy of the player at the
blended position and the simulation never reads the recorded values, so the
steps, their order and their inputs are unchanged. A move of more than 48 px
in one step (a respawn, a camera snap, a new level) is shown where it lands.
Debug mode (whose hitbox outlines must sit on the sprite) and smoke, replay
and experiment runs draw the latest step itself. The time-trial ghost is
blended the same way between its last two samples (`game_timing_view_point`),
so a run that matches the best one draws both in the same place. Spiders,
jumping spiders, birds, fish and their faster variants, circular saws, spike
blocks and float platforms are blended the same way from the positions
`game_timing_note_step` stored before the step. A bird's height stays on its
sine wave because only its x is blended and the wave is computed from that x.
A step that moves any of them more than 48 px is shown where it is now. A
saw's patrol position is blended; its spin angle stays on the latest step, as
do axe swings, flame eruptions, bouncepad squashes and crumbling bridge
bricks. The player is not carried by those. The accumulator restarts half a step full after pauses and loads, which
absorbs sub-millisecond vsync jitter. Frame time is clamped to 0.25 s and at most
5 steps run per frame; excess time is dropped (below 12 FPS the game slows down
instead of spiralling). Smoke and scripted replays feed exactly one step per
frame regardless of the wall clock. Experiment captures (`format_version = 3`)
store one row per fixed step, `[input bits, physics values...]`, with no
duration column; a `format_version = 1` capture from the variable-timestep
engine, or a `format_version = 2` one from before the camera snapped to the
level start and the respawn point (waiting spike blocks start when the camera
reveals them, so they could start on other steps), is rejected with a request
to record it again. Completion, game over or
a route stops the remaining steps of that frame. The debug inspector's freeze,
single-step and slow-motion keys ([Controls](../controls/#debug-inspector-keys))
only change how much real time reaches the accumulator. They never override the
focus, settings or end-of-level blockers, and touch taps are discarded only
while such a screen owns input, not on zero-step frames.
Render-only timers (debug FPS readout, log ages) use real frame time.

During an active game update, authored checkpoints are sampled after player movement and before lethal collision handling. Legacy screen-boundary checkpoint sampling runs only when the active level has no authored records.

### Update Order (one fixed step)

`game_update_active()` in `src/core/game_update.c` runs one step. It moves the
player first (`game_player_step`: sampled input, motion, landing on surfaces,
bounce response), samples authored checkpoints and tests for a fall into a
floor gap. Then it walks the `s_update_steps` table, top to bottom, and last
it moves the camera. A step that costs a life or ends the game skips the rows
that are left; the camera still moves.

Each row of `s_update_steps` is a name and a small `update_<things>()`
function in the same file. That function calls the one in the last column
with the inputs its kind needs, so it is the place to see what a kind
depends on. `make docs-drift` checks this table against `s_update_steps`:
the same names in the same order, each row calling the function listed here.

| Row | Name | Calls | Reads, besides its own array and the step time |
|-----|------|-------|------------------------------------------------|
| 1 | spiders | `spiders_update` | floor gaps |
| 2 | jumping spiders | `jumping_spiders_update` | floor gaps, attack sound, player x, camera |
| 3 | birds | `birds_update` | flap sound, player x, camera |
| 4 | faster birds | `faster_birds_update` | flap sound, player x, camera |
| 5 | fish | `fish_update` | world width |
| 6 | faster fish | `faster_fish_update` | world width |
| 7 | spike blocks | `spike_blocks_update` | camera (a block waits until it is on screen) |
| 8 | float platforms | `game_float_platforms_update` | the float platform the player landed on; moves a rail rider |
| 9 | bridges | `game_bridges_update` | the bridge the player landed on, player x |
| 10 | axe traps | `axe_traps_update` | axe sound, player x, camera |
| 11 | circular saws | `circular_saws_update` | nothing else |
| 12 | blue flames | `blue_flames_update` | nothing else |
| 13 | fire flames | `blue_flames_update` | nothing else (the same module, a second array) |
| 14 | collision | `game_collide` | the whole world: damage, coins, stars, the level's end |
| 15 | screen checkpoints | `game_checkpoint_update` | player x; only levels without authored checkpoints |
| 16 | water | `water_update` | whether the level has water |
| 17 | fog | `fog_update` | whether the level has fog |
| 18 | bouncepads (medium) | `bouncepads_update` | nothing else |
| 19 | bouncepads (small) | `bouncepads_update` | nothing else |
| 20 | bouncepads (high) | `bouncepads_update` | nothing else |

Rows 1 to 13 run before collision so that hitboxes are tested where this step
put them. Rows 16 to 20 only animate.

### Render Order (back to front)

This table is the `s_render_layers` table in `src/render/game_render.c`, row
for row. Each row there is the layer's number, its name, and a small
`render_<layer>()` function in the same file (`render_spiders`,
`render_water`, ...) that calls the function in the last column with the
array, count and texture it needs. `game_render_frame()` clears the canvas
and walks the table from row 1, so a later row covers an earlier one.

`make docs-drift` keeps the two tables in step: the same number of layers,
each **What** cell starting with the code row's name, and each **Drawn by**
cell naming exactly the `*_render` functions that row's `render_<layer>()`
calls. It also fails if `game_render_frame()` draws a layer without going
through the table.

| Layer | What | Drawn by |
|-------|------|----------|
| 1 | Background: per-level `background_layers` from `assets/sprites/backgrounds/`, tiled with each layer's scroll speed | `parallax_render` |
| 2 | Platforms: 9-slice pillar stacks, drawn before the floor so they sink into the ground | `platforms_render` |
| 3 | Floor: the level's floor tile across the world at `FLOOR_Y`, with floor-gap openings | `render_floor`, a loop in `game_render.c` |
| 4 | Float platforms: 3-slice hovering surfaces (static, crumble, rail) | `float_platforms_render` |
| 5 | Spike rows on the floor | `spike_rows_render` |
| 6 | Spike platforms | `spike_platforms_render` |
| 7 | Bridges: tiled crumble walkways | `bridges_render` |
| 8 | Bouncepads (medium) | `bouncepads_render` |
| 9 | Bouncepads (small) | `bouncepads_render` |
| 10 | Bouncepads (high) | `bouncepads_render` |
| 11 | Rails: bitmask tile tracks for spike blocks and float platforms | `rails_render` |
| 12 | Vines (`vine_green.png` / `vine_brown.png`) | `vines_render` |
| 13 | Ladders | `ladders_render` |
| 14 | Ropes | `ropes_render` |
| 15 | Coins | `coins_render` |
| 16 | Health stars: yellow, then green, then red | `health_stars_render` |
| 17 | Last star | `last_star_render` |
| 18 | Blue and fire flames erupting from floor gaps | `blue_flames_render` |
| 19 | Fish, drawn before the water so they look submerged | `fish_render` |
| 20 | Faster fish | `faster_fish_render` |
| 21 | Water: the animated strip at the bottom | `water_render` |
| 22 | Spike blocks riding their rails | `spike_blocks_render` |
| 23 | Axe traps | `axe_traps_render` |
| 24 | Circular saws | `circular_saws_render` |
| 25 | Spiders | `spiders_render` |
| 26 | Jumping spiders | `jumping_spiders_render` |
| 27 | Birds | `birds_render` |
| 28 | Faster birds | `faster_birds_render` |
| 29 | Time-trial ghost: the best run's Mango, translucent (see [Controls](../controls/#time-trial-ghost)) | `render_ghost` in `game_render.c` |
| 30 | Player | `player_render` |
| 31 | Fog: per-level `fog_layers` from `assets/sprites/foregrounds/` | `fog_render` |
| 32 | HUD: hearts, lives, score. With the high-contrast setting, the player's outline and a dark strip behind the HUD are drawn first | `hud_render` |
| 33 | Debug overlay and inspector panels, with `--debug` | `debug_render`, `game_inspector_render` |

> **Note:** Per-level visual layers are split by role: `background_layers` feed the parallax renderer, `foreground_layers` select the water/lava foreground strip texture, and `fog_layers` feed the atmospheric fog system. Fog renders before the HUD so hearts/lives/score remain legible.

The pause, game-over and completion overlays and the settings panel are drawn
after these 33 layers.

### Level Completion and Terminal Actions

Collecting `last_star` calls `game_complete_level()`. The game snapshots elapsed time, coins collected, total coins, and the resolved `next_phase` path (if any), then shows a completion overlay. While it is active, gameplay update pauses. Its action list is **Next Level**, **Replay**, **Level Select**, **Exit** when a phase is pending; otherwise it is **Replay**, **Level Select**, **Exit**. Up/Down or D-pad moves the focused row with wraparound. Enter/Space/Start confirms it (controller A also confirms). Esc/Back exits immediately (controller B is equivalent).

Next Level uses `game_load_next_phase()` without replacing the game screen. If loading fails, the completion overlay stays visible with a "Next level failed to load" line (`completion.next_phase_failed`), the dead Next Level row is removed and focus moves to Replay; the current level is untouched, so the remaining actions keep working. That holds by construction: the next file is parsed and validated once into a heap copy and its sprites are checked before anything changes; only then is the active `LevelDef` swapped and applied, a step with no failure path. Level Select closes the game screen and opens the start menu in the same `AppSession`. Replay closes the game screen and opens the same TOML path in a new `GameState`, on native and in the browser alike; the window, audio device, frame callback and profile stay alive.

### Pause Overlay Flow

During active gameplay, Esc or controller Start toggles the player pause reason through the overlay helper in `src/core/game_overlay.c`. Paused frames keep rendering the last camera position, skip gameplay updates, pause music, and draw a semi-transparent pause overlay with resume hints. Enter, Space, Esc, or controller Start resumes gameplay. Window focus loss uses a separate focus pause reason, so regaining focus does not clear an intentional player pause. Music follows one predicate, `game_music_should_play()`: it is silent while the pause overlay or the settings panel is up and plays otherwise (including under completion and game-over screens), so regaining focus cannot resume music beneath open settings. Completion and game-over overlays take priority over pause.

### Game-Over Flow

When lethal damage consumes the final life, `apply_damage()` sets `gs->screen.game_over` and returns without resetting the level. The shared overlay helper reports `GAME_OVERLAY_GAME_OVER`, so the loop blocks gameplay updates and rendering draws a game-over overlay with the final score. Its terminal action list is **Retry**, **Level Select**, **Exit**. Retry calls `game_restart_after_game_over()` in place: it restores level-defined lives/hearts, resets score and bonus-life threshold, makes every coin collectable again, resets the current level, resumes music, and clears its input latch after held controls are released. Level Select and Exit follow the session routes above.

---

## Coordinate System

The 2D Y-axis increases **downward**. The origin (0, 0) is at the **top-left** of the logical canvas.

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

The game renders to a **400×300 `RenderTexture2D`**, configured with point
filtering when the screen creates it. `display_present` applies aspect-preserving
scaling and the render-texture Y correction. At 800×600 this gives a **2x** pixel
scale. `input_pointer_to_logical` applies the inverse viewport transform once.
Both take the viewport from `display_viewport`: once the window holds at least
one whole canvas, it uses the largest whole-number scale that fits and centres
it on whole-pixel offsets, so every canvas pixel covers the same square of
window pixels (a 1920×1080 window shows the game at 3x, 1200×900, with black
margins). A fractional scale would make nearest-neighbour sampling draw some
columns one pixel wider than others, and the art would shimmer as it scrolls.
Only a window smaller than the canvas gets a fractional fit, so that nothing
is cut off.

On macOS `display_open` also asks for raylib's `FLAG_WINDOW_HIGHDPI`, so a
Retina display gets a full-density framebuffer instead of a half-density one
the compositor would blur. Window sizes, `GetScreenWidth` and GLFW cursor
positions all stay in points there; raylib's screen-scale matrix turns the
presentation rectangle into device pixels, so the viewport and its inverse
still agree. Windows and Linux do not set the flag: there raylib would resize
the window by the monitor's scale and rescale the mouse through
`SetMouseScale`, which the raw GLFW cursor callbacks in `input_backend.c` do
not pass through.
Gameplay hitboxes remain integer `IntRect` values; raylib's floating `Rectangle`
is a drawing boundary type. Two hitboxes that merely touch edges do not overlap.

---

## GameState Struct

Defined in `game.h`. The **single container** for active-game resources; `AppSession` owns app-wide runtime state and the active screen. The plain numbers it and the entity code share (`GAME_W`, `FLOOR_Y`, `GRAVITY`, `MAX_FLOOR_GAPS`, camera tuning) live in `game_constants.h`, which `game.h` includes; entity, hazard, surface and effect `.c` files that need only those numbers include `game_constants.h` alone, so a `GameState` change does not recompile them.

`GameState` is split into three named parts, so the path to a value also
says what kind of state it is:

| Part | Type | Holds | Example |
|------|------|-------|---------|
| `gs->world` | `GameWorld` | The level being played: player, camera, every entity array and count, score, lives, checkpoints, simulated time, the active `LevelDef` and the level's scenery (parallax, water, fog, platform tiles) | `gs->world.spiders[i]` |
| `gs->screen` | `GameScreen` | The game screen around the level: render target, HUD, pause / game-over / completion overlays, the route for `AppSession`, frame-loop clock, input latches, replay script, debug tools, profile and ghost | `gs->screen.paused` |
| `gs->assets` | `GameAssets` | The sprites and sound effects every level uses, borrowed from `AppSession` | `gs->assets.textures.spider` |

```c
typedef struct {
    char      level_path[GAME_LEVEL_PATH_MAX];
    void     *level_def;          /* owned active LevelDef backing storage */
    LevelRuntime runtime;         /* active LevelDef, world width, effect flags */
    GameRules    rules;
    Player     player;            /* embedded by value */
    GameCamera camera;
    ParallaxSystem parallax;  Water water;  FogSystem fog;
    Texture2D  *floor_tile;       /* the level's own floor; NULL = shared default */
    MusicTrack *music;            /* the level's music stream */
    Platform   platforms[MAX_PLATFORMS];
    int        platform_count;
    Spider     spiders[MAX_SPIDERS];
    int        spider_count;
    /* ... fixed-size arrays + counts for every enemy, hazard, surface,
       collectible, in that order ... */
    int     hearts, lives, score, score_life_next;
    float   respawn_x, respawn_y;
    int     checkpoint_index;     /* -1 before the first authored record */
    CheckpointFeedbackKind checkpoint_feedback_kind;
    uint32_t checkpoint_feedback_until; /* deadline on game_checkpoint_clock_ms */
    int     legacy_checkpoint_screen;
    uint32_t sim_steps;           /* fixed steps since the level was applied; stops while paused */
} GameWorld;

typedef struct {
    RenderTexture2D frame_target; /* screen owns target; session owns window */
    Hud     hud;
    int     controller;           /* raylib index + 1; zero means none */
    int     running;
    GameRoute route;              /* request consumed by AppSession */
    int     game_over;
    int     paused;
    unsigned int pause_reasons;
    GameCompletionState completion;
    GameLoopState       loop;
    int     smoke_test_frames;
    /* ... replay script, input latches, profile, ghost, inspector ... */
    int     debug_mode;
    DebugOverlay debug;
} GameScreen;

typedef struct GameAssets {      /* in game_assets.h */
    TextureResources textures;    /* Texture2D slots, owned by AppSession */
    AudioResources   audio;       /* SoundEffect slots, owned by AppSession */
} GameAssets;

typedef struct GameState {
    GameWorld  world;
    GameScreen screen;
    GameAssets assets;            /* a copy of the session's pointers */
    int        owns_assets;       /* 1 only when game_init loaded them */
    AssetCache *cache;            /* the session's cache; NULL without one */
} GameState;
```

**Key design decisions:**

- `world` is the level itself: what is in it, where it is and how the player is doing (hearts, lives, score, checkpoints). "What happens to a spider each frame" is a walk over `gs->world.spiders`. `screen` is how this screen presents and steers that level: the HUD, the pause, game-over and completion overlays, routes, the frame clock, input, replay scripts and debug tools.
- Textures are grouped in `TextureResources` (`gs->assets.textures.*`) and sound effects in `AudioResources` (`gs->assets.audio.*`) so loading and cleanup walk one table each (`src/core/game_resources.c`).
- Those shared sprites and sounds belong to the session, not the game: `AppSession` loads one `GameAssets` when the first game opens, copies it into every `GameState` it creates and unloads it once when the session ends, so Replay, Level Select → Play and Next Level reuse the same textures. A copy holds the same pointers, so a game only borrows them (`owns_assets` stays 0) and `game_cleanup` leaves them loaded. A `GameState` started without a session (the tests do this) loads a private set in `game_init` and unloads it in `game_cleanup`. Everything a level file names (background layers, floor tileset, water strip, fog, platform tiles, music) is in `world` and is acquired and released with the level (`src/levels/level_resources.c`).
- Those level files, and the player sheet, HUD coin icon and font, come from the session's **asset cache** (`AssetCache`, `src/shared/asset_cache.h`), described in the next section. `gs->cache` is a borrowed pointer to it.
- `Player` is **embedded by value**, not a pointer. This avoids a heap allocation and keeps the struct self-contained. The same applies to `Platform`, `Water`, `FogSystem`, all entity arrays and the three parts themselves.
- Owning pointers are cleared after release. Borrowed pointers and aliases still require correct lifetime handling.
- Active-game storage is heap-owned and zero-initialized before initialization; the structs above are an abridged ownership map, not complete declarations.
- The resolved respawn state is `respawn_x`, `respawn_y`, and `checkpoint_index`; `legacy_checkpoint_screen` is used only when the active level has no authored records. `checkpoint_index` and `respawn_x`/`respawn_y` are always written together (on load, on retry and when a record is reached), so a valid index always names the current respawn.

### The asset cache: files that outlive a game

A Replay closes one `GameState` and opens another on the same level. Loading a level's files with the game and freeing them with it therefore read the same PNGs and the same music again each time. `AppSession` owns one `AssetCache` (`src/shared/asset_cache.h`) for the whole run, and every game acquires its files from it:

```c
player->texture = asset_cache_texture_acquire(cache, "assets/sprites/player/player.png");
/* ... the game runs ... */
asset_cache_texture_release(cache, player->texture);
player->texture = NULL;
```

The cache keeps one entry per file path, with a count of **holders**: acquires that have not been released yet. This is a reference count. Three rules follow from it:

- **Acquire** returns the cached texture when the path is already there (no file is read) and loads it otherwise. A file that fails to load is not remembered, so the next game tries it again.
- **Release** only lowers the count. An entry with no holders is *idle*: still loaded, because the next game very likely wants it. A released music track is also stopped and rewound.
- **Trim** (`asset_cache_trim`) unloads the idle entries. `game_level_commit` calls it once a level has finished loading: the new level then holds everything it needs, so what is still idle belonged only to the level before.

| Step | Files read from disk on `01_lugio_01` |
|------|----------------------------------------|
| First game of the session | 45 textures (30 shared, 15 of the level), 8 sounds, 1 music track |
| Replay, Retry, Level Select → Play the same level | none |
| Next Level to `02_lugio_02` (same scenery and music) | none |
| Next Level from `00_sandbox_01` (other scenery) | 12 textures and 1 music track; the sandbox's 7 are unloaded |

Memory stays bounded by the level, not by how many levels were visited: the cache holds the current level's files (at most 15 textures, 3.1 MiB of pixels, in the shipped levels) plus, between closing one level and finishing the load of the next, the previous level's. Keeping every file until the session ends would also have made Replay free, but its size would grow with each level added; the nine shipped levels together name 25 images (5.7 MiB).

The start menu uses the same cache for its font, logo and confirm sound. Nothing would hold those three while a game is on screen, so the trim after the next level load would unload them and every return to Level Select would read them again. `AppSession` therefore takes a reference of its own to each when the first menu opens and keeps it until the session ends (`session_keep_menu_files`): a holder that exists only to keep something loaded. The first menu of a session reads one texture and one sound; later menus read nothing. They cost 12 KiB of pixels and a 0.22 s sound.

The cache and `GameAssets` are two answers to the same question, "who frees this?". The shared sprites never go away while the session runs, so a game simply borrows the pointers and nobody counts. A level's files should go away when no level uses them any more, and only a count can tell when that is.

### Authored Checkpoint Flow

`LevelDef` owns optional immutable `CheckpointPlacement { x, y }` records. Each active frame samples authored records after player movement and before lethal collisions. The furthest record with `x <= player.x` becomes the resolved respawn point, so a death in the same frame preserves a crossed checkpoint. The runtime never regresses to an earlier record.

Authored records disable automatic screen-boundary checkpoints for that level. A level with no records saves automatically when the player enters a new screen; the respawn column is the screen edge, or the nearest column to its left (over ground already crossed) with solid floor and no floor gap, spike row, spike platform or flame. If no such column exists the previous checkpoint is kept. Losing a life respawns at the resolved checkpoint and keeps collected coins collected; Retry, replay, and successful next-phase loads restore every coin and reset to the effective start of their respective level; a failed next-phase load retains the active level and its resolved checkpoint. The HUD shows brief `CHECKPOINT CP n` (1.2 s) and `RESPAWN CP n` (0.9 s) notices. They are timed in simulated time (`sim_steps`, a count of fixed steps turned into milliseconds with integers, restarted whenever a level is applied, F8 restarts included), so a pause does not use them up and they last exactly 72 and 54 steps, the same steps in every replay. The debug inspector exposes the stored checkpoint index; the regular HUD does not keep a permanent checkpoint label after the notice expires.

---

## Error Handling Strategy

| Situation | Action |
|-----------|--------|
| Window/audio initialization failure | Session creation fails and releases initialized resources; the top-level runner returns `EXIT_FAILURE` |
| Resource load failure (shared sprites when the first game opens; player, HUD, water or level files in `game_init`) | `fprintf(stderr, ...)` → release what was partly loaded → return `-1`, so the game does not open; for `--level` the top-level runner returns `EXIT_FAILURE` |
| Optional sound load failure | Warn and retain an empty slot; `sound_play` accepts NULL |
| Missing gameplay-critical shared sprite | Reject the level before replacing active level state; identify the required asset path |
| Optional presentation texture load failure | Warn and preserve the documented visual fallback |

Application errors identify the failing asset/path or lifecycle operation. raylib's
warnings provide backend detail. Text uses raylib's built-in default font; the
game frees only its small handle, never the borrowed atlas. Textures and sound
aliases are released before their owning context/sample/device. The game
requires an audio device: if none can be opened, session creation fails. Menu/game transitions retain the same
window, including when a candidate level fails to load.

Version-1 profile binding numbers (USB HID keyboard usage IDs; the ones code names directly are `BINDING_KEY_*` in `src/input/input_backend.h`) are translated explicitly. Unsupported legacy
media/paddle/touchpad bindings retain their stored values and show a remap warning;
fixed keyboard navigation remains available. Native preference paths retain both
organization and application components. Browser input handlers are scoped to
`Module.canvas`; save namespaces and asynchronous commit/teardown ownership stay
unchanged.
