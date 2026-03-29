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
#define VINE_H      48              /* sprite height (natural size)          */

/*
 * VineDecor — world-space position of one vine instance.
 *
 * x : left edge in logical world pixels.
 * y : top  edge in logical world pixels (bottom = y + VINE_H).
 */
typedef struct {
    float x;
    float y;
} VineDecor;

/* Populate the vine array with ground and platform placements. */
void vine_init(VineDecor *vines, int *count);

/* Blit every vine with world-to-screen camera offset applied. */
void vine_render(const VineDecor *vines, int count,
                 SDL_Renderer *renderer, SDL_Texture *tex, int cam_x);
