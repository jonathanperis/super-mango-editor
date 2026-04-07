/*
 * ladder.h — Public interface for the Ladder decoration / climbable.
 *
 * A Ladder works identically to a vine for climbing purposes: the player
 * can grab it, climb up/down, and jump-dismount.  It uses the same
 * VineDecor-compatible struct layout so the climbing code in player.c
 * can treat vines, ladders, and ropes uniformly.
 *
 * Sprite: assets/Ladder.png — 16×48 RGBA, a single ladder frame.
 */
#pragma once

#include <SDL.h>

#define MAX_LADDERS   16
#define LADDER_W      16              /* sprite width  (px)                    */
#define LADDER_H      22              /* content height after cropping padding */
#define LADDER_SRC_Y  13              /* first pixel row with content          */
#define LADDER_SRC_H  22              /* height of content area (rows 13–34)   */
#define LADDER_STEP    8              /* tight overlap so tiles merge visually */

/*
 * LadderDecor — world-space position and length of one ladder.
 *
 * x          : left edge in logical world pixels.
 * y          : top edge (attachment point).
 * tile_count : number of LADDER_H-px tiles stacked downward.
 */
typedef struct {
    float x;
    float y;
    int   tile_count;
} LadderDecor;

/* Populate the ladder array with level placements. */
void ladder_init(LadderDecor *ladders, int *count);

/* Blit every ladder with world-to-screen camera offset applied. */
void ladder_render(const LadderDecor *ladders, int count,
                   SDL_Renderer *renderer, SDL_Texture *tex, int cam_x);
