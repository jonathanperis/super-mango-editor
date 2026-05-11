/*
 * player_jump.h — Player jump impulse, buffering, and release-cut helpers.
 *
 * Keyboard and gamepad input both feed the same jump behavior.  This module
 * keeps coyote-time, input buffering, and short-hop release cuts in one place.
 */

#pragma once

#include "player.h"  /* Player, Mix_Chunk */

/* Start a jump immediately and consume coyote/buffer state. */
void player_start_jump(Player *player, Mix_Chunk *snd_jump);

/* Handle a fresh jump-button press from keyboard or gamepad input. */
void player_press_jump(Player *player, Mix_Chunk *snd_jump);

/* Handle jump-button release, including short-hop cut while rising. */
void player_release_jump(Player *player);
