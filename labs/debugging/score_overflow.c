/*
 * score_overflow.c — Debugging lab: signed integer overflow.
 *
 * Adding to an `int` past INT_MAX is undefined behaviour in C: the compiler
 * may assume it never happens. The game's game_award_score() in
 * src/core/game_score.c compares before adding (and does its bonus-life
 * arithmetic in int64_t) so the score saturates instead. This file shows the
 * naive version next to the safe one.
 *
 * This file is NOT part of the game build. Compile it on its own from the
 * repository root:
 *
 *   mkdir -p out/labs
 *   cc -std=c11 -g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer \
 *      labs/debugging/score_overflow.c -o out/labs/score_overflow
 *   ./out/labs/score_overflow
 *
 * The walkthrough is in docs/wiki/debugging-c.md.
 */
#include <limits.h>
#include <stdio.h>

static int award_naive(int score, int amount)
{
    return score + amount; /* BUG: overflows when score is near INT_MAX */
}

static int award_saturating(int score, int amount)
{
    /* The same test game_award_score() uses: compare first, then add. */
    if (amount <= 0) return score;
    return amount > INT_MAX - score ? INT_MAX : score + amount;
}

int main(void)
{
    int score = INT_MAX - 50; /* a long play session, or a hand-edited profile */
    int coin_score = 100;

    printf("naive:      %d\n", award_naive(score, coin_score));
    printf("saturating: %d\n", award_saturating(score, coin_score));
    return 0;
}
