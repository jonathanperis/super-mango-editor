/*
 * spike_block.c — SpikeBlock: a rail-riding hazard that pushes the player.
 */

#include <SDL.h>
#include <math.h>   /* sqrtf */
#include <stdio.h>

#include "spike_block.h"
#include "../surfaces/rail.h"
#include "../player/player.h"
#include "../game.h"   /* GAME_W, GAME_H — used only for potential bounds checks */

/* ------------------------------------------------------------------ */

/*
 * spike_block_init — Place one SpikeBlock on a rail at starting offset t0.
 *
 * rail  : the Rail this block will travel on; stored as a non-owning pointer.
 * t0    : starting position in [0, rail->count).  Different starting offsets
 *         for different blocks prevent all blocks from being synchronised.
 * speed : rail traversal speed in tiles per second (use one of SPIKE_SPEED_SLOW/NORMAL/FAST).
 */
void spike_block_init(SpikeBlock *sb, const Rail *rail, float t0, float speed) {
    sb->rail      = rail;
    sb->t         = t0;
    sb->speed     = speed;
    sb->direction = 1;    /* always start moving forward / rightward */
    /*
     * Open rails with no end cap (fall-off mode) start in the waiting state:
     * the block holds still at t=0 until the camera reaches it, so the player
     * always sees the block begin its run from the left end rather than finding
     * it mid-track or already gone.
     * Closed loops and bouncing rails are always running, so they start active.
     */
    sb->waiting   = (!rail->closed && !rail->end_cap) ? 1 : 0;
    sb->detached  = 0;    /* starts riding the rail, not in free-fall */
    sb->fall_vx   = 0.0f;
    sb->fall_vy   = 0.0f;
    sb->w          = SPIKE_DISPLAY_W;
    sb->h          = SPIKE_DISPLAY_H;
    sb->active     = 1;
    sb->spin_angle = 0.0f;

    /*
     * Set the initial world position from the rail immediately so the block
     * is never drawn at (0,0) on the first frame before update runs.
     */
    float cx, cy;
    rail_get_world_pos(rail, t0, &cx, &cy);
    sb->x = cx - sb->w * 0.5f;
    sb->y = cy - sb->h * 0.5f;
}

/* ------------------------------------------------------------------ */

/*
 * spike_blocks_init — Create one SpikeBlock per rail, each at a different speed.
 *
 * The three speed tiers (SLOW / NORMAL / FAST) are distributed across the
 * three rails so the player encounters progressively more challenging hazards
 * as they traverse the level left to right.
 *
 *   Block 0 — Rail 0 (screen 2, closed loop): SLOW  — easiest to avoid.
 *   Block 1 — Rail 1 (screen 3, closed loop): NORMAL.
 *   Block 2 — Rail 2 (screen 4, open line)  : FAST  — bounces end-to-end.
 *
 * Blocks 1 and 2 start at offset positions so they are not all clustered
 * at tile 0 when the player first enters their screen.
 */
void spike_blocks_init(SpikeBlock *blocks, int *count, const Rail *rails) {
    /*
     * Block 0 — Rail 0, SLOW.  Starts at tile 0 (top-left corner).
     * At 1.5 tiles/s the 28-tile loop takes ~18.7 s — very readable.
     */
    spike_block_init(&blocks[0], &rails[0], 0.0f, SPIKE_SPEED_SLOW);

    /*
     * Block 1 — Rail 1, NORMAL.  Starts halfway around the 22-tile loop
     * (≈ tile 11) so it is never synchronised with block 0.
     */
    spike_block_init(&blocks[1], &rails[1],
                     (float)(rails[1].count / 2), SPIKE_SPEED_NORMAL);

    /*
     * Block 2 — Rail 2, FAST.  The horizontal open rail uses bounce
     * movement (direction flips at each endpoint).  Starts at tile 0
     * (left end) moving rightward.
     */
    spike_block_init(&blocks[2], &rails[2], 0.0f, SPIKE_SPEED_FAST);

    *count = 3;
}

/* ------------------------------------------------------------------ */

/*
 * spike_block_update — Advance one block for this frame.
 *
 * Three distinct states:
 *
 *   1. Detached (free-fall)
 *      The block has left an open rail with no end cap.  We apply GRAVITY
 *      to fall_vy each frame, move x/y by the velocity vector, and deactivate
 *      the block once it has scrolled 128 px below the logical canvas (GAME_H).
 *
 *   2. Closed loop
 *      rail_advance wraps t continuously in [0, count); direction stays +1.
 *
 *   3. Open line
 *      t advances by speed × direction × dt.  At each endpoint:
 *        • Near end (t ≤ 0)        → always bounce (start cap always present).
 *        • Far  end (t ≥ count−1)  → bounce if end_cap = 1; detach if end_cap = 0.
 *      On detach, fall_vx is set to the rail's pixel speed in the current
 *      direction (speed tiles/s × RAIL_TILE_W px/tile).  fall_vy starts at 0
 *      and gravity accumulates from the next frame onward.
 */
void spike_block_update(SpikeBlock *sb, float dt, int cam_x) {
    if (!sb->active) return;

    /*
     * Advance the spin angle every frame, even while waiting or in free-fall,
     * so the block always looks alive.  Wrap within [0, 360) to avoid float
     * drift over time.
     */
    sb->spin_angle += SPIKE_SPIN_DEG_PER_SEC * dt;
    if (sb->spin_angle >= 360.0f) sb->spin_angle -= 360.0f;

    /* ---- Waiting state: hold at start until the rail enters the viewport - */
    /*
     * A fall-off rail (open, no end cap) keeps the block frozen at t=0 until
     * the camera reveals it.  This guarantees the player always witnesses the
     * block's full run from the left end, making the hazard predictable and
     * fair.
     *
     * Visibility test: the block's world-space rect overlaps the camera window
     * [cam_x, cam_x + GAME_W).  Because the block starts at the left end of
     * the rail and the player approaches from the left, the trigger fires as
     * soon as the block's left edge scrolls into the right side of the screen.
     */
    if (sb->waiting) {
        int block_screen_x = (int)sb->x - cam_x;   /* screen-space left edge */
        if (block_screen_x < GAME_W) {
            sb->waiting = 0;   /* now visible — begin moving */
        } else {
            return;            /* still off-screen — stay frozen */
        }
    }

    /* ---- State 1: free-fall after detaching from an open rail ---------- */
    if (sb->detached) {
        /*
         * Apply gravity: accelerate downward at GRAVITY px/s².
         * GRAVITY is defined in game.h (800.0f).
         */
        sb->fall_vy += GRAVITY * dt;

        sb->x += sb->fall_vx * dt;
        sb->y += sb->fall_vy * dt;

        /*
         * Deactivate once the block has fallen 128 px below the bottom of
         * the logical canvas.  128 px gives a small margin so the block
         * finishes its exit animation before disappearing.
         */
        if (sb->y > (float)(GAME_H + 128)) {
            sb->active = 0;
        }
        return;
    }

    /* ---- State 2: closed loop ----------------------------------------- */
    if (sb->rail->closed) {
        sb->t = rail_advance(sb->rail, sb->t, sb->speed, dt);
    }
    /* ---- State 3: open line ------------------------------------------- */
    else {
        sb->t += sb->speed * (float)sb->direction * dt;

        if (sb->t >= (float)(sb->rail->count - 1)) {
            if (sb->rail->end_cap) {
                /*
                 * End cap present: bounce back toward the start.
                 * Clamp t exactly at the last tile so the block doesn't
                 * overshoot the visual end-cap tile.
                 */
                sb->t         = (float)(sb->rail->count - 1);
                sb->direction = -1;
            } else {
                /*
                 * No end cap: detach and enter free-fall.
                 * fall_vx inherits the horizontal pixel speed the block had
                 * on the rail (speed tiles/s × 16 px/tile × direction).
                 * fall_vy starts at 0; gravity will build it each frame.
                 */
                sb->detached = 1;
                sb->fall_vx  = sb->speed * (float)RAIL_TILE_W * (float)sb->direction;
                sb->fall_vy  = 0.0f;
                return;   /* skip rail_get_world_pos this frame */
            }
        } else if (sb->t <= 0.0f) {
            /* Near end: always bounce (start cap is always present) */
            sb->t         = 0.0f;
            sb->direction = 1;
        }
    }

    /*
     * For open rails, clamp t just below (count − 1) before the position
     * lookup to ensure the next-tile index j never wraps to tile[0].
     */
    float t_safe = sb->t;
    if (!sb->rail->closed && t_safe >= (float)(sb->rail->count - 1)) {
        t_safe = (float)(sb->rail->count - 1) - 0.0001f;
    }

    /*
     * Recompute world-space position.
     * rail_get_world_pos returns the interpolated centre; subtract half the
     * display size to get the top-left corner for the SDL_Rect.
     */
    float cx, cy;
    rail_get_world_pos(sb->rail, t_safe, &cx, &cy);
    sb->x = cx - sb->w * 0.5f;
    sb->y = cy - sb->h * 0.5f;
}

void spike_blocks_update(SpikeBlock *blocks, int count, float dt, int cam_x) {
    for (int i = 0; i < count; i++)
        spike_block_update(&blocks[i], dt, cam_x);
}

/* ------------------------------------------------------------------ */

/*
 * spike_block_render — Draw one SpikeBlock at its current world position.
 *
 * Spike_Block.png is a single 16×16 frame used as the full source texture.
 * Passing NULL as the source rect to SDL_RenderCopy means "use the entire
 * texture", which is correct for a single-frame sprite.
 */
void spike_block_render(const SpikeBlock *sb,
                        SDL_Renderer *renderer, SDL_Texture *tex, int cam_x) {
    if (!sb->active) return;

    /*
     * dst — destination on screen.
     * x − cam_x converts world space to screen space.
     * w/h are SPIKE_DISPLAY_W × SPIKE_DISPLAY_H (24×24 logical px).
     */
    SDL_Rect dst = {
        .x = (int)sb->x - cam_x,
        .y = (int)sb->y,
        .w = sb->w,
        .h = sb->h,
    };

    /*
     * SDL_RenderCopyEx — blit the full texture into dst with rotation.
     *
     *   angle  : spin_angle in degrees, advances each frame.
     *   center : NULL = rotate around the rect's own centre (natural pivot).
     *   flip   : SDL_FLIP_NONE — no mirror; rotation alone drives the spin.
     *
     * SDL scales from 16×16 (source) to 24×24 (dst) using the nearest-
     * neighbour hint set in game_init, preserving the pixel-art look.
     */
    SDL_RenderCopyEx(renderer, tex, NULL, &dst,
                     (double)sb->spin_angle, NULL, SDL_FLIP_NONE);
}

void spike_blocks_render(const SpikeBlock *blocks, int count,
                         SDL_Renderer *renderer, SDL_Texture *tex, int cam_x) {
    for (int i = 0; i < count; i++)
        spike_block_render(&blocks[i], renderer, tex, cam_x);
}

/* ------------------------------------------------------------------ */

/*
 * spike_block_get_hitbox — Return the collision rect in world coordinates.
 *
 * The hitbox matches the display rect (top-left x,y; size w×h).
 * No inset is applied because the spike art fills the entire frame.
 */
SDL_Rect spike_block_get_hitbox(const SpikeBlock *sb) {
    SDL_Rect r = {
        .x = (int)sb->x,
        .y = (int)sb->y,
        .w = sb->w,
        .h = sb->h,
    };
    return r;
}

/* ------------------------------------------------------------------ */

/*
 * spike_block_push_player — Apply a push impulse opposite to player movement.
 *
 * Algorithm:
 *   1. If the player has a non-zero velocity, normalise it, negate the
 *      direction, and scale to SPIKE_PUSH_SPEED.  This reverses the player
 *      directly back along their movement vector.
 *   2. If the player is stationary, push based on the relative horizontal
 *      position of the block vs. the player (away from the block).
 *   3. In both cases add SPIKE_PUSH_VY upward so there is always a visible
 *      bounce-back effect even during horizontal movement.
 *   4. Clear on_ground so gravity immediately continues (prevents the player
 *      from being "stuck" on the ground while the push is applied).
 */
void spike_block_push_player(const SpikeBlock *sb, Player *player) {
    float vx = player->vx;
    float vy = player->vy;
    float len = sqrtf(vx * vx + vy * vy);

    if (len > 1.0f) {
        /*
         * Normalise the player's velocity and reverse it to get the push
         * direction.  Scale to SPIKE_PUSH_SPEED for a consistent impulse
         * regardless of how fast the player was moving before impact.
         */
        player->vx = -(vx / len) * SPIKE_PUSH_SPEED;
        player->vy = -(vy / len) * SPIKE_PUSH_SPEED + SPIKE_PUSH_VY;
    } else {
        /*
         * Player was stationary: push horizontally away from the block's
         * centre, plus a fixed upward component.
         */
        float block_cx = sb->x + sb->w * 0.5f;
        float player_cx = player->x + player->w * 0.5f;
        float dir = (player_cx >= block_cx) ? 1.0f : -1.0f;
        player->vx = dir * SPIKE_PUSH_SPEED;
        player->vy = SPIKE_PUSH_VY;
    }

    /*
     * Lift the player off the ground so the upward component takes effect.
     * Without this, player_update would immediately snap them back to the
     * floor on the same frame.
     */
    player->on_ground = 0;
}
