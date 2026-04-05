/*
 * vine.h — Public interface for static vine/plant decorations.
 *
 * Vines are purely visual: no update step, no collision.
 * They are placed on the ground floor and on select platform tops
 * to add organic variety to the level scenery.
 *
 * Sprites:
 *   vine_green.png — forest/fertile themes (16×48 RGBA, single frame)
 *   vine_brown.png — arid/inhospitable themes (same dimensions)
 */
#pragma once

#include <SDL.h>

#define MAX_VINES   24              /* upper bound on decoration instances   */
#define VINE_W      16              /* sprite width  (natural size)          */
#define VINE_H      32              /* content height after removing 8 px transparent padding top+bottom */
#define VINE_SRC_Y   8              /* first pixel row with content in Vine.png */
#define VINE_SRC_H  32              /* height of content area in Vine.png    */
#define VINE_STEP   19              /* vertical spacing between stacked tiles */

/*
 * VineType — visual variant for vine decorations.
 *
 * VINE_GREEN : lush green vine (forest/fertile themes)
 * VINE_BROWN : dried brown vine (arid/volcanic/cave themes)
 */
typedef enum {
    VINE_GREEN = 0,
    VINE_BROWN
} VineType;

/*
 * VineDecor — world-space position and length of one hanging vine.
 *
 * x          : left edge in logical world pixels.
 * y          : top edge (attachment to platform surface).
 * tile_count : number of VINE_H-px tiles stacked downward (2–4).
 * type       : VINE_GREEN or VINE_BROWN — selects the texture.
 */
typedef struct {
    float    x;
    float    y;
    int      tile_count;
    VineType type;
} VineDecor;

/* Populate the vine array with ground and platform placements. */
void vine_init(VineDecor *vines, int *count);

/* Blit every vine with world-to-screen camera offset applied. */
void vine_render(const VineDecor *vines, int count,
                 SDL_Renderer *renderer,
                 SDL_Texture *green_tex, SDL_Texture *brown_tex,
                 int cam_x);
