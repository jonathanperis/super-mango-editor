#include <stdio.h>

#include "core/game_checkpoint.h"
#include "levels/level.h"

void debug_log(DebugOverlay *dbg, const char *fmt, ...)
{
    (void)dbg;
    (void)fmt;
}

static int expect_float(const char *name, float actual, float expected)
{
    float diff = actual - expected;
    if (diff < 0.0f) diff = -diff;
    if (diff > 0.001f) {
        fprintf(stderr, "game_checkpoint_test: %s got %.3f expected %.3f\n",
                name, actual, expected);
        return 1;
    }
    return 0;
}

static int expect_int(const char *name, int actual, int expected)
{
    if (actual != expected) {
        fprintf(stderr, "game_checkpoint_test: %s got %d expected %d\n",
                name, actual, expected);
        return 1;
    }
    return 0;
}

static int authored_checkpoints_advance_by_highest_x(void)
{
    GameState gs = {0};
    LevelDef def;

    level_def_init_defaults(&def);
    def.checkpoint_count = 3;
    def.checkpoints[0].x = 500.0f;
    def.checkpoints[0].y = 210.0f;
    def.checkpoints[1].x = 200.0f;
    def.checkpoints[1].y = 110.0f;
    def.checkpoints[2].x = 400.0f;
    def.checkpoints[2].y = 160.0f;
    gs.runtime.current_level = &def;
    gs.respawn_x = 80.0f;
    gs.respawn_y = 172.0f;
    gs.checkpoint_index = -1;

    /* One large movement crosses all placements; highest x wins. */
    gs.player.x = 550.0f;
    game_checkpoint_update(&gs);
    if (expect_int("crossed index", gs.checkpoint_index, 0) != 0) return 1;
    if (expect_float("crossed x", gs.respawn_x, 500.0f) != 0) return 1;
    if (expect_float("crossed y", gs.respawn_y, 210.0f) != 0) return 1;

    gs.player.x = 250.0f;
    game_checkpoint_update(&gs);
    if (expect_int("no regression index", gs.checkpoint_index, 0) != 0) return 1;
    if (expect_float("no regression x", gs.respawn_x, 500.0f) != 0) return 1;
    return 0;
}

static int authored_checkpoints_disable_legacy_fallback(void)
{
    GameState gs = {0};
    LevelDef def;

    level_def_init_defaults(&def);
    def.checkpoint_count = 1;
    def.checkpoints[0].x = 200.0f;
    def.checkpoints[0].y = 90.0f;
    gs.runtime.current_level = &def;
    gs.respawn_x = 80.0f;
    gs.respawn_y = 172.0f;
    gs.checkpoint_index = -1;
    gs.player.x = 450.0f;
    game_checkpoint_update(&gs);

    if (expect_float("authored x beats screen fallback", gs.respawn_x, 200.0f) != 0)
        return 1;
    if (expect_float("authored y", gs.respawn_y, 90.0f) != 0) return 1;
    return 0;
}

static int legacy_checkpoints_keep_screen_boundary_behavior(void)
{
    GameState gs = {0};
    LevelDef def;

    level_def_init_defaults(&def);
    gs.runtime.current_level = &def;
    gs.respawn_x = 80.0f;
    gs.respawn_y = 172.0f;
    gs.legacy_checkpoint_screen = 0;

    gs.player.x = 399.0f;
    game_checkpoint_update(&gs);
    if (expect_float("before boundary", gs.respawn_x, 80.0f) != 0) return 1;
    gs.player.x = 801.0f;
    game_checkpoint_update(&gs);
    if (expect_float("screen boundary", gs.respawn_x, 800.0f) != 0) return 1;
    gs.player.x = 410.0f;
    game_checkpoint_update(&gs);
    if (expect_float("legacy no regression", gs.respawn_x, 800.0f) != 0) return 1;
    return 0;
}

static int feedback_save_and_expiry_are_explicit(void)
{
    GameState gs = {0};

    game_checkpoint_feedback_set(&gs, CHECKPOINT_FEEDBACK_SAVED, 1000, 1200);
    if (expect_int("save feedback reason", gs.checkpoint_feedback_kind,
                   CHECKPOINT_FEEDBACK_SAVED) != 0) return 1;
    if (expect_int("save feedback deadline", (int)gs.checkpoint_feedback_until,
                   2200) != 0) return 1;
    game_checkpoint_feedback_clear_expired(&gs, 2199);
    if (expect_int("feedback before deadline", gs.checkpoint_feedback_kind,
                   CHECKPOINT_FEEDBACK_SAVED) != 0) return 1;
    game_checkpoint_feedback_clear_expired(&gs, 2200);
    if (expect_int("feedback after deadline", gs.checkpoint_feedback_kind,
                   CHECKPOINT_FEEDBACK_NONE) != 0) return 1;
    return 0;
}

int main(void)
{
    if (authored_checkpoints_advance_by_highest_x() != 0) return 1;
    if (authored_checkpoints_disable_legacy_fallback() != 0) return 1;
    if (legacy_checkpoints_keep_screen_boundary_behavior() != 0) return 1;
    if (feedback_save_and_expiry_are_explicit() != 0) return 1;
    puts("game_checkpoint_test: ok");
    return 0;
}
