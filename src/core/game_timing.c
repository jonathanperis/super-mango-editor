/*
 * game_timing.c — Fixed-step frame timing and loop bookkeeping helpers.
 *
 * See game_timing.h for why the simulation uses a fixed step. The pattern is
 * the classic "accumulator" loop:
 *
 *   accumulator += real time since the previous frame
 *   while (accumulator >= step) { update(step); accumulator -= step; }
 *   render();
 */

#include "game_timing.h"
#include <stdio.h>

float game_timing_frame_seconds(GameState *gs)
{
    /* Deterministic runs must not depend on how fast this machine is. */
    if (gs->screen.smoke_test_frames > 0 || gs->screen.replay_script_path[0])
        return GAME_FIXED_STEP;

    /* GetTime() is raylib's monotonic clock in seconds (a double), so frame
     * times keep sub-millisecond precision instead of whole milliseconds. */
    double now = GetTime();
    double seconds = gs->screen.loop.clock_started ? now - gs->screen.loop.prev_time : 0.0;
    gs->screen.loop.prev_time = now;
    gs->screen.loop.clock_started = 1;

    if (seconds < 0.0) seconds = 0.0;
    if (seconds > GAME_MAX_FRAME_SECONDS) seconds = GAME_MAX_FRAME_SECONDS;
    return (float)seconds;
}

int game_timing_take_steps(GameState *gs, float seconds)
{
    const double step = GAME_FIXED_STEP;
    int steps = 0;

    if (seconds > 0.0f) gs->screen.loop.accumulator += seconds;
    while (gs->screen.loop.accumulator >= step && steps < GAME_MAX_STEPS_PER_FRAME) {
        gs->screen.loop.accumulator -= step;
        steps++;
    }
    /* Still a whole step behind after the cap: drop that time rather than
     * trying to catch up next frame (and falling further behind). Keep the
     * same half-step slack that game_timing_restart_clock uses. */
    if (gs->screen.loop.accumulator >= step) gs->screen.loop.accumulator = step * 0.5;
    return steps;
}

void game_timing_note_step(GameState *gs)
{
    gs->screen.loop.prev_player_x = gs->world.player.x;
    gs->screen.loop.prev_player_y = gs->world.player.y;
    gs->screen.loop.prev_camera_x = gs->world.camera.x;
    gs->screen.loop.has_prev = 1;
}

/* from + (to - from) * t, or to itself when the step jumped too far. */
static float blend(float from, float to, float t)
{
    float d = to - from;
    if (d > GAME_INTERP_MAX_JUMP || d < -GAME_INTERP_MAX_JUMP) return to;
    return from + d * t;
}

GameRenderView game_timing_render_view(const GameState *gs, int cam_x)
{
    GameRenderView view = {cam_x, gs->world.player.x, gs->world.player.y, 1.0f};
    const GameLoopState *loop = &gs->screen.loop;
    if (!loop->has_prev || gs->screen.debug_mode || gs->screen.replay_script_path[0] ||
        gs->screen.smoke_test_frames > 0 || loop->smoke_frames_run > 0 || gs->screen.experiment)
        return view;
    float t = (float)(loop->accumulator / GAME_FIXED_STEP);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    view.t = t;
    view.cam_x = (int)blend(loop->prev_camera_x, gs->world.camera.x, t);
    game_timing_view_point(&view, loop->prev_player_x, loop->prev_player_y, &view.player_x, &view.player_y);
    return view;
}

void game_timing_view_point(const GameRenderView *view, float from_x, float from_y, float *x, float *y)
{
    /* A respawn may move one axis far and not the other; both belong to
     * the same jump, so either one shows the point where it is now. */
    float dx = *x - from_x, dy = *y - from_y;
    if (dx <= GAME_INTERP_MAX_JUMP && dx >= -GAME_INTERP_MAX_JUMP &&
        dy <= GAME_INTERP_MAX_JUMP && dy >= -GAME_INTERP_MAX_JUMP) {
        *x = from_x + dx * view->t;
        *y = from_y + dy * view->t;
    }
}

void game_timing_tick_smoke(GameState *gs)
{
    if (gs->screen.smoke_test_frames > 0) {
        gs->screen.loop.smoke_frames_run++;
        gs->screen.smoke_test_frames--;
        if (gs->screen.smoke_test_frames == 0) {
            /* The run ends here even when this frame also asked for a
             * Replay, the next level or Level Select: those would go on
             * with no frames left to count, and so never end. An exit or
             * a fatal error already ends the run. */
            if (gs->screen.route != GAME_ROUTE_EXIT && gs->screen.route != GAME_ROUTE_FATAL) {
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
