/*
 * game_float_platforms.c — Floating platform update and rider carry.
 */

#include "game_float_platforms.h"

#include "../surfaces/float_platform.h"

void game_float_platforms_update(GameState *gs, float dt, int fp_landed_idx)
{
    float_platforms_update(gs->float_platforms, gs->float_platform_count,
                           dt, fp_landed_idx);

    if (fp_landed_idx < 0 || fp_landed_idx >= gs->float_platform_count) {
        fp_landed_idx = -1;
    }

    if (fp_landed_idx >= 0) {
        const FloatPlatform *fp = &gs->float_platforms[fp_landed_idx];
        if (fp->mode == FLOAT_PLATFORM_RAIL) {
            /*
             * Carry the rider by exactly the distance the platform just
             * moved, along both axes. Without the vertical part, a platform
             * moving down would leave the player hovering above it each step
             * (falling, landing, falling again), and one moving up would sink
             * the feet into it until the next step pulled them back on top.
             */
            gs->player.x += fp->x - fp->prev_x;
            gs->player.y += fp->y - fp->prev_y;
        }
    }

    gs->loop.fp_prev_riding = fp_landed_idx;
}
