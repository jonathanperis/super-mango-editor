/*
 * level_resources.h — Get and release the files a level names (background,
 * floor tileset, water strip, fog, music). gs->world holds them from the
 * asset cache, gs->cache.
 */
#pragma once

#include "level.h"
#include "../game_fwd.h"  /* GameState, used by pointer only */

/* Apply level-specific background, floor, foreground, fog, and music assets. */
void level_resources_apply(GameState *gs, const LevelDef *def);

/* Release them all (and the platform tiles); called once from game_cleanup. */
void level_resources_cleanup(GameState *gs);
