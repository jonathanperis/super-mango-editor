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
    game_checkpoint_update_authored(&gs);
    if (expect_int("crossed index", gs.checkpoint_index, 0) != 0) return 1;
    if (expect_float("crossed x", gs.respawn_x, 500.0f) != 0) return 1;
    if (expect_float("crossed y", gs.respawn_y, 210.0f) != 0) return 1;

    gs.player.x = 250.0f;
    game_checkpoint_update_authored(&gs);
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

    /* The end-of-step legacy update leaves authored levels alone: no screen
     * checkpoint, and no second sampling of the authored placements (that
     * happens once per step, in game_checkpoint_update_authored). */
    game_checkpoint_update(&gs);
    if (expect_int("legacy update leaves authored index", gs.checkpoint_index, -1) != 0)
        return 1;
    if (expect_float("legacy update leaves respawn", gs.respawn_x, 80.0f) != 0) return 1;
    if (expect_int("no screen checkpoint", gs.legacy_checkpoint_screen, 0) != 0) return 1;

    game_checkpoint_update_authored(&gs);
    if (expect_float("authored x beats screen fallback", gs.respawn_x, 200.0f) != 0)
        return 1;
    if (expect_float("authored y", gs.respawn_y, 90.0f) != 0) return 1;
    return 0;
}

static int stale_checkpoint_index_before_first_placement_is_ignored(void)
{
    GameState gs = {0};
    LevelDef def;

    level_def_init_defaults(&def);
    def.checkpoint_count = 1;
    def.checkpoints[0].x = 300.0f;
    def.checkpoints[0].y = 120.0f;
    gs.runtime.current_level = &def;
    /* An out-of-range remembered index, and the player stands before every
     * placement: no checkpoint qualifies. The update must not index
     * checkpoints[-1] (ASan catches it in sanitize). */
    gs.checkpoint_index = 5;
    gs.respawn_x = 80.0f;
    gs.respawn_y = 172.0f;
    gs.player.x = 100.0f;
    game_checkpoint_update_authored(&gs);
    if (expect_float("unreached respawn x", gs.respawn_x, 80.0f) != 0) return 1;
    if (expect_float("unreached respawn y", gs.respawn_y, 172.0f) != 0) return 1;
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

static int legacy_checkpoints_skip_gaps_and_hazards_at_screen_edge(void)
{
    GameState gs = {0};
    LevelDef def;

    level_def_init_defaults(&def);
    gs.runtime.current_level = &def;
    gs.respawn_x = 80.0f;
    gs.respawn_y = 172.0f;

    /* A floor gap starts exactly on the 400 px screen edge. Columns 400..360
     * overlap it; 352..400 ends where the gap begins, so it is solid floor. */
    gs.floor_gap_count = 1;
    gs.floor_gaps[0] = 400;
    gs.player.x = 450.0f;
    game_checkpoint_update(&gs);
    if (expect_float("gap at edge moves respawn left", gs.respawn_x, 352.0f) != 0) return 1;

    /* Next edge: gap at 800 plus spikes at 744..776 just before it. */
    gs.floor_gaps[1] = 800;
    gs.floor_gap_count = 2;
    gs.spike_row_count = 1;
    gs.spike_rows[0] = (SpikeRow){.x = 744.0f, .y = 236.0f, .count = 2, .active = 1};
    gs.player.x = 850.0f;
    game_checkpoint_update(&gs);
    if (expect_float("spikes before edge also skipped", gs.respawn_x, 696.0f) != 0) return 1;

    /* No safe column between the previous checkpoint and the next edge:
     * keep the previous checkpoint rather than respawning on spikes. */
    for (int i = 0; i < 2; i++)
        gs.spike_rows[1 + i] = (SpikeRow){.x = 704.0f + i * 256.0f, .y = 236.0f,
                                          .count = 16, .active = 1};
    gs.spike_row_count = 3;
    gs.player.x = 1250.0f;
    game_checkpoint_update(&gs);
    if (expect_float("unsafe screen keeps previous", gs.respawn_x, 696.0f) != 0) return 1;
    if (expect_int("unsafe screen still recorded", gs.legacy_checkpoint_screen, 3) != 0) return 1;
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
    if (stale_checkpoint_index_before_first_placement_is_ignored() != 0) return 1;
    if (legacy_checkpoints_keep_screen_boundary_behavior() != 0) return 1;
    if (legacy_checkpoints_skip_gaps_and_hazards_at_screen_edge() != 0) return 1;
    if (feedback_save_and_expiry_are_explicit() != 0) return 1;
    puts("game_checkpoint_test: ok");
    return 0;
}
