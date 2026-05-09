/*
 * game_effects.c — Per-frame level effect updates.
 */

#include "game_effects.h"

#include "fog.h"
#include "water.h"

void game_effects_update(GameState *gs, float dt)
{
    if (gs->runtime.water_enabled) water_update(&gs->water, dt);
    if (gs->runtime.fog_enabled) fog_update(&gs->fog, dt);
}
