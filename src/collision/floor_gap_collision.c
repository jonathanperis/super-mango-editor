/*
 * floor_gap_collision.c — Detect and apply floor-gap life loss.
 */

#include "floor_gap_collision.h"

#include <SDL_mixer.h>

#include "collision_damage.h"
#include "../effects/water.h"

void floor_gap_handle_collision(GameState *gs)
{
    float pcx = gs->player.x + gs->player.w / 2.0f;
    float pcy = gs->player.y + gs->player.h / 2.0f;

    for (int g = 0; g < gs->floor_gap_count; g++) {
        float gx = (float)gs->floor_gaps[g];
        if (pcx >= gx && pcx < gx + (float)FLOOR_GAP_W &&
            pcy > (float)(GAME_H - WATER_ART_H)) {
            if (gs->debug_mode) debug_log(&gs->debug, "HIT floor gap[%d]", g);
            if (gs->audio.dive) Mix_PlayChannel(-1, gs->audio.dive, 0);
            apply_damage(gs, gs->hearts, 0, 0.0f, 0.0f);
            break;
        }
    }
}
