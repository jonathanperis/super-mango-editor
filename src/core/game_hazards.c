/*
 * game_hazards.c — Per-frame hazard animation updates.
 */

#include "game_hazards.h"

#include "../hazards/axe_trap.h"
#include "../hazards/blue_flame.h"
#include "../hazards/circular_saw.h"

void game_hazards_update(GameState *gs, float dt, int cam_x)
{
    float player_cx = gs->world.player.x + gs->world.player.w / 2.0f;

    axe_traps_update(gs->world.axe_traps, gs->world.axe_trap_count, dt,
                     gs->assets.audio.axe, player_cx, cam_x);
    circular_saws_update(gs->world.circular_saws, gs->world.circular_saw_count, dt);
    blue_flames_update(gs->world.blue_flames, gs->world.blue_flame_count, dt);
    blue_flames_update(gs->world.fire_flames, gs->world.fire_flame_count, dt);
}
