/*
 * game_random.c — The gameplay and cosmetic random streams (game_random.h).
 */
#include "game_random.h"

/* A seed of 0 is replaced by this (the 32-bit golden-ratio constant). */
#define RANDOM_DEFAULT_SEED UINT32_C(0x9e3779b9)
/* XORed into the seed to start the cosmetic stream somewhere else. */
#define RANDOM_COSMETIC_SALT UINT32_C(0x85ebca6b)

static uint32_t s_gameplay = 1;
static uint32_t s_cosmetic = 1 ^ RANDOM_COSMETIC_SALT;

/* One xorshift32 step: scramble the state in place and return it. */
static uint32_t next(uint32_t *state)
{
    *state ^= *state << 13;
    *state ^= *state >> 17;
    *state ^= *state << 5;
    return *state;
}

/* The top 24 bits as a float in [0, 1]: a float holds 24 bits exactly. */
static float unit(uint32_t bits)
{
    return (float)(bits >> 8) / 16777215.0f;
}

void game_random_seed(uint32_t seed)
{
    s_gameplay = seed ? seed : RANDOM_DEFAULT_SEED;
    s_cosmetic = s_gameplay ^ RANDOM_COSMETIC_SALT;
    /* The salt could cancel the seed exactly; 0 must never be the state. */
    if (s_cosmetic == 0) s_cosmetic = RANDOM_DEFAULT_SEED;
}

uint32_t game_random(void) { return next(&s_gameplay); }
float game_random_unit(void) { return unit(game_random()); }

uint32_t game_random_cosmetic(void) { return next(&s_cosmetic); }
float game_random_cosmetic_unit(void) { return unit(game_random_cosmetic()); }
