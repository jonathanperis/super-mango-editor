/*
 * game_bridges.h — Game-loop bridge update helper.
 */

#pragma once

#include "../game.h"

/*
 * Advance every bridge's crumble state. bridge_landed_idx is the bridge the
 * player landed on this step (from game_player_step), or -1.
 */
void game_bridges_update(GameState *gs, float dt, int bridge_landed_idx);
