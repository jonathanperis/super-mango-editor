/*
 * game_window.h — Screen-owned raylib render-target lifecycle.
 */

#pragma once

#include "../game.h"

/* Create the logical canvas in the AppSession-owned graphics context. */
int game_window_init(GameState *gs);

/* Release the logical canvas owned by GameState. */
void game_window_cleanup(GameState *gs);
