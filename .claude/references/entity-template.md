# Entity Implementation Template

Use this template whenever adding a new game entity: coins, enemies, platforms, projectiles, power-ups, etc.

The Makefile `src/*.c` wildcard picks up new files automatically — no Makefile change needed.

---

## 1. Header file — `src/<entity>.h`

```c
/*
 * <entity>.h — Public interface for the <Entity> module.
 *
 * Defines the <Entity> struct and declares the functions that manage
 * its full lifecycle: init, update, render, cleanup.
 * Include this header in game.h (via GameState) and in <entity>.c.
 */
#pragma once

#include <SDL.h>       /* SDL_Texture, SDL_Renderer, SDL_Rect */
#include <SDL_mixer.h> /* Mix_Chunk — if this entity plays sounds */

/*
 * <Entity> — all the data needed to represent one <entity>.
 *
 * Positions and velocities use float for frame-rate-independent movement.
 * We only cast to int at the last moment when drawing (SDL_Rect requires int).
 */
typedef struct {
    float x;             /* horizontal position in logical pixels (top-left) */
    float y;             /* vertical   position in logical pixels (top-left) */
    float vx;            /* horizontal velocity in logical px/s              */
    float vy;            /* vertical   velocity in logical px/s              */
    int   w;             /* display width  in logical pixels                 */
    int   h;             /* display height in logical pixels                 */
    int   active;        /* 1 = alive and rendered, 0 = collected/dead       */
    SDL_Rect     frame;  /* source rect: which frame to cut from the sheet   */
    SDL_Texture *texture;/* GPU image; NULL until <entity>_init runs         */
} <Entity>;

/* Load the texture and place the entity at its starting position. */
void <entity>_init(<Entity> *e, SDL_Renderer *renderer, float x, float y);

/* Update position, physics, and animation for this frame. */
void <entity>_update(<Entity> *e, float dt);

/* Draw the entity sprite at its current position. */
void <entity>_render(<Entity> *e, SDL_Renderer *renderer);

/* Release the entity's GPU texture. */
void <entity>_cleanup(<Entity> *e);
```

---

## 2. Source file — `src/<entity>.c`

```c
/*
 * <entity>.c — Implementation of <Entity> init, update, render, cleanup.
 */

#include <SDL_image.h> /* IMG_LoadTexture */
#include <stdio.h>
#include <stdlib.h>

#include "<entity>.h"
#include "game.h"   /* GAME_W, GAME_H, FLOOR_Y, GRAVITY, TILE_SIZE */

/* Width and height of one animation frame in the sprite sheet (pixels). */
#define FRAME_W  48
#define FRAME_H  48

/* ------------------------------------------------------------------ */

/*
 * <entity>_init — Load the sprite sheet and place the entity.
 *
 * Called once per entity at game_init time (or at spawn time for
 * dynamically created entities). Sets position, frame rect, and defaults.
 */
void <entity>_init(<Entity> *e, SDL_Renderer *renderer, float x, float y) {
    /*
     * IMG_LoadTexture — decode the PNG and upload it to GPU memory.
     * The returned SDL_Texture* lives on the GPU until SDL_DestroyTexture.
     * Path is relative to the working directory (repo root when using make run).
     */
    e->texture = IMG_LoadTexture(renderer, "assets/<entity>.png");
    if (!e->texture) {
        fprintf(stderr, "Failed to load <Entity>.png: %s\n", IMG_GetError());
        exit(EXIT_FAILURE);
    }

    /*
     * frame — the source clipping rect: selects which 48×48 cell in the
     * sprite sheet to display. {x=0, y=0} is the top-left (first) frame.
     */
    e->frame = (SDL_Rect){ 0, 0, FRAME_W, FRAME_H };

    e->w      = FRAME_W;
    e->h      = FRAME_H;
    e->x      = x;
    e->y      = y;
    e->vx     = 0.0f;
    e->vy     = 0.0f;
    e->active = 1;   /* entity starts alive */
}

/* ------------------------------------------------------------------ */

/*
 * <entity>_update — Advance physics, animation, and game logic.
 *
 * Called once per frame from game_loop, before rendering.
 * dt is in seconds (e.g. 0.016 at 60 FPS).
 */
void <entity>_update(<Entity> *e, float dt) {
    if (!e->active) return;  /* skip inactive entities */

    /* Apply gravity if this entity is airborne */
    /* e->vy += GRAVITY * dt; */

    /* Move by velocity × dt (frame-rate-independent displacement) */
    e->x += e->vx * dt;
    e->y += e->vy * dt;

    /* Floor collision — snap to FLOOR_Y and stop vertical movement */
    /*
    if (e->y + e->h >= FLOOR_Y) {
        e->y  = (float)(FLOOR_Y - e->h);
        e->vy = 0.0f;
    }
    */

    /* Horizontal clamping — keep entity within the logical canvas */
    if (e->x < 0.0f)              e->x = 0.0f;
    if (e->x > GAME_W - e->w)     e->x = (float)(GAME_W - e->w);
}

/* ------------------------------------------------------------------ */

/*
 * <entity>_render — Draw the entity's current frame to the back buffer.
 *
 * Called once per frame from game_loop, after all updates.
 * Uses SDL_RenderCopy to blit one 48×48 region from the sheet to the screen.
 */
void <entity>_render(<Entity> *e, SDL_Renderer *renderer) {
    if (!e->active) return;

    /*
     * dst — where on screen and at what size to draw this entity.
     * Cast from float to int here — SDL_Rect requires integer pixel coords.
     */
    SDL_Rect dst = {
        .x = (int)e->x,
        .y = (int)e->y,
        .w = e->w,
        .h = e->h,
    };

    /*
     * SDL_RenderCopy — blit a portion of the texture (e->frame) onto the screen (dst).
     *   renderer  → the GPU drawing context
     *   e->texture → the full sprite sheet on the GPU
     *   &e->frame  → source rect: the 48×48 cell we want from the sheet
     *   &dst       → destination: screen position and display size
     */
    SDL_RenderCopy(renderer, e->texture, &e->frame, &dst);
}

/* ------------------------------------------------------------------ */

/*
 * <entity>_cleanup — Release the GPU texture held by this entity.
 *
 * Must be called before the renderer is destroyed.
 * Setting texture to NULL after free prevents accidental double-frees
 * (SDL_DestroyTexture(NULL) is a safe no-op).
 */
void <entity>_cleanup(<Entity> *e) {
    if (e->texture) {
        SDL_DestroyTexture(e->texture);
        e->texture = NULL;
    }
}
```

---

## 3. Wire into `game.h` (GameState)

```c
/* In the GameState struct: */
<Entity>  <entity>;       /* stored by value; no heap allocation needed */
/* or, for multiple instances: */
<Entity>  <entities>[16]; /* fixed-size array — simple and cache-friendly */
int       <entity>_count; /* how many are currently active               */
```

---

## 4. Wire into `game.c`

```c
/* In game_init — after renderer and textures: */
<entity>_init(&gs-><entity>, gs->renderer, start_x, start_y);

/* In game_loop — update section: */
<entity>_update(&gs-><entity>, dt);

/* In game_loop — render section (correct layer order): */
<entity>_render(&gs-><entity>, gs->renderer);

/* In game_cleanup — before renderer is destroyed: */
<entity>_cleanup(&gs-><entity>);
```

---

## 5. Collision Detection Snippet

Simple AABB (axis-aligned bounding box) overlap check between two entities:

```c
/*
 * aabb_overlap — returns 1 if the two rectangles overlap, 0 if not.
 *
 * Uses the separating axis theorem for axis-aligned boxes:
 * if there is ANY axis along which the boxes don't overlap, they don't collide.
 * We check x-overlap and y-overlap independently.
 */
static int aabb_overlap(float ax, float ay, int aw, int ah,
                         float bx, float by, int bw, int bh) {
    return ax < bx + bw &&   /* a's left edge is left of b's right edge  */
           ax + aw > bx   &&  /* a's right edge is right of b's left edge */
           ay < by + bh   &&  /* a's top is above b's bottom              */
           ay + ah > by;      /* a's bottom is below b's top              */
}

/* Usage — check player vs coin: */
if (coin.active &&
    aabb_overlap(player->x, player->y, player->w, player->h,
                 coin.x,    coin.y,    coin.w,    coin.h)) {
    coin.active = 0;                          /* collect it    */
    Mix_PlayChannel(-1, gs->snd_coin, 0);     /* play sound    */
}
```
