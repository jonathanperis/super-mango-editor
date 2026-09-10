/*
 * game_score.c — Shared score and bonus-life helpers.
 */
#include "game_score.h"
#include <limits.h>
#include <stdint.h>

void game_award_score(GameState *gs, int amount)
{
    if (!gs || amount <= 0) return;
    gs->score = amount > INT_MAX - gs->score ? INT_MAX : gs->score + amount;
    if (gs->rules.score_per_life <= 0 || gs->score_life_next <= 0 ||
        gs->score < gs->score_life_next) return;

    /* Award all crossed thresholds in constant time. Zero means the next
     * threshold exceeds the score ceiling, so no further bonus is possible. */
    int64_t bonus = ((int64_t)gs->score - gs->score_life_next) / gs->rules.score_per_life + 1;
    int64_t lives = (int64_t)gs->lives + bonus;
    int64_t next = (int64_t)gs->score_life_next + bonus * gs->rules.score_per_life;
    gs->lives = lives > INT_MAX ? INT_MAX : (int)lives;
    gs->score_life_next = next > INT_MAX ? 0 : (int)next;
}
