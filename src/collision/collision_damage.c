/*
 * collision_damage.c — Damage application system implementation.
 *
 * Centralized damage and knockback handler used by collision detection
 * and other game systems.
 */

#include "collision_damage.h"

#include "../levels/level.h"
#include "../core/debug.h"
#include "../hazards/spike.h"  /* SPIKE_PUSH_SPEED, SPIKE_PUSH_VY */

#include <SDL_mixer.h>  /* Mix_PlayChannel */
#include <math.h>       /* sqrtf */

void apply_damage(GameState *gs, int amount, int push,
                  float src_cx, float src_cy)
{
    if (push) {
        float vx  = gs->player.vx;
        float vy  = gs->player.vy;
        float len = sqrtf(vx * vx + vy * vy);
        if (len > 1.0f) {
            gs->player.vx = -(vx / len) * SPIKE_PUSH_SPEED;
            gs->player.vy = -(vy / len) * SPIKE_PUSH_SPEED + SPIKE_PUSH_VY;
        } else {
            float dir = (gs->player.x + gs->player.w * 0.5f >= src_cx) ? 1.0f : -1.0f;
            gs->player.vx = dir * SPIKE_PUSH_SPEED;
            gs->player.vy = SPIKE_PUSH_VY;
        }
        gs->player.on_ground = 0;
    }
    (void)src_cy;   /* reserved for future vertical-push logic */

    gs->player.hurt_timer = 1.5f;
    if (gs->snd_hit) Mix_PlayChannel(-1, gs->snd_hit, 0);

    gs->hearts -= amount;
    if (gs->hearts <= 0) {
        gs->lives--;
        if (gs->lives < 0) {
            /* Game over — reset to level-defined starting values */
            const LevelDef *def = (const LevelDef *)gs->current_level;
            gs->lives           = def && def->initial_lives  > 0
                                ? def->initial_lives  : DEFAULT_LIVES;
            gs->score           = 0;
            gs->score_life_next = gs->score_per_life;
            if (gs->debug_mode) debug_log(&gs->debug, "GAME OVER - reset");
        }
        if (gs->debug_mode) debug_log(&gs->debug, "LIFE LOST lives=%d", gs->lives);
        {
            const LevelDef *def = (const LevelDef *)gs->current_level;
            gs->hearts = def && def->initial_hearts > 0
                       ? def->initial_hearts : MAX_HEARTS;
        }
        reset_current_level(gs, &gs->fp_prev_riding);
    }
}
