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

`AppSession` is the sole production loop owner. It samples physical input, pumps the music stream, frames the active screen, then consumes its route. raylib's `EndDrawing` owns presentation, event polling and normal **60 FPS** pacing. Hidden smoke runs are uncapped and retain fixed simulation steps.

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
  1. Delta Time   — measure ms since last frame → dt (seconds)
  2. Events       — drain project-owned InputEvent commands (quit / pause / overlays)
                     INPUT_PAD_ADDED / INPUT_PAD_REMOVED — update selected gamepad index
                     terminal: Up/Down or D-pad selects; Enter/Space/Start (or A) confirms
                     terminal: Esc/Back (or B) exits; Start toggles active-game pause
   3. Update       — inspector selects simulation dt; pause/settings/terminal state can block it
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

### Delta Time

```c
uint64_t now = clock_millis();
float  dt  = (float)(now - prev) / 1000.0f;
prev = now;
```

Velocities are expressed in **pixels per second**. Multiplication by `dt` gives a displacement, but discrete acceleration and collision sampling still introduce timestep-dependent error. `make timing-lab` demonstrates this distinction. Replay uses recorded steps; the inspector can freeze, single-step, slow and tune a simulation without overriding focus/settings/terminal blockers.

Targeting 60 rendered frames per second does not make normal gameplay a fixed
step: `game_timing_step` measures elapsed time and clamps it to 0.1 seconds.
Smoke/scripted input uses `1 / TARGET_FPS`; captured experiments use their
recorded durations. These choices are separate from presentation pacing.

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

The 32 rows group rendering passes: green/red stars share the collectible pass;
terminal and settings overlays render after the gameplay/debug layers.

### Level Completion and Terminal Actions

Collecting `last_star` calls `game_complete_level()`. The game snapshots elapsed time, coins collected, total coins, and the resolved `next_phase` path (if any), then shows a completion overlay. While it is active, gameplay update pauses. Its action list is **Next Level**, **Replay**, **Level Select**, **Exit** when a phase is pending; otherwise it is **Replay**, **Level Select**, **Exit**. Up/Down or D-pad moves the focused row with wraparound. Enter/Space/Start confirms it (controller A also confirms). Esc/Back exits immediately (controller B is equivalent).

Next Level uses `game_load_next_phase()` without replacing the game screen. If loading fails, the completion overlay remains visible and keeps its current focus. Level Select closes the game screen and opens the start menu in the same `AppSession`. Native Replay closes the game screen and opens the same TOML path in a new `GameState`; Browser Replay persists that path in session storage, cancels the Emscripten callback, tears down once, reloads, and then boots the stored level.

### Pause Overlay Flow

During active gameplay, Esc or controller Start toggles the player pause reason through the overlay helper in `src/core/game_overlay.c`. Paused frames keep rendering the last camera position, skip gameplay updates, pause music, and draw a semi-transparent pause overlay with resume hints. Enter, Space, Esc, or controller Start resumes gameplay. Window focus loss uses a separate focus pause reason, so regaining focus does not clear an intentional player pause. Completion and game-over overlays take priority over pause.

### Game-Over Flow

When lethal damage consumes the final life, `apply_damage()` sets `gs->game_over` and returns without resetting the level. The shared overlay helper reports `GAME_OVERLAY_GAME_OVER`, so the loop blocks gameplay updates and rendering draws a game-over overlay with the final score. Its terminal action list is **Retry**, **Level Select**, **Exit**. Retry calls `game_restart_after_game_over()` in place: it restores level-defined lives/hearts, resets score and bonus-life threshold, reloads the current level, resumes music, and clears its input latch after held controls are released. Level Select and Exit follow the session routes above.

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

Defined in `game.h`. The **single container** for active-game resources; `AppSession` owns app-wide runtime state and the active screen.

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
    int     game_over;
    int     paused;
    unsigned int pause_reasons;
    float   respawn_x, respawn_y;
    int     checkpoint_index;
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
- `checkpoint_x` is no longer a `GameState` field. The resolved respawn state is `respawn_x`, `respawn_y`, and `checkpoint_index`; `legacy_checkpoint_screen` is used only when the active level has no authored records.

### Authored Checkpoint Flow

`LevelDef` owns optional immutable `CheckpointPlacement { x, y }` records. Each active frame samples authored records after player movement and before lethal collisions. The furthest record with `x <= player.x` becomes the resolved respawn point, so a death in the same frame preserves a crossed checkpoint. The runtime never regresses to an earlier record.

Authored records disable automatic screen-boundary checkpoints for that level. A level with no records preserves the legacy boundary behavior. Retry, replay, and successful next-phase loads reset to the effective start of their respective level; a failed next-phase load retains the active level and its resolved checkpoint. The HUD shows brief `CHECKPOINT CP n` and `RESPAWN CP n` notices. The debug inspector exposes the stored checkpoint index; the regular HUD does not keep a permanent checkpoint label after the notice expires.

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
warnings provide backend detail. Fonts, textures and sound aliases are released
before their owning context/sample/device. Menu/game transitions retain the same
window, including when a candidate level fails to load.

Version-1 profile binding numbers are translated explicitly. Unsupported legacy
media/paddle/touchpad bindings retain their stored values and show a remap warning;
fixed keyboard navigation remains available. Native preference paths retain both
organization and application components. Browser input handlers are scoped to
`Module.canvas`; save namespaces and asynchronous commit/teardown ownership stay
unchanged.
