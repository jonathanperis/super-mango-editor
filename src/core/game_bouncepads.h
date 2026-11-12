/*
 * game_bouncepads.h — Game-loop bouncepad helpers.
 */

#pragma once

#include "../game.h"

/*
 * The three pad arrays as views for player_update, always in this order.
 * The numbers are the list a PlayerLanding names, so the two functions
 * below agree on which array "list 1" is without counting anything.
 */
enum {
    GAME_BOUNCEPAD_LIST_MEDIUM = 0,
    GAME_BOUNCEPAD_LIST_SMALL  = 1,
    GAME_BOUNCEPAD_LIST_HIGH   = 2,
    GAME_BOUNCEPAD_LIST_COUNT  = 3
};
void game_bouncepads_lists(const GameState *gs,
                           BouncepadList lists[GAME_BOUNCEPAD_LIST_COUNT]);

/* Start the squash animation and sound for pad `index` of list `list` (one
 * of GAME_BOUNCEPAD_LIST_*). A pair that names no pad (-1, or out of
 * range) does nothing. */
void game_bouncepads_handle_hit(GameState *gs, int list, int index);
