/*
 * rope.h — Public interface for the Rope decoration / climbable.
 *
 * A Rope works identically to a vine for climbing purposes.
 * It is rendered on smaller (medium, 2-tile) columns.
 *
 * Sprite: assets/Rope.png — 16×48 RGBA, a single rope frame.
 */
#pragma once

#include <SDL.h>

#define MAX_ROPES     8
#define ROPE_W       12              /* display width  with some padding       */
#define ROPE_H       36              /* display height with some padding       */
#define ROPE_SRC_X    2              /* source crop x (2 px padding each side) */
#define ROPE_SRC_Y    6              /* source crop y (6 px top padding)       */
#define ROPE_SRC_W   12              /* source crop width                      */
#define ROPE_SRC_H   36              /* source crop height                     */
#define ROPE_STEP    34              /* vertical spacing between stacked tiles */

/*
 * RopeDecor — world-space position and length of one rope.
 */
typedef struct {
    float x;
    float y;
    int   tile_count;
} RopeDecor;

/* Populate the rope array with level placements. */
void rope_init(RopeDecor *ropes, int *count);

/* Blit every rope with world-to-screen camera offset applied. */
void rope_render(const RopeDecor *ropes, int count,
                 SDL_Renderer *renderer, SDL_Texture *tex, int cam_x);
