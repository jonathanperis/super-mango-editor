# Developer Guide

← [Home](home)

---

This guide covers the patterns and conventions used in Super Mango and explains how to extend the game safely and consistently.

---

## Coding Conventions

### Language & Standard
- **C11** (`-std=c11`)
- Compiler: `clang` (default), `gcc` compatible

### Naming

| Category | Convention | Example |
|----------|------------|---------|
| Files | `snake_case` | `player.c`, `coin.h` |
| Functions | `module_verb` | `player_init`, `coin_update` |
| Struct types | `PascalCase` via `typedef` | `Player`, `GameState`, `Coin` |
| Enum values | `UPPER_SNAKE_CASE` | `ANIM_IDLE`, `ANIM_WALK` |
| Constants (`#define`) | `UPPER_SNAKE_CASE` | `FLOOR_Y`, `TILE_SIZE` |
| Local variables | `snake_case` | `dt`, `frame_ms`, `elapsed` |
| Assets | `snake_case` | `player.png`, `coin.png`, `spider.png` |
| Sounds | `component_descriptor.wav` | `player_jump.wav`, `coin.wav`, `bird.wav` |

### Memory & Safety Rules

- Every pointer must be set to `NULL` **immediately after freeing**. (`SDL_Destroy*` and `free()` on `NULL` are no-ops, preventing double-free crashes.)
- Error paths call `SDL_GetError()` / `IMG_GetError()` / `Mix_GetError()` and write to `stderr`.
- Resources are **always freed in reverse init order**.
- Use `float` for positions and velocities; cast to `int` only at render time (`SDL_Rect` fields are `int`).

### Coordinate System

All game-object positions and sizes live in **logical space (400x300)**.
Never use `WINDOW_W` / `WINDOW_H` for game math -- SDL scales the logical canvas to the OS window automatically.

See [Constants Reference](constants_reference) for all defined constants.

---

## Adding a New Entity

Every entity follows the same lifecycle pattern:

```
entity_init    -> load texture, set initial state
entity_update  -> move, apply physics, detect events
entity_render  -> draw to renderer
entity_cleanup -> SDL_DestroyTexture, set to NULL
```

And optionally:

```
entity_handle_input   -> if player-controlled
entity_animate        -> static helper, called from entity_update
```

### Step-by-Step

#### 1. Create the header -- `src/coin.h`

```c
#pragma once
#include <SDL.h>

typedef struct {
    float        x, y;    /* logical position (top-left) */
    int          w, h;    /* display size in logical px  */
    int          active;  /* 1 = visible, 0 = collected  */
    SDL_Texture *texture;
} Coin;

void coin_init(Coin *coin, SDL_Renderer *renderer, float x, float y);
void coin_update(Coin *coin, float dt);
void coin_render(Coin *coin, SDL_Renderer *renderer);
void coin_cleanup(Coin *coin);
```

#### 2. Create the implementation -- `src/coin.c`

```c
#include <SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include "coin.h"

void coin_init(Coin *coin, SDL_Renderer *renderer, float x, float y) {
    coin->texture = IMG_LoadTexture(renderer, "assets/coin.png");
    if (!coin->texture) {
        fprintf(stderr, "Failed to load coin.png: %s\n", IMG_GetError());
        exit(EXIT_FAILURE);
    }
    coin->x = x;
    coin->y = y;
    coin->w = 48;
    coin->h = 48;
    coin->active = 1;
}

void coin_render(Coin *coin, SDL_Renderer *renderer) {
    if (!coin->active) return;
    SDL_Rect dst = { (int)coin->x, (int)coin->y, coin->w, coin->h };
    SDL_RenderCopy(renderer, coin->texture, NULL, &dst);
}

void coin_cleanup(Coin *coin) {
    if (coin->texture) {
        SDL_DestroyTexture(coin->texture);
        coin->texture = NULL;
    }
}
```

The Makefile picks up `coin.c` automatically -- **no Makefile changes needed**.

#### 3. Add texture to `GameState` in `game.h`

Textures are loaded in `game_init()` and stored in `GameState`. The entity array and count also live in `GameState`:

```c
#include "coin.h"

typedef struct {
    // ... existing fields ...
    SDL_Texture *tex_coin;    /* GPU texture, loaded in game_init */
    Coin coins[32];           /* fixed-size array -- simple and cache-friendly */
    int  coin_count;          /* how many are currently active */
} GameState;
```

#### 4. Wire up in `game.c`

```c
// game_init -- load texture and init entities:
gs->tex_coin = IMG_LoadTexture(gs->renderer, "assets/coin.png");
coin_init(&gs->coins[0], gs->tex_coin, 200.0f, 100.0f);
gs->coin_count = 1;

// game_loop update section:
for (int i = 0; i < gs->coin_count; i++)
    coin_update(&gs->coins[i], dt);

// game_loop render section (correct layer order):
for (int i = 0; i < gs->coin_count; i++)
    coin_render(&gs->coins[i], gs->renderer);

// game_cleanup (before SDL_DestroyRenderer):
for (int i = 0; i < gs->coin_count; i++)
    coin_cleanup(&gs->coins[i]);
```

#### 5. Place in sandbox -- `src/sandbox.c`

Entity spawn positions are defined in `sandbox.c`. Add your entity placements there:

```c
// In sandbox_init or the appropriate placement function:
coin_init(&gs->coins[0], gs->tex_coin, 120.0f, 180.0f);
coin_init(&gs->coins[1], gs->tex_coin, 200.0f, 140.0f);
gs->coin_count = 2;
```

#### 6. Add debug hitbox -- `src/debug.c`

Every entity must have hitbox visualization in `debug.c`:

```c
// In debug_render:
for (int i = 0; i < gs->coin_count; i++) {
    if (!gs->coins[i].active) continue;
    SDL_Rect hb = { (int)gs->coins[i].x, (int)gs->coins[i].y,
                     gs->coins[i].w, gs->coins[i].h };
    SDL_SetRenderDrawColor(gs->renderer, 255, 255, 0, 128);
    SDL_RenderDrawRect(gs->renderer, &hb);
}
```

Also add `debug_log` calls in `game.c` for any significant entity events (collection, destruction, spawn).

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

/* Horizontal clamp */
if (entity->x < 0.0f)                entity->x = 0.0f;
if (entity->x > GAME_W - entity->w)  entity->x = (float)(GAME_W - entity->w);
```

`GRAVITY`, `FLOOR_Y`, `GAME_W`, and `GAME_H` are all defined in `game.h` and available to any file that includes it. See [Constants Reference](constants_reference) for values.

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

1. Place `.wav` in `sounds/`.
2. Add `Mix_Chunk *snd_<name>;` to `GameState` in `game.h`.
3. Load in `game_init` (non-fatal -- warn but continue):
   ```c
   gs->snd_<name> = Mix_LoadWAV("sounds/<name>.wav");
   if (!gs->snd_<name>) {
       fprintf(stderr, "Warning: could not load <name>.wav: %s\n", Mix_GetError());
   }
   ```
4. Free in `game_cleanup`:
   ```c
   if (gs->snd_<name>) { Mix_FreeChunk(gs->snd_<name>); gs->snd_<name> = NULL; }
   ```
5. Play wherever needed:
   ```c
   if (gs->snd_<name>) Mix_PlayChannel(-1, gs->snd_<name>, 0);
   ```

See [Sounds](sounds) for the full list of available sound files.

---

## Adding Background Music

Background music is loaded via `Mix_LoadMUS` (not `Mix_LoadWAV`). The current track is `game_music.wav`:

```c
// Load
gs->music = Mix_LoadMUS("sounds/game_music.wav");

// Play (looping)
Mix_PlayMusic(gs->music, -1);
Mix_VolumeMusic(64);  // 50% -- adjust as needed

// Cleanup
Mix_HaltMusic();
Mix_FreeMusic(gs->music);
gs->music = NULL;
```

---

## Adding HUD / Text Rendering

`SDL2_ttf` is already initialized in `main.c`. The font `round9x13.ttf` is in `assets/`.

```c
// Load font
TTF_Font *font = TTF_OpenFont("assets/round9x13.ttf", 13);
if (!font) { fprintf(stderr, "TTF_OpenFont: %s\n", TTF_GetError()); }

// Render text to a surface, then upload to a texture
SDL_Color white = {255, 255, 255, 255};
SDL_Surface *surf = TTF_RenderText_Solid(font, "Score: 0", white);
SDL_Texture *tex  = SDL_CreateTextureFromSurface(renderer, surf);
SDL_FreeSurface(surf);

// Draw the texture
SDL_Rect dst = {10, 10, surf->w, surf->h};
SDL_RenderCopy(renderer, tex, NULL, &dst);

// Cleanup
SDL_DestroyTexture(tex);
TTF_CloseFont(font);
```

The HUD renders hearts (lives), life counter, and score. It is drawn after all game entities so it always appears on top.

---

## Render Layer Order

Always draw in painter's algorithm order (back to front). The game currently uses 32 layers:

```
 1. Parallax background    (7 parallax_*.png layers from assets/)
 2. Platforms              (platform.png 9-slice pillars)
 3. Floor tiles            (grass_tileset.png at FLOOR_Y, with sea-gap openings)
 4. Float platforms        (float_platform.png 3-slice hovering surfaces)
 5. Spike rows             (spike.png ground-level spike strips)
 6. Spike platforms        (spike_platform.png elevated spike hazards)
 7. Bridges                (bridge.png tiled crumble walkways)
 8. Bouncepads medium      (bouncepad_medium.png standard spring pads)
 9. Bouncepads small       (bouncepad_small.png low spring pads)
10. Bouncepads high        (bouncepad_high.png tall spring pads)
11. Rails                  (rail.png bitmask tile tracks)
12. Vines                  (vine.png climbable)
13. Ladders                (ladder.png climbable)
14. Ropes                  (rope.png climbable)
15. Coins                  (coin.png collectibles)
16. Yellow stars           (yellow_star.png health pickups)
17. Last star              (end-of-level star using HUD star sprite)
18. Blue flames            (blue_flame.png erupting from sea gaps)
19. Fish                   (fish.png jumping water enemies)
20. Faster fish            (faster_fish.png fast jumping enemies)
21. Water                  (water.png animated strip)
22. Spike blocks           (spike_block.png rail-riding hazards)
23. Axe traps              (axe_trap.png swinging hazards)
24. Circular saws          (circular_saw.png patrol hazards)
25. Spiders                (spider.png ground patrol)
26. Jumping spiders        (jumping_spider.png jumping patrol)
27. Birds                  (bird.png slow sine-wave)
28. Faster birds           (faster_bird.png fast sine-wave)
29. Player                 (player.png animated)
30. Fog                    (fog_background_1/2.png sliding overlay)
31. HUD                    (hearts, lives, score -- always on top)
32. Debug overlay          (FPS, hitboxes, event log -- when --debug)
```

See [Architecture](architecture) for details on the render pipeline.

---

## Sprite Sheet Workflow

To analyze a new sprite sheet:

```sh
python3 .claude/scripts/analyze_sprite.py assets/<sprite>.png
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

See [Assets](assets) for sprite sheet dimensions and [Player Module](player_module) for animation state machine details.

---

## Checklist: Adding a New Entity

- [ ] Create `src/<entity>.h` with struct and function declarations
- [ ] Create `src/<entity>.c` with init, update, render, cleanup
- [ ] Add `#include "<entity>.h"` to `game.h`
- [ ] Add texture pointer, entity array, and count to `GameState` (by value, not pointer)
- [ ] Load texture in `game_init` in `game.c`
- [ ] Call `<entity>_init` in `game_init`
- [ ] Call `<entity>_update` in `game_loop` update section
- [ ] Call `<entity>_render` in `game_loop` render section (correct layer order)
- [ ] Call `<entity>_cleanup` in `game_cleanup` (before `SDL_DestroyRenderer`)
- [ ] Set all freed pointers to `NULL`
- [ ] Place entity spawn positions in `sandbox.c`
- [ ] Add hitbox visualization in `debug.c`
- [ ] Add `debug_log` calls in `game.c` for significant entity events
- [ ] Build with `make` -- no Makefile changes needed
- [ ] Test with `--debug` flag to verify hitboxes render correctly

---

## Related Pages

- [Home](home) -- project overview
- [Architecture](architecture) -- system design and game loop
- [Build System](build_system) -- compiling and running
- [Source Files](source_files) -- module-by-module reference
- [Assets](assets) -- sprite sheets and textures
- [Sounds](sounds) -- audio files and music
- [Player Module](player_module) -- player-specific details
- [Constants Reference](constants_reference) -- all defined constants
