/*
 * red_star.h — Public interface for the Red Star collectible module.
 *
 * Red Stars are rare pickups placed throughout the level.  Collecting one
 * restores one heart (star) immediately, up to MAX_HEARTS.  Unlike coins,
 * red stars do not award score — they are purely a health pickup.
 *
 * Sprite: assets/Star_Red.png — a small red star icon, displayed at
 * RED_STAR_DISPLAY_W × RED_STAR_DISPLAY_H logical pixels.
 */
#pragma once

#include <SDL.h>

/* ---- Constants ---------------------------------------------------------- */

#define MAX_RED_STARS       3     /* maximum red star instances per level     */
#define RED_STAR_DISPLAY_W 16     /* render width  in logical pixels          */
#define RED_STAR_DISPLAY_H 16     /* render height in logical pixels          */

/* ---- Types -------------------------------------------------------------- */

/*
 * RedStar — state for one red star collectible.
 *
 * x, y   : top-left position in logical world pixels.
 * active : 1 = visible and collectible, 0 = already collected this phase.
 */
typedef struct {
    float x;              /* horizontal position in logical pixels         */
    float y;              /* vertical   position in logical pixels         */
    int   active;         /* 1 = on screen, 0 = collected                  */
} RedStar;

/* ---- Function declarations ---------------------------------------------- */

/*
 * red_stars_init — Place red stars at handpicked positions across the level.
 * Sets *count to the number of stars placed.  Called at game_init and on
 * level_reset to re-activate all stars.
 */
void red_stars_init(RedStar *stars, int *count);

/*
 * red_stars_render — Draw all active red stars using the shared texture.
 * cam_x is the camera offset for world-to-screen conversion.
 */
void red_stars_render(const RedStar *stars, int count,
                      SDL_Renderer *renderer, SDL_Texture *tex, int cam_x);
