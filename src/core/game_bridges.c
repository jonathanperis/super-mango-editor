/*
 * game_bridges.c — Bridge landing detection and crumble update.
 */

#include "game_bridges.h"

#include "../player/player.h"

static int game_bridge_find_landing(const GameState *gs, float player_cx,
                                    const SDL_Rect *player_hitbox)
{
    int player_bottom = player_hitbox->y + player_hitbox->h;

    for (int i = 0; i < gs->bridge_count; i++) {
        const Bridge *br = &gs->bridges[i];
        SDL_Rect brect = bridge_get_rect(br);

        if (player_bottom >= brect.y && player_bottom <= brect.y + 4 &&
            player_hitbox->x + player_hitbox->w > brect.x &&
            player_hitbox->x < brect.x + brect.w &&
            bridge_has_solid_at(br, player_cx)) {
            return i;
        }
    }

    return -1;
}

static void game_bridge_log_touch(GameState *gs, int bridge_landed_idx,
                                  float player_cx)
{
    if (bridge_landed_idx < 0 || !gs->debug_mode) return;

    const Bridge *br = &gs->bridges[bridge_landed_idx];
    int idx = (int)((player_cx - br->x) / BRIDGE_TILE_W);

    if (idx >= 0 && idx < br->brick_count) {
        const BridgeBrick *bk = &br->bricks[idx];
        if (bk->active && !bk->falling && bk->fall_delay < 0.0f) {
            debug_log(&gs->debug, "BRIDGE brick[%d] touched", idx);
        }
    }
}

void game_bridges_update(GameState *gs, float dt)
{
    float player_cx = gs->player.x + gs->player.w / 2.0f;
    int bridge_landed_idx = -1;

    if (gs->player.on_ground) {
        SDL_Rect player_hitbox = player_get_hitbox(&gs->player);
        bridge_landed_idx = game_bridge_find_landing(gs, player_cx,
                                                     &player_hitbox);
    }

    game_bridge_log_touch(gs, bridge_landed_idx, player_cx);
    bridges_update(gs->bridges, gs->bridge_count, dt, bridge_landed_idx,
                   player_cx);
}
