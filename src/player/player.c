/*
 * player.c — Player physics update orchestration.
 */

#include "player.h"
#include "player_animation.h"
#include "player_climb.h"
#include "player_internal.h"
#include "player_jump.h"
#include "player_motion.h"
#include "player_surfaces.h"
#include "../surfaces/bouncepad.h"      /* Bouncepad, BOUNCEPAD_VY — for bouncepad landing collision */
#include "../surfaces/float_platform.h" /* FloatPlatform — for one-way landing collision             */
#include "../surfaces/bridge.h"         /* Bridge — for one-way landing collision on bridges         */
#include "../surfaces/ladder.h"         /* LadderDecor — climbable, same mechanics as vine          */
#include "../surfaces/rope.h"           /* RopeDecor — climbable, same mechanics as vine            */
#include "../game.h"                    /* GRAVITY — vertical acceleration */

/* ------------------------------------------------------------------ */

/*
 * player_update -- Apply gravity and velocity to position, handle floor and
 *                  one-way platform collisions.
 *
 * dt (delta time) is the time in seconds since the last frame (e.g. 0.016).
 * Multiplying velocity (px/s) by dt (s) gives displacement in pixels.
 * This makes movement speed identical regardless of frame rate.
 *
 * One-way platforms:
 *   Only the TOP SURFACE of each platform triggers a landing.  The player
 *   can jump through from below; collision is only checked when the player
 *   is moving downward (vy >= 0) and their bottom edge crosses the platform
 *   top surface this frame (crossing test prevents tunnelling at high speed).
 */
void player_update(Player *player, float dt, Mix_Chunk *snd_jump,
                   const Platform *platforms, int platform_count,
                   const FloatPlatform *float_platforms, int float_platform_count,
                   const Bouncepad *bouncepads, int bouncepad_count,
                   const VineDecor *vines, int vine_count,
                   const LadderDecor *ladders, int ladder_count,
                   const RopeDecor *ropes, int rope_count,
                   const Bridge *bridges, int bridge_count,
                   const SpikePlatform *spike_platforms, int spike_platform_count,
                   const int *floor_gaps, int floor_gap_count,
                   int *out_bounce_idx,
                   int *out_fp_landed_idx,
                   int prev_fp_landed_idx,
                   int world_w) {

    (void)vine_count;    /* vine_index selects the climbable; count unused here */
    (void)ladder_count;
    (void)rope_count;

    if (player_update_climbing(player, dt, vines, ladders, ropes, world_w)) {
        return;
    }


    /*
     * Capture the ground state from LAST frame before clearing it.
     * on_ground is about to be reset below; the acceleration block needs the
     * previous value to decide between ground accel and air accel.
     */
    const int was_on_ground = player->on_ground;

    /*
     * Reset on_ground every frame so the player immediately starts falling
     * when they walk off the edge of a platform.  Collision checks below
     * will set it back to 1 if the player is resting on a surface.
     */
    player->on_ground = 0;

    /*
     * Sample the physics bottom BEFORE this frame's movement so the
     * platform crossing test can compare "where was the player last frame"
     * vs "where is the player now".
     *
     * Why capture it here instead of back-projecting later:
     *   back-projection computes  prev = (y_new + 32) - vy_new * dt
     *   which is mathematically   (y_old + vy_new*dt + 32) - vy_new*dt = y_old + 32.
     *   In IEEE 754 single-precision, (a + b) - b can differ from a by up
     *   to 1 ULP due to rounding during the addition.  When the player is
     *   snapped to a platform at y=plat->y, that 1-ULP error makes
     *   prev_bottom > plat->y, the crossing test fails, and the player
     *   silently sinks through the platform every frame until they hit the
     *   floor.  Capturing the true absolute value before the update avoids
     *   all cancellation error.
     */
    const float prev_bottom = player->y + player->h - FLOOR_SINK;
    /*
     * prev_top — sprite top edge before this frame's physics update.
     * Derived from prev_bottom: prev_top = prev_bottom − player->h + FLOOR_SINK.
     * Combined with PHYS_PAD_TOP below to obtain the physical head position
     * for the spike-platform ceiling crossing test.
     */
    const float prev_top    = prev_bottom - player->h + FLOOR_SINK;

    /*
     * Gravity: accelerate downward each frame.
     * Because on_ground was just cleared, gravity always runs here; the
     * floor/platform snap below cancels the tiny fall each frame while the
     * player stands still, keeping the character rock-solid on the ground.
     */
    player->vy += GRAVITY * dt;

    player_apply_horizontal_motion(player, dt, was_on_ground);

    player->x += player->vx * dt;   /* move horizontally */
    player->y += player->vy * dt;   /* move vertically   */

    player_resolve_floor_collision(player, bouncepads, bouncepad_count,
                                   floor_gaps, floor_gap_count,
                                   out_bounce_idx);

    player_resolve_platform_collisions(player, platforms, platform_count,
                                       float_platforms, float_platform_count,
                                       prev_bottom, out_fp_landed_idx,
                                       prev_fp_landed_idx);

    player_resolve_bridge_collision(player, bridges, bridge_count, prev_bottom);
    player_resolve_spike_platform_top_collision(player, spike_platforms,
                                                spike_platform_count,
                                                prev_bottom);
    player_resolve_spike_platform_ceiling_collision(player, spike_platforms,
                                                    spike_platform_count,
                                                    prev_top);

    player_resolve_world_bounds(player, world_w);

    if (player_update_jump_timers(player, dt, was_on_ground)) {
        player_start_jump(player, snd_jump);
    }

    /*
     * Advance the sprite animation based on the resolved physics state.
     * Convert dt (seconds) to milliseconds for the frame timer.
     */
    player_animate(player, (Uint32)(dt * 1000.0f));
}
