/*
 * level_resources.h — Load and release the files a level names
 * (background, floor tileset, water strip, fog, music), owned by gs->world.
 */
#pragma once

#include "level.h"
#include "../game.h"

/* Apply level-specific background, floor, foreground, fog, and music assets. */
void level_resources_apply(GameState *gs, const LevelDef *def);

/* Release them all (and the platform tiles); called once from game_cleanup. */
void level_resources_cleanup(GameState *gs);
