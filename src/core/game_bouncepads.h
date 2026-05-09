/*
 * game_bouncepads.h — Game-loop bouncepad helpers.
 */

#pragma once

#include "../game.h"

int game_bouncepads_collect(const GameState *gs, Bouncepad *out_pads);
void game_bouncepads_handle_hit(GameState *gs, int bounce_idx);
