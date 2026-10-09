/*
 * bridge.h — Public interface for the Bridge module.
 *
 * A Bridge is a horizontal walkway built from individual Bridge.png bricks
 * (16×16 px each).  Each brick the player stands on starts its own timer
 * and drops BRIDGE_FALL_DELAY seconds later. Only bricks the player actually
 * steps on fall; walking along the bridge drops them one after another
 * behind the player.
 *
 * Collision (top-surface one-way landing) is handled inside player_update.
 */
#pragma once

#include "../shared/graphics.h"

/* ---- Constants ---------------------------------------------------------- */

#define MAX_BRIDGES         16
#define MAX_BRIDGE_BRICKS   16     /* max bricks in a single bridge          */
#define BRIDGE_TILE_W       16     /* width  of one Bridge.png tile (px)     */
#define BRIDGE_TILE_H       16     /* height of one Bridge.png tile (px)     */

/*
 * BRIDGE_FALL_DELAY — seconds between the player first touching a brick
 * and the brick starting to fall.  The touch is registered immediately
 * (fall_delay goes from -1 to 0 on the first contact step), then fall_delay
 * counts UP by dt each step; when it reaches this value the brick drops.
 * The timer never resets, even if the player steps off.
 */
#define BRIDGE_FALL_DELAY    0.2f

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
    float fall_delay;     /* -1 = untouched; else seconds since first touched,
                             counting up to BRIDGE_FALL_DELAY                */
} BridgeBrick;

typedef struct {
    float x;              /* left edge in world-space logical pixels         */
    float base_y;         /* original top edge (never changes)               */
    int   brick_count;    /* number of bricks in this bridge                 */
    BridgeBrick bricks[MAX_BRIDGE_BRICKS];
} Bridge;

/* ---- Function declarations ---------------------------------------------- */

/*
 * bridges_update — Advance every brick's fall state.
 *
 * landed_idx : the bridge the player landed on this step, or -1; the brick
 *              under player_cx on that bridge starts its fall timer.
 * Returns the index of the brick that was touched for the first time this
 * step (for the debug log), or -1 when no brick was newly touched.
 */
int bridges_update(Bridge *bridges, int count, float dt,
                   int landed_idx, float player_cx);

void bridges_render(const Bridge *bridges, int count,
                    Texture2D *tex, int cam_x);

/* Return the world-space bounding rectangle (full bridge, for debug). */
IntRect bridge_get_rect(const Bridge *b);

/*
 * bridge_brick_active_at — Return 1 if the bridge has an active (not fallen)
 * brick at world-space x coordinate wx.  Used by player_update to check if
 * there is still solid ground under the player's feet.
 */
int bridge_has_solid_at(const Bridge *b, float wx);
