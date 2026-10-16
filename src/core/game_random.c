#include "game_random.h"
static uint32_t state = 1;
void game_random_seed(uint32_t seed) { state = seed ? seed : UINT32_C(0x9e3779b9); }
uint32_t game_random(void)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}
float game_random_unit(void) { return (float)(game_random() >> 8) / 16777215.0f; }
