/*
 * game_input.h — Input system public interface.
 *
 * Handles background gamepad initialization and input-related utilities.
 */
#pragma once

#include "../game.h"
#include "../core/game_profile.h"

/* Physical input bits use PLAYER_INPUT_* values plus this confirmation bit. */
#define GAME_INPUT_CONFIRM (1u << 6)

typedef struct {
    unsigned int keyboard_mask;
    unsigned int controller_mask;
} GameInputPhysicalState;

/* AppSession owns subsystem initialization. These helpers only open/close handles. */
void game_input_set_controller_init_pending(GameState *gs, int pending);
void gamepad_refresh_controller(GameState *gs);
void gamepad_close_controller(GameState *gs);

/* Read physical controls without consuming SDL events. */
void game_input_read_physical(SDL_GameController *controller,
                              GameInputPhysicalState *state);
void game_input_read_bound(SDL_GameController *controller, const GameSettings *settings,
                            GameInputPhysicalState *state);
unsigned int game_input_keyboard_mask(const Uint8 *keys, const GameSettings *settings);
unsigned int game_input_controller_mask(const Uint8 *buttons, Sint16 x, Sint16 y,
                                         const GameSettings *settings);

/* Arm a route gate. Inherited state covers a controller closed during a swap. */
void game_input_arm_release_latch(GameState *gs,
                                  const GameInputPhysicalState *inherited);

/* Return physical gameplay input, suppressing held route-confirm inputs. */
unsigned int game_input_sample(GameState *gs);

/* Forget a controller latch when its device disappears. */
void game_input_clear_controller_latch(GameState *gs);

/* Narrow deterministic seam used by input/session tests; production leaves it off. */
void game_input_test_set_physical_state(unsigned int keyboard_mask,
                                         unsigned int controller_mask);
void game_input_test_clear_physical_state(void);
