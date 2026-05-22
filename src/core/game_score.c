/*
 * game_score.c — Shared score and bonus-life helpers.
 */
#include "game_score.h"

void game_award_score(GameState *gs, int amount)
{
    if (!gs) return;

    gs->score += amount;
    if (gs->rules.score_per_life <= 0) return;

    while (gs->score >= gs->score_life_next) {
        gs->lives++;
        gs->score_life_next += gs->rules.score_per_life;
    }
}
