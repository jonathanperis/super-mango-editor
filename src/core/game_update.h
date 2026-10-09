/*
 * game_update.h — Active-frame game update pipeline.
 */

#pragma once

#include "../game_fwd.h"  /* GameState, used by pointer only */

int game_update_active(GameState *gs, float dt, int cam_x);
