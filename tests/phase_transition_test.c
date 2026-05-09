#include <stdio.h>
#include <string.h>

#include "levels/phase_transition.h"

static int expect_int(const char *name, int actual, int expected)
{
    if (actual != expected) {
        fprintf(stderr, "phase_transition_test: %s got %d expected %d\n", name, actual, expected);
        return 1;
    }
    return 0;
}

static int expect_str(const char *name, const char *actual, const char *expected)
{
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "phase_transition_test: %s got '%s' expected '%s'\n", name, actual, expected);
        return 1;
    }
    return 0;
}

static int copies_next_phase_path(void)
{
    LevelDef def = {0};
    char path[256] = {0};
    strncpy(def.next_phase, "levels/02_lugio_02.toml", sizeof(def.next_phase) - 1);

    if (expect_int("copy result", phase_next_path(&def, path, sizeof(path)), 0) != 0)
        return 1;
    if (expect_str("path", path, "levels/02_lugio_02.toml") != 0) return 1;

    return 0;
}

static int rejects_missing_next_phase(void)
{
    LevelDef def = {0};
    char path[8] = "keep";

    if (expect_int("missing result", phase_next_path(&def, path, sizeof(path)), -1) != 0)
        return 1;
    if (expect_str("unchanged path", path, "keep") != 0) return 1;

    return 0;
}

static int rejects_invalid_next_phase_args(void)
{
    LevelDef def = {0};
    char path[8] = "keep";

    strncpy(def.next_phase, "levels/02_lugio_02.toml", sizeof(def.next_phase) - 1);

    if (expect_int("null current", phase_next_path(NULL, path, sizeof(path)), -1) != 0)
        return 1;
    if (expect_str("null current unchanged", path, "keep") != 0) return 1;

    if (expect_int("null output", phase_next_path(&def, NULL, sizeof(path)), -1) != 0)
        return 1;
    if (expect_str("null output unchanged", path, "keep") != 0) return 1;

    if (expect_int("zero output size", phase_next_path(&def, path, 0), -1) != 0)
        return 1;
    if (expect_str("zero size unchanged", path, "keep") != 0) return 1;

    return 0;
}

static int detects_next_phase_presence(void)
{
    LevelDef def = {0};

    if (expect_int("null has next", phase_has_next(NULL), 0) != 0) return 1;
    if (expect_int("empty has next", phase_has_next(&def), 0) != 0) return 1;

    strncpy(def.next_phase, "levels/02_lugio_02.toml", sizeof(def.next_phase) - 1);
    if (expect_int("set has next", phase_has_next(&def), 1) != 0) return 1;

    return 0;
}

static int truncates_path_safely(void)
{
    LevelDef def = {0};
    char path[8] = {0};
    strncpy(def.next_phase, "levels/02_lugio_02.toml", sizeof(def.next_phase) - 1);

    if (expect_int("truncate result", phase_next_path(&def, path, sizeof(path)), 0) != 0)
        return 1;
    if (expect_str("truncated path", path, "levels/") != 0) return 1;

    return 0;
}

static int preserves_progress_across_reload(void)
{
    GameState gs = {0};
    PhaseProgress progress;

    gs.score = 1500;
    gs.lives = 4;
    gs.hearts = 2;
    gs.score_life_next = 2000;
    phase_progress_save(&gs, &progress);

    gs.score = 0;
    gs.lives = 1;
    gs.hearts = 1;
    gs.score_life_next = 1000;
    phase_progress_restore(&gs, &progress);

    if (expect_int("score", gs.score, 1500) != 0) return 1;
    if (expect_int("lives", gs.lives, 4) != 0) return 1;
    if (expect_int("hearts", gs.hearts, 2) != 0) return 1;
    if (expect_int("score_life_next", gs.score_life_next, 2000) != 0) return 1;

    return 0;
}

static int ignores_null_progress_args(void)
{
    GameState gs = {0};
    PhaseProgress progress = {0};

    gs.score = 300;
    gs.lives = 2;
    gs.hearts = 1;
    gs.score_life_next = 1000;
    progress.score = 900;
    progress.lives = 5;
    progress.hearts = 4;
    progress.score_life_next = 2000;

    phase_progress_save(NULL, &progress);
    if (expect_int("null save score", progress.score, 900) != 0) return 1;
    if (expect_int("null save lives", progress.lives, 5) != 0) return 1;

    phase_progress_save(&gs, NULL);
    phase_progress_restore(NULL, &progress);
    phase_progress_restore(&gs, NULL);

    if (expect_int("null restore score", gs.score, 300) != 0) return 1;
    if (expect_int("null restore lives", gs.lives, 2) != 0) return 1;
    if (expect_int("null restore hearts", gs.hearts, 1) != 0) return 1;
    if (expect_int("null restore next", gs.score_life_next, 1000) != 0) return 1;

    return 0;
}

int main(void)
{
    if (copies_next_phase_path() != 0) return 1;
    if (rejects_missing_next_phase() != 0) return 1;
    if (rejects_invalid_next_phase_args() != 0) return 1;
    if (detects_next_phase_presence() != 0) return 1;
    if (truncates_path_safely() != 0) return 1;
    if (preserves_progress_across_reload() != 0) return 1;
    if (ignores_null_progress_args() != 0) return 1;

    puts("phase_transition_test: ok");
    return 0;
}
