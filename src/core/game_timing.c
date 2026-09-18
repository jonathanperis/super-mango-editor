/*
 * game_timing.c — Frame timing and loop bookkeeping helpers.
 */

#include "game_timing.h"
#include <stdio.h>

float game_timing_step(GameState *gs, uint64_t *frame_start_ticks)
{
    uint64_t now = clock_millis();
    float dt = (float)(now - gs->loop.prev_ticks) / 1000.0f;
    gs->loop.prev_ticks = now;
    if (frame_start_ticks) *frame_start_ticks = now;

    if (gs->smoke_test_frames > 0 || gs->replay_script_path[0])
        return 1.0f / TARGET_FPS;

    /* Clamp huge deltas after focus loss, window dragging, or OS stalls. */
    if (dt > 0.1f) dt = 0.1f;
    return dt;
}

void game_timing_cap_frame(uint64_t frame_start_ticks, uint32_t frame_ms)
{
#ifndef __EMSCRIPTEN__
    uint64_t elapsed = clock_millis() - frame_start_ticks;
    if (elapsed < frame_ms) {
        clock_wait((uint32_t)(frame_ms - elapsed));
    }
#else
    (void)frame_start_ticks;
    (void)frame_ms;
#endif
}

void game_timing_tick_smoke(GameState *gs)
{
    if (gs->smoke_test_frames > 0) {
        gs->loop.smoke_frames_run++;
        gs->smoke_test_frames--;
        if (gs->smoke_test_frames == 0) {
            if (gs->route == GAME_ROUTE_NONE) {
                gs->route = GAME_ROUTE_SMOKE_EXIT;
                printf("SMOKE_STATE {\"frames\":%d,\"x\":%.6f,\"y\":%.6f,"
                       "\"vx\":%.6f,\"vy\":%.6f,\"score\":%d,\"lives\":%d,"
                       "\"hearts\":%d,\"elapsed\":%.6f,\"paused\":%d,\"complete\":%d}\n",
                       gs->loop.smoke_frames_run, gs->player.x, gs->player.y,
                       gs->player.vx, gs->player.vy, gs->score, gs->lives,
                       gs->hearts, gs->completion.level_elapsed, gs->paused,
                       gs->completion.complete);
            }
        }
    }
}
