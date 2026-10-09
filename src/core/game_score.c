/*
 * game_score.c — Shared score and bonus-life helpers.
 */
#include "game_score.h"
#include <limits.h>
#include <stdint.h>

void game_award_score(GameState *gs, int amount)
{
    if (!gs || amount <= 0) return;
    gs->world.score = amount > INT_MAX - gs->world.score ? INT_MAX : gs->world.score + amount;
    if (gs->world.rules.score_per_life <= 0 || gs->world.score_life_next <= 0 ||
        gs->world.score < gs->world.score_life_next) return;

    /* Award all crossed thresholds in constant time. Zero means the next
     * threshold exceeds the score ceiling, so no further bonus is possible. */
    int64_t bonus = ((int64_t)gs->world.score - gs->world.score_life_next) / gs->world.rules.score_per_life + 1;
    int64_t lives = (int64_t)gs->world.lives + bonus;
    int64_t next = (int64_t)gs->world.score_life_next + bonus * gs->world.rules.score_per_life;
    gs->world.lives = lives > INT_MAX ? INT_MAX : (int)lives;
    gs->world.score_life_next = next > INT_MAX ? 0 : (int)next;
}
