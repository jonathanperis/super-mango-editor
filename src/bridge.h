/*
 * bridge.h — Public interface for the Bridge module.
 *
 * A Bridge is a horizontal walkway built from individual Bridge.png bricks
 * (16×16 px each).  When the player touches the bridge, each brick begins
 * falling independently after a short staggered delay — the brick under the
 * player's feet drops first, then the adjacent bricks cascade outward.
 *
 * Collision (top-surface one-way landing) is handled inside player_update.
 */
#pragma once

#include <SDL.h>

/* ---- Constants ---------------------------------------------------------- */

#define MAX_BRIDGES          2
#define MAX_BRIDGE_BRICKS   16     /* max bricks in a single bridge          */
#define BRIDGE_TILE_W       16     /* width  of one Bridge.png tile (px)     */
#define BRIDGE_TILE_H       16     /* height of one Bridge.png tile (px)     */

/*
 * BRIDGE_STAND_LIMIT — seconds after the player first touches the bridge
 * before the first brick starts falling.  The timer never resets.
 */
/*
 * BRIDGE_FALL_DELAY — milliseconds between the player touching a brick
 * and the brick starting to fall.  The touch is registered immediately
 * (fall_delay is set on the first contact frame); the brick holds for
 * this duration then drops.
 */
#define BRIDGE_FALL_DELAY    0.1f

/*
 * BRIDGE_CASCADE_DELAY — extra seconds between each successive brick's
 * fall start, counted outward from the trigger brick.  Creates a domino
 * ripple effect: 0.06 s × 4 bricks = 0.24 s for the wave to cross 4 bricks.
 */
#define BRIDGE_CASCADE_DELAY 0.06f

/*
 * Fall physics — gentle descent per brick.
 */
#define BRIDGE_FALL_GRAVITY      250.0f
#define BRIDGE_FALL_INITIAL_VY    20.0f

/* ---- Types -------------------------------------------------------------- */

/*
 * BridgeBrick — one 16×16 tile within a bridge.
 */
typedef struct {
    float y_offset;       /* vertical offset from bridge base y (0 = resting)*/
    float fall_vy;        /* downward velocity when falling (px/s)           */
    int   falling;        /* 1 = this brick is actively falling              */
    int   active;         /* 1 = visible, 0 = fallen off-screen              */
    float fall_delay;     /* seconds remaining before this brick starts falling */
} BridgeBrick;

typedef struct {
    float x;              /* left edge in world-space logical pixels         */
    float base_y;         /* original top edge (never changes)               */
    int   brick_count;    /* number of bricks in this bridge                 */
    BridgeBrick bricks[MAX_BRIDGE_BRICKS];
} Bridge;

/* ---- Function declarations ---------------------------------------------- */

void bridges_init(Bridge *bridges, int *count);

void bridges_update(Bridge *bridges, int count, float dt,
                    int landed_idx, float player_cx);

void bridges_render(const Bridge *bridges, int count,
                    SDL_Renderer *renderer, SDL_Texture *tex, int cam_x);

/* Return the world-space bounding rectangle (full bridge, for debug). */
SDL_Rect bridge_get_rect(const Bridge *b);

/*
 * bridge_brick_active_at — Return 1 if the bridge has an active (not fallen)
 * brick at world-space x coordinate wx.  Used by player_update to check if
 * there is still solid ground under the player's feet.
 */
int bridge_has_solid_at(const Bridge *b, float wx);
