#include <stdio.h>

#include "core/game_score.h"
#include "game.h"

static int expect_int(const char *name, int actual, int expected)
{
    if (actual != expected) {
        fprintf(stderr, "gameplay_score_test: %s got %d expected %d\n",
                name, actual, expected);
        return 1;
    }
    return 0;
}

static int score_award_grants_every_crossed_bonus_life(void)
{
    GameState gs = {0};

    gs.score = 900;
    gs.lives = 3;
    gs.rules.score_per_life = 1000;
    gs.score_life_next = 1000;

    game_award_score(&gs, 2500);

    if (expect_int("score", gs.score, 3400) != 0) return 1;
    if (expect_int("lives", gs.lives, 6) != 0) return 1;
    if (expect_int("next life", gs.score_life_next, 4000) != 0) return 1;

    return 0;
}

static int score_award_ignores_invalid_bonus_cadence(void)
{
    GameState gs = {0};

    gs.score = 90;
    gs.lives = 2;
    gs.rules.score_per_life = 0;
    gs.score_life_next = 100;

    game_award_score(&gs, 25);

    if (expect_int("invalid cadence score", gs.score, 115) != 0) return 1;
    if (expect_int("invalid cadence lives", gs.lives, 2) != 0) return 1;
    if (expect_int("invalid cadence next life", gs.score_life_next, 100) != 0)
        return 1;

    return 0;
}

int main(void)
{
    if (score_award_grants_every_crossed_bonus_life() != 0) return 1;
    if (score_award_ignores_invalid_bonus_cadence() != 0) return 1;

    puts("gameplay_score_test: ok");
    return 0;
}
