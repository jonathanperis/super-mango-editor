/*
 * player_surfaces.h — Player surface collision helpers.
 */

#pragma once

#include "player.h"  /* Player, Bouncepad */

/* Resolve floor landing and floor-level bouncepad launches. */
void player_resolve_floor_collision(Player *player,
                                    const Bouncepad *bouncepads, int bouncepad_count,
                                    const int *floor_gaps, int floor_gap_count,
                                    int *out_bounce_idx);
