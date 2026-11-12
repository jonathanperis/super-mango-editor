/*
 * player_jump.h — Player jump impulse, buffering, and release-cut helpers.
 *
 * Keyboard and gamepad input both feed the same jump behavior.  This module
 * keeps coyote-time, input buffering, and short-hop release cuts in one place.
 */

#pragma once

#include "player.h"  /* Player, SoundEffect */

/* Start a jump immediately and consume coyote/buffer state. */
void player_start_jump(Player *player, SoundEffect *snd_jump);

/* Handle a fresh jump-button press from keyboard or gamepad input. */
void player_press_jump(Player *player, SoundEffect *snd_jump);

/* Handle jump-button release, including short-hop cut while rising. */
void player_release_jump(Player *player);

/*
 * A launch that is not a jump (bouncepad, knockback) takes over the
 * player's vertical speed. Drop every piece of jump state that could
 * replace or shorten it: the coyote window (a jump press would overwrite
 * vy with JUMP_VY), the buffered press, and the release cut.
 *
 * `static inline` so the collision code can call it without linking the
 * rest of player_jump.c (which plays sounds).
 */
static inline void player_cancel_jump_state(Player *player)
{
    player->coyote_timer      = 0.0f;
    player->jump_buffer_timer = 0.0f;
    player->jump_cut_armed    = 0;
}

/* Update coyote time; return nonzero when buffered jump should start. */
int player_update_jump_timers(Player *player, float dt, int was_on_ground);
