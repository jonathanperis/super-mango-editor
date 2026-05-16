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

static int game_over_overlay_blocks_update(void)
{
    GameState gs = {0};

    gs.game_over = 1;

    if (expect_int("game over overlay", game_overlay_state(&gs), GAME_OVERLAY_GAME_OVER) != 0)
        return 1;
    if (expect_int("game over blocks update", game_overlay_blocks_update(&gs), 1) != 0)
        return 1;

    return 0;
}

static int completion_overlay_wins_over_game_over(void)
{
    GameState gs = {0};

    gs.game_over = 1;
    gs.completion.complete = 1;

    if (expect_int("completion priority over game over",
                   game_overlay_state(&gs), GAME_OVERLAY_LEVEL_COMPLETE) != 0)
        return 1;

    return 0;
}

static int player_pause_toggle_pauses_and_resumes_active_gameplay(void)
{
    GameState gs = {0};

    game_overlay_toggle_pause(&gs);
    if (expect_int("toggle pauses active gameplay", gs.paused, 1) != 0)
        return 1;
    if (expect_int("toggle pause overlay", game_overlay_state(&gs), GAME_OVERLAY_PAUSED) != 0)
        return 1;

    game_overlay_toggle_pause(&gs);
    if (expect_int("toggle resumes paused gameplay", gs.paused, 0) != 0)
        return 1;
    if (expect_int("resumed overlay", game_overlay_state(&gs), GAME_OVERLAY_NONE) != 0)
        return 1;

    return 0;
}

static int pause_toggle_ignores_completion_overlay(void)
{
    GameState gs = {0};

    gs.completion.complete = 1;

    game_overlay_toggle_pause(&gs);
    if (expect_int("completion toggle leaves pause off", gs.paused, 0) != 0)
        return 1;
    if (expect_int("completion toggle overlay", game_overlay_state(&gs), GAME_OVERLAY_LEVEL_COMPLETE) != 0)
        return 1;

    return 0;
}

static int resume_from_pause_only_clears_pause_overlay(void)
{
    GameState gs = {0};

    gs.paused = 1;
    game_overlay_resume(&gs);
    if (expect_int("resume clears pause", gs.paused, 0) != 0)
        return 1;
    if (expect_int("resume clears overlay", game_overlay_state(&gs), GAME_OVERLAY_NONE) != 0)
        return 1;

    gs.paused = 1;
    gs.completion.complete = 1;
    game_overlay_resume(&gs);
    if (expect_int("completion resume leaves pause untouched", gs.paused, 1) != 0)
        return 1;
    if (expect_int("completion resume overlay", game_overlay_state(&gs), GAME_OVERLAY_LEVEL_COMPLETE) != 0)
        return 1;

    return 0;
}

static int pause_reasons_preserve_player_pause_after_focus_resume(void)
{
    GameState gs = {0};

    game_overlay_toggle_pause(&gs);
    game_overlay_set_pause_reason(&gs, GAME_PAUSE_REASON_FOCUS, 1);
    if (expect_int("player plus focus pause", gs.paused, 1) != 0)
        return 1;
    if (expect_int("player plus focus reasons",
                   game_overlay_pause_reasons(&gs),
                   GAME_PAUSE_REASON_PLAYER | GAME_PAUSE_REASON_FOCUS) != 0)
        return 1;

    game_overlay_set_pause_reason(&gs, GAME_PAUSE_REASON_FOCUS, 0);
    if (expect_int("focus resume preserves player pause", gs.paused, 1) != 0)
        return 1;
    if (expect_int("focus resume preserves player reason",
                   game_overlay_pause_reasons(&gs), GAME_PAUSE_REASON_PLAYER) != 0)
        return 1;

    game_overlay_resume(&gs);
    if (expect_int("player resume clears final pause", gs.paused, 0) != 0)
        return 1;

    game_overlay_toggle_pause(&gs);
    game_overlay_set_pause_reason(&gs, GAME_PAUSE_REASON_FOCUS, 1);
    game_overlay_resume(&gs);
    if (expect_int("player resume preserves focus pause", gs.paused, 1) != 0)
        return 1;
    if (expect_int("player resume preserves focus reason",
                   game_overlay_pause_reasons(&gs), GAME_PAUSE_REASON_FOCUS) != 0)
        return 1;

    return 0;
}

int main(void)
{
    if (zeroed_game_state_has_no_overlay() != 0) return 1;
    if (pause_overlay_blocks_update() != 0) return 1;
    if (completion_overlay_blocks_update() != 0) return 1;
    if (completion_overlay_wins_over_pause() != 0) return 1;
    if (game_over_overlay_blocks_update() != 0) return 1;
    if (completion_overlay_wins_over_game_over() != 0) return 1;
    if (player_pause_toggle_pauses_and_resumes_active_gameplay() != 0) return 1;
    if (pause_toggle_ignores_completion_overlay() != 0) return 1;
    if (resume_from_pause_only_clears_pause_overlay() != 0) return 1;
    if (pause_reasons_preserve_player_pause_after_focus_resume() != 0) return 1;

    puts("game_overlay_test: ok");
    return 0;
}
