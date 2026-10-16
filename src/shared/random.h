/*
 * random.h — Small reproducible random streams, owned by their caller.
 *
 * Each RandomStream is an independent xorshift32 generator. Explicit
 * unsigned arithmetic gives the same numbers on native and WASM builds.
 *
 * Owning the state (instead of one hidden global, as raylib's
 * GetRandomValue has) lets a program keep separate streams: drawing more
 * numbers from one never shifts the numbers another produces.
 *
 * A zero-initialized stream is valid and behaves as if seeded with 1.
 */
#pragma once
#include <stdint.h>

typedef struct {
    uint32_t state;
} RandomStream;

/* Start the stream over from seed. Seed 0 is replaced by a fixed nonzero
 * value, because xorshift would otherwise return 0 forever. */
void random_seed(RandomStream *stream, uint32_t seed);

/* Next 32-bit value. */
uint32_t random_next(RandomStream *stream);

/* Next value in [0, 1], from the top 24 bits. */
float random_unit(RandomStream *stream);
