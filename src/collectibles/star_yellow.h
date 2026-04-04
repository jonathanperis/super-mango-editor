/*
 * star_yellow.h — Public interface for the Star Yellow collectible module.
 *
 * Star Yellows are rare pickups placed throughout the level.  Collecting one
 * restores one heart immediately, up to MAX_HEARTS.  Unlike coins,
 * star yellows do not award score — they are purely a health pickup.
 *
 * Sprite: assets/star_yellow.png — a small yellow star icon, displayed at
 * STAR_YELLOW_DISPLAY_W x STAR_YELLOW_DISPLAY_H logical pixels.
 */
#pragma once

#include <SDL.h>

/* ---- Constants ---------------------------------------------------------- */

#define MAX_STAR_YELLOWS      16     /* maximum star yellow instances per level */
#define STAR_YELLOW_DISPLAY_W 16     /* render width  in logical pixels        */
#define STAR_YELLOW_DISPLAY_H 16     /* render height in logical pixels        */

/* ---- Types -------------------------------------------------------------- */

/*
 * StarYellow — state for one star yellow collectible.
 *
 * x, y   : top-left position in logical world pixels.
 * active : 1 = visible and collectible, 0 = already collected this phase.
 */
typedef struct {
    float x;
    float y;
    int   active;
} StarYellow;

/* ---- Function declarations ---------------------------------------------- */

void star_yellows_init(StarYellow *stars, int *count);

void star_yellows_render(const StarYellow *stars, int count,
                         SDL_Renderer *renderer, SDL_Texture *tex, int cam_x);
