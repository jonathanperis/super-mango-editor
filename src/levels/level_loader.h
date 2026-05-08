/*
 * level_loader.h — Public interface for loading and resetting levels.
 *
 * level_load  : Populate all GameState entity arrays from a LevelDef.
 *               Called once from game_init (game startup).
 *
 * level_reset : Re-populate all mutable entity arrays and reset the player.
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

/* Load all entities defined in def into gs.  Builds rails, then all entities. */
void level_load(GameState *gs, const LevelDef *def);

/*
 * Reset all mutable entities (enemies, collectibles, hazards) to their initial
 * placement state.  Static geometry (platforms, rails, sea gaps) is preserved.
 * Also resets the player to the spawn position.
 */
void level_reset(GameState *gs, const LevelDef *def);
