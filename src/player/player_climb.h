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

/*
 * player_vine_grab_rect — Return the padded grab zone for one vine.
 *
 * The rectangle spans the full vine height and extends a small horizontal
 * grace pad around the visual sprite so climbing grabs feel forgiving.
 */
SDL_Rect player_vine_grab_rect(const VineDecor *v);

/*
 * player_ladder_grab_rect — Return the padded grab zone for one ladder.
 *
 * Uses the same horizontal grace pad as vines so ladder grabs feel identical
 * even though the ladder sprite has its own width and step constants.
 */
SDL_Rect player_ladder_grab_rect(const LadderDecor *ld);

/*
 * player_rope_grab_rect — Return the padded grab zone for one rope.
 *
 * Uses the rope tile count and sprite dimensions to build the full climbable
 * region checked by keyboard and gamepad input.
 */
SDL_Rect player_rope_grab_rect(const RopeDecor *rp);

/*
 * player_try_grab_climbable — Enter climbing mode if the player overlaps one.
 *
 * Checks vines, then ladders, then ropes using the current player hitbox.  On
 * success this sets on_vine, vine_index, climb_source, clears ground contact,
 * and zeroes velocity so input can drive climbing movement immediately.
 */
int player_try_grab_climbable(Player *player,
                              const VineDecor *vines, int vine_count,
                              const LadderDecor *ladders, int ladder_count,
                              const RopeDecor *ropes, int rope_count);

/*
 * player_climb_get_bounds — Return geometry for the active climbable.
 *
 * player->climb_source selects vines, ladders, or ropes; player->vine_index
 * selects the active item inside that array.  Outputs the grab rect plus top
 * and bottom y bounds used to clamp or detach while climbing.
 */
void player_climb_get_bounds(const Player *player,
                             const VineDecor *vines,
                             const LadderDecor *ladders,
                             const RopeDecor *ropes,
                             SDL_Rect *out_grab, float *out_top,
                             float *out_bottom);
