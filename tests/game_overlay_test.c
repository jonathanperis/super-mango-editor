#include <stdio.h>

#include "core/game_overlay.h"

static int expect_int(const char *name, int actual, int expected)
{
    if (actual != expected) {
        fprintf(stderr, "game_overlay_test: %s got %d expected %d\n",
                name, actual, expected);
        return 1;
    }
    return 0;
}

static int zeroed_game_state_has_no_overlay(void)
{
    GameState gs = {0};

    if (expect_int("zeroed overlay", game_overlay_state(&gs), GAME_OVERLAY_NONE) != 0)
        return 1;
    if (expect_int("zeroed blocks update", game_overlay_blocks_update(&gs), 0) != 0)
        return 1;

    return 0;
}

static int pause_overlay_blocks_update(void)
{
    GameState gs = {0};

    gs.paused = 1;

    if (expect_int("paused overlay", game_overlay_state(&gs), GAME_OVERLAY_PAUSED) != 0)
        return 1;
    if (expect_int("paused blocks update", game_overlay_blocks_update(&gs), 1) != 0)
        return 1;

    return 0;
}

static int completion_overlay_blocks_update(void)
{
    GameState gs = {0};

    gs.completion.complete = 1;

    if (expect_int("completion overlay", game_overlay_state(&gs), GAME_OVERLAY_LEVEL_COMPLETE) != 0)
        return 1;
    if (expect_int("completion blocks update", game_overlay_blocks_update(&gs), 1) != 0)
        return 1;

    return 0;
}

static int completion_overlay_wins_over_pause(void)
{
    GameState gs = {0};

    gs.paused = 1;
    gs.completion.complete = 1;

    if (expect_int("completion priority", game_overlay_state(&gs), GAME_OVERLAY_LEVEL_COMPLETE) != 0)
        return 1;
    if (expect_int("completion priority blocks update", game_overlay_blocks_update(&gs), 1) != 0)
        return 1;

    return 0;
}

int main(void)
{
    if (zeroed_game_state_has_no_overlay() != 0) return 1;
    if (pause_overlay_blocks_update() != 0) return 1;
    if (completion_overlay_blocks_update() != 0) return 1;
    if (completion_overlay_wins_over_pause() != 0) return 1;

    puts("game_overlay_test: ok");
    return 0;
}
