/*
 * game_bridges.c — Bridge crumble update.
 *
 * Which bridge the player stands on is decided once, by the landing test in
 * player_resolve_bridge_collision (player_surfaces.c), and handed in here as
 * bridge_landed_idx. Guessing it again from the player's position would be a
 * second rule that could disagree with the first, for example crumbling a
 * bridge under a player who is really standing on a platform beside it.
 */

#include "game_bridges.h"

void game_bridges_update(GameState *gs, float dt, int bridge_landed_idx)
{
    float player_cx = gs->world.player.x + gs->world.player.w / 2.0f;

    if (bridge_landed_idx < 0 || bridge_landed_idx >= gs->world.bridge_count) {
        bridge_landed_idx = -1;
    }

    int touched = bridges_update(gs->world.bridges, gs->world.bridge_count, dt,
                                 bridge_landed_idx, player_cx);
    if (touched >= 0 && gs->screen.debug_mode) {
        debug_log(&gs->screen.debug, "BRIDGE brick[%d] touched", touched);
    }
}
