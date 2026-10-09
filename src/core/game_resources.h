/*
 * game_resources.h — Load and release the shared sprites and sounds.
 *
 * GameAssets (game.h) holds every texture and sound effect that does not
 * depend on the level. Its owner loads it once and unloads it once:
 * AppSession for the real game, game_init/game_cleanup for a GameState
 * started without a session. A GameState opened by the session holds a
 * copy of the session's GameAssets: the same pointers, only borrowed.
 */

#pragma once

#include "../game_assets.h"  /* GameAssets */
#include "../game_fwd.h"     /* GameState, used by pointer only */
#include "../levels/level.h"

/* The shared default floor tileset (assets->textures.floor_tile). A level
 * whose floor_tile_path is empty or names this file draws that copy. */
#define DEFAULT_FLOOR_TILE_PATH "assets/sprites/levels/grass_tileset.png"

/* Fill every slot of *assets; -1 (after saying which file) when a required
 * sprite is missing. Call game_resources_unload afterwards either way. */
int game_resources_load(GameAssets *assets);

/* Release every slot in reverse load order and clear it. Safe on a zeroed
 * or partly loaded GameAssets. */
void game_resources_unload(GameAssets *assets);

/* 1 when *assets holds a loaded set. The default floor tileset is a
 * required texture, so every successful game_resources_load sets it. */
int game_resources_loaded(const GameAssets *assets);

/* Verify shared sprites before committing a level/phase. */
int game_resources_require_level_textures(const GameState *gs, const LevelDef *def);
