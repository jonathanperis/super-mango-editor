/*
 * mtiming.h — A fixed-step clock: real frame time in, whole steps out.
 *
 * It knows nothing about the game. The caller picks the step length, the
 * longest frame it accepts and the most steps one frame may run; see
 * core/game_timing.h for the values the game uses and why.
 */

#pragma once

#include <raylib.h>

typedef struct {
    double prev_time;    /* GetTime() seconds when the previous frame began */
    double accumulator;  /* real seconds waiting to be simulated            */
    int    started;      /* 0 = no previous frame time yet (first frame)    */
} FixedStepClock;

/*
 * Real seconds since the previous call, clamped to 0 .. max_seconds and read
 * from raylib's GetTime(). The first call after a reset measures 0 s.
 */
float fixed_step_frame_seconds(FixedStepClock *clock, double max_seconds);

/*
 * Add real seconds to the accumulator and return how many steps of `step`
 * seconds are now due, at most max_steps. A whole step still pending after
 * the cap is dropped (keeping half a step of slack, as in fixed_step_restart)
 * so a slow machine slows the simulation down instead of falling behind.
 */
int fixed_step_take(FixedStepClock *clock, float seconds, double step, int max_steps);

/*
 * Forget pending time and restart the clock now, with the accumulator half a
 * step full so frame-to-frame jitter does not flip frames between 0 and 2
 * steps. static inline keeps it usable without linking mtiming.c.
 */
static inline void fixed_step_restart(FixedStepClock *clock, double step)
{
    clock->prev_time = GetTime();
    clock->started = 1;
    clock->accumulator = step * 0.5;
}
