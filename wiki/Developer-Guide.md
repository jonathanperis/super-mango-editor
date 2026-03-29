# Developer Guide

← [Home](Home)

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

### Memory & Safety Rules

- Every pointer must be set to `NULL` **immediately after freeing**. (`SDL_Destroy*` and `free()` on `NULL` are no-ops, preventing double-free crashes.)
- Error paths call `SDL_GetError()` / `IMG_GetError()` / `Mix_GetError()` and write to `stderr`.
- Resources are **always freed in reverse init order**.
- Use `float` for positions and velocities; cast to `int` only at render time (`SDL_Rect` fields are `int`).

### Coordinate System

All game-object positions and sizes live in **logical space (400×300)**.  
Never use `WINDOW_W` / `WINDOW_H` for game math — SDL scales the logical canvas to the OS window automatically.

---

## Adding a New Entity

Every entity follows the same five-function pattern as `Player`:

```
entity_init    → load texture, set initial state
entity_update  → move, apply physics, detect events
entity_render  → draw to renderer
entity_cleanup → SDL_DestroyTexture, set to NULL
```

And optionally:

```
entity_handle_input   → if player-controlled
entity_animate        → static helper, called from entity_update
```

### Step-by-Step

#### 1. Create the header — `src/coin.h`

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
void coin_update(Coin *coin, /* player rect or whatever triggers collection */);
void coin_render(Coin *coin, SDL_Renderer *renderer);
void coin_cleanup(Coin *coin);
```

#### 2. Create the implementation — `src/coin.c`

```c
#include <SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include "coin.h"

void coin_init(Coin *coin, SDL_Renderer *renderer, float x, float y) {
    coin->texture = IMG_LoadTexture(renderer, "assets/Coin.png");
    if (!coin->texture) {
        fprintf(stderr, "Failed to load Coin.png: %s\n", IMG_GetError());
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

The Makefile picks up `coin.c` automatically — **no Makefile changes needed**.

#### 3. Add to `GameState` in `game.h`

```c
#include "coin.h"   // add this include

typedef struct {
    // ... existing fields ...
    Coin coin;      // embedded by value, like Player
} GameState;
```

#### 4. Wire up in `game.c`

```c
// game_init:
coin_init(&gs->coin, gs->renderer, 200.0f, 100.0f);

// game_loop render section (after floor, before player — or after player):
coin_render(&gs->coin, gs->renderer);

// game_cleanup (before SDL_DestroyRenderer):
coin_cleanup(&gs->coin);
```

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

`GRAVITY`, `FLOOR_Y`, `GAME_W`, and `GAME_H` are all defined in `game.h` and available to any file that includes it.

---

## Adding a New Sound Effect

1. Place `.wav` in `sounds/`.
2. Add `Mix_Chunk *snd_<name>;` to `GameState` in `game.h`.
3. Load in `game_init` (fatal if missing):
   ```c
   gs->snd_<name> = Mix_LoadWAV("sounds/<name>.wav");
   if (!gs->snd_<name>) { /* cleanup chain + exit */ }
   ```
4. Free in `game_cleanup`:
   ```c
   if (gs->snd_<name>) { Mix_FreeChunk(gs->snd_<name>); gs->snd_<name> = NULL; }
   ```
5. Play wherever needed:
   ```c
   if (gs->snd_<name>) Mix_PlayChannel(-1, gs->snd_<name>, 0);
   ```

See [Sounds](Sounds) for the full list of available WAV files.

---

## Adding a Background Music Track

```c
// Load
gs->music = Mix_LoadMUS("sounds/new-track.mp3");

// Play (looping)
Mix_PlayMusic(gs->music, -1);
Mix_VolumeMusic(64);  // 50% — adjust as needed

// Cleanup
Mix_HaltMusic();
Mix_FreeMusic(gs->music);
gs->music = NULL;
```

---

## Adding HUD / Text Rendering

`SDL2_ttf` is already initialized in `main.c`. The font `Round9x13.ttf` is in `assets/`.

```c
// Load font
TTF_Font *font = TTF_OpenFont("assets/Round9x13.ttf", 13);
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

---

## Render Layer Order

Always draw in painter's algorithm order (back to front):

```
1. Background        (parallax_render: 6 layers from assets/Parallax/)
2. Floor tiles       (Grass_Tileset.png 9-slice at FLOOR_Y)
3. Platforms         (Grass_Oneway.png 9-slice pillar stacks)
4. Coins             (Coin.png collectibles on top of platforms)
5. Water             (Water.png animated scrolling strip)
6. Enemies           (Spider_1.png ground patrol)
7. Player            (Player.png animated sprite)
8. Fog / Overlays    (Sky_Background sliding layers)
9. HUD / UI          (hud_render: hearts, lives, score — always on top)
```

---

## Sprite Sheet Workflow

To analyze a new sprite sheet:

```sh
python3 .github/skills/pixelart-game-designer/scripts/analyze_sprite.py assets/<sprite>.png
```

Frame math:

```
source_x = (frame_index % num_cols) * frame_w
source_y = (frame_index / num_cols) * frame_h
```

Standard animation row layout (most assets in this pack):

| Row | Animation | Notes |
|-----|-----------|-------|
| 0 | Idle | 1–4 frames, subtle |
| 1 | Walk / Run | 6–8 frames, looping |
| 2 | Jump (up) | 2–4 frames, one-shot |
| 3 | Fall / Land | 2–4 frames |
| 4 | Attack | 4–8 frames, one-shot |
| 5 | Death / Hurt | 4–6 frames, one-shot |

---

## Checklist: Adding a New Entity

- [ ] Create `src/<entity>.h` with struct and function declarations
- [ ] Create `src/<entity>.c` with all implementations
- [ ] Add `#include "<entity>.h"` to `game.h`
- [ ] Add the entity field to `GameState` (by value, not pointer)
- [ ] Call `<entity>_init` in `game_init`
- [ ] Call `<entity>_update` in `game_loop` update section
- [ ] Call `<entity>_render` in `game_loop` render section (correct layer order)
- [ ] Call `<entity>_cleanup` in `game_cleanup` (before `SDL_DestroyRenderer`)
- [ ] Set all freed pointers to `NULL`
- [ ] Build with `make` — no Makefile changes needed
