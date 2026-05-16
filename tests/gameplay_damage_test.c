#include <stdio.h>

#include "collision/collision_damage.h"
#include "core/game_overlay.h"
#include "levels/level.h"

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
    gs->player.x = gs->player.spawn_x;
    gs->player.y = gs->player.spawn_y;
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
    gs.hearts = 3;
    gs.lives = 2;
    gs.player.x = 64.0f;
    gs.player.w = 48;
    gs.player.vx = 0.0f;
    gs.player.vy = 0.0f;
    gs.player.on_ground = 1;

    apply_damage(&gs, 1, 1, 0.0f, 0.0f);

    if (expect_int("hearts", gs.hearts, 2) != 0) return 1;
    if (expect_int("lives", gs.lives, 2) != 0) return 1;
    if (expect_int("on_ground", gs.player.on_ground, 0) != 0) return 1;
    if (expect_int("reset calls", s_reset_calls, 0) != 0) return 1;
    if (expect_float_positive("hurt_timer", gs.player.hurt_timer) != 0) return 1;
    if (expect_float_positive("knockback vx", gs.player.vx) != 0) return 1;

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

    gs.runtime.current_level = &def;
    gs.hearts = 1;
    gs.lives = 2;
    gs.player.spawn_x = 88.0f;
    gs.player.spawn_y = 120.0f;
    gs.loop.fp_prev_riding = 7;

    apply_damage(&gs, 1, 0, 0.0f, 0.0f);

    if (expect_int("hearts reset", gs.hearts, 2) != 0) return 1;
    if (expect_int("lives decremented", gs.lives, 1) != 0) return 1;
    if (expect_int("reset calls", s_reset_calls, 1) != 0) return 1;
    if (expect_int("float platform reset", gs.loop.fp_prev_riding, -1) != 0)
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

    gs.runtime.current_level = &def;
    gs.hearts = 1;
    gs.lives = 0;
    gs.score = 1200;
    gs.rules.score_per_life = 1000;
    gs.score_life_next = 2000;

    apply_damage(&gs, 1, 0, 0.0f, 0.0f);

    if (expect_int("game over flag", gs.game_over, 1) != 0) return 1;
    if (expect_int("game over overlay", game_overlay_state(&gs), GAME_OVERLAY_GAME_OVER) != 0)
        return 1;
    if (expect_int("lives stay depleted", gs.lives, -1) != 0) return 1;
    if (expect_int("hearts stay depleted", gs.hearts, 0) != 0) return 1;
    if (expect_int("score preserved for overlay", gs.score, 1200) != 0) return 1;
    if (expect_int("next life preserved", gs.score_life_next, 2000) != 0) return 1;
    if (expect_int("reset waits for confirmation", s_reset_calls, 0) != 0) return 1;

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

    gs.runtime.current_level = &def;
    gs.hearts = 0;
    gs.lives = -1;
    gs.game_over = 1;
    gs.score = 1200;
    gs.checkpoint_x = 320.0f;
    gs.player.spawn_x = 320.0f;
    gs.player.spawn_y = 172.0f;
    gs.rules.score_per_life = 1000;
    gs.score_life_next = 2000;

    game_restart_after_game_over(&gs);

    if (expect_int("game over cleared", gs.game_over, 0) != 0) return 1;
    if (expect_int("overlay cleared", game_overlay_state(&gs), GAME_OVERLAY_NONE) != 0)
        return 1;
    if (expect_int("lives restored", gs.lives, 5) != 0) return 1;
    if (expect_int("hearts restored", gs.hearts, 3) != 0) return 1;
    if (expect_int("score reset", gs.score, 0) != 0) return 1;
    if (expect_float_near("checkpoint reset", gs.checkpoint_x, 0.0f, 0.001f) != 0)
        return 1;
    if (expect_float_near("spawn x restored", gs.player.spawn_x, 80.0f, 0.001f) != 0)
        return 1;
    if (expect_float_near("spawn y restored", gs.player.spawn_y, 172.0f, 0.001f) != 0)
        return 1;
    if (expect_int("next life reset", gs.score_life_next, 1000) != 0) return 1;
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

    gs.runtime.current_level = &def;
    gs.hearts = 0;
    gs.lives = -1;
    gs.game_over = 1;
    gs.checkpoint_x = 320.0f;
    gs.player.spawn_x = 320.0f;
    gs.player.spawn_y = 172.0f;
    gs.rules.score_per_life = 1000;

    game_restart_after_game_over(&gs);

    if (expect_float_near("default spawn x restored", gs.player.spawn_x, 80.0f, 0.001f) != 0)
        return 1;
    if (expect_float_near("default spawn y restored", gs.player.spawn_y,
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
    if (confirming_game_over_restores_level_lives_and_score() != 0) return 1;
    if (confirming_game_over_restores_default_spawn_when_level_start_unset() != 0)
        return 1;

    puts("gameplay_damage_test: ok");
    return 0;
}
