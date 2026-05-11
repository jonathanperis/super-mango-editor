/*
 * player_jump.c — Player jump impulse, buffering, and release-cut helpers.
 */

#include "player_jump.h"

#include <SDL_mixer.h>  /* Mix_PlayChannel */

/* Jump feel helpers. Coyote time gives a tiny grace window after leaving
 * ground; jump cut shortens ascent when the jump button is released early. */
#define JUMP_VY              -325.0f
#define JUMP_BUFFER_TIME        0.10f
#define JUMP_CUT_FACTOR         0.45f

void player_start_jump(Player *player, Mix_Chunk *snd_jump) {
    player->vy                = JUMP_VY;
    player->on_ground         = 0;
    player->jump_held         = 1;
    player->coyote_timer      = 0.0f;
    player->jump_buffer_timer = 0.0f;
    if (snd_jump) Mix_PlayChannel(-1, snd_jump, 0);
}

void player_press_jump(Player *player, Mix_Chunk *snd_jump) {
    player->jump_buffer_timer = JUMP_BUFFER_TIME;
    if (player->on_ground || player->coyote_timer > 0.0f) {
        player_start_jump(player, snd_jump);
    } else {
        player->jump_held = 1;
    }
}

void player_release_jump(Player *player) {
    if (player->jump_held && player->vy < 0.0f) {
        player->vy *= JUMP_CUT_FACTOR;
    }
    player->jump_held = 0;
}
