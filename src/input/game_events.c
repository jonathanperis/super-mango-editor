/*
 * game_events.c — Drain SDL events and update high-level game state.
 */

#include "game_events.h"

#include "../collision/collision_damage.h"
#include "../core/game_overlay.h"

#include <SDL.h>
#include <SDL_mixer.h>

static void continue_after_completion(GameState *gs)
{
    if (gs->completion.pending_next_phase) {
        if (game_load_next_phase(gs) == 0) return;
    }

    gs->running = 0;
}

static void confirm_game_over(GameState *gs)
{
    game_restart_after_game_over(gs);
    Mix_ResumeMusic();
    gs->loop.prev_ticks = SDL_GetTicks64();
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
        game_overlay_set_pause_reason(gs, GAME_PAUSE_REASON_FOCUS, 1);
        Mix_PauseMusic();
    } else if (event->event == SDL_WINDOWEVENT_FOCUS_GAINED) {
        game_overlay_set_pause_reason(gs, GAME_PAUSE_REASON_FOCUS, 0);
        if (game_overlay_state(gs) != GAME_OVERLAY_PAUSED) {
            Mix_ResumeMusic();
            gs->loop.prev_ticks = SDL_GetTicks64();
        }
    }
}

static void toggle_player_pause(GameState *gs)
{
    game_overlay_toggle_pause(gs);
    if (game_overlay_state(gs) == GAME_OVERLAY_PAUSED) {
        Mix_PauseMusic();
    } else {
        Mix_ResumeMusic();
        gs->loop.prev_ticks = SDL_GetTicks64();
    }
}

static void resume_player_pause(GameState *gs)
{
    if (game_overlay_state(gs) != GAME_OVERLAY_PAUSED) return;
    game_overlay_resume(gs);
    if (game_overlay_state(gs) != GAME_OVERLAY_PAUSED) {
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
            GameOverlayState overlay = game_overlay_state(gs);
            if (event.key.repeat) continue;
            if (overlay == GAME_OVERLAY_LEVEL_COMPLETE) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    gs->running = 0;
                } else if (event.key.keysym.sym == SDLK_RETURN ||
                           event.key.keysym.sym == SDLK_SPACE) {
                    continue_after_completion(gs);
                }
            } else if (overlay == GAME_OVERLAY_GAME_OVER) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    gs->running = 0;
                } else if (event.key.keysym.sym == SDLK_RETURN ||
                           event.key.keysym.sym == SDLK_SPACE) {
                    confirm_game_over(gs);
                }
            } else if (event.key.keysym.sym == SDLK_ESCAPE) {
                toggle_player_pause(gs);
            } else if (event.key.keysym.sym == SDLK_RETURN ||
                       event.key.keysym.sym == SDLK_SPACE) {
                resume_player_pause(gs);
            }

        } else if (event.type == SDL_CONTROLLERDEVICEADDED) {
            if (!gs->controller) {
                gs->controller = SDL_GameControllerOpen(event.cdevice.which);
            }

        } else if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
            handle_controller_removed(gs, &event.cdevice);

        } else if (event.type == SDL_CONTROLLERBUTTONDOWN) {
            if (event.cbutton.button == SDL_CONTROLLER_BUTTON_BACK &&
                game_overlay_state(gs) == GAME_OVERLAY_GAME_OVER) {
                gs->running = 0;
            } else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_START) {
                GameOverlayState overlay = game_overlay_state(gs);
                if (overlay == GAME_OVERLAY_LEVEL_COMPLETE) continue_after_completion(gs);
                else if (overlay == GAME_OVERLAY_GAME_OVER) confirm_game_over(gs);
                else toggle_player_pause(gs);
            }

        } else if (event.type == SDL_WINDOWEVENT) {
            handle_window_event(gs, &event.window);
        }
    }
}
