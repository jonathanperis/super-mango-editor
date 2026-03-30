/*
 * spike.h — Public interface for the Spike module.
 *
 * A Spike row is a horizontal strip of individual 16×16 spike tiles
 * placed on the ground.  Each tile is rendered independently (like
 * bridge bricks) but unlike a bridge they never fall — they are static
 * hazards that damage the player on contact.
 *
 * The spikes share the same apply_damage path as other hazards (1 heart,
 * with knockback).
 */
#pragma once

#include <SDL.h>

/* ---- Constants ---------------------------------------------------------- */

#define MAX_SPIKE_ROWS      4
#define MAX_SPIKE_TILES    16     /* max tiles in a single spike row          */
#define SPIKE_TILE_W       16     /* width  of one Spike.png tile (px)        */
#define SPIKE_TILE_H       16     /* height of one Spike.png tile (px)        */

/* ---- Types -------------------------------------------------------------- */

/*
 * SpikeRow — a horizontal strip of spike tiles on the ground.
 *
 * x        : left edge in world-space logical pixels.
 * y        : top edge (usually FLOOR_Y − SPIKE_TILE_H to sit on the floor).
 * count    : number of 16×16 tiles in this row.
 * active   : 1 = hazard is live, 0 = disabled.
 */
typedef struct {
    float x;
    float y;
    int   count;
    int   active;
} SpikeRow;

/* ---- Function declarations ---------------------------------------------- */

/* Populate the spike row array with level placements. */
void spike_rows_init(SpikeRow *rows, int *count);

/* Draw all active spike rows as tiled 16×16 blocks. */
void spike_rows_render(const SpikeRow *rows, int count,
                       SDL_Renderer *renderer, SDL_Texture *tex, int cam_x);

/*
 * spike_row_get_rect — Return the full bounding rectangle of a spike row
 * in world space (for collision detection).
 */
SDL_Rect spike_row_get_rect(const SpikeRow *row);

/*
 * spike_row_hit_test — Return 1 if the given player rect overlaps any
 * spike tile in the row.
 */
int spike_row_hit_test(const SpikeRow *row, const SDL_Rect *prect);
