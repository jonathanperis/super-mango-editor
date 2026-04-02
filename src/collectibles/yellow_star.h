/*
 * yellow_star.h — Public interface for the Yellow Star collectible module.
 *
 * Yellow Stars are rare pickups placed throughout the level.  Collecting one
 * restores one heart immediately, up to MAX_HEARTS.  Unlike coins,
 * yellow stars do not award score — they are purely a health pickup.
 *
 * Sprite: assets/Star_Yellow.png — a small yellow star icon, displayed at
 * YELLOW_STAR_DISPLAY_W × YELLOW_STAR_DISPLAY_H logical pixels.
 */
#pragma once

#include <SDL.h>

/* ---- Constants ---------------------------------------------------------- */

#define MAX_YELLOW_STARS       3     /* maximum yellow star instances per level */
#define YELLOW_STAR_DISPLAY_W 16     /* render width  in logical pixels        */
#define YELLOW_STAR_DISPLAY_H 16     /* render height in logical pixels        */

/* ---- Types -------------------------------------------------------------- */

/*
 * YellowStar — state for one yellow star collectible.
 *
 * x, y   : top-left position in logical world pixels.
 * active : 1 = visible and collectible, 0 = already collected this phase.
 */
typedef struct {
    float x;
    float y;
    int   active;
} YellowStar;

/* ---- Function declarations ---------------------------------------------- */

void yellow_stars_init(YellowStar *stars, int *count);

void yellow_stars_render(const YellowStar *stars, int count,
                         SDL_Renderer *renderer, SDL_Texture *tex, int cam_x);
