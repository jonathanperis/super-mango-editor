/*
 * level_session.h — Active level load and phase transition helpers.
 */

#pragma once

#include "../game.h"

/* Load the startup level from GameState::level_path or fall back to an empty level. */
void game_level_load_initial(GameState *gs);

/* Free active level storage owned by GameState. */
void game_level_session_cleanup(GameState *gs);
