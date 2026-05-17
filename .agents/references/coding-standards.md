# Coding Standards & Comment Style

This project is intentionally written as a **learning resource**. Every piece of code must be readable by someone who knows basic programming but is new to C and SDL2. Comments explain the **why**, not just the what.

---

## File Structure

### Header files (`.h`)

```c
/*
 * <name>.h — one-line summary of what this module exposes.
 *
 * Optional: 2-3 lines on design decisions, what it defines,
 * and what other files should include this header.
 */
#pragma once

#include <SDL.h>          /* explain what you need from SDL */
#include "other.h"        /* explain the dependency */

/* ---- Constants --------------------------------------------------- */

#define FOO  42           /* explain what FOO represents and its unit */

/* ---- Types ------------------------------------------------------- */

typedef struct {
    float x;              /* position: logical pixels, top-left corner */
    int   active;         /* 1 = alive and rendered, 0 = inactive      */
    SDL_Texture *texture; /* GPU image; NULL until init runs            */
} Foo;

/* ---- Function declarations --------------------------------------- */

/* Brief one-line description of what init does. */
void foo_init(Foo *foo, SDL_Renderer *renderer);

/* Brief one-line description of what update does. */
void foo_update(Foo *foo, float dt);
```

### Source files (`.c`)

```c
/*
 * <name>.c — one-line summary of what this file implements.
 */

#include <SDL.h>
#include "<name>.h"
#include "game.h"   /* explain which constants you need */

/* ---- */

/*
 * foo_init — What this function sets up and why.
 *
 * Called once at: game_init() / foo spawn / etc.
 * Important notes about the renderer or other dependencies.
 */
void foo_init(Foo *foo, SDL_Renderer *renderer) {
    /*
     * SDL API call — explain what it does, what the arguments mean,
     * and what the return value means. Explain the "why" of the flags.
     */
    foo->texture = IMG_LoadTexture(renderer, "assets/Foo.png");
    if (!foo->texture) {
        fprintf(stderr, "Failed to load Foo.png: %s\n", IMG_GetError());
        exit(EXIT_FAILURE);
    }
}
```

---

## Comment Rules

### Rule 1 — Explain the SDL2 API

Readers may not know SDL2. Every non-trivial SDL call gets a comment:

```c
/*
 * SDL_RenderCopy — draw a (region of a) texture onto the back buffer.
 *   renderer   → the drawing context
 *   texture    → the full sprite sheet on the GPU
 *   &src       → source rect: cuts one 48×48 frame from the sheet
 *   &dst       → destination: where on screen to paste it (and at what size)
 */
SDL_RenderCopy(renderer, player->texture, &player->frame, &dst);
```

### Rule 2 — Explain numeric values

Never use a magic number without explaining its unit and origin:

```c
player->speed = 160.0f;  /* horizontal speed: 160 logical px per second */
player->vy    = -520.0f; /* jump impulse: 520 px/s upward (negative = up in SDL) */
#define GRAVITY  800.0f  /* downward acceleration in logical px/s²               */
```

### Rule 3 — Explain pointer conventions

```c
/* We pass &gs->player (pointer) so the function can modify the struct in place.
 * In C, structs are passed by value (copied) unless you use a pointer. */
player_init(&gs->player, gs->renderer);
```

### Rule 4 — Explain float vs int

```c
/* Position uses float so sub-pixel movement accumulated over many frames
 * isn't lost to integer truncation. We only cast to int at draw time. */
float x;

/* Cast to int here — SDL_Rect requires integer pixels. */
SDL_Rect dst = { (int)player->x, (int)player->y, player->w, player->h };
```

### Rule 5 — Explain the reverse-free pattern

```c
/*
 * Always free in reverse init order — later objects may depend on earlier
 * ones (textures need the renderer, renderer needs the window).
 * Setting to NULL after free makes accidental double-frees safe
 * because SDL_DestroyTexture(NULL) is a no-op.
 */
if (gs->floor_tile) { SDL_DestroyTexture(gs->floor_tile); gs->floor_tile = NULL; }
if (gs->background) { SDL_DestroyTexture(gs->background); gs->background = NULL; }
if (gs->renderer)   { SDL_DestroyRenderer(gs->renderer);  gs->renderer   = NULL; }
if (gs->window)     { SDL_DestroyWindow(gs->window);      gs->window     = NULL; }
```

### Rule 6 — Explain delta time

```c
/*
 * Delta time (dt) — seconds elapsed since the last frame (e.g. 0.016 at 60 FPS).
 * Multiplying velocity (px/s) × dt (s) gives displacement in pixels this frame.
 * This keeps movement speed identical whether the game runs at 30 or 120 FPS.
 */
float dt = (float)(now - prev) / 1000.0f;
player->x += player->vx * dt;
```

---

## Error Handling Convention

| Severity | Pattern |
|----------|---------|
| Fatal (can't continue) | `fprintf(stderr, "...: %s\n", SDL_GetError()); exit(EXIT_FAILURE);` |
| Non-fatal (game can run without it) | `fprintf(stderr, "Warning: ...: %s\n", Mix_GetError());` — continue |

Sound effects are **always non-fatal**. Textures required for gameplay are **fatal**.

---

## Constants Naming

| Pattern | Example | Rule |
|---------|---------|------|
| `WINDOW_W / WINDOW_H` | `800 / 600` | OS window size only — never use for game logic |
| `GAME_W / GAME_H` | `400 / 300` | Logical canvas — use for all game positions |
| `TILE_SIZE` | `48` | Pixel size of one tile in logical space |
| `FLOOR_Y` | `GAME_H - TILE_SIZE` | Top edge of floor in logical coords |
| `GRAVITY` | `800.0f` | Always in logical px/s² |
| `TARGET_FPS` | `60` | Desired frame rate |
