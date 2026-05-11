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

/* Resolve one-way bridge landings. */
void player_resolve_bridge_collision(Player *player,
                                     const Bridge *bridges, int bridge_count,
                                     float prev_bottom);

/* Resolve one-way spike-platform top landings. */
void player_resolve_spike_platform_top_collision(Player *player,
                                                 const SpikePlatform *spike_platforms,
                                                 int spike_platform_count,
                                                 float prev_bottom);

/* Resolve spike-platform underside ceiling collisions. */
void player_resolve_spike_platform_ceiling_collision(Player *player,
                                                     const SpikePlatform *spike_platforms,
                                                     int spike_platform_count,
                                                     float prev_top);
