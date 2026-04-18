/*
 * star_green.h — Public interface for the Star Green collectible module.
 *
 * Star Greens are health-restoring pickups placed throughout the level.
 * Collecting one restores one heart immediately, up to MAX_HEARTS.
 * Unlike coins, star greens do not award score — they are purely a health pickup.
 */
#pragma once

#include <SDL.h>

/* ---- Constants ---------------------------------------------------------- */

#define MAX_STAR_GREENS       16     /* maximum star green instances per level  */
#define STAR_GREEN_DISPLAY_W  16     /* render width  in logical pixels        */
#define STAR_GREEN_DISPLAY_H  16     /* render height in logical pixels        */

/* ---- Types -------------------------------------------------------------- */

/*
 * StarGreen — state for one star green collectible.
 *
 * x, y   : top-left position in logical world pixels.
 * active : 1 = visible and collectible, 0 = already collected this phase.
 */
typedef struct {
    float x;
    float y;
    int   active;
} StarGreen;

/* ---- Function declarations ---------------------------------------------- */

void star_greens_render(const StarGreen *stars, int count,
                        SDL_Renderer *renderer, SDL_Texture *tex, int cam_x);
