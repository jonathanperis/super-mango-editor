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

/* Resolve one-way static and float platform landings. */
void player_resolve_platform_collisions(Player *player,
                                        const Platform *platforms, int platform_count,
                                        const FloatPlatform *float_platforms,
                                        int float_platform_count,
                                        float prev_bottom,
                                        int *out_fp_landed_idx,
                                        int prev_fp_landed_idx);
