/*
 * game_camera.h — Camera follow and clamp helpers.
 */

#pragma once

#include "../game.h"

/* Ease the camera toward the player (plus lookahead) for one step and
 * return its left edge as a whole pixel. */
int game_camera_update(GameState *gs, float dt);

/*
 * Jump the camera straight to where it wants to be for the player's current
 * position, with no easing. Used whenever the player is placed rather
 * than moved: at level start (game_init), after a phase transition or an
 * experiment restart, and after a respawn or Retry. Easing from wherever
 * the camera was would visibly pan the whole way across the level.
 */
void game_camera_snap(GameState *gs);
