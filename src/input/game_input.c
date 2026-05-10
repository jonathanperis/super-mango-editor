/*
 * game_input.c — Input system implementation.
 *
 * Handles background gamepad initialization and input-related utilities.
 */

#include "game_input.h"

#include <SDL_ttf.h> /* TTF_RenderText_Solid — gamepad init HUD message */
#include <stdio.h>  /* fprintf, stderr */

int ctrl_init_worker(void *data)
{
    GameState *gs = (GameState *)data;
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "Warning: SDL_INIT_GAMECONTROLLER failed: %s — "
                        "gamepad support unavailable\n", SDL_GetError());
    }
    /* Atomic write: signal the main thread that the subsystem is ready. */
    SDL_AtomicSet(&gs->ctrl_init_done, 1);
    return 0;
}

void gamepad_update_deferred_init(GameState *gs)
{
#ifndef __EMSCRIPTEN__
    if (gs->ctrl_pending_init == 1) {
        SDL_AtomicSet(&gs->ctrl_init_done, 0);
        gs->ctrl_init_thread = SDL_CreateThread(ctrl_init_worker, "ctrl_init", gs);
        if (!gs->ctrl_init_thread) {
            fprintf(stderr, "Warning: could not start gamepad init thread: %s\n",
                    SDL_GetError());
            gs->ctrl_pending_init = 0;  /* give up gracefully */
        } else {
            gs->ctrl_pending_init = 2;

            if (gs->hud.font) {
                SDL_Color white = {255, 255, 255, 200};
                SDL_Surface *surf = TTF_RenderText_Solid(
                    gs->hud.font, "A inicializar controle...", white);
                if (surf) {
                    gs->textures.ctrl_init_msg =
                        SDL_CreateTextureFromSurface(gs->renderer, surf);
                    SDL_FreeSurface(surf);
                }
            }
        }
    } else if (gs->ctrl_pending_init == 2 &&
               SDL_AtomicGet(&gs->ctrl_init_done)) {
        SDL_WaitThread(gs->ctrl_init_thread, NULL);
        gs->ctrl_init_thread = NULL;

        if (!gs->controller) {
            for (int i = 0; i < SDL_NumJoysticks(); i++) {
                if (SDL_IsGameController(i)) {
                    gs->controller = SDL_GameControllerOpen(i);
                    if (gs->controller) break;
                }
            }
        }
        gs->ctrl_pending_init = 0;

        if (gs->textures.ctrl_init_msg) {
            SDL_DestroyTexture(gs->textures.ctrl_init_msg);
            gs->textures.ctrl_init_msg = NULL;
        }
    }
#else
    (void)gs;
#endif
}

void gamepad_schedule_deferred_init(GameState *gs)
{
#ifndef __EMSCRIPTEN__
    if (gs->smoke_test_frames == 0) {
        gs->ctrl_pending_init = 1;
    }
#else
    (void)gs;
#endif
}

void gamepad_cleanup(GameState *gs)
{
#ifndef __EMSCRIPTEN__
    if (gs->ctrl_init_thread) {
        SDL_WaitThread(gs->ctrl_init_thread, NULL);
        gs->ctrl_init_thread = NULL;
        gs->ctrl_pending_init = 0;
        SDL_AtomicSet(&gs->ctrl_init_done, 0);
    }
#endif

    if (gs->controller) {
        SDL_GameControllerClose(gs->controller);
        gs->controller = NULL;
    }
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
}
