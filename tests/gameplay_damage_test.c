#include <stdio.h>
#include <string.h>

#include "collision/collision_damage.h"
#include "core/game_overlay.h"
#include "levels/level.h"
#include "screens/hud.h"
#include "core/game_checkpoint.h"  /* game_checkpoint_clock_ms */

void debug_log(DebugOverlay *dbg, const char *fmt, ...)
{
    (void)dbg;
    (void)fmt;
}

static int s_reset_calls;

void reset_current_level(GameState *gs, int *fp_prev_riding)
{
    s_reset_calls++;
    if (fp_prev_riding) *fp_prev_riding = -1;
    gs->world.player.spawn_x = gs->world.respawn_x;
    gs->world.player.spawn_y = gs->world.respawn_y;
    gs->world.player.x = gs->world.player.spawn_x;
    gs->world.player.y = gs->world.player.spawn_y;
}

static int expect_int(const char *name, int actual, int expected)
{
    if (actual != expected) {
        fprintf(stderr, "gameplay_damage_test: %s got %d expected %d\n",
                name, actual, expected);
        return 1;
    }
    return 0;
}

static int expect_float_positive(const char *name, float actual)
{
    if (actual <= 0.0f) {
        fprintf(stderr, "gameplay_damage_test: %s got %.2f expected > 0\n",
                name, actual);
        return 1;
    }
    return 0;
}

static int expect_float_near(const char *name, float actual, float expected, float eps)
{
    float diff = actual - expected;
    if (diff < 0.0f) diff = -diff;
    if (diff > eps) {
        fprintf(stderr,
                "gameplay_damage_test: %s got %.3f expected %.3f (eps %.3f)\n",
                name, actual, expected, eps);
        return 1;
    }
    return 0;
}

static int nonlethal_damage_sets_invincibility_and_push(void)
{
    GameState gs = {0};

    s_reset_calls = 0;
    gs.world.hearts = 3;
    gs.world.lives = 2;
    gs.world.player.x = 64.0f;
    gs.world.player.w = 48;
    gs.world.player.vx = 0.0f;
    gs.world.player.vy = 0.0f;
    gs.world.player.on_ground = 1;

    apply_damage(&gs, 1, 1, 0.0f, 0.0f);

    if (expect_int("hearts", gs.world.hearts, 2) != 0) return 1;
    if (expect_int("lives", gs.world.lives, 2) != 0) return 1;
    if (expect_int("on_ground", gs.world.player.on_ground, 0) != 0) return 1;
    if (expect_int("reset calls", s_reset_calls, 0) != 0) return 1;
    if (expect_float_positive("hurt_timer", gs.world.player.hurt_timer) != 0) return 1;
    if (expect_float_positive("knockback vx", gs.world.player.vx) != 0) return 1;

    return 0;
}

static int lethal_damage_consumes_life_and_resets_level(void)
{
    GameState gs = {0};
    LevelDef def;

    s_reset_calls = 0;
    level_def_init_defaults(&def);
    def.initial_hearts = 2;
    def.initial_lives = 4;

    gs.world.runtime.current_level = &def;
    gs.world.hearts = 1;
    gs.world.lives = 2;
    gs.world.player.spawn_x = 88.0f;
    gs.world.player.spawn_y = 120.0f;
    gs.screen.loop.fp_prev_riding = 7;
    gs.world.sim_time = 50.0;   /* 50 s of play so far */

    apply_damage(&gs, 1, 0, 0.0f, 0.0f);

    if (expect_int("hearts reset", gs.world.hearts, 2) != 0) return 1;
    if (expect_int("lives decremented", gs.world.lives, 1) != 0) return 1;
    if (expect_int("respawn feedback reason", gs.world.checkpoint_feedback_kind,
                   CHECKPOINT_FEEDBACK_RESPAWN) != 0) return 1;
    /* 0.9 s of game time after the reset, on the simulated clock. */
    if (expect_int("respawn feedback deadline", (int)gs.world.checkpoint_feedback_until, 50900) != 0)
        return 1;
    if (expect_int("HUD respawn visible",
                   hud_checkpoint_feedback_visible(gs.world.checkpoint_feedback_kind,
                                                   gs.world.checkpoint_feedback_until,
                                                   game_checkpoint_clock_ms(&gs)), 1) != 0)
        return 1;
    if (expect_int("HUD respawn label",
                   strcmp(hud_checkpoint_feedback_label(gs.world.checkpoint_feedback_kind, -1),
                          "RESPAWN") == 0, 1) != 0)
        return 1;
    if (expect_int("reset calls", s_reset_calls, 1) != 0) return 1;
    if (expect_int("float platform reset", gs.screen.loop.fp_prev_riding, -1) != 0)
        return 1;

    return 0;
}

static int game_over_sets_overlay_without_resetting_level(void)
{
    GameState gs = {0};
    LevelDef def;

    s_reset_calls = 0;
    level_def_init_defaults(&def);
    def.initial_hearts = 3;
    def.initial_lives = 5;

    gs.world.runtime.current_level = &def;
    gs.world.hearts = 1;
    gs.world.lives = 0;
    gs.world.score = 1200;
    gs.world.rules.score_per_life = 1000;
    gs.world.score_life_next = 2000;

    apply_damage(&gs, 1, 0, 0.0f, 0.0f);

    if (expect_int("game over flag", gs.screen.game_over, 1) != 0) return 1;
    if (expect_int("game over overlay", game_overlay_state(&gs), GAME_OVERLAY_GAME_OVER) != 0)
        return 1;
    if (expect_int("lives stay depleted", gs.world.lives, -1) != 0) return 1;
    if (expect_int("hearts stay depleted", gs.world.hearts, 0) != 0) return 1;
    if (expect_int("score preserved for overlay", gs.world.score, 1200) != 0) return 1;
    if (expect_int("next life preserved", gs.world.score_life_next, 2000) != 0) return 1;
    if (expect_int("reset waits for confirmation", s_reset_calls, 0) != 0) return 1;

    return 0;
}

static int life_loss_uses_saved_respawn_coordinates(void)
{
    GameState gs = {0};
    LevelDef def;

    s_reset_calls = 0;
    level_def_init_defaults(&def);
    def.initial_hearts = 3;
    gs.world.runtime.current_level = &def;
    gs.world.hearts = 1;
    gs.world.lives = 2;
    gs.world.respawn_x = 240.0f;
    gs.world.respawn_y = 96.0f;
    gs.world.player.spawn_x = 80.0f;
    gs.world.player.spawn_y = 172.0f;

    apply_damage(&gs, 1, 0, 0.0f, 0.0f);

    if (expect_float_near("life-loss respawn x", gs.world.player.spawn_x, 240.0f, 0.001f) != 0)
        return 1;
    if (expect_float_near("life-loss respawn y", gs.world.player.spawn_y, 96.0f, 0.001f) != 0)
        return 1;
    if (expect_int("life-loss reset calls", s_reset_calls, 1) != 0) return 1;
    return 0;
}

static int confirming_game_over_restores_level_lives_and_score(void)
{
    GameState gs = {0};
    LevelDef def;

    s_reset_calls = 0;
    level_def_init_defaults(&def);
    def.initial_hearts = 3;
    def.initial_lives = 5;
    def.player_start_x = 80.0f;
    def.player_start_y = 172.0f;
    def.checkpoint_count = 1;
    def.checkpoints[0].x = 320.0f;
    def.checkpoints[0].y = 96.0f;

    gs.world.runtime.current_level = &def;
    gs.world.hearts = 0;
    gs.world.lives = -1;
    gs.screen.game_over = 1;
    gs.world.score = 1200;
    gs.world.respawn_x = 320.0f;
    gs.world.respawn_y = 172.0f;
    gs.world.checkpoint_index = -1;
    gs.world.player.spawn_x = 320.0f;
    gs.world.player.spawn_y = 172.0f;
    gs.world.rules.score_per_life = 1000;
    gs.world.score_life_next = 2000;

    game_restart_after_game_over(&gs);

    if (expect_int("game over cleared", gs.screen.game_over, 0) != 0) return 1;
    if (expect_int("overlay cleared", game_overlay_state(&gs), GAME_OVERLAY_NONE) != 0)
        return 1;
    if (expect_int("lives restored", gs.world.lives, 5) != 0) return 1;
    if (expect_int("hearts restored", gs.world.hearts, 3) != 0) return 1;
    if (expect_int("score reset", gs.world.score, 0) != 0) return 1;
    if (expect_float_near("respawn x reset", gs.world.respawn_x, 80.0f, 0.001f) != 0)
        return 1;
    if (expect_float_near("respawn y reset", gs.world.respawn_y, 172.0f, 0.001f) != 0)
        return 1;
    if (expect_int("checkpoint index reset", gs.world.checkpoint_index, -1) != 0)
        return 1;
    if (expect_float_near("spawn x restored", gs.world.player.spawn_x, 80.0f, 0.001f) != 0)
        return 1;
    if (expect_float_near("spawn y restored", gs.world.player.spawn_y, 172.0f, 0.001f) != 0)
        return 1;
    if (expect_int("next life reset", gs.world.score_life_next, 1000) != 0) return 1;
    if (expect_int("reset calls", s_reset_calls, 1) != 0) return 1;

    return 0;
}

static int confirming_game_over_restores_default_spawn_when_level_start_unset(void)
{
    GameState gs = {0};
    LevelDef def;
    const int default_spawn_y = FLOOR_Y - 2 * TILE_SIZE + 16;

    s_reset_calls = 0;
    level_def_init_defaults(&def);
    def.initial_hearts = 3;
    def.initial_lives = 5;

    gs.world.runtime.current_level = &def;
    gs.world.hearts = 0;
    gs.world.lives = -1;
    gs.screen.game_over = 1;
    gs.world.respawn_x = 320.0f;
    gs.world.respawn_y = 172.0f;
    gs.world.player.spawn_x = 320.0f;
    gs.world.player.spawn_y = 172.0f;
    gs.world.rules.score_per_life = 1000;

    game_restart_after_game_over(&gs);

    if (expect_float_near("default spawn x restored", gs.world.player.spawn_x, 80.0f, 0.001f) != 0)
        return 1;
    if (expect_float_near("default spawn y restored", gs.world.player.spawn_y,
                          (float)default_spawn_y, 0.001f) != 0)
        return 1;
    if (expect_int("default spawn reset calls", s_reset_calls, 1) != 0) return 1;

    return 0;
}

int main(void)
{
    if (nonlethal_damage_sets_invincibility_and_push() != 0) return 1;
    if (lethal_damage_consumes_life_and_resets_level() != 0) return 1;
    if (game_over_sets_overlay_without_resetting_level() != 0) return 1;
    if (life_loss_uses_saved_respawn_coordinates() != 0) return 1;
    if (confirming_game_over_restores_level_lives_and_score() != 0) return 1;
    if (confirming_game_over_restores_default_spawn_when_level_start_unset() != 0)
        return 1;

    puts("gameplay_damage_test: ok");
    return 0;
}
