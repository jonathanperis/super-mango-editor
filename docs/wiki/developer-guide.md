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
- A file a level names, or one every game needs, is acquired from the session's asset cache, not loaded directly: `asset_cache_texture_acquire(gs->cache, path)` and one `asset_cache_texture_release` for it at cleanup. The cache owns it and decides when to unload, so never pass an acquired texture, sound, track or font to `texture_unload`, `sound_unload`, `music_unload` or `font_unload`. The start menu gets its font, logo and confirm sound the same way. Direct `texture_load` remains right for something with one owner for its whole life, such as the rows of the shared sprite table in `src/core/game_resources.c`.
- Use `float` for positions and velocities. Preserve integer `IntRect` hitbox construction and edge rules; convert to raylib `Rectangle` at drawing boundaries.
- Asset and level paths stay relative (`"assets/..."`, `"levels/..."`). Startup moves a foreign working folder to the asset root once (`src/shared/asset_root.c`), so no module needs to build absolute asset paths. A new command-line *path* option must be made absolute in `main.c` before that move, as `--profile` is.

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

In the game itself, a kind is a module plus one row in each table it takes
part in. The tables are plain arrays of structs with a function pointer, and
each is walked by one `for` loop, top to bottom:

| Table | File | Its rows, in order, are |
|-------|------|-------------------------|
| `s_level_loaders` | `src/levels/level_loader.c` | what a level load (and a lost life) places in the world |
| `s_update_steps` | `src/core/game_update.c` | one fixed step of the simulation, after the player has moved |
| `s_damage_sources` | `src/collision/game_collision.c` | the enemies and hazards tested against the player; the first hit counts |
| `s_render_layers` | `src/render/game_render.c` | the frame's 33 render layers, back to front |

A row names a small function written beside the others like it
(`load_spiders`, `update_spiders`, `spider_touches`, `render_spiders`). The
update and render ones call the entity's module with the inputs that kind
needs. To change *when* something happens, move its row. To change *what*
happens, edit the module. Moving a row in `s_level_loaders` or
`s_update_steps` can change what a seeded run does, and then recorded
experiments and the replay tests no longer match.

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
`gs->world.runtime.world_w` as `world_w`; real player movement also resolves gaps,
one-way surfaces and the sprite's inset foot position in `player_surfaces.c`.

---

## Adding a New Sound Effect

All sound files are `.wav` format, named with the convention `component_descriptor.wav`. They are synthesized by `tools/gen_sounds.py` (12 mono 16-bit 22050 Hz files); `make docs-drift` runs `gen_sounds.py --check` to catch committed files that drift from the generator. [Sounds](../sounds/) lists every file and where the game plays it.

Steps to add a new sound:

1. Add a generator for the sound to `tools/gen_sounds.py` and run `make sounds`; it writes `assets/sounds/<category>/<name>.wav`.
2. Add `SoundEffect *<name>;` to `AudioResources` in `game.h`.
3. Add a row to `s_optional_chunks` in `src/core/game_resources.c`. Loading is non-fatal (a missing file warns and leaves the slot NULL). The session loads the table once and lends the sounds to every game, and frees it in reverse order when it ends:

```c
{ CHUNK_FIELD(<name>), "assets/sounds/<category>/<name>.wav", "<name>.wav" },
```

4. Play wherever needed:

```c
sound_play(gs->assets.audio.<name>, 128); // null-safe; per-play volume in authored units
```

---

## Adding Background Music

Background music uses raylib streams through the project `MusicTrack` owner. Runtime levels provide the active music path through TOML:

```c
// Acquire from the session's asset cache, by the current LevelDef's path
gs->world.music = asset_cache_music_acquire(gs->cache, def->music_path);

// Play (looping)
music_play(gs->world.music);
music_set_volume(64); // 50%; normal sessions combine level/user/mute settings

// Cleanup: give the reference back. The cache stops the track and keeps
// it loaded for the next game on this level.
asset_cache_music_release(gs->cache, gs->world.music);
gs->world.music = NULL;
```

`music_load` / `music_unload` are still what the cache itself calls; game
code goes through the cache so a Replay does not open the file again.

---

## Adding HUD / Text Rendering

All text uses raylib's built-in bitmap font, so no font file is loaded. raylib
creates it inside `InitWindow` and frees it in `CloseWindow`; `TextFont` only
borrows it and must never pass it to `UnloadFont`. Text is drawn at the font's
10 px base size (`TEXT_FONT_SIZE`) with point filtering, so every glyph pixel
lands on one canvas pixel. Characters outside ASCII/Latin-1 draw as `?`.
Labels are drawn straight from the font's atlas texture (`font_draw`, raylib's
`DrawTextEx`), so no per-label texture exists to own or free.

```c
// Borrow the default font (requires a live graphics context)
TextFont *font = font_load();
if (!font) return -1;

// Draw while the frame's render target is active
font_draw(font, "Score: 0", 10, 10, WHITE);
// Centre a label on x = 200 (measures the text for you)
font_draw_centered(font, "Paused", GAME_W / 2, 92, WHITE);

// Cleanup before closing the graphics context (frees only the handle)
font_unload(font);
```

The HUD takes its `TextFont` handle and coin icon from the session's asset
cache instead (`asset_cache_font_acquire`, `asset_cache_texture_acquire` in
`hud_init`; one release each in `hud_cleanup`), so a Replay's new HUD gets
the same two back.

The HUD renders hearts (health), life counter and score. It is drawn after game entities; terminal/settings overlays can cover it. `hud_layout()` computes every HUD rectangle once; `hud_render` draws into them and the `--debug` hitbox view outlines the same ones, so a layout change needs one edit.

Draw labels with `font_draw`/`font_draw_centered` every frame. raylib's default
font is a single atlas texture and `DrawTextEx` batches its glyphs with the rest
of the frame, so there is nothing to rasterize, cache or release per label.

Widget colours come from the `UIStyle` in `UIState` (`src/shared/ui.h`), a table
of colour roles in the manner of raygui's style properties: panel, title, input,
button, hot, selected, text, dim text, accent, warning, error and border, plus a
control `border_width`. Widgets pick a role by state (hovered, selected,
disabled), and panels an owner draws itself should ask `ui_color(ui, role)` rather
than use a fixed colour, so a style change reaches them too. `UI_STYLE_DEFAULT`
is the editor look; `UI_STYLE_HIGH_CONTRAST` is used by the editor's Contrast
toggle and by the game's settings menu when the high-contrast setting is on.
`editor_ui_test` checks that the default still matches the `UI_*` colours and
that every high-contrast text colour keeps 7:1 on every face.

A scrolled panel or list keeps a `UIScroll` (offset plus the wheel's leftover
fraction), modelled on raygui's `GuiScrollPanel`. `ui_scroll_wheel` turns wheel
notches into whole pixels or rows and keeps a trackpad's fractions for the next
event; `ui_scroll_begin`/`ui_scroll_end` clamp the offset, clip drawing to the
view and say where the content starts; `ui_scroll_reveal` scrolls a field into
view. The editor's palette, Level Config, validation list and Campaign rows all
use it.

---

## Render Layer Order

Draw back to front (the painter's algorithm). The full 33-layer order, with
the function that draws each layer, is in
[Architecture](../architecture/#render-order-back-to-front). That table and
`s_render_layers` in `src/render/game_render.c` are the same list: give a new
entity a `render_<things>()` function and a row at the matching place in
both. `make docs-drift` fails while they differ.

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
