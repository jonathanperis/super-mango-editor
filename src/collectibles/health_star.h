/*
 * health_star.h — Public interface for the health star collectibles.
 *
 * Yellow, green and red stars are one collectible in three colours.
 * Collecting any of them restores one heart immediately, up to MAX_HEARTS;
 * unlike coins they award no score. Levels list each colour separately
 * ([[star_yellows]], [[star_greens]], [[star_reds]]) so GameState keeps three
 * arrays and three textures, but every array uses this one struct, renderer
 * and hitbox — the same idea as bird.c and fish.c for their two variants.
 *
 * Sprites: assets/sprites/collectibles/star_{yellow,green,red}.png, drawn at
 * HEALTH_STAR_DISPLAY_W x HEALTH_STAR_DISPLAY_H logical pixels.
 */
#pragma once

#include "../shared/graphics.h"

/* ---- Constants ---------------------------------------------------------- */

#define MAX_STAR_YELLOWS       16    /* maximum yellow stars per level         */
#define MAX_STAR_GREENS        16    /* maximum green stars per level          */
#define MAX_STAR_REDS          16    /* maximum red stars per level            */
#define HEALTH_STAR_DISPLAY_W  16    /* render width  in logical pixels        */
#define HEALTH_STAR_DISPLAY_H  16    /* render height in logical pixels        */

/* ---- Types -------------------------------------------------------------- */

/*
 * HealthStar — state for one star of any colour.
 *
 * x, y   : top-left position in logical world pixels.
 * active : 1 = visible and collectible, 0 = collected during this life.
 */
typedef struct {
    float x;
    float y;
    int   active;
} HealthStar;

/* ---- Function declarations ---------------------------------------------- */

/* Draw every active star of one colour; tex selects the colour. */
void health_stars_render(const HealthStar *stars, int count,
                         Texture2D *tex, int cam_x);

/* World-space pickup rectangle (the full 16x16 sprite). */
IntRect health_star_get_hitbox(const HealthStar *star);
