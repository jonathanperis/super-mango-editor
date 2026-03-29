/*
 * vine.h — Public interface for static vine/plant decorations.
 *
 * Vines are purely visual: no update step, no collision.
 * They are placed on the ground floor and on select platform tops
 * to add organic variety to the level scenery.
 *
 * Sprite: assets/Vine.png — 16×48 RGBA, a single plant frame.
 */
#pragma once

#include <SDL.h>

#define MAX_VINES   24              /* upper bound on decoration instances   */
#define VINE_W      16              /* sprite width  (natural size)          */
#define VINE_H      32              /* content height after removing 8 px transparent padding top+bottom */
#define VINE_SRC_Y   8              /* first pixel row with content in Vine.png */
#define VINE_SRC_H  32              /* height of content area in Vine.png    */

/*
 * VineDecor — world-space position and length of one hanging vine.
 *
 * x          : left edge in logical world pixels.
 * y          : top edge (attachment to platform surface).
 * tile_count : number of VINE_H-px tiles stacked downward (2–4).
 */
typedef struct {
    float x;
    float y;
    int   tile_count;
} VineDecor;

/* Populate the vine array with ground and platform placements. */
void vine_init(VineDecor *vines, int *count);

/* Blit every vine with world-to-screen camera offset applied. */
void vine_render(const VineDecor *vines, int count,
                 SDL_Renderer *renderer, SDL_Texture *tex, int cam_x);
