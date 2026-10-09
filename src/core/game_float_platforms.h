/*
 * game_float_platforms.h — Game-loop floating platform helper.
 */

#pragma once

#include "../game_fwd.h"  /* GameState, used by pointer only */

void game_float_platforms_update(GameState *gs, float dt, int fp_landed_idx);
