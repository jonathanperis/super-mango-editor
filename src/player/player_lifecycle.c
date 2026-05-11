/*
 * player_lifecycle.c — Player rendering, reset, hitbox, and cleanup helpers.
 */

#include "player.h"
#include "player_internal.h"
#include "../game.h"  /* TILE_SIZE — respawn centering on level spawn tile */

/*
 * player_render — Draw the player sprite at its current position.
 *
 * While hurt_timer > 0 the sprite blinks on/off every 100 ms to give visual
 * feedback that the player was hit and is temporarily invincible.
 */
void player_render(Player *player, SDL_Renderer *renderer, int cam_x)
{
    if (player->hurt_timer > 0.0f) {
        int interval = (int)(player->hurt_timer * 1000.0f) / 100;
        if (interval % 2 != 0) return;
    }

    /* Cast float world position to integer screen pixels at render time. */
    SDL_Rect dst = {
        .x = (int)player->x - cam_x,
        .y = (int)player->y,
        .w = player->w,
        .h = player->h
    };

    SDL_RendererFlip flip = player->facing_left ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
    SDL_RenderCopyEx(renderer, player->texture, &player->frame, &dst,
                     0.0, NULL, flip);
}

/*
 * player_get_hitbox — Return the player's inset physics hitbox.
 *
 * The hitbox trims transparent sprite padding so enemy and hazard collision
 * matches the visible character instead of the full 48×48 frame.
 */
SDL_Rect player_get_hitbox(const Player *player)
{
    SDL_Rect r;
    r.x = (int)(player->x) + PLAYER_PHYS_PAD_X;
    r.y = (int)(player->y) + PLAYER_PHYS_PAD_TOP;
    r.w = player->w - 2 * PLAYER_PHYS_PAD_X;
    r.h = player->h - PLAYER_PHYS_PAD_TOP - PLAYER_FLOOR_SINK;
    return r;
}

/*
 * player_reset — Reset position, velocity, and animation without reloading.
 *
 * Called when the player loses all hearts and spends a life.  Texture and
 * tunable physics fields stay intact because they were configured by init and
 * level_load().
 */
void player_reset(Player *player)
{
    player->x = player->spawn_x + (TILE_SIZE - player->w) / 2.0f;
    player->y = player->spawn_y - player->h + PLAYER_FLOOR_SINK;
    player->vx = 0.0f;
    player->vy = 0.0f;
    player->on_ground = 1;

    player->anim_state = ANIM_IDLE;
    player->anim_frame_index = 0;
    player->anim_timer_ms = 0;
    player->facing_left = 0;
    player->on_vine = 0;
    player->vine_index = 0;
    player->climb_source = 0;
    player->jump_held = 0;
    player->coyote_timer = PLAYER_COYOTE_TIME;
    player->jump_buffer_timer = 0.0f;
    player->move_dir = 0;
    player->is_running = 0;
    player->air_is_running = 0;
    player->hurt_timer = 0.0f;

    player->frame.x = 0;
    player->frame.y = 0;
}

/*
 * player_cleanup — Release GPU memory held by the player's texture.
 *
 * Must run before the renderer is destroyed, because SDL_Texture objects are
 * owned by the renderer that created them.
 */
void player_cleanup(Player *player)
{
    if (player->texture) {
        SDL_DestroyTexture(player->texture);
        player->texture = NULL;
    }
}
