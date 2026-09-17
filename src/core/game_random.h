/* Small reproducible PRNG shared by simulation and decorative initialization.
 * Explicit unsigned arithmetic defines the same stream on native and WASM. */
#pragma once
#include <stdint.h>
void game_random_seed(uint32_t seed);
uint32_t game_random(void);
float game_random_unit(void);
