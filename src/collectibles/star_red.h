/*
 * star_red.h — Public interface for the Star Red collectible module.
 *
 * Star Reds are health-restoring pickups placed throughout the level.
 * Collecting one restores one heart immediately, up to MAX_HEARTS.
 * Unlike coins, star reds do not award score — they are purely a health pickup.
 */
#pragma once

#include <SDL.h>

/* ---- Constants ---------------------------------------------------------- */

#define MAX_STAR_REDS         16     /* maximum star red instances per level    */
#define STAR_RED_DISPLAY_W    16     /* render width  in logical pixels        */
#define STAR_RED_DISPLAY_H    16     /* render height in logical pixels        */

/* ---- Types -------------------------------------------------------------- */

/*
 * StarRed — state for one star red collectible.
 *
 * x, y   : top-left position in logical world pixels.
 * active : 1 = visible and collectible, 0 = already collected this phase.
 */
typedef struct {
    float x;
    float y;
    int   active;
} StarRed;

/* ---- Function declarations ---------------------------------------------- */

void star_reds_render(const StarRed *stars, int count,
                      SDL_Renderer *renderer, SDL_Texture *tex, int cam_x);
