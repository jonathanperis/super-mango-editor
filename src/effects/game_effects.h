/*
 * game_effects.h — Game-loop effect update helper.
 */

#pragma once

#include "../game_fwd.h"  /* GameState, used by pointer only */

void game_effects_update(GameState *gs, float dt);
