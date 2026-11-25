/*
 * rope.h — Public interface for the Rope decoration / climbable.
 *
 * A Rope works identically to a vine for climbing purposes.
 * It is rendered on smaller (medium, 2-tile) columns.
 *
 * Sprite: assets/sprites/surfaces/rope.png — 16×48 RGBA, one frame.
 */
#pragma once

#include "../shared/graphics.h"
#include "surface_defs.h"  /* MAX_ROPES, ROPE_* */

/*
 * RopeDecor — world-space position and length of one rope.
 */
typedef struct {
    float x;
    float y;
    int   tile_count;
} RopeDecor;

/* Blit every rope with world-to-screen camera offset applied. */
void ropes_render(const RopeDecor *ropes, int count,
                  Texture2D *tex, int cam_x);
