/*
 * bouncepad_medium.c — Wood bouncepad placement (medium jump).
 */

#include "bouncepad_medium.h"
#include "game.h"

void bouncepads_medium_init(Bouncepad *pads, int *count) {
    /*
     * Wood pad 0 — screen 1, right of the 3-tile pillar at x=256.
     */
    pads[0].x             = 310.0f;
    pads[0].y             = (float)(FLOOR_Y - BOUNCEPAD_SRC_H);
    pads[0].w             = BOUNCEPAD_W;
    pads[0].h             = BOUNCEPAD_SRC_H;
    pads[0].state         = BOUNCE_IDLE;
    pads[0].anim_frame    = 2;
    pads[0].anim_timer_ms = 0;
    pads[0].launch_vy     = BOUNCEPAD_VY_MEDIUM;
    pads[0].pad_type      = BOUNCEPAD_WOOD;

    *count = 1;
}
