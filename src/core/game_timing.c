/*
 * game_timing.c — Fixed-step frame timing and loop bookkeeping helpers.
 *
 * See game_timing.h for why the simulation uses a fixed step. The clock
 * itself is shared/fixed_step.c; this file supplies the game's limits and
 * the deterministic smoke/replay mode.
 */

#include "game_timing.h"
#include <stdio.h>

float game_timing_frame_seconds(GameState *gs)
{
    /* Deterministic runs must not depend on how fast this machine is. */
    if (gs->screen.smoke_test_frames > 0 || gs->screen.replay_script_path[0])
        return GAME_FIXED_STEP;
    return fixed_step_frame_seconds(&gs->screen.loop.clock, GAME_MAX_FRAME_SECONDS);
}

int game_timing_take_steps(GameState *gs, float seconds)
{
    return fixed_step_take(&gs->screen.loop.clock, seconds, GAME_FIXED_STEP,
                           GAME_MAX_STEPS_PER_FRAME);
}

void game_timing_tick_smoke(GameState *gs)
{
    if (gs->screen.smoke_test_frames > 0) {
        gs->screen.loop.smoke_frames_run++;
        gs->screen.smoke_test_frames--;
        if (gs->screen.smoke_test_frames == 0) {
            if (gs->screen.route == GAME_ROUTE_NONE) {
                gs->screen.route = GAME_ROUTE_SMOKE_EXIT;
                printf("SMOKE_STATE {\"frames\":%d,\"x\":%.6f,\"y\":%.6f,"
                       "\"vx\":%.6f,\"vy\":%.6f,\"score\":%d,\"lives\":%d,"
                       "\"hearts\":%d,\"elapsed\":%.6f,\"paused\":%d,\"complete\":%d}\n",
                       gs->screen.loop.smoke_frames_run, gs->world.player.x, gs->world.player.y,
                       gs->world.player.vx, gs->world.player.vy, gs->world.score, gs->world.lives,
                       gs->world.hearts, gs->screen.completion.level_elapsed, gs->screen.paused,
                       gs->screen.completion.complete);
            }
        }
    }
}
