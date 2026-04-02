/*
 * fog.h — Public interface for the fog/mist overlay effect.
 *
 * Manages a pool of semi-transparent sky layers that slide across the
 * screen to create an atmospheric fog effect. Up to FOG_MAX instances
 * can be active simultaneously, which is needed for the 1-second overlap
 * between consecutive animations. Include this header in game.h.
 */
#pragma once

#include <SDL.h>   /* SDL_Texture, SDL_Renderer */

/* Number of fog texture assets in rotation (Sky_Background_1 and 2) */
#define FOG_TEX_COUNT  2

/*
 * Maximum concurrent fog instances. Two would suffice for the overlap,
 * but four gives safe headroom when durations are at their minimum (2 s).
 */
#define FOG_MAX        4

/* ------------------------------------------------------------------ */

/*
 * FogInstance — one sliding fog layer currently on screen.
 *
 * Each instance picks a random texture, direction, and duration when
 * spawned, then moves at a constant speed until it exits the far side.
 */
typedef struct {
    int   active;        /* 1 = currently animating, 0 = free slot         */
    int   tex_index;     /* which texture to draw: 0, 1, or 2              */
    float x;            /* current horizontal offset in logical pixels     */
    float duration;     /* total travel time in seconds (2.0 – 3.0)       */
    float elapsed;      /* seconds elapsed since this instance was spawned */
    int   dir;          /* +1 = left→right, -1 = right→left               */
    int   spawned_next; /* 1 once we have already triggered the next wave  */
} FogInstance;

/*
 * FogSystem — owns all three textures and the pool of active instances.
 * Stored by value inside GameState — no heap allocation needed.
 */
typedef struct {
    SDL_Texture *textures[FOG_TEX_COUNT]; /* GPU images for the three assets */
    FogInstance  instances[FOG_MAX];      /* pool of simultaneous fog layers  */
} FogSystem;

/* ------------------------------------------------------------------ */

/* Load the three sky textures and spawn the first fog wave immediately. */
void fog_init(FogSystem *fog, SDL_Renderer *renderer);

/* Advance every active instance; spawn the next wave when it is time. */
void fog_update(FogSystem *fog, float dt);

/* Draw all active fog layers — call this after player_render (topmost). */
void fog_render(FogSystem *fog, SDL_Renderer *renderer);

/* Release all GPU textures owned by the fog system. */
void fog_cleanup(FogSystem *fog);
