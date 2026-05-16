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

void game_restart_after_game_over(GameState *gs)
{
    const LevelDef *def;

    if (!gs || !gs->game_over) return;

    def = (const LevelDef *)gs->runtime.current_level;
    gs->game_over = 0;
    gs->completion.complete = 0;
    gs->pause_reasons = 0;
    gs->paused = 0;
    gs->lives = def && def->initial_lives > 0 ? def->initial_lives : DEFAULT_LIVES;
    gs->hearts = def && def->initial_hearts > 0 ? def->initial_hearts : MAX_HEARTS;
    gs->score = 0;
    gs->checkpoint_x = 0.0f;
    if (def && (def->player_start_x != 0.0f || def->player_start_y != 0.0f)) {
        gs->player.spawn_x = def->player_start_x;
        gs->player.spawn_y = def->player_start_y;
    } else {
        gs->player.spawn_x = 80.0f;
        gs->player.spawn_y = (float)(FLOOR_Y - 2 * TILE_SIZE + 16);
    }
    gs->score_life_next = gs->rules.score_per_life;
    reset_current_level(gs, &gs->loop.fp_prev_riding);
}

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
    if (gs->audio.hit) Mix_PlayChannel(-1, gs->audio.hit, 0);

    gs->hearts -= amount;
    if (gs->hearts <= 0) {
        gs->lives--;
        if (gs->lives < 0) {
            gs->game_over = 1;
            gs->pause_reasons = 0;
            gs->paused = 0;
            if (gs->debug_mode) debug_log(&gs->debug, "GAME OVER");
            return;
        }
        if (gs->debug_mode) debug_log(&gs->debug, "LIFE LOST lives=%d", gs->lives);
        {
            const LevelDef *def = (const LevelDef *)gs->runtime.current_level;
            gs->hearts = def && def->initial_hearts > 0
                       ? def->initial_hearts : MAX_HEARTS;
        }
        reset_current_level(gs, &gs->loop.fp_prev_riding);
    }
}
