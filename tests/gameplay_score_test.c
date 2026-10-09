#include <stdio.h>
#include <limits.h>

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

    gs.world.score = 900;
    gs.world.lives = 3;
    gs.world.rules.score_per_life = 1000;
    gs.world.score_life_next = 1000;

    game_award_score(&gs, 2500);

    if (expect_int("score", gs.world.score, 3400) != 0) return 1;
    if (expect_int("lives", gs.world.lives, 6) != 0) return 1;
    if (expect_int("next life", gs.world.score_life_next, 4000) != 0) return 1;

    return 0;
}

static int score_award_ignores_invalid_bonus_cadence(void)
{
    GameState gs = {0};

    gs.world.score = 90;
    gs.world.lives = 2;
    gs.world.rules.score_per_life = 0;
    gs.world.score_life_next = 100;

    game_award_score(&gs, 25);

    if (expect_int("invalid cadence score", gs.world.score, 115) != 0) return 1;
    if (expect_int("invalid cadence lives", gs.world.lives, 2) != 0) return 1;
    if (expect_int("invalid cadence next life", gs.world.score_life_next, 100) != 0)
        return 1;

    return 0;
}

int main(void)
{
    if (score_award_grants_every_crossed_bonus_life() != 0) return 1;
    if (score_award_ignores_invalid_bonus_cadence() != 0) return 1;

    GameState gs = {0};
    gs.world.score = INT_MAX - 100;
    gs.world.score_life_next = INT_MAX - 100;
    gs.world.rules.score_per_life = 1000;
    gs.world.lives = INT_MAX;
    game_award_score(&gs, 200);
    if (expect_int("score saturates", gs.world.score, INT_MAX) ||
        expect_int("lives saturate", gs.world.lives, INT_MAX) ||
        expect_int("threshold exhausted", gs.world.score_life_next, 0)) return 1;
    game_award_score(&gs, 200);
    if (expect_int("exhausted does not wrap", gs.world.lives, INT_MAX)) return 1;

    puts("gameplay_score_test: ok");
    return 0;
}
