/*
 * level_resources.h — Runtime resource reloads driven by LevelDef metadata.
 */
#pragma once

#include "level.h"
#include "../game.h"

/* Apply level-specific background, floor, foreground, fog, and music assets. */
void level_resources_apply(GameState *gs, const LevelDef *def);
