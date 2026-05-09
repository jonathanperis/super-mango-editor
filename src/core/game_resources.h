/*
 * game_resources.h — Game texture/audio resource lifecycle.
 */

#pragma once

#include "../game.h"

void game_resources_load(GameState *gs);
void game_resources_cleanup(GameState *gs);
