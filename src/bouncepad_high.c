/*
 * bouncepad_high.c — Red bouncepad placement (high jump).
 */

#include "bouncepad_high.h"
#include "game.h"

void bouncepads_high_init(Bouncepad *pads, int *count) {
    /*
     * Red pad 0 — screen 4, left of the last tall pillar at x=1480.
     */
    pads[0].x             = 1420.0f;
    pads[0].y             = (float)(FLOOR_Y - BOUNCEPAD_SRC_H);
    pads[0].w             = BOUNCEPAD_W;
    pads[0].h             = BOUNCEPAD_SRC_H;
    pads[0].state         = BOUNCE_IDLE;
    pads[0].anim_frame    = 2;
    pads[0].anim_timer_ms = 0;
    pads[0].launch_vy     = BOUNCEPAD_VY_HIGH;
    pads[0].pad_type      = BOUNCEPAD_RED;

    *count = 1;
}
