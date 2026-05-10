/*
 * game_update.h — Active-frame game update pipeline.
 */

#pragma once

#include "../game.h"

int game_update_active(GameState *gs, float dt, int cam_x);
