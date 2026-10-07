/*
 * coin_overflow.c — Debugging lab: writing past the end of a fixed array.
 *
 * The game keeps entities in fixed-size arrays (for example
 * `Coin coins[MAX_COINS]` inside GameState). It refuses a level whose count
 * is larger than the array before it copies anything: see
 * level_validate_counts() in src/levels/level_validate.c. This file leaves
 * that check out on purpose, so the copy loop runs off the end of the array.
 *
 * This file is NOT part of the game build. Compile it on its own from the
 * repository root:
 *
 *   mkdir -p out/labs
 *   cc -std=c11 -g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer \
 *      labs/debugging/coin_overflow.c -o out/labs/coin_overflow
 *   ./out/labs/coin_overflow
 *
 * The walkthrough is in docs/wiki/debugging-c.md.
 */
#include <stdio.h>
#include <stdlib.h>

#define MAX_COINS 4

/* What the level file says: where a coin starts. */
typedef struct {
    float x, y;
} CoinPlacement;

/* What the running game keeps for each coin. */
typedef struct {
    float x, y;
    int   active;
} Coin;

/*
 * A cut-down GameState. The array is the LAST member on purpose: an
 * overflow then leaves the allocation, which AddressSanitizer can see.
 * If another field followed the array, the extra coin would silently
 * overwrite that field instead, and no sanitizer would notice. That is
 * why the real game checks counts before copying.
 */
typedef struct {
    int  coin_count;
    Coin coins[MAX_COINS];
} CoinWorld;

static void load_coins(CoinWorld *world, const CoinPlacement *placements, int count)
{
    /* BUG: nothing compares count with MAX_COINS before this loop. */
    for (int i = 0; i < count; i++) {
        world->coins[i].x      = placements[i].x;
        world->coins[i].y      = placements[i].y;
        world->coins[i].active = 1;
    }
    world->coin_count = count;
}

int main(void)
{
    /* Five coins in the "level file", room for four in the game. */
    const CoinPlacement placements[] = {
        { 32.0f, 200.0f },
        { 64.0f, 200.0f },
        { 96.0f, 200.0f },
        { 128.0f, 200.0f },
        { 160.0f, 200.0f },
    };
    int count = (int)(sizeof(placements) / sizeof(placements[0]));

    CoinWorld *world = calloc(1, sizeof(*world));
    if (!world) return 1;

    load_coins(world, placements, count);
    printf("loaded %d coins\n", world->coin_count);

    free(world);
    return 0;
}
