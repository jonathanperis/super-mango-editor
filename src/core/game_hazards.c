/*
 * game_hazards.c — Per-frame hazard animation updates.
 */

#include "game_hazards.h"

#include "../hazards/axe_trap.h"
#include "../hazards/blue_flame.h"
#include "../hazards/circular_saw.h"

void game_hazards_update(GameState *gs, float dt, int cam_x)
{
    float player_cx = gs->player.x + gs->player.w / 2.0f;

    axe_traps_update(gs->axe_traps, gs->axe_trap_count, dt,
                     gs->audio.axe, player_cx, cam_x);
    circular_saws_update(gs->circular_saws, gs->circular_saw_count, dt);
    blue_flames_update(gs->blue_flames, gs->blue_flame_count, dt);
    blue_flames_update(gs->fire_flames, gs->fire_flame_count, dt);
}
