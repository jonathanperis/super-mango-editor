/*
 * game_bouncepads.h — Game-loop bouncepad helpers.
 */

#pragma once

#include "../game.h"

/* The three pad arrays (medium, small, high) as views for player_update. */
#define GAME_BOUNCEPAD_LIST_COUNT 3
void game_bouncepads_lists(const GameState *gs,
                           BouncepadList lists[GAME_BOUNCEPAD_LIST_COUNT]);

/* Start the squash animation and sound for the pad at a flat list index. */
void game_bouncepads_handle_hit(GameState *gs, int bounce_idx);
void game_bouncepads_update_animations(GameState *gs, float dt);
