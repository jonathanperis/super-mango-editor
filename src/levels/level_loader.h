/*
 * level_loader.h — Public interface for loading and resetting levels.
 *
 * level_load  : Validate a LevelDef, then populate all GameState entity
 *               arrays from it.
 * level_apply : The populate step alone, for a LevelDef that
 *               level_load_toml already validated (game start, Next Level,
 *               experiment restart). It cannot fail.
 *
 * level_reset : Re-populate mutable entity arrays and reset the player.
 *               Collected coins stay collected (no score farming by dying).
 *               Called from reset_current_level (player death / level retry).
 *               Geometry (platforms, rails) and sea gaps are not re-applied
 *               because they never change during a play session.
 */
#pragma once

#include <stddef.h>     /* size_t */

#include "level.h"      /* LevelDef */
#include "../game.h"    /* GameState */

/* Validate LevelDef count fields before fixed-size GameState array copies. */
int level_validate_counts(const LevelDef *def, char *err, size_t err_size);

/* Validate links and per-entity dimensions that can overrun nested arrays. */
int level_validate_runtime(const LevelDef *def, char *err, size_t err_size);

/* Validate def, then load all its entities into gs (builds rails first).
 * Returns -1 without touching gs when def is invalid. */
int level_load(GameState *gs, const LevelDef *def);

/* Load a def that already passed level_validate_runtime (level_load_toml
 * output). It cannot fail, so a level switch has no failure point after the
 * old level is replaced. def must outlive its use as the active level. */
void level_apply(GameState *gs, const LevelDef *def);

/*
 * Reset mutable entities (enemies, stars, hazards, surfaces) to their initial
 * placement state.  Static geometry (platforms, rails, sea gaps) and the
 * coins already collected during this attempt are preserved.
 * Also resets the player to the spawn position.
 */
void level_reset(GameState *gs, const LevelDef *def);

/*
 * Unload the platform tile textures the level loader shares between
 * platforms (GameState.world.platform_tiles) and clear every borrowed pointer.
 * Called once from game cleanup; a new level_load keeps the ones it reuses.
 */
void level_release_platform_tiles(GameState *gs);

#ifdef MANGO_TESTING
/* Test builds only: how many platform tile images have been loaded so far. */
int level_loader_test_tile_loads(void);
#endif
