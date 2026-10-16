/*
 * mtiming.c — The classic "accumulator" loop, without the loop:
 *
 *   accumulator += real time since the previous frame
 *   while (accumulator >= step) { update(step); accumulator -= step; }
 *   render();
 *
 * The caller runs update() once per step mtiming_take returns.
 */

#include "mtiming.h"

float mtiming_frame_seconds(FixedStepClock *clock, double max_seconds)
{
    /* GetTime() is raylib's monotonic clock in seconds (a double), so frame
     * times keep sub-millisecond precision instead of whole milliseconds. */
    double now = GetTime();
    double seconds = clock->started ? now - clock->prev_time : 0.0;
    clock->prev_time = now;
    clock->started = 1;

    if (seconds < 0.0) seconds = 0.0;
    if (seconds > max_seconds) seconds = max_seconds;
    return (float)seconds;
}

int mtiming_take(FixedStepClock *clock, float seconds, double step, int max_steps)
{
    int steps = 0;

    if (seconds > 0.0f) clock->accumulator += seconds;
    while (clock->accumulator >= step && steps < max_steps) {
        clock->accumulator -= step;
        steps++;
    }
    /* Still a whole step behind after the cap: drop that time rather than
     * trying to catch up next frame (and falling further behind). */
    if (clock->accumulator >= step) clock->accumulator = step * 0.5;
    return steps;
}
