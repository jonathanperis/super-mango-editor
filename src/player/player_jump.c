/*
 * player_jump.c — Player jump impulse, buffering, and release-cut helpers.
 */

#include "player_jump.h"

#include "../shared/audio.h"

#include "player_internal.h"

/* Jump feel helpers. Coyote time gives a tiny grace window after leaving
 * ground; jump cut shortens ascent when the jump button is released early.
 * The jump impulse itself, JUMP_VY, is in player.h. */
#define JUMP_BUFFER_TIME        0.10f
#define JUMP_CUT_FACTOR         0.45f

void player_start_jump(Player *player, SoundEffect *snd_jump) {
    player->vy                = JUMP_VY;
    player->on_ground         = 0;
    player->jump_held         = 1;
    player->jump_cut_armed    = 1;
    player->coyote_timer      = 0.0f;
    player->jump_buffer_timer = 0.0f;
    sound_play(snd_jump, 128);
}

void player_press_jump(Player *player, SoundEffect *snd_jump) {
    player->jump_buffer_timer = JUMP_BUFFER_TIME;
    if (player->on_ground || player->coyote_timer > 0.0f) {
        player_start_jump(player, snd_jump);
    } else {
        player->jump_held = 1;
    }
}

/*
 * Releasing jump shortens a rise only when that rise came from a jump
 * (jump_cut_armed), and only once. jump_held alone is not enough: it is
 * also set by a press in mid-air, and a bouncepad launch or a knockback
 * must keep its full height whatever the jump key does.
 */
void player_release_jump(Player *player) {
    if (player->jump_cut_armed && player->vy < 0.0f) {
        player->vy *= JUMP_CUT_FACTOR;
    }
    player->jump_cut_armed = 0;
    player->jump_held = 0;
}

int player_update_jump_timers(Player *player, float dt, int was_on_ground) {
    /* Past the top of the jump there is no rise left to shorten. */
    if (player->vy >= 0.0f) player->jump_cut_armed = 0;

    if (player->on_ground || (was_on_ground && player->vy >= 0.0f)) {
        player->coyote_timer = COYOTE_TIME;
    } else if (player->coyote_timer > 0.0f) {
        player->coyote_timer -= dt;
        if (player->coyote_timer < 0.0f) player->coyote_timer = 0.0f;
    }

    const int landed_this_frame = !was_on_ground && player->on_ground;
    if (player->jump_buffer_timer > 0.0f && landed_this_frame) {
        return 1;
    } else if (player->jump_buffer_timer > 0.0f) {
        player->jump_buffer_timer -= dt;
        if (player->jump_buffer_timer < 0.0f) player->jump_buffer_timer = 0.0f;
    }

    return 0;
}
