/*
 * player_animation.h — Player sprite animation state machine.
 *
 * The physics/update path resolves position, velocity, ground contact, and
 * climb state first.  This module then chooses the visible animation row,
 * advances frame timing, and updates Player.frame to point at the sprite-sheet
 * cell that should be rendered.
 */

#pragma once

#include "player.h"  /* Player */

/*
 * player_animate — Choose and advance the player's current animation.
 *
 * dt_ms is elapsed frame time in milliseconds.  The function derives the
 * target AnimState from Player velocity and contact flags, resets frame index
 * on state changes, advances the frame timer, and writes Player.frame.
 */
void player_animate(Player *player, Uint32 dt_ms);
