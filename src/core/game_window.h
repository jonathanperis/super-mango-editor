/*
 * game_window.h — SDL window and renderer lifecycle helpers.
 */

#pragma once

#include "../game.h"

/* Create the game window and renderer, then set the logical canvas size. */
int game_window_init(GameState *gs);

/* Destroy renderer and window resources owned by GameState. */
void game_window_cleanup(GameState *gs);
