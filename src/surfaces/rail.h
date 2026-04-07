/*
 * rail.h — Public interface for the Rail module.
 *
 * A Rail is a closed loop of tiles sampled from Rails.png (64×64, 4×4 grid,
 * 16×16 px per tile).  Each tile stores its world-space position and a
 * bitmask describing which of the four cardinal directions it connects to.
 *
 * The renderer picks the correct source rect from the sheet based on the
 * bitmask.  Objects that ride the rail store a float `t` in [0, tile_count)
 * and call rail_get_world_pos() each frame to obtain their current position.
 *
 * Include this header in game.h (GameState) and in any .c that rides the rail.
 */
#pragma once

#include <SDL.h>

/* ---- Connection direction bitmask flags ---------------------------------- */

/*
 * Each flag represents one cardinal opening of a rail tile.
 * A tile with RAIL_E|RAIL_S has openings to the right and downward —
 * it is the top-left corner of a clockwise rectangular loop.
 */
#define RAIL_N  (1 << 0)   /* tile opens upward    */
#define RAIL_E  (1 << 1)   /* tile opens rightward */
#define RAIL_S  (1 << 2)   /* tile opens downward  */
#define RAIL_W  (1 << 3)   /* tile opens leftward  */

/* ---- Sprite sheet constants --------------------------------------------- */

/*
 * Rails.png is 64×64 px arranged as a 4×4 grid.
 * Each tile in the sheet is RAIL_TILE_W × RAIL_TILE_H pixels.
 */
#define RAIL_TILE_W   16   /* width  of one tile in the sprite sheet, px */
#define RAIL_TILE_H   16   /* height of one tile in the sprite sheet, px */

/* ---- Capacity ------------------------------------------------------------ */

#define MAX_RAIL_TILES  128   /* max tiles in a single Rail path      */
#define MAX_RAILS        16   /* max Rail instances in one level      */

/* ---- Types --------------------------------------------------------------- */

/*
 * RailTile — one cell of a rail path.
 *
 * x, y       : top-left world-space position of the tile, in logical pixels.
 * connections: bitmask of RAIL_N/E/S/W indicating which directions this tile
 *              connects to its neighbours.  Used to pick the correct sprite.
 */
typedef struct {
    int x;
    int y;
    int connections;
} RailTile;

/*
 * Rail — a closed (or open) sequence of RailTile entries that form a track.
 *
 * Tiles are stored in traversal order (clockwise for closed loops starting
 * from the top-left corner moving right).  The object riding the rail stores
 * a float t in [0, count) and interpolates between consecutive tile centres.
 *
 * closed  : 1 = the path wraps from tile[count-1] back to tile[0] (loop).
 *           0 = open path; behaviour at each end is governed by end_cap.
 *
 * end_cap : only meaningful when closed = 0.
 *           1 = the far end has a visual end-cap tile; the riding object
 *               bounces back when it reaches that tile.
 *           0 = no end-cap tile; the riding object detaches at the far end
 *               and enters free-fall, inheriting the rail's speed as its
 *               initial horizontal velocity.
 *           The near end (start) always has a visual start-cap tile and
 *           always bounces — a rail always has a defined entry point.
 */
typedef struct {
    RailTile tiles[MAX_RAIL_TILES];
    int      count;
    int      closed;
    int      end_cap;
} Rail;

/* ---- Function declarations ----------------------------------------------- */

/* Build all rail instances from the sandbox level (legacy; positions baked in). */
void rail_init(Rail *rails, int *count);

/*
 * rail_init_from_placements is declared in level.h (after RailPlacement is defined).
 * Include level.h instead of calling this function via rail.h.
 */

/* Draw every tile of every rail using the given texture and camera offset. */
void rail_render(const Rail *rails, int count,
                 SDL_Renderer *renderer, SDL_Texture *tex, int cam_x);

/*
 * rail_get_world_pos — Interpolate the world-space centre of position t.
 *
 * t is in [0, rail->count).  The integer part selects the tile; the
 * fractional part lerps linearly to the next tile's centre.
 * Results are written into *out_x and *out_y.
 */
void rail_get_world_pos(const Rail *r, float t, float *out_x, float *out_y);

/*
 * rail_advance — Move t forward by speed × dt, wrapping for closed rails.
 *
 * speed is in tiles per second.  Returns the new t value.
 */
float rail_advance(const Rail *r, float t, float speed, float dt);
