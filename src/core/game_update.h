/*
 * game_update.h — Active-frame game update pipeline.
 */

#pragma once

#include "../game_fwd.h"  /* GameState, used by pointer only */

/*
 * game_update_active — Advance the simulation by one fixed step of dt
 * seconds: the player first, then every row of the s_update_steps table
 * (game_update.c), then the camera.
 *
 * cam_x is the camera's left edge before the step. Returns it after the
 * step, for the frame that will be drawn.
 */
int game_update_active(GameState *gs, float dt, int cam_x);
