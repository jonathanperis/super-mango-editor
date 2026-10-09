/*
 * game_actors.h — Game-loop actor movement helper.
 */

#pragma once

#include "../game_fwd.h"  /* GameState, used by pointer only */

void game_actors_update(GameState *gs, float dt, int cam_x);
