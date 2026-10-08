# Developer Guide

<a id="home"></a>

---

This guide covers the patterns and conventions used in Super Mango and explains how to extend the game safely and consistently.

For a guided sequence, start with [Sandbox School](../learning-path/). The
[Entity Walkthrough](../entity-walkthrough/) is the step-by-step route for
adding an entity; this page covers the conventions around it.

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
| `DESIGN.md` | How the website looks: built from the game's own sprites, palette and fonts. |
| `docs/wiki/developer-guide.md` | Coding conventions, entity integration, resource ownership, and verification. |
| `CODEOWNERS` | GitHub ownership hints for review routing. |

Treat source and workflows as authoritative. When project documents, README, or GH Pages copy drift from implementation, update the docs and run [Testing & Smoke Matrix](../testing/) checks before shipping.

---

## Verification and Runtime Controls

Run the 17-test `make test` suite (17 native binaries plus Python and JavaScript host checks) for runtime/editor changes. `make validate-levels` checks level and campaign data; `make docs-drift` checks semantic docs drift, generated catalog freshness, and roadmap quality. For documentation changes, also run `bun run lint`, `bun run build` and `bun run check-site` from `docs/`; compilation alone does not verify links. The game requires an audio device to start; on a machine with a display but no sound hardware, build the test-only `RAYLIB_AUDIO=null` variant (miniaudio's null playback device; release targets refuse it). See [Build System](../build-system/) and [Testing & Smoke Matrix](../testing/) for the full gates.

Terminal overlays use Up/Down or D-pad to select, Enter/Space/Start to confirm (A also confirms), and Esc/Back to exit (B also exits). Completion offers Next Level when configured, Replay, Level Select, and Exit; game over offers Retry, Level Select, and Exit. See [Controls](../controls/) for the full input reference.

---

## Adding a New Entity

Adding an entity touches the file format, the runtime, the editor, undo and the
tests. The [Entity Walkthrough](../entity-walkthrough/) is the one place that
lists every step, with the module pattern, the resource table row, the editor
files and a checklist. Follow it rather than copying an existing entity by eye.

---

## Adding Physics to an Entity

Use the same pattern as `player_update`. `dt` is the simulation step passed
down from `game_update_active`: always the fixed 1/60 s `GAME_FIXED_STEP`, in
live play and in every kind of replay, never the measured frame time:

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

`GRAVITY`, `FLOOR_Y`, `GAME_W`, and `GAME_H` are all defined in `game_constants.h`. An entity `.c` file that needs only these numbers includes that header; `game.h` includes it too, so files that use `GameState` already have them. See [Constants Reference](../constants-reference/) for values.

This example is a simplified solid-floor integrator. Pass the active
`gs->runtime.world_w` as `world_w`; real player movement also resolves gaps,
one-way surfaces and the sprite's inset foot position in `player_surfaces.c`.

---

## Adding a New Sound Effect

All sound files are `.wav` format, named with the convention `component_descriptor.wav`. They are synthesized by `tools/gen_sounds.py` (12 mono 16-bit 22050 Hz files); `make docs-drift` runs `gen_sounds.py --check` to catch committed files that drift from the generator. [Sounds](../sounds/) lists every file and where the game plays it.

Steps to add a new sound:

1. Add a generator for the sound to `tools/gen_sounds.py` and run `make sounds`; it writes `assets/sounds/<category>/<name>.wav`.
2. Add `SoundEffect *<name>;` to `AudioResources` in `game.h`.
3. Add a row to `s_optional_chunks` in `src/core/game_resources.c`. Loading is non-fatal (a missing file warns and leaves the slot NULL), and cleanup frees the table in reverse order:

```c
{ CHUNK_FIELD(<name>), "assets/sounds/<category>/<name>.wav", "<name>.wav" },
```

4. Play wherever needed:

```c
sound_play(gs->audio.<name>, 128); // null-safe; per-play volume in authored units
```

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

All text uses raylib's built-in bitmap font, so no font file is loaded. raylib
creates it inside `InitWindow` and frees it in `CloseWindow`; `TextFont` only
borrows it and must never pass it to `UnloadFont`. Text is drawn at the font's
10 px base size (`TEXT_FONT_SIZE`) with point filtering, so every glyph pixel
lands on one canvas pixel. Characters outside ASCII/Latin-1 draw as `?`.
Cached label textures still have explicit owners.

```c
// Borrow the default font (requires a live graphics context)
TextFont *font = font_load();
if (!font) return -1;

// Draw while the frame's render target is active
font_draw(font, "Score: 0", 10, 10, WHITE);

// Cleanup before closing the graphics context (frees only the handle)
font_unload(font);
```

The HUD renders hearts (health), life counter and score. It is drawn after game entities; terminal/settings overlays can cover it.

For repeated labels, reuse `UIState`'s bounded cache or `font_texture`. Rebuild
only when content/appearance changes. `texture_unload` flushes pending raylib
draws before freeing a texture, which matters when a cache entry is evicted
within a frame. Release cached textures before their font and graphics context.

---

## Render Layer Order

Draw back to front (the painter's algorithm). The full 32-layer order, with
the function that draws each layer, is in
[Architecture](../architecture/#render-order-back-to-front); put a new
entity's render call in `src/render/game_render.c` at the matching place.

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

Measure each sheet rather than assuming a common frame size or row layout. Advance animation using accumulated elapsed time (a `float` millisecond timer, so the 0.67 ms fraction of each 16.67 ms step is not truncated away); reset the frame on state entry, loop repeating states, and clamp one-shot animations to their last frame. Reuse right-facing art with `sprite_draw` and `SPRITE_FLIP_X` for left-facing rendering.

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
