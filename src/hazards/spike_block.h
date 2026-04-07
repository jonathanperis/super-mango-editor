/*
 * spike_block.h — Public interface for the SpikeBlock entity.
 *
 * A SpikeBlock rides a Rail, moving clockwise at a constant speed.
 * It damages the player on contact (same rules as spider/fish) and also
 * applies a push impulse opposite to the player's movement direction.
 *
 * The block's world position is recomputed each frame via rail_get_world_pos.
 * No physics are applied to the block itself — the rail fully controls it.
 */
#pragma once

#include <SDL.h>
#include "../surfaces/rail.h"  /* Rail — the block needs a pointer to its rail */
#include "../player/player.h"  /* Player — for the push response signature     */

/* ------------------------------------------------------------------ */

/*
 * SPIKE_DISPLAY_W / SPIKE_DISPLAY_H — on-screen size in logical pixels.
 *
 * Spike_Block.png is a single 16×16 frame.  We display it at 24×24 to make
 * it more visible against rail tiles (which are also 16×16).  Nearest-
 * neighbour scaling (set in game_init) keeps the pixel art sharp.
 */
#define SPIKE_DISPLAY_W  24
#define SPIKE_DISPLAY_H  24

/*
 * SPIKE_SPIN_DEG_PER_SEC — rotation speed of the spinning animation.
 *
 * The block rotates continuously clockwise at this many degrees per second.
 * 360° per second = one full rotation every second, giving a clear visual
 * cue that the block is a moving hazard.
 */
#define SPIKE_SPIN_DEG_PER_SEC  360.0f

/*
 * Speed presets — rail traversal speed in tiles per second.
 *
 * Three named tiers cover the range of interesting gameplay pacing:
 *
 *   SLOW   : 1.5 tiles/s — 28-tile loop ≈ 18.7 s; 14-tile line ≈ 9.3 s
 *   NORMAL : 3.0 tiles/s — 28-tile loop ≈  9.3 s; 14-tile line ≈ 4.7 s
 *   FAST   : 6.0 tiles/s — 28-tile loop ≈  4.7 s; 14-tile line ≈ 2.3 s
 *
 * Pass one of these to spike_block_init as the `speed` argument.
 * For closed loops, speed drives clockwise traversal.
 * For open (horizontal) rails, the block bounces and the same speed
 * applies in both directions.
 */
#define SPIKE_SPEED_SLOW    1.5f
#define SPIKE_SPEED_NORMAL  3.0f
#define SPIKE_SPEED_FAST    6.0f

/*
 * Push impulse constants — applied to the player on contact.
 *
 * SPIKE_PUSH_SPEED : magnitude of the push velocity vector, in logical px/s.
 *                    Applied in the direction OPPOSITE to the player's movement.
 * SPIKE_PUSH_VY    : extra upward component added regardless of movement direction.
 *                    Gives a small bounce-back feel even when the player is
 *                    moving horizontally.  Negative = upward in SDL screen-space.
 */
#define SPIKE_PUSH_SPEED  220.0f
#define SPIKE_PUSH_VY    -150.0f

/* ------------------------------------------------------------------ */

#define MAX_SPIKE_BLOCKS 16   /* max SpikeBlock instances in one level */

/*
 * SpikeBlock — a hazard that travels along a Rail, and can fall off open ends.
 *
 * t         : current position on the rail, in [0, rail->count).
 *             Integer part = tile index; fractional part = lerp toward next tile.
 * speed     : traversal speed in tiles per second (one of the SPIKE_SPEED_* presets).
 * direction : +1 = forward (clockwise on loops, rightward on open lines),
 *             -1 = backward (bounce mode on open rails with end_cap = 1).
 *             Always +1 on closed loops since they wrap continuously.
 * waiting   : 1 = block is paused at its start position, waiting to enter
 *                 the camera viewport before it begins moving.  Only set on
 *                 open rails with end_cap = 0 (the "fall-off" variant).
 *                 Closed loops and bouncing rails start active immediately
 *                 because they loop/bounce infinitely and are always relevant.
 *             0 = block is moving or in free-fall.
 * detached  : 0 = riding the rail normally.
 *             1 = has left the rail and is in free-fall.  In this state the
 *                 block ignores the rail entirely; physics (vx/vy + gravity)
 *                 drive the position until the block exits the screen.
 * fall_vx   : horizontal velocity (px/s) at the moment of detachment.
 *             Derived from rail speed × tile size × travel direction, so the
 *             block continues moving in the same direction it was going.
 * fall_vy   : vertical velocity (px/s) during free-fall.  Starts at 0 and
 *             accumulates GRAVITY each frame until the block leaves the screen.
 * x, y      : world-space top-left corner, updated every frame.
 * w, h      : display size (SPIKE_DISPLAY_W × SPIKE_DISPLAY_H).
 * active    : 1 = alive (on rail or falling), 0 = off-screen and eliminated.
 * rail      : pointer to the Rail this block travels on.  Not owned here.
 */
typedef struct {
    float        t;
    float        speed;
    int          direction;
    int          waiting;
    int          detached;
    float        fall_vx;
    float        fall_vy;
    float        x;
    float        y;
    int          w;
    int          h;
    int          active;
    float        spin_angle;  /* current rotation in degrees [0, 360) */
    const Rail  *rail;
} SpikeBlock;

/* ---- Function declarations ---------------------------------------- */

/* Place one spike block on a specific rail at starting position t0. */
void spike_block_init(SpikeBlock *sb, const Rail *rail, float t0, float speed);

/* Initialise all spike block instances. */
void spike_blocks_init(SpikeBlock *blocks, int *count, const Rail *rails);

/* Advance one block along its rail. cam_x is the current camera left edge. */
void spike_block_update(SpikeBlock *sb, float dt, int cam_x);

/* Advance all active blocks. */
void spike_blocks_update(SpikeBlock *blocks, int count, float dt, int cam_x);

/* Draw one block at its current world position. */
void spike_block_render(const SpikeBlock *sb,
                        SDL_Renderer *renderer, SDL_Texture *tex, int cam_x);

/* Draw all active blocks. */
void spike_blocks_render(const SpikeBlock *blocks, int count,
                         SDL_Renderer *renderer, SDL_Texture *tex, int cam_x);

/*
 * spike_block_get_hitbox — Return the collision rectangle in world space.
 *
 * The hitbox matches the display rect exactly (no inset), because the
 * spike art fills the full sprite frame.
 */
SDL_Rect spike_block_get_hitbox(const SpikeBlock *sb);

/*
 * spike_block_push_player — Apply push impulse to the player.
 *
 * Called immediately after a hit is detected.  Reverses the player's
 * normalised movement direction and scales it to SPIKE_PUSH_SPEED,
 * then adds SPIKE_PUSH_VY upward.  If the player is stationary the push
 * is based on the relative position of the block vs. the player.
 */
void spike_block_push_player(const SpikeBlock *sb, Player *player);
