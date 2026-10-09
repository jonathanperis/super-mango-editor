/*
 * game_completion.c — Level completion summary helpers.
 */

#include "game_completion.h"

#include "../levels/level.h"
#include "../levels/phase_transition.h"

static int game_count_collected_coins(const GameState *gs)
{
    int collected = 0;

    for (int i = 0; i < gs->world.coin_count; i++) {
        if (!gs->world.coins[i].active) collected++;
    }

    return collected;
}

void game_completion_reset_summary(GameState *gs)
{
    gs->screen.completion.level_elapsed = 0.0f;
    gs->screen.completion.level_coin_total = gs->world.coin_count;
    gs->screen.completion.coins_collected = 0;
    gs->screen.completion.coin_total = gs->world.coin_count;
    gs->screen.completion.elapsed = 0.0f;
    gs->screen.completion.pending_next_phase = 0;
    gs->screen.completion.next_phase_failed = 0;
    gs->screen.completion.next_phase[0] = '\0';
}

void game_complete_level(GameState *gs)
{
    const LevelDef *def = (const LevelDef *)gs->world.runtime.current_level;

    gs->screen.completion.coins_collected = game_count_collected_coins(gs);
    gs->screen.completion.coin_total = gs->screen.completion.level_coin_total;
    gs->screen.completion.elapsed = gs->screen.completion.level_elapsed;
    gs->screen.completion.pending_next_phase = 0;
    gs->screen.completion.next_phase_failed = 0;
    gs->screen.completion.next_phase[0] = '\0';

    if (phase_has_next(def) &&
        phase_next_path(def, gs->screen.completion.next_phase,
                        sizeof(gs->screen.completion.next_phase)) == 0) {
        gs->screen.completion.pending_next_phase = 1;
    }

    gs->screen.completion.complete = 1;
    gs->screen.terminal_action_index = 0;
}
