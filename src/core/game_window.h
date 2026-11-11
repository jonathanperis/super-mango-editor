/*
 * game_window.h — Screen-owned raylib render-target lifecycle.
 */

#pragma once

#include "../game_fwd.h"  /* GameState, used by pointer only */

/* Create the logical canvas in the AppSession-owned graphics context. */
int game_window_init(GameState *gs);

/* Before drawing a frame: make the canvas as sharp as the window shows it
 * (see game_window.c). Returns 0 when a new canvas could not be made; the
 * old one is then still in place. */
int game_window_fit(GameState *gs);

/* Release the logical canvas owned by GameState. */
void game_window_cleanup(GameState *gs);
