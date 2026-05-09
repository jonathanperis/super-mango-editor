/*
 * game_checkpoint.c — Save respawn checkpoints as player reaches new screens.
 */

#include "game_checkpoint.h"

void game_checkpoint_update(GameState *gs)
{
    int current_screen = (int)(gs->player.x / GAME_W);
    float new_checkpoint = current_screen * GAME_W;

    if (new_checkpoint > gs->checkpoint_x) {
        gs->checkpoint_x = new_checkpoint;
        if (gs->debug_mode) {
            debug_log(&gs->debug, "CHECKPOINT saved at x=%.0f", gs->checkpoint_x);
        }
    }
}
