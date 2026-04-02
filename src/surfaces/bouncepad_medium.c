/*
 * bouncepad_medium.c — Wood bouncepad placement (medium jump).
 *
 * Delegates field initialisation to bouncepad_place() so only the position
 * and variant-specific values need to be specified here.
 */

#include "bouncepad_medium.h"
#include "game.h"

void bouncepads_medium_init(Bouncepad *pads, int *count) {
    /* Wood pad 0 — screen 1, right of the 3-tile pillar at x=256. */
    bouncepad_place(&pads[0], 310.0f, BOUNCEPAD_VY_MEDIUM, BOUNCEPAD_WOOD);
    *count = 1;
}
