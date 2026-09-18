# Developer Guide

<a id="home"></a>

---

This guide covers the patterns and conventions used in Super Mango and explains how to extend the game safely and consistently.

For a guided sequence, start with [Sandbox School](../learning-path/). The
[Entity Walkthrough](../entity-walkthrough/) traces all runtime, schema and editor
integration points; the abbreviated examples here introduce conventions.

---

## Coding Conventions

### Language and Standard

- **C11** (`-std=c11`)
- Compiler: `clang` (default), `gcc` compatible
- Keep builds warning-free under `-Wall -Wextra -Wpedantic`; fix warnings rather than suppressing them.

### Comments and Module Layout

The source is a learning resource for C and raylib. Readable layout and teaching
comments are intentional, even when shorter code could do the same job:

- Start headers and source files with a short module summary. Headers use `#pragma once` and expose constants, types, and public function declarations.
- Explain nontrivial raylib calls, including important arguments, return values, and resource ownership.
- Give numeric constants their units and origin, especially logical pixels, pixels per second, and animation durations.
- Document pointer ownership, float-to-integer rendering casts, and cleanup order where they matter.
- Keep separate actions and nontrivial `switch` cases on separate lines. Prefer
  an explicit loop or early return to a dense expression when it helps a learner
  trace the state change. Share genuinely repeated behavior without hiding each
  small step behind a new helper.
- Explain unfamiliar C idioms when useful: designated initializers, borrowed
  pointers, bounds-before-casts, and frame-time unit conversions. Do not remove
  a useful explanation solely because the implementation is now shorter.

raylib APIs use names such as `LoadTexture` and `DrawTexturePro`. The snake-case
helpers in `src/shared/` and `src/input/input_backend.*` belong to this project;
they preserve explicit ownership, logical coordinates and saved-binding IDs.

### Naming

| Category | Convention | Example |
|----------|------------|---------|
| Files | `snake_case` | `player.c`, `coin.h` |
| Functions | `module_verb` | `player_init`, `coins_render` |
| Struct types | `PascalCase` via `typedef` | `Player`, `GameState`, `Coin` |
| Enum values | `UPPER_SNAKE_CASE` | `ANIM_IDLE`, `ANIM_WALK` |
| Constants (`#define`) | `UPPER_SNAKE_CASE` | `FLOOR_Y`, `TILE_SIZE` |
| Local variables | `snake_case` | `dt`, `frame_ms`, `elapsed` |
| Assets | `snake_case` under `assets/sprites/<category>/` | `player/player.png`, `collectibles/coin.png`, `entities/spider.png` |
| Sounds | `component_descriptor.wav` under `assets/sounds/<category>/` | `player/player_jump.wav`, `collectibles/coin.wav`, `entities/bird.wav` |

### Memory and Safety Rules

- Clear each owning pointer after releasing its resource. A NULL guard only protects an already-NULL pointer; aliases and borrowed pointers require explicit lifetime discipline.
- Error paths identify the failing operation and asset path; raylib warnings provide backend detail.
- Required initialization failures return failure to the caller for cleanup; only the top-level runner returns `EXIT_FAILURE`. Optional sound-effect loads warn and continue; `sound_play` accepts an empty `SoundEffect` slot.
- Release dependents before owners: aliases before samples, cached labels before
  fonts, and all screen resources before the graphics/audio context. Reverse
  initialization order is a useful way to achieve this, not a reason to free a
  borrowed resource twice.
- Use `float` for positions and velocities. Preserve integer `IntRect` hitbox construction and edge rules; convert to raylib `Rectangle` at drawing boundaries.

### Coordinate System

Game-object positions and sizes use **logical pixels**. The viewport is 400×300,
but world X can extend across `screen_count` screens; rendering subtracts camera X.
Never use `WINDOW_W` / `WINDOW_H` for game math. The shared presentation helper scales the logical render target to the OS window with nearest filtering and an inverse pointer transform.

See [Constants Reference](../constants-reference/) for all defined constants.

---

## Project Documents and Ownership

| File | Purpose |
|------|---------|
| `PRODUCT.md` | Product direction and feature framing. |
| `DESIGN.md` | Visual/UX design notes for the cabinet-style presentation. |
| `docs/wiki/developer-guide.md` | Coding conventions, entity integration, resource ownership, and verification. |
| `CODEOWNERS` | GitHub ownership hints for review routing. |

Treat source and workflows as authoritative. When project documents, README, or GH Pages copy drift from implementation, update the docs and run [Testing & Smoke Matrix](../testing/) checks before shipping.

---

## Verification and Runtime Controls

Run the 15-test `make test` suite (15 native binaries plus Python and JavaScript host checks) for runtime/editor changes. `make validate-levels` checks level and campaign data; `make docs-drift` checks semantic docs drift, generated catalog freshness, and roadmap quality. For documentation changes, also run `bun run lint`, `bun run build` and `bun run check-site` from `docs/`; compilation alone does not verify links. See [Build System](../build-system/) and [Testing & Smoke Matrix](../testing/) for the full gates.

Terminal overlays use Up/Down or D-pad to select, Enter/Space/Start to confirm (A also confirms), and Esc/Back to exit (B also exits). Completion offers Next Level when configured, Replay, Level Select, and Exit; game over offers Retry, Level Select, and Exit. See [Controls](../controls/) for the full input reference.

---

## Adding a New Entity

Most active entities follow this lifecycle pattern:

```text
entity_init    -> set initial state (textures often live shared in GameState)
entity_update  -> move, apply physics, detect events
entity_render  -> draw to the active raylib target
entity_cleanup -> release only owned resources, then clear their slots
```

Collectibles and simple decorations may use lighter helpers. For example, coins store only placement state in `Coin` and render through `coins_render()` using a shared texture from `GameState`.
The coin renderer borrows that texture; it must not unload it. Resource cleanup
in `game_resources.c` owns the shared slot.

Active entities may also expose:

```text
entity_handle_input   -> if player-controlled
entity_animate        -> static helper, called from entity_update
```

### Step-by-Step

#### 1. Create the header -- coin-like collectible example

```c
#pragma once
#include "../shared/graphics.h"

#define MAX_COINS       64
#define COIN_DISPLAY_W  16
#define COIN_DISPLAY_H  16
#define COIN_SCORE     100

typedef struct {
    float x;      /* logical position (top-left) */
    float y;
    int   active; /* 1 = visible, 0 = collected */
} Coin;

void coins_render(const Coin *coins, int count,
                  Texture2D *tex, int cam_x);
```

#### 2. Create the implementation -- `src/collectibles/coin.c`

```c
#include "collectibles/coin.h"

void coins_render(const Coin *coins, int count,
                  Texture2D *tex, int cam_x) {
    if (!tex) return;

    for (int i = 0; i < count; i++) {
        if (!coins[i].active) continue;

        IntRect dst = {
            (int)(coins[i].x - cam_x),
            (int)coins[i].y,
            COIN_DISPLAY_W,
            COIN_DISPLAY_H
        };
        sprite_draw(tex, NULL, &dst, 0, SPRITE_NORMAL, WHITE);
    }
}
```

The Makefile picks up `coin.c` automatically from the `src/collectibles/` subdirectory -- **no Makefile changes needed**. New source directories require an explicit wildcard entry in the Makefile.

#### 3. Add texture to `TextureResources` in `game.h`

Textures are loaded in `game_init()` and stored under `gs->textures`. The entity array and count live directly in `GameState`:

```c
#include "collectibles/coin.h"

typedef struct {
    // ... existing fields ...
    TextureResources textures; /* contains Texture2D *coin */
    Coin coins[MAX_COINS];    /* fixed-size array -- simple and cache-friendly */
    int  coin_count;          /* populated slots; each Coin has its own active flag */
} GameState;
```

#### 4. Wire up in the runtime core

```c
// src/core/game_resources.c -- load shared texture:
gs->textures.coin = texture_load("assets/sprites/collectibles/coin.png");
if (!gs->textures.coin) {
    fprintf(stderr, "Failed to load assets/sprites/collectibles/coin.png\n");
    return -1;
}

// level_loader.c -- populate array from TOML placements:
gs->coins[i] = (Coin){ .x = def->coins[i].x, .y = def->coins[i].y, .active = 1 };
gs->coin_count = def->coin_count;

// focused runtime helper render section, in the correct layer order:
coins_render(gs->coins, gs->coin_count, gs->textures.coin, (int)gs->camera.x);

// src/core/game_resources.c cleanup, before closing the graphics context:
DESTROY_TEX(gs->textures.coin);
```

Use the focused runtime module that owns the behavior: resource loading belongs in `src/core/game_resources.c`, lifecycle orchestration in `src/core/game_lifecycle.c`, per-frame update orchestration in `src/core/game_update.c` and its specialized helpers, and collision/pickup behavior in `src/collision/`.

#### 5. Add to a TOML level file

Entity spawn positions are defined in TOML level files in the `levels/` directory. Add your entity's array table entry there:

```toml
# In levels/your_level.toml:
[[coins]]
x = 120.0
y = 180.0

[[coins]]
x = 200.0
y = 140.0
```

Register and parse the array in `src/shared/serializer_parse.c` and the relevant `serializer_load_*.c`; emit it in `serializer_save.c` and validate it in both `level_validate.c` and `tools/validate_levels.py`. Then extend `level_loader.c` to translate the validated placements into `GameState`. Complete palette/tools/preview/property/undo/clipboard/hash integration using the [Entity Walkthrough](../entity-walkthrough/).

You can also use the visual level editor (`make run-editor`) to place entities interactively without writing TOML by hand.

#### 6. Add debug hitbox -- `src/core/debug.c`

Every entity must have hitbox visualization in `core/debug.c`:

```c
// In debug_render:
for (int i = 0; i < gs->coin_count; i++) {
    if (!gs->coins[i].active) continue;
    IntRect hb = { (int)gs->coins[i].x - cam_x, (int)gs->coins[i].y,
                    COIN_DISPLAY_W, COIN_DISPLAY_H };
    DrawRectangleLines(hb.x, hb.y, hb.w, hb.h, (Color){255,255,0,128});
}
```

Also add `debug_log` calls in the module that owns the event, such as `src/collision/game_collision.c`, `src/core/game_update.c`, or the relevant focused runtime helper.

---

## Adding Physics to an Entity

Use the same pattern as `player_update`:

```c
/* Apply gravity while airborne */
if (!entity->on_ground) {
    entity->vy += GRAVITY * dt;
}

/* Integrate position */
entity->x += entity->vx * dt;
entity->y += entity->vy * dt;

/* Floor collision */
if (entity->y + entity->h >= FLOOR_Y) {
    entity->y        = (float)(FLOOR_Y - entity->h);
    entity->vy       = 0.0f;
    entity->on_ground = 1;
} else {
    entity->on_ground = 0;
}

/* Horizontal clamp to the active level width */
if (entity->x < 0.0f)                entity->x = 0.0f;
if (entity->x > world_w - entity->w) entity->x = (float)(world_w - entity->w);
```

`GRAVITY`, `FLOOR_Y`, `GAME_W`, and `GAME_H` are all defined in `game.h` and available to any file that includes it. See [Constants Reference](../constants-reference/) for values.

This example is a simplified solid-floor integrator. Pass the active
`gs->runtime.world_w` as `world_w`; real player movement also resolves gaps,
one-way surfaces and the sprite's inset foot position in `player_surfaces.c`.

---

## Adding a New Sound Effect

All sound files are `.wav` format, named with the convention `component_descriptor.wav`:

| Sound | File |
|-------|------|
| Player jump | `player_jump.wav` |
| Player hit | `player_hit.wav` |
| Coin collect | `coin.wav` |
| Bouncepad | `bouncepad.wav` |
| Bird | `bird.wav` |
| Fish | `fish.wav` |
| Spider | `spider.wav` |
| Axe trap | `axe_trap.wav` |

Steps to add a new sound:

1. Place `.wav` in `assets/sounds/<category>/`.
2. Add `SoundEffect *<name>;` to `AudioResources` in `game.h`.
3. Load in `game_init` (non-fatal -- warn but continue):

```c
gs->audio.<name> = sound_load("assets/sounds/<category>/<name>.wav");
if (!gs->audio.<name>) {
    fprintf(stderr, "Warning: could not load assets/sounds/<category>/<name>.wav\n");
}
```

4. Free in `game_cleanup`:

```c
FREE_CHUNK(gs->audio.<name>);
```

5. Play wherever needed:

```c
sound_play(gs->audio.<name>, 128); // null-safe; per-play volume in authored units
```

See [Sounds](../sounds/) for the full list of available sound files.

---

## Adding Background Music

Background music uses raylib streams through the project `MusicTrack` owner. Runtime levels provide the active music path through TOML:

```c
// Load from current LevelDef
gs->audio.music = music_load(def->music_path);

// Play (looping)
music_play(gs->audio.music);
music_set_volume(64); // 50%; normal sessions combine level/user/mute settings

// Cleanup
music_unload(gs->audio.music);
gs->audio.music = NULL;
```

---

## Adding HUD / Text Rendering

The graphics context must exist before loading fonts. `TextFont` requests additional
UTF-8 glyphs as text changes; unavailable glyphs still use the font fallback.
Font atlases and cached label textures have
explicit owners. The font is in `assets/fonts/`.

```c
// Load font
TextFont *font = font_load("assets/fonts/round9x13.ttf", 13);
if (!font) return -1;

// Draw while the frame's render target is active
font_draw(font, "Score: 0", 10, 10, WHITE);

// Cleanup before closing the graphics context
font_unload(font);
```

The HUD renders hearts (health), life counter and score. It is drawn after game entities; terminal/settings overlays can cover it.

For repeated labels, reuse `UIState`'s bounded cache or `font_texture`. Rebuild
only when content/appearance changes. `texture_unload` flushes pending raylib
draws before freeing a texture, which matters when a cache entry is evicted
within a frame. Release cached textures before their font and graphics context.

---

## Render Layer Order

Always draw in painter's algorithm order (back to front). The game currently uses 32 layers:

```
 1. Parallax background    (`assets/sprites/backgrounds/*.png` layers)
 2. Platforms              (`assets/sprites/levels/*_platform.png`, 9-slice pillars)
 3. Floor tiles            (level floor tile at FLOOR_Y, with floor-gap openings)
 4. Float platforms        (`assets/sprites/surfaces/float_platform.png`)
 5. Spike rows             (`assets/sprites/hazards/spike.png`)
 6. Spike platforms        (`assets/sprites/hazards/spike_platform.png`)
 7. Bridges                (`assets/sprites/surfaces/bridge.png`)
 8. Bouncepads medium      (`assets/sprites/surfaces/bouncepad_medium.png`)
 9. Bouncepads small       (`assets/sprites/surfaces/bouncepad_small.png`)
10. Bouncepads high        (`assets/sprites/surfaces/bouncepad_high.png`)
11. Rails                  (`assets/sprites/surfaces/rail.png`)
12. Vines                  (`assets/sprites/surfaces/vine_green.png` / `vine_brown.png`)
13. Ladders                (`assets/sprites/surfaces/ladder.png`)
14. Ropes                  (`assets/sprites/surfaces/rope.png`)
15. Coins                  (`assets/sprites/collectibles/coin.png`)
16. Yellow stars           (`assets/sprites/collectibles/star_yellow.png`)
17. Last star              (`assets/sprites/collectibles/last_star.png`)
18. Blue/fire flames       (`assets/sprites/hazards/blue_flame.png` / `fire_flame.png`)
19. Fish                   (`assets/sprites/entities/fish.png`)
20. Faster fish            (`assets/sprites/entities/faster_fish.png`)
21. Water                  (`assets/sprites/foregrounds/water.png`)
22. Spike blocks           (`assets/sprites/hazards/spike_block.png`)
23. Axe traps              (`assets/sprites/hazards/axe_trap.png`)
24. Circular saws          (`assets/sprites/hazards/circular_saw.png`)
25. Spiders                (`assets/sprites/entities/spider.png`)
26. Jumping spiders        (`assets/sprites/entities/jumping_spider.png`)
27. Birds                  (`assets/sprites/entities/bird.png`)
28. Faster birds           (`assets/sprites/entities/faster_bird.png`)
29. Player                 (`assets/sprites/player/player.png`)
30. Fog                    (`assets/sprites/foregrounds/fog_1.png` / `fog_2.png`)
31. HUD                    (hearts, lives, score -- always on top)
32. Debug overlay          (FPS, hitboxes, event log -- when --debug)
```

See [Architecture](../architecture/) for details on the render pipeline.

---

## Sprite Sheet Workflow

To analyze a new sprite sheet:

```sh
python3 tools/analyze_sprite.py assets/sprites/<category>/<sprite>.png
```

Frame math:

```
source_x = (frame_index % num_cols) * frame_w
source_y = (frame_index / num_cols) * frame_h
```

Standard animation row layout (most assets in this pack):

| Row | Animation | Notes |
|-----|-----------|-------|
| 0 | Idle | 1-4 frames, subtle |
| 1 | Walk / Run | 6-8 frames, looping |
| 2 | Jump (up) | 2-4 frames, one-shot |
| 3 | Fall / Land | 2-4 frames |
| 4 | Attack | 4-8 frames, one-shot |
| 5 | Death / Hurt | 4-6 frames, one-shot |

See [Assets](../assets/) for sprite sheet dimensions and [Player Module](../player-module/) for animation state machine details.

Measure each sheet rather than assuming a common frame size or row layout. Advance animation using accumulated elapsed time; reset the frame on state entry, loop repeating states, and clamp one-shot animations to their last frame. Reuse right-facing art with `sprite_draw` and `SPRITE_FLIP_X` for left-facing rendering.

---

## Checklist: Adding a New Entity

- [ ] Create `src/<category>/<entity>.h` with struct and function declarations (e.g. `src/entities/`, `src/collectibles/`, `src/hazards/`, `src/surfaces/`)
- [ ] Create `src/<category>/<entity>.c` with init, update, render, cleanup
- [ ] Add `#include "<category>/<entity>.h"` to `game.h`
- [ ] Add texture pointer to `TextureResources`, plus entity array and count to `GameState` (by value, not pointer)
- [ ] Load texture in the resource-loading path (`src/core/game_resources.c`)
- [ ] Call `<entity>_init` in `game_init`
- [ ] Call `<entity>_update` from the relevant `src/core/` update helper
- [ ] Call `<entity>_render` from `src/render/game_render.c` or its focused render helper (correct layer order)
- [ ] Call `<entity>_cleanup` in `game_cleanup` before the session closes the graphics context
- [ ] Set all freed pointers to `NULL`
- [ ] Wire shared schema/parser/emitter, C/Python validation, and editor palette/tools/preview/properties/undo/clipboard/document hashing
- [ ] Add entity placement to a TOML level file in `levels/` (or use the visual level editor)
- [ ] Add hitbox visualization in `core/debug.c`
- [ ] Add `debug_log` calls in the module that owns significant entity events
- [ ] Build game with `make` -- no Makefile changes needed for new `.c` files in existing source directories
- [ ] Build editor with `make editor` if editor placement/schema behavior changed
- [ ] Run `make test`
- [ ] Run `make validate-levels` after any level/schema/editor serializer change
- [ ] Test with `--debug` flag to verify hitboxes render correctly
- [ ] Run relevant docs lint/build command when documentation pages changed

---

## Related Pages

- [Overview](../) -- project overview
- [Architecture](../architecture/) -- system design and game loop
- [Build System](../build-system/) -- compiling and running
- [Source Files](../source-files/) -- module-by-module reference
- [Assets](../assets/) -- sprite sheets and textures
- [Sounds](../sounds/) -- audio files and music
- [Player Module](../player-module/) -- player-specific details
- [Constants Reference](../constants-reference/) -- all defined constants
