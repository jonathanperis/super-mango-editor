/*
 * game_timing.c — Frame timing and loop bookkeeping helpers.
 */

#include "game_timing.h"

float game_timing_step(GameState *gs, Uint64 *frame_start_ticks)
{
    Uint64 now = SDL_GetTicks64();
    float dt = (float)(now - gs->loop.prev_ticks) / 1000.0f;
    gs->loop.prev_ticks = now;
    if (frame_start_ticks) *frame_start_ticks = now;

    /* Clamp huge deltas after focus loss, window dragging, or OS stalls. */
    if (dt > 0.1f) dt = 0.1f;
    return dt;
}

void game_timing_cap_frame(Uint64 frame_start_ticks, Uint32 frame_ms)
{
#ifndef __EMSCRIPTEN__
    Uint64 elapsed = SDL_GetTicks64() - frame_start_ticks;
    if (elapsed < frame_ms) {
        SDL_Delay((Uint32)(frame_ms - elapsed));
    }
#else
    (void)frame_start_ticks;
    (void)frame_ms;
#endif
}

void game_timing_tick_smoke(GameState *gs)
{
    if (gs->smoke_test_frames > 0) {
        gs->smoke_test_frames--;
        if (gs->smoke_test_frames == 0) {
            gs->running = 0;
        }
    }
}
