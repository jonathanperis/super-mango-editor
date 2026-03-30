/*
 * float_platform.h — Public interface for the FloatPlatform module.
 *
 * A FloatPlatform is a one-way horizontal surface that can behave in one
 * of three modes:
 *
 *   STATIC  — hovers at a fixed world position; never moves.
 *   CRUMBLE — falls off-screen after the player stands on it long enough.
 *   RAIL    — travels along a Rail path, reusing the same rail_get_world_pos
 *             and rail_advance infrastructure as SpikeBlock.
 *
 * Collision (top-surface one-way landing) is handled inside player_update,
 * which receives the float_platform array alongside the static Platform array.
 * This keeps the animation state (idle vs. fall) correct on every frame.
 *
 * Platform.png is 48×16 px — a 3-slice horizontal sprite:
 *   slice 0 (x=0):  left  cap (16×16)
 *   slice 1 (x=16): centre fill, tileable (16×16)
 *   slice 2 (x=32): right cap (16×16)
 */
#pragma once

#include <SDL.h>
#include "rail.h"   /* Rail — needed for the RAIL mode pointer */

/* ---- Sprite constants ---------------------------------------------------- */

/*
 * FLOAT_PLATFORM_PIECE_W / _H — size of each 3-slice piece in logical pixels.
 * Platform.png is 48×16, so each of the three pieces is 16×16.
 */
#define FLOAT_PLATFORM_PIECE_W  16
#define FLOAT_PLATFORM_H        16

/* ---- Capacity ------------------------------------------------------------ */

#define MAX_FLOAT_PLATFORMS  6   /* max FloatPlatform instances in one level */

/* ---- Crumble parameters -------------------------------------------------- */

/*
 * CRUMBLE_STAND_LIMIT — seconds the player must stand continuously before the
 * platform starts falling.  1.5 s gives the player time to jump off once they
 * feel the platform begin to shake (future visual cue).
 */
#define CRUMBLE_STAND_LIMIT  1.5f

/* ---- Mode enum ----------------------------------------------------------- */

/*
 * FloatPlatformMode — which of the three behaviours this platform uses.
 * Set once at init; never changes during a play session.
 */
typedef enum {
    FLOAT_PLATFORM_STATIC,   /* hovers at a fixed world position forever     */
    FLOAT_PLATFORM_CRUMBLE,  /* falls after the player stands on it          */
    FLOAT_PLATFORM_RAIL,     /* follows a Rail path (closed loop or bouncing)*/
} FloatPlatformMode;

/* ---- Struct -------------------------------------------------------------- */

/*
 * FloatPlatform — data for one floating platform instance.
 *
 * Geometry:
 *   x, y   : top-left world-space corner in logical pixels.
 *            y is the one-way TOP SURFACE — the Y the player lands on.
 *   w, h   : display size; w = w_tiles × FLOAT_PLATFORM_PIECE_W.
 *
 * CRUMBLE-specific:
 *   stand_timer : seconds the player has been continuously on top.
 *   stand_limit : threshold that triggers the fall (CRUMBLE_STAND_LIMIT).
 *   falling     : 1 = crumble triggered; physics drive y each frame.
 *   fall_vy     : downward speed during crumble-fall (px/s); grows with GRAVITY.
 *
 * RAIL-specific:
 *   rail      : non-owning pointer to the Rail this platform travels on.
 *   t         : position on the rail in [0, rail->count).
 *   speed     : traversal speed in tiles/s.
 *   direction : +1 forward / −1 backward (only used for open rails).
 *   prev_x    : x at the start of the current frame; used to compute the
 *               horizontal delta so the player is nudged along with the platform.
 */
typedef struct {
    float x;
    float y;
    int   w;
    int   h;
    int   active;
    FloatPlatformMode mode;

    /* CRUMBLE */
    float stand_timer;
    float stand_limit;
    int   falling;
    float fall_vy;

    /* RAIL */
    const Rail *rail;
    float        t;
    float        speed;
    int          direction;
    float        prev_x;
} FloatPlatform;

/* ---- Function declarations ----------------------------------------------- */

/*
 * float_platform_init — Initialise one platform instance.
 *
 * mode        : STATIC / CRUMBLE / RAIL.
 * x, y        : world-space top-left corner (ignored for RAIL — overridden by
 *               the initial rail position).
 * w_tiles     : platform width in 16-px pieces (minimum 2).
 * stand_limit : seconds before crumble-fall (only used for CRUMBLE).
 * rail        : pointer to the Rail to ride (only used for RAIL; pass NULL otherwise).
 * t           : starting position on the rail (only used for RAIL).
 * speed       : traversal speed in tiles/s (only used for RAIL).
 */
void float_platform_init(FloatPlatform *fp, FloatPlatformMode mode,
                         float x, float y, int w_tiles,
                         float stand_limit,
                         const Rail *rail, float t, float speed);

/* Populate the level's float-platform array. */
void float_platforms_init(FloatPlatform *fps, int *count, const Rail *rails);

/*
 * float_platform_update — Advance one platform for this frame.
 *
 * player_on_top : 1 if the player is currently standing on the top surface
 *                 of this platform (set by player_update via out_fp_landed_idx).
 */
void float_platform_update(FloatPlatform *fp, float dt, int player_on_top);

/*
 * float_platforms_update — Advance every active platform.
 *
 * landed_idx : index of the platform the player landed on this frame, as
 *              returned by player_update's out_fp_landed_idx.  Pass -1 if
 *              the player is not on any float platform.
 */
void float_platforms_update(FloatPlatform *fps, int count,
                            float dt, int landed_idx);

/* Draw one platform using the 3-slice sprite. */
void float_platform_render(const FloatPlatform *fp,
                           SDL_Renderer *renderer, SDL_Texture *tex, int cam_x);

/* Draw all active platforms. */
void float_platforms_render(const FloatPlatform *fps, int count,
                            SDL_Renderer *renderer, SDL_Texture *tex, int cam_x);

/*
 * float_platform_get_rect — Return the world-space bounding rectangle.
 * Used for debug rendering; collision is driven by the y top-surface, not this rect.
 */
SDL_Rect float_platform_get_rect(const FloatPlatform *fp);
