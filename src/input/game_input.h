/*
 * game_input.h — Input system public interface.
 *
 * Handles background gamepad initialization and input-related utilities.
 */
#pragma once

#include "../game.h"

/* ------------------------------------------------------------------ */
/* Background input initialization                                    */
/* ------------------------------------------------------------------ */

/*
 * ctrl_init_worker — background thread that calls SDL_InitSubSystem.
 *
 * SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) enumerates HID devices and
 * can block for 20-30 seconds on Windows when antivirus software is active.
 * Running it in a separate thread keeps the main loop responsive so the OS
 * does not show "application not responding" and the game continues drawing.
 *
 * Only SDL_InitSubSystem is called here — SDL gamepad functions
 * (SDL_NumJoysticks, SDL_GameControllerOpen) are not thread-safe and must
 * be called from the main thread after this thread finishes.
 *
 * This function is designed to be passed to SDL_CreateThread.
 */
int ctrl_init_worker(void *data);
