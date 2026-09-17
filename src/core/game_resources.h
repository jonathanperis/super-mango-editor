/*
 * game_resources.h — Game texture/audio resource lifecycle.
 */

#pragma once

#include "../game.h"
#include "../levels/level.h"

int game_resources_load(GameState *gs);
/* Verify shared sprites before committing a level/phase. */
int game_resources_require_level_textures(const GameState *gs, const LevelDef *def);
void game_resources_cleanup(GameState *gs);
