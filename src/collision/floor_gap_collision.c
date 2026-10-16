/*
 * floor_gap_collision.c — Detect and apply floor-gap life loss.
 */

#include "floor_gap_collision.h"
#include "../game.h"  /* GameState: this file reads its fields */

#include "../shared/audio.h"
#include "../shared/geometry.h"  /* span_find */

#include "collision_damage.h"
#include "../effects/water.h"

void floor_gap_handle_collision(GameState *gs)
{
    float pcx = gs->world.player.x + gs->world.player.w / 2.0f;
    float pcy = gs->world.player.y + gs->world.player.h / 2.0f;

    /* The depth test does not depend on the gap, so the first gap under the
     * centre decides, as it did when this was one loop. */
    int g = span_find(pcx, gs->world.floor_gaps, gs->world.floor_gap_count, FLOOR_GAP_W);
    if (g >= 0 && pcy > (float)(GAME_H - WATER_ART_H)) {
        if (gs->screen.debug_mode) debug_log(&gs->screen.debug, "HIT floor gap[%d]", g);
        sound_play(gs->assets.audio.dive, 128);
        apply_damage(gs, gs->world.hearts, 0, 0.0f, 0.0f);
    }
}
