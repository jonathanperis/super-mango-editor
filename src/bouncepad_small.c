/*
 * bouncepad_small.c — Green bouncepad placement (small jump).
 */

#include "bouncepad_small.h"
#include "game.h"

void bouncepads_small_init(Bouncepad *pads, int *count) {
    /*
     * Green pad 0 — screen 2, right of the tall pillar at x=680.
     */
    pads[0].x             = 734.0f;
    pads[0].y             = (float)(FLOOR_Y - BOUNCEPAD_SRC_H);
    pads[0].w             = BOUNCEPAD_W;
    pads[0].h             = BOUNCEPAD_SRC_H;
    pads[0].state         = BOUNCE_IDLE;
    pads[0].anim_frame    = 2;
    pads[0].anim_timer_ms = 0;
    pads[0].launch_vy     = BOUNCEPAD_VY_SMALL;
    pads[0].pad_type      = BOUNCEPAD_GREEN;

    *count = 1;
}
