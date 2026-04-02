/*
 * bouncepad_small.c — Green bouncepad placement (small jump).
 *
 * Delegates field initialisation to bouncepad_place() so only the position
 * and variant-specific values need to be specified here.
 */

#include "bouncepad_small.h"
#include "game.h"

void bouncepads_small_init(Bouncepad *pads, int *count) {
    /* Green pad 0 — screen 2, right of the tall pillar at x=680. */
    bouncepad_place(&pads[0], 734.0f, BOUNCEPAD_VY_SMALL, BOUNCEPAD_GREEN);
    *count = 1;
}
