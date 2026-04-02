/*
 * bouncepad_medium.h — Wood bouncepad (medium jump) placement.
 *
 * Uses Bouncepad_Wood.png.  Launch impulse: BOUNCEPAD_VY_MEDIUM.
 */
#pragma once

#include "bouncepad.h"

#define MAX_BOUNCEPADS_MEDIUM  4

void bouncepads_medium_init(Bouncepad *pads, int *count);
