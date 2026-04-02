/*
 * bouncepad_high.c — Red bouncepad placement (high jump).
 *
 * Delegates field initialisation to bouncepad_place() so only the position
 * and variant-specific values need to be specified here.
 */

#include "bouncepad_high.h"
#include "game.h"

void bouncepads_high_init(Bouncepad *pads, int *count) {
    /* Red pad 0 — screen 4, left of the last tall pillar at x=1480. */
    bouncepad_place(&pads[0], 1420.0f, BOUNCEPAD_VY_HIGH, BOUNCEPAD_RED);
    *count = 1;
}
