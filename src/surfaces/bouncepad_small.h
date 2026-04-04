/*
 * bouncepad_small.h — Green bouncepad (small jump) placement.
 *
 * Uses Bouncepad_Green.png.  Launch impulse: BOUNCEPAD_VY_SMALL.
 */
#pragma once

#include "bouncepad.h"

#define MAX_BOUNCEPADS_SMALL 16

void bouncepads_small_init(Bouncepad *pads, int *count);
