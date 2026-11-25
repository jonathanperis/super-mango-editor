/*
 * vine.h — Public interface for static vine/plant decorations.
 *
 * Vines have no independent update step. Player climbing code uses their
 * bounds as grab regions; they are not solid collision surfaces.
 *
 * Sprites:
 *   vine_green.png — forest/fertile themes (16×48 RGBA, single frame)
 *   vine_brown.png — arid/inhospitable themes (same dimensions)
 */
#pragma once

#include "../shared/graphics.h"
#include "surface_defs.h"  /* MAX_VINES, VINE_*, VineType */

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

/* Blit every vine with world-to-screen camera offset applied. */
void vines_render(const VineDecor *vines, int count,
                  Texture2D *green_tex, Texture2D *brown_tex,
                  int cam_x);
