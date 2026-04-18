/*
 * game_input.c — Input system implementation.
 *
 * Handles background gamepad initialization and input-related utilities.
 */

#include "game_input.h"

#include <stdio.h>  /* fprintf, stderr */

int ctrl_init_worker(void *data)
{
    GameState *gs = (GameState *)data;
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "Warning: SDL_INIT_GAMECONTROLLER failed: %s — "
                        "gamepad support unavailable\n", SDL_GetError());
    }
    /*
     * Atomic write: signal the main thread that the subsystem is ready.
     * volatile ensures the compiler does not cache this value in a register.
     */
    gs->ctrl_init_done = 1;
    return 0;
}
