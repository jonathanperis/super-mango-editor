/*
 * game_timing.h — Frame timing and loop bookkeeping helpers.
 */

#pragma once

#include <stdint.h>

#include "../game.h"

/* Advance frame clock and return clamped delta time in seconds. */
float game_timing_step(GameState *gs, uint64_t *frame_start_ticks);

/* Count down smoke-test frames and stop the game when the budget reaches zero. */
void game_timing_tick_smoke(GameState *gs);
