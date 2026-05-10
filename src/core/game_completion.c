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
    gs->level_elapsed = 0.0f;
    gs->level_coin_total = gs->coin_count;
    gs->completion_coins_collected = 0;
    gs->completion_coin_total = gs->coin_count;
    gs->completion_elapsed = 0.0f;
    gs->completion_pending_next_phase = 0;
    gs->completion_next_phase[0] = '\0';
}

void game_complete_level(GameState *gs)
{
    const LevelDef *def = (const LevelDef *)gs->runtime.current_level;

    gs->completion_coins_collected = game_count_collected_coins(gs);
    gs->completion_coin_total = gs->level_coin_total;
    gs->completion_elapsed = gs->level_elapsed;
    gs->completion_pending_next_phase = 0;
    gs->completion_next_phase[0] = '\0';

    if (phase_has_next(def) &&
        phase_next_path(def, gs->completion_next_phase,
                        sizeof(gs->completion_next_phase)) == 0) {
        gs->completion_pending_next_phase = 1;
    }

    gs->level_complete = 1;
}
