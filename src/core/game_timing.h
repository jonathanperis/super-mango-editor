/*
 * game_timing.h — Fixed-step frame timing and loop bookkeeping helpers.
 *
 * The game renders as often as the display allows, but the simulation always
 * advances in steps of exactly GAME_FIXED_STEP seconds:
 *
 *   - A jump reaches the same apex at 30, 60 or 144 Hz. With a variable dt
 *     the numerical integration error (see `make timing-lab`) changed with
 *     the frame rate, so jump heights and arcs did too.
 *   - Per-step movement is bounded (speed × 1/60 s), so a slow frame can no
 *     longer move the player far enough to skip past a collision test.
 *   - Live play, smoke tests, scripted replays and experiment captures run the
 *     very same steps, so a recorded run behaves identically when replayed.
 */

#pragma once

#include "../game.h"

/* Seconds simulated by one update step. */
#define GAME_FIXED_STEP (1.0f / TARGET_FPS)

/*
 * GAME_MAX_FRAME_SECONDS — longest real frame time accepted. A window drag,
 * a breakpoint or an OS stall is treated as a quarter-second hiccup instead
 * of minutes of game time to catch up on.
 */
#define GAME_MAX_FRAME_SECONDS 0.25

/*
 * GAME_MAX_STEPS_PER_FRAME — cap on update steps run before one render.
 * Without it a machine slower than the simulation would fall further behind
 * every frame ("spiral of death"). Time beyond the cap is dropped, so below
 * TARGET_FPS / 5 = 12 rendered frames per second the game slows down instead.
 */
#define GAME_MAX_STEPS_PER_FRAME 5

/*
 * Real seconds since the previous frame, clamped to GAME_MAX_FRAME_SECONDS
 * and read from raylib's GetTime(). Smoke tests and scripted replays report
 * exactly GAME_FIXED_STEP so they simulate one step per frame on any machine.
 */
float game_timing_frame_seconds(GameState *gs);

/*
 * Add real seconds to the accumulator and return how many fixed steps are
 * now due (0 .. GAME_MAX_STEPS_PER_FRAME). Zero steps is normal on a display
 * faster than TARGET_FPS: the frame still renders, the time is kept.
 */
int game_timing_take_steps(GameState *gs, float seconds);

/*
 * Forget pending time and restart the clock now. Call after a pause, a load
 * or any other wait so it is not "caught up" in one burst.
 *
 * Restarting at the current time (rather than "on the next frame") matters:
 * this runs in the middle of a frame, after that frame was measured. If the
 * next frame instead measured 0 s, it would run no step and the game would
 * hitch once after every unpause, retry or level change.
 *
 * The accumulator restarts half a step full, not empty. A 60 Hz display
 * measures frames of 16.67 ms give or take a fraction of a millisecond; from
 * an empty accumulator that jitter flips frames between 0 and 2 steps (a
 * visible stutter). Half a step of slack absorbs it, so each frame runs one.
 *
 * static inline keeps this tiny helper usable without linking game_timing.c.
 */
static inline void game_timing_restart_clock(GameState *gs)
{
    gs->screen.loop.prev_time = GetTime();
    gs->screen.loop.clock_started = 1;
    gs->screen.loop.accumulator = GAME_FIXED_STEP * 0.5;
}

/* Count down smoke-test frames and stop the game when the budget reaches zero. */
void game_timing_tick_smoke(GameState *gs);
