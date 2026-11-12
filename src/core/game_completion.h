/*
 * game_completion.h — Level completion summary helpers.
 */

#pragma once

#include <stdint.h>       /* uint32_t */

#include "../game_fwd.h"  /* GameState, used by pointer only */

/*
 * The level timer — how long the current attempt has been played.
 *
 * The game counts it in fixed steps (completion.level_steps, one per
 * game_update_active), and works seconds out only when someone asks:
 * the summary, the best time, the ghost, a saved Continue point. Adding
 * 1/60 s to a float on every step would round a little each time, and
 * the error grows with the run: ten minutes read 599.90 s, an hour
 * 3597.2 s. One division of a whole number is exact.
 */

/* The level timer in seconds: level_steps / TARGET_FPS. */
float game_level_seconds(const GameState *gs);

/* A time in seconds (from a saved Continue point) as a whole number of
 * steps, rounded to the nearest. Negative or not-a-number gives 0; a time
 * too long for the count gives the largest count. */
uint32_t game_level_steps_from_seconds(float seconds);

void game_completion_reset_summary(GameState *gs);
