/*
 * game_trig.h — Sine and cosine that give the same answer on every machine.
 *
 * Why not sinf and cosf from <math.h>? The C standard does not say exactly
 * what they return, only roughly. Each C library (Apple's, glibc, the
 * Windows one, Emscripten's musl) rounds its last bit its own way. A
 * recorded run, a ghost or an experiment capture replays only if every
 * step computes the very same numbers, and a swinging axe's hitbox or a
 * bird's height that differ by one bit can, sooner or later, decide a hit
 * differently. So the fixed step uses these two instead.
 *
 * They use only + - * / and floor(), which IEEE 754 defines exactly: every
 * machine rounds them the same way, as long as the compiler does not fuse
 * a multiply and an add into one instruction. The Makefile passes
 * -ffp-contract=off for that. (fmodf and sqrtf are exact by the standard
 * too, so the step keeps using them.)
 *
 * How it works, for sin(x):
 *   1. Take away whole turns of 2π, leaving y in [-π, π]. sin repeats
 *      every turn, so the answer does not change.
 *   2. Fold y into [-π/2, π/2] with sin(π - y) = sin(y).
 *   3. Add up the Taylor series y - y³/3! + y⁵/5! - ... to the y¹⁵ term.
 *      On [-π/2, π/2] the first term left out is below 1e-11, far under
 *      what a float can hold.
 * The work is done in double and rounded to float once at the end, so the
 * result is within one float step of the true value.
 *
 * Drawing code that does not touch the simulation may keep using sinf.
 */
#pragma once

#include <math.h>   /* floor */

#define GAME_TRIG_PI      3.14159265358979323846
#define GAME_TRIG_HALF_PI 1.57079632679489661923
#define GAME_TRIG_TWO_PI  6.28318530717958647692

/* sin(x) for x in radians, in double, by the three steps above. */
static inline double game_trig_sin(double x)
{
    double turns = floor(x / GAME_TRIG_TWO_PI + 0.5);   /* nearest whole turn */
    double y = x - turns * GAME_TRIG_TWO_PI;            /* now in [-π, π]     */
    double y2;

    if (y > GAME_TRIG_HALF_PI) y = GAME_TRIG_PI - y;
    else if (y < -GAME_TRIG_HALF_PI) y = -GAME_TRIG_PI - y;

    /* y - y³/3! + y⁵/5! - ... written inside out (Horner's rule): fewer
     * multiplications, and the small terms are added first. */
    y2 = y * y;
    return y * (1.0 + y2 * (-1.0 / 6.0 + y2 * (1.0 / 120.0 + y2 * (-1.0 / 5040.0 +
           y2 * (1.0 / 362880.0 + y2 * (-1.0 / 39916800.0 + y2 * (1.0 / 6227020800.0 +
           y2 * (-1.0 / 1307674368000.0))))))));
}

/* sinf and cosf for the fixed step: x in radians. */
static inline float game_sinf(float x)
{
    return (float)game_trig_sin((double)x);
}

static inline float game_cosf(float x)
{
    /* cos(x) = sin(x + π/2); the sum is formed in double, so nothing is
     * lost before the reduction. */
    return (float)game_trig_sin((double)x + GAME_TRIG_HALF_PI);
}
