/*
 * sandbox.h — Public interface for the Sandbox level/phase.
 *
 * The sandbox is the main gameplay phase containing all entity placements,
 * sea gap definitions, and level layout.  It is separated from the game
 * engine (game.c) so the engine can host multiple phases in the future.
 *
 * sandbox_load_level  — called once from game_init after textures are loaded.
 * sandbox_reset_level — called on player death to reinitialise all entities.
 */
#pragma once

#include "game.h"

/* Place all entities, define sea gaps, and set up the sandbox level. */
void sandbox_load_level(GameState *gs);

/*
 * Reset all entities to their initial state after a player death.
 * fp_prev_riding is the float-platform ride index (reset to -1).
 */
void sandbox_reset_level(GameState *gs, int *fp_prev_riding);
