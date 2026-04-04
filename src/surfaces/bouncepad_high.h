/*
 * bouncepad_high.h — Red bouncepad (high jump) placement.
 *
 * Uses Bouncepad_Red.png.  Launch impulse: BOUNCEPAD_VY_HIGH.
 */
#pragma once

#include "bouncepad.h"

#define MAX_BOUNCEPADS_HIGH 16

void bouncepads_high_init(Bouncepad *pads, int *count);
