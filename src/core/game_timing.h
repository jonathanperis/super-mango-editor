/*
 * game_timing.h — Frame timing and loop bookkeeping helpers.
 */

#pragma once

#include <SDL.h>

#include "../game.h"

/* Advance frame clock and return clamped delta time in seconds. */
float game_timing_step(GameState *gs, Uint64 *frame_start_ticks);

/* Apply native manual frame cap fallback when the frame finishes early. */
void game_timing_cap_frame(Uint64 frame_start_ticks, Uint32 frame_ms);

/* Count down smoke-test frames and stop the game when the budget reaches zero. */
void game_timing_tick_smoke(GameState *gs);
