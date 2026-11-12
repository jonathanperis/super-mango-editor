/*
 * game_completion.c — Level completion summary helpers.
 */

#include "game_completion.h"
#include "../game.h"  /* GameState: this file reads its fields */

#include "../game_constants.h"  /* TARGET_FPS */
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

float game_level_seconds(const GameState *gs)
{
    /* double holds every uint32_t exactly, so the only rounding is the
     * one division (and the final narrowing to float). */
    return (float)((double)gs->screen.completion.level_steps / TARGET_FPS);
}

uint32_t game_level_steps_from_seconds(float seconds)
{
    double steps = (double)seconds * TARGET_FPS + 0.5;
    /* Less than one step rounds to 0. Written as "not >= 1" so that a
     * NaN, which fails every comparison, lands here too. */
    if (!(steps >= 1.0)) return 0;
    if (steps >= (double)UINT32_MAX) return UINT32_MAX;
    return (uint32_t)steps;   /* truncates: + 0.5 above made it round */
}

void game_completion_reset_summary(GameState *gs)
{
    gs->screen.completion.level_steps = 0;
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
    gs->screen.completion.elapsed = game_level_seconds(gs);
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
