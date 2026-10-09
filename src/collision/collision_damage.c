/*
 * collision_damage.c — Damage application system implementation.
 *
 * Centralized damage and knockback handler used by collision detection
 * and other game systems.
 */

#include "collision_damage.h"

#include "../levels/level.h"
#include "../core/debug.h"
#include "../core/game_checkpoint.h"
#include "../core/game_ghost.h"
#include "../hazards/spike_block.h"  /* SPIKE_PUSH_SPEED, SPIKE_PUSH_VY */

#include "../shared/audio.h"
#include <math.h>       /* sqrtf */

void game_restart_after_game_over(GameState *gs)
{
    const LevelDef *def;

    if (!gs || !gs->screen.game_over) return;

    def = (const LevelDef *)gs->world.runtime.current_level;
    gs->screen.game_over = 0;
    gs->screen.completion.complete = 0;
    gs->screen.completion.level_elapsed = 0.0f;
    gs->screen.pause_reasons = 0;
    gs->screen.paused = 0;
    gs->world.lives = def && def->initial_lives > 0 ? def->initial_lives : DEFAULT_LIVES;
    gs->world.hearts = def && def->initial_hearts > 0 ? def->initial_hearts : MAX_HEARTS;
    gs->world.score = 0;
    gs->world.level_score_start = 0;
    level_effective_spawn(def, &gs->world.respawn_x, &gs->world.respawn_y);
    gs->world.checkpoint_index = -1;
    game_checkpoint_feedback_set(gs, CHECKPOINT_FEEDBACK_NONE, 0, 0);
    gs->world.legacy_checkpoint_screen = 0;
    gs->world.player.spawn_x = gs->world.respawn_x;
    gs->world.player.spawn_y = gs->world.respawn_y;
    gs->world.score_life_next = gs->world.rules.score_per_life;
    /* Retry is a fresh attempt with score 0, so every coin returns. A life
     * loss (reset_current_level alone) keeps collected coins gone. */
    for (int i = 0; i < gs->world.coin_count; i++) gs->world.coins[i].active = 1;
    /* A fresh attempt from the level start: no longer a continued run, and
     * the time-trial recording (and the ghost race) start over with it. */
    gs->screen.resumed = 0;
    game_ghost_restart(gs);
    reset_current_level(gs, &gs->screen.loop.fp_prev_riding);
}

void apply_damage(GameState *gs, int amount, int push,
                  float src_cx, float src_cy)
{
    /*
     * Knockback, shared by every hazard and enemy:
     *   1. A moving player is pushed straight back along the reverse of the
     *      normalised velocity, scaled to SPIKE_PUSH_SPEED, so the impulse is
     *      the same size however fast they ran in.
     *   2. A (nearly) stationary player is pushed horizontally away from the
     *      damage source's centre instead.
     *   3. Both cases add SPIKE_PUSH_VY upward and clear on_ground; otherwise
     *      the next floor snap would cancel the bounce on the same frame.
     *   4. A climbing player lets go (on_vine = 0). The climbing controls
     *      set vx and vy from the keys every step, so they would wipe the
     *      push out; PLAYER_KNOCKBACK_GRAB_LOCK stops an immediate re-grab.
     */
    if (push) {
        float vx  = gs->world.player.vx;
        float vy  = gs->world.player.vy;
        float len = sqrtf(vx * vx + vy * vy);
        if (len > 1.0f) {
            gs->world.player.vx = -(vx / len) * SPIKE_PUSH_SPEED;
            gs->world.player.vy = -(vy / len) * SPIKE_PUSH_SPEED + SPIKE_PUSH_VY;
        } else {
            float dir = (gs->world.player.x + gs->world.player.w * 0.5f >= src_cx) ? 1.0f : -1.0f;
            gs->world.player.vx = dir * SPIKE_PUSH_SPEED;
            gs->world.player.vy = SPIKE_PUSH_VY;
        }
        gs->world.player.on_ground = 0;
        gs->world.player.on_vine   = 0;
    }
    (void)src_cy;   /* reserved for future vertical-push logic */

    gs->world.player.hurt_timer = PLAYER_HURT_TIME;
    sound_play(gs->assets.audio.hit, 128);

    gs->world.hearts -= amount;
    if (gs->world.hearts <= 0) {
        gs->world.lives--;
        if (gs->world.lives < 0) {
            gs->screen.game_over = 1;
            gs->screen.terminal_action_index = 0;
            gs->screen.pause_reasons = 0;
            gs->screen.paused = 0;
            if (gs->screen.debug_mode) debug_log(&gs->screen.debug, "GAME OVER");
            return;
        }
        if (gs->screen.debug_mode) debug_log(&gs->screen.debug, "LIFE LOST lives=%d", gs->world.lives);
        {
            const LevelDef *def = (const LevelDef *)gs->world.runtime.current_level;
            gs->world.hearts = def && def->initial_hearts > 0
                       ? def->initial_hearts : MAX_HEARTS;
        }
        reset_current_level(gs, &gs->screen.loop.fp_prev_riding);
        game_checkpoint_feedback_set(gs, CHECKPOINT_FEEDBACK_RESPAWN,
                                      game_checkpoint_clock_ms(gs), 900);
    }
}
