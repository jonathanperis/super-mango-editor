/*
 * spike_platform.h — Public interface for the SpikePlatform module.
 *
 * A SpikePlatform is a static elevated platform rendered using the
 * Spike_Platform.png 3-slice sprite (48×16 px: left cap, centre fill,
 * right cap — each 16×16).  It acts as a one-way landing surface (like
 * the existing float platforms and pillar platforms) AND has a damage
 * hitbox — the player takes 1 heart of damage when touching the spikes
 * from the sides or from below.
 *
 * Unlike bridges, spike platforms never fall or crumble.
 */
#pragma once

#include <SDL.h>

/* ---- Constants ---------------------------------------------------------- */

#define MAX_SPIKE_PLATFORMS     4
#define SPIKE_PLAT_PIECE_W     16   /* width  of one 3-slice piece (px)       */
#define SPIKE_PLAT_H           16   /* full frame height (px)                 */
#define SPIKE_PLAT_SRC_Y        5   /* first content row in each piece        */
#define SPIKE_PLAT_SRC_H       11   /* content height (rows 5–15)             */

/* ---- Types -------------------------------------------------------------- */

/*
 * SpikePlatform — a static elevated hazard platform.
 *
 * x, y    : world-space top-left corner.
 * w       : total width in logical px (multiple of SPIKE_PLAT_PIECE_W).
 * active  : 1 = hazard is live, 0 = disabled.
 */
typedef struct {
    float x;
    float y;
    int   w;
    int   active;
} SpikePlatform;

/* ---- Function declarations ---------------------------------------------- */

/* Populate the spike platform array with level placements. */
void spike_platforms_init(SpikePlatform *sps, int *count);

/* Draw all active spike platforms using 3-slice rendering. */
void spike_platforms_render(const SpikePlatform *sps, int count,
                            SDL_Renderer *renderer, SDL_Texture *tex,
                            int cam_x);

/* Return the full bounding rectangle in world space (for landing check). */
SDL_Rect spike_platform_get_rect(const SpikePlatform *sp);
