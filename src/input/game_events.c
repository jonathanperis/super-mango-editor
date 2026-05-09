/*
 * game_events.c — Drain SDL events and update high-level game state.
 */

#include "game_events.h"

#include <SDL.h>
#include <SDL_mixer.h>

static void continue_after_completion(GameState *gs)
{
    if (gs->completion_pending_next_phase) {
        if (game_load_next_phase(gs) == 0) return;
    }

    gs->running = 0;
}

static void handle_controller_removed(GameState *gs, const SDL_ControllerDeviceEvent *event)
{
    if (gs->controller) {
        SDL_Joystick *joy = SDL_GameControllerGetJoystick(gs->controller);
        if (SDL_JoystickInstanceID(joy) == event->which) {
            SDL_GameControllerClose(gs->controller);
            gs->controller = NULL;
        }
    }
}

static void handle_window_event(GameState *gs, const SDL_WindowEvent *event)
{
    if (event->event == SDL_WINDOWEVENT_FOCUS_LOST) {
        gs->paused = 1;
        Mix_PauseMusic();
    } else if (event->event == SDL_WINDOWEVENT_FOCUS_GAINED) {
        gs->paused = 0;
        Mix_ResumeMusic();
        gs->loop.prev_ticks = SDL_GetTicks64();
    }
}

void game_handle_events(GameState *gs)
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            gs->running = 0;

        } else if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                gs->running = 0;
            } else if (gs->level_complete &&
                       (event.key.keysym.sym == SDLK_RETURN ||
                        event.key.keysym.sym == SDLK_SPACE)) {
                continue_after_completion(gs);
            }

        } else if (event.type == SDL_CONTROLLERDEVICEADDED) {
            if (!gs->controller) {
                gs->controller = SDL_GameControllerOpen(event.cdevice.which);
            }

        } else if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
            handle_controller_removed(gs, &event.cdevice);

        } else if (event.type == SDL_CONTROLLERBUTTONDOWN) {
            if (event.cbutton.button == SDL_CONTROLLER_BUTTON_START) {
                if (gs->level_complete) continue_after_completion(gs);
                else gs->running = 0;
            }

        } else if (event.type == SDL_WINDOWEVENT) {
            handle_window_event(gs, &event.window);
        }
    }
}
