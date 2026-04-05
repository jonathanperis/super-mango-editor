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

/*
 * MAX_FOG_TEXTURES — maximum number of fog texture assets the system can hold.
 * Actual count is determined per-level via the fog_layers[] in LevelDef.
 */
#define MAX_FOG_TEXTURES  4

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
 * FogSystem — owns the loaded textures and the pool of active instances.
 * Stored by value inside GameState — no heap allocation needed.
 * tex_count tracks how many textures were actually loaded from the level
 * definition (may be less than MAX_FOG_TEXTURES).
 */
typedef struct {
    SDL_Texture *textures[MAX_FOG_TEXTURES]; /* GPU images for the fog assets  */
    int          tex_count;                  /* number of textures loaded       */
    FogInstance  instances[FOG_MAX];         /* pool of simultaneous fog layers */
} FogSystem;

/* ------------------------------------------------------------------ */

/*
 * fog_init — Load fog textures from level-defined paths and spawn the first wave.
 *
 * paths  : array of PNG file paths (up to MAX_FOG_TEXTURES entries).
 * count  : number of paths provided; clamped to MAX_FOG_TEXTURES internally.
 *
 * If count is 0, no textures are loaded and no fog waves will spawn.
 * Each texture is non-fatal: a missing asset prints a warning and leaves
 * its pointer NULL (fog_render silently skips NULL entries).
 */
void fog_init(FogSystem *fog, SDL_Renderer *renderer,
              const char (*paths)[64], int count);

/* Advance every active instance; spawn the next wave when it is time. */
void fog_update(FogSystem *fog, float dt);

/* Draw all active fog layers — call this after player_render (topmost). */
void fog_render(FogSystem *fog, SDL_Renderer *renderer);

/* Release all GPU textures owned by the fog system. */
void fog_cleanup(FogSystem *fog);
