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
       ├── no `--level` → start_menu_create()
       └── `--level` / `--sandbox` → session_open_game() → game_init(gs)
            ├── create a screen-owned logical render target
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
  ├── clear input handlers and sound voices after screen assets are released
  ├── CloseAudioDevice → CloseWindow
  └── free AppSession
```

---

## App Session and Game Frame

`AppSession` is the sole production loop owner. It samples physical input, pumps the music stream, frames the active screen, then consumes its route. raylib's `EndDrawing` owns presentation and event polling. Native visible windows use its **60 FPS** limiter; hidden smoke runs are uncapped and retain fixed simulation steps.

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
                     → actors → moving platforms/rider carry → bridges → hazards
                     → game_collide (enemy/hazard damage, coins/stars, completion)
                     → legacy checkpoints → effects → bouncepad animation → camera
                     Death/game-over returns early after camera update, avoiding stale collisions.
  4. Render       — clear → parallax background → platforms → floor tiles
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
redraws. The accumulator restarts half a step full after pauses and loads, which
absorbs sub-millisecond vsync jitter. Frame time is clamped to 0.25 s and at most
5 steps run per frame; excess time is dropped (below 12 FPS the game slows down
instead of spiralling). Smoke and scripted replays feed exactly one step per
frame regardless of the wall clock. Experiment captures (`format_version = 2`)
store one row per fixed step, `[input bits, physics values...]`, with no
duration column; a `format_version = 1` capture from the variable-timestep
engine is rejected with a request to record it again. Completion, game over or
a route stops the remaining steps of that frame. The debug inspector's freeze,
single-step and slow-motion keys ([Controls](../controls/#debug-inspector-keys))
only change how much real time reaches the accumulator. They never override the
focus, settings or end-of-level blockers, and touch taps are discarded only
while such a screen owns input, not on zero-step frames.
Render-only timers (debug FPS readout, log ages) use real frame time.

During an active game update, authored checkpoints are sampled after player movement and before lethal collision handling. Legacy screen-boundary checkpoint sampling runs only when the active level has no authored records.

### Render Order (back to front)

Each row names the function that draws the layer, in the order
`game_render_frame()` reaches it. The frame calls a short list of `draw_*`
helpers in `src/render/game_render.c`, each drawing a run of neighbouring
layers (`draw_ground`, `draw_surfaces`, `draw_collectibles`,
`draw_water_layer`, `draw_moving_hazards`, `draw_enemies`, `draw_foreground`,
`draw_hud_and_overlays`), and those call the functions below.
`make docs-drift` follows the helpers and checks that this order matches the
source.

| Layer | What | Drawn by |
|-------|------|----------|
| 1 | Background: per-level `background_layers` from `assets/sprites/backgrounds/`, tiled with each layer's scroll speed | `parallax_render` |
| 2 | Platforms: 9-slice pillar stacks, drawn before the floor so they sink into the ground | `platforms_render` |
| 3 | Floor: the level's floor tile across the world at `FLOOR_Y`, with floor-gap openings | `draw_floor`, a loop in `game_render.c` |
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
| 29 | Player | `player_render` |
| 30 | Fog: per-level `fog_layers` from `assets/sprites/foregrounds/` | `fog_render` |
| 31 | HUD: hearts, lives, score | `hud_render` |
| 32 | Debug overlay and inspector panels, with `--debug` | `debug_render`, `game_inspector_render` |

> **Note:** Per-level visual layers are split by role: `background_layers` feed the parallax renderer, `foreground_layers` select the water/lava foreground strip texture, and `fog_layers` feed the atmospheric fog system. Fog renders before the HUD so hearts/lives/score remain legible.

The pause, game-over and completion overlays and the settings panel are drawn
after these 32 layers.

### Level Completion and Terminal Actions

Collecting `last_star` calls `game_complete_level()`. The game snapshots elapsed time, coins collected, total coins, and the resolved `next_phase` path (if any), then shows a completion overlay. While it is active, gameplay update pauses. Its action list is **Next Level**, **Replay**, **Level Select**, **Exit** when a phase is pending; otherwise it is **Replay**, **Level Select**, **Exit**. Up/Down or D-pad moves the focused row with wraparound. Enter/Space/Start confirms it (controller A also confirms). Esc/Back exits immediately (controller B is equivalent).

Next Level uses `game_load_next_phase()` without replacing the game screen. If loading fails, the completion overlay stays visible with a "Next level failed to load" line (`completion.next_phase_failed`), the dead Next Level row is removed and focus moves to Replay; the current level is untouched, so the remaining actions keep working. Level Select closes the game screen and opens the start menu in the same `AppSession`. Native Replay closes the game screen and opens the same TOML path in a new `GameState`; Browser Replay persists that path in session storage, cancels the Emscripten callback, tears down once, reloads, and then boots the stored level.

### Pause Overlay Flow

During active gameplay, Esc or controller Start toggles the player pause reason through the overlay helper in `src/core/game_overlay.c`. Paused frames keep rendering the last camera position, skip gameplay updates, pause music, and draw a semi-transparent pause overlay with resume hints. Enter, Space, Esc, or controller Start resumes gameplay. Window focus loss uses a separate focus pause reason, so regaining focus does not clear an intentional player pause. Music follows one predicate, `game_music_should_play()`: it is silent while the pause overlay or the settings panel is up and plays otherwise (including under completion and game-over screens), so regaining focus cannot resume music beneath open settings. Completion and game-over overlays take priority over pause.

### Game-Over Flow

When lethal damage consumes the final life, `apply_damage()` sets `gs->game_over` and returns without resetting the level. The shared overlay helper reports `GAME_OVERLAY_GAME_OVER`, so the loop blocks gameplay updates and rendering draws a game-over overlay with the final score. Its terminal action list is **Retry**, **Level Select**, **Exit**. Retry calls `game_restart_after_game_over()` in place: it restores level-defined lives/hearts, resets score and bonus-life threshold, makes every coin collectable again, resets the current level, resumes music, and clears its input latch after held controls are released. Level Select and Exit follow the session routes above.

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
Gameplay hitboxes remain integer `IntRect` values; raylib's floating `Rectangle`
is a drawing boundary type. Two hitboxes that merely touch edges do not overlap.

---

## GameState Struct

Defined in `game.h`. The **single container** for active-game resources; `AppSession` owns app-wide runtime state and the active screen. The plain numbers it and the entity code share (`GAME_W`, `FLOOR_Y`, `GRAVITY`, `MAX_FLOOR_GAPS`, camera tuning) live in `game_constants.h`, which `game.h` includes; entity, hazard, surface and effect `.c` files that need only those numbers include `game_constants.h` alone, so a `GameState` change does not recompile them.

```c
typedef struct {
    RenderTexture2D frame_target;  /* screen owns target; session owns window */
    int controller;               /* raylib index + 1; zero means none */
    TextureResources textures;    /* owned Texture2D slots */
    AudioResources audio;         /* owned SoundEffect / MusicTrack slots */

    ParallaxSystem parallax;
    Player         player;
    Platform       platforms[MAX_PLATFORMS];
    Water          water;
    FogSystem      fog;
    /* fixed-size arrays + counts for every enemy, hazard, collectible, surface */

    Hud     hud;
    GameCamera camera;
    int     hearts, lives, score, score_life_next;
    int     running;
    GameRoute route;              /* request consumed by AppSession */
    int     game_over;
    int     paused;
    unsigned int pause_reasons;
    float   respawn_x, respawn_y;
    int     checkpoint_index;     /* -1 before the first authored record */
    CheckpointFeedbackKind checkpoint_feedback_kind;
    uint32_t checkpoint_feedback_until;
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
- Active-game storage is heap-owned and zero-initialized before initialization; the struct above is an abridged ownership map, not a complete declaration.
- The resolved respawn state is `respawn_x`, `respawn_y`, and `checkpoint_index`; `legacy_checkpoint_screen` is used only when the active level has no authored records. `checkpoint_index` and `respawn_x`/`respawn_y` are always written together (on load, on retry and when a record is reached), so a valid index always names the current respawn.

### Authored Checkpoint Flow

`LevelDef` owns optional immutable `CheckpointPlacement { x, y }` records. Each active frame samples authored records after player movement and before lethal collisions. The furthest record with `x <= player.x` becomes the resolved respawn point, so a death in the same frame preserves a crossed checkpoint. The runtime never regresses to an earlier record.

Authored records disable automatic screen-boundary checkpoints for that level. A level with no records saves automatically when the player enters a new screen; the respawn column is the screen edge, or the nearest column to its left (over ground already crossed) with solid floor and no floor gap, spike row, spike platform or flame. If no such column exists the previous checkpoint is kept. Losing a life respawns at the resolved checkpoint and keeps collected coins collected; Retry, replay, and successful next-phase loads restore every coin and reset to the effective start of their respective level; a failed next-phase load retains the active level and its resolved checkpoint. The HUD shows brief `CHECKPOINT CP n` and `RESPAWN CP n` notices. The debug inspector exposes the stored checkpoint index; the regular HUD does not keep a permanent checkpoint label after the notice expires.

---

## Error Handling Strategy

| Situation | Action |
|-----------|--------|
| Window/audio initialization failure | Session creation fails and releases initialized resources; the top-level runner returns `EXIT_FAILURE` |
| Resource load failure (in `game_init`) | `fprintf(stderr, ...)` → clean up partially-created `GameState` resources → return `-1`; the top-level runner returns `EXIT_FAILURE` |
| Optional sound load failure | Warn and retain an empty slot; `sound_play` accepts NULL |
| Missing gameplay-critical shared sprite | Reject the level before replacing active level state; identify the required asset path |
| Optional presentation texture load failure | Warn and preserve the documented visual fallback |

Application errors identify the failing asset/path or lifecycle operation. raylib's
warnings provide backend detail. Text uses raylib's built-in default font; the
game frees only its small handle, never the borrowed atlas. Textures and sound
aliases are released before their owning context/sample/device. The game
requires an audio device: if none can be opened, session creation fails. Menu/game transitions retain the same
window, including when a candidate level fails to load.

Version-1 profile binding numbers are translated explicitly. Unsupported legacy
media/paddle/touchpad bindings retain their stored values and show a remap warning;
fixed keyboard navigation remains available. Native preference paths retain both
organization and application components. Browser input handlers are scoped to
`Module.canvas`; save namespaces and asynchronous commit/teardown ownership stay
unchanged.
