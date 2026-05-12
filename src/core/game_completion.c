/*
 * game_completion.c — Level completion summary helpers.
 */

#include "game_completion.h"

#include "../levels/level.h"
#include "../levels/phase_transition.h"

static int game_count_collected_coins(const GameState *gs)
{
    int collected = 0;

    for (int i = 0; i < gs->coin_count; i++) {
        if (!gs->coins[i].active) collected++;
    }

    return collected;
}

void game_completion_reset_summary(GameState *gs)
{
    gs->completion.level_elapsed = 0.0f;
    gs->completion.level_coin_total = gs->coin_count;
    gs->completion.coins_collected = 0;
    gs->completion.coin_total = gs->coin_count;
    gs->completion.elapsed = 0.0f;
    gs->completion.pending_next_phase = 0;
    gs->completion.next_phase[0] = '\0';
}

void game_complete_level(GameState *gs)
{
    const LevelDef *def = (const LevelDef *)gs->runtime.current_level;

    gs->completion.coins_collected = game_count_collected_coins(gs);
    gs->completion.coin_total = gs->completion.level_coin_total;
    gs->completion.elapsed = gs->completion.level_elapsed;
    gs->completion.pending_next_phase = 0;
    gs->completion.next_phase[0] = '\0';

    if (phase_has_next(def) &&
        phase_next_path(def, gs->completion.next_phase,
                        sizeof(gs->completion.next_phase)) == 0) {
        gs->completion.pending_next_phase = 1;
    }

    gs->completion.complete = 1;
}
