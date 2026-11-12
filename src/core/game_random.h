/*
 * game_random.h — Two small reproducible random number streams.
 *
 * Both are xorshift32 generators: three shift-and-XOR steps on a 32-bit
 * state, with explicit unsigned arithmetic, so native and WebAssembly
 * builds produce the same numbers from the same seed.
 *
 * Why two?  Some random draws decide what happens in the level (when a
 * fish jumps); others only decide how something looks (which fog image
 * drifts past, and how fast).  With one shared stream, every cosmetic draw
 * moved the gameplay draws along: a level's fog layers changed its fish,
 * and any effect that drew only on some machines or with some settings
 * (reduced motion skips the fog) would quietly make two runs with the
 * same seed and input differ.  Keeping them apart means a seeded run
 * depends only on the level and the input.
 *
 *   game_random / game_random_unit                    gameplay
 *   game_random_cosmetic / game_random_cosmetic_unit  looks only
 *
 * A draw that changes anything the simulation reads belongs to the first
 * pair.  When unsure, it is gameplay.
 */
#pragma once
#include <stdint.h>

/* Seed both streams from one number (0 picks a fixed non-zero seed: an
 * xorshift state of 0 would stay 0 forever).  The cosmetic stream starts
 * from a different state derived from the same seed. */
void game_random_seed(uint32_t seed);

/* The gameplay stream: the next 32 random bits, and a float in [0, 1]. */
uint32_t game_random(void);
float game_random_unit(void);

/* The cosmetic stream: the same two calls, for things that are only drawn. */
uint32_t game_random_cosmetic(void);
float game_random_cosmetic_unit(void);
