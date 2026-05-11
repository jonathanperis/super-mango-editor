/*
 * player_climb.h — Shared climbable grab-zone helpers for the player module.
 *
 * Vines, ladders, and ropes all use the same "grab zone" concept: an
 * axis-aligned rectangle around the visible sprite that lets the player enter
 * or remain in climbing mode.  The main player input/update code uses these
 * helpers for keyboard and gamepad climbing checks.
 */

#pragma once

#include <SDL.h>      /* SDL_Rect */

#include "player.h"  /* Player, VineDecor, LadderDecor, RopeDecor */

/* Return the padded grab zone for each climbable type. */
SDL_Rect player_vine_grab_rect(const VineDecor *v);
SDL_Rect player_ladder_grab_rect(const LadderDecor *ld);
SDL_Rect player_rope_grab_rect(const RopeDecor *rp);

/* Return grab rect and vertical bounds for the player's active climbable. */
void player_climb_get_bounds(const Player *player,
                             const VineDecor *vines,
                             const LadderDecor *ladders,
                             const RopeDecor *ropes,
                             SDL_Rect *out_grab, float *out_top,
                             float *out_bottom);
