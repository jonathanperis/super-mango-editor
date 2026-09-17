/*
 * game_events.c — Drain SDL events and update high-level game state.
 */

#include "game_events.h"

#include "../collision/collision_damage.h"
#include "../core/game_overlay.h"
#include "../core/game_terminal.h"
#include "../core/game_inspector.h"
#include "game_input.h"
#include "../screens/settings_menu.h"

#include <SDL.h>
#include <SDL_mixer.h>

static int terminal_overlay(const GameState *gs)
{
    GameOverlayState overlay = game_overlay_state(gs);
    return overlay == GAME_OVERLAY_LEVEL_COMPLETE ||
           overlay == GAME_OVERLAY_GAME_OVER;
}

static void request_terminal_action(GameState *gs, GameTerminalAction action)
{
    if (!gs || gs->route != GAME_ROUTE_NONE) return;

    if (action == GAME_TERMINAL_ACTION_RETRY) {
        game_restart_after_game_over(gs);
        Mix_ResumeMusic();
        gs->loop.prev_ticks = SDL_GetTicks64();
        game_input_arm_release_latch(gs, NULL);
        return;
    }

    gs->route = game_terminal_action_route(action);
}

static void confirm_terminal(GameState *gs)
{
    request_terminal_action(gs, game_terminal_focused_action(gs));
}

static void move_terminal(GameState *gs, int direction)
{
    if (gs && gs->route == GAME_ROUTE_NONE) {
        game_terminal_move(gs, direction);
    }
}

static void handle_controller_removed(GameState *gs, const SDL_ControllerDeviceEvent *event)
{
    if (gs->controller) {
        SDL_Joystick *joy = SDL_GameControllerGetJoystick(gs->controller);
        if (SDL_JoystickInstanceID(joy) == event->which) {
            SDL_GameControllerClose(gs->controller);
            gs->controller = NULL;
            game_input_clear_controller_latch(gs);
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
        int was_open = gs->settings_menu && gs->settings_menu->open;
        if (gs->debug_mode && was_open && gs->settings_menu->capture == 1 && event.type == SDL_KEYDOWN &&
            ((event.key.keysym.sym >= SDLK_F2 && event.key.keysym.sym <= SDLK_F10) ||
             event.key.keysym.sym == SDLK_MINUS || event.key.keysym.sym == SDLK_EQUALS)) {
            SDL_strlcpy(gs->settings_menu->message, "Reserved for debug inspection; choose another key.",
                        sizeof(gs->settings_menu->message));
            continue;
        }
        if (gs->route == GAME_ROUTE_NONE && settings_menu_event(gs->settings_menu, gs->profile, &event,
                                 terminal_overlay(gs) ? -1 : SDL_CONTROLLER_BUTTON_BACK)) {
            if (was_open && !gs->settings_menu->open) {
                GameInputPhysicalState inherited = {0};
                if (event.type == SDL_CONTROLLERBUTTONDOWN &&
                    (event.cbutton.button == SDL_CONTROLLER_BUTTON_A || event.cbutton.button == SDL_CONTROLLER_BUTTON_START))
                    inherited.controller_mask = GAME_INPUT_CONFIRM;
                game_input_arm_release_latch(gs, &inherited);
            }
            continue;
        }
        if (game_inspector_event(gs, &event)) continue;
        if (event.type == SDL_QUIT) {
            if (gs->route == GAME_ROUTE_NONE) gs->route = GAME_ROUTE_EXIT;

        } else if (event.type == SDL_KEYDOWN) {
            if (event.key.repeat) continue;
            if (terminal_overlay(gs)) {
                if (event.key.keysym.sym == SDLK_UP ||
                    event.key.keysym.sym == SDLK_w) {
                    move_terminal(gs, -1);
                } else if (event.key.keysym.sym == SDLK_DOWN ||
                           event.key.keysym.sym == SDLK_s) {
                    move_terminal(gs, 1);
                } else if (event.key.keysym.sym == SDLK_ESCAPE) {
                    request_terminal_action(gs, GAME_TERMINAL_ACTION_EXIT);
                } else if (event.key.keysym.sym == SDLK_RETURN ||
                            event.key.keysym.sym == SDLK_KP_ENTER ||
                           event.key.keysym.sym == SDLK_SPACE) {
                    confirm_terminal(gs);
                }
            } else if (event.key.keysym.sym == SDLK_ESCAPE) {
                toggle_player_pause(gs);
            } else if (event.key.keysym.sym == SDLK_RETURN ||
                        event.key.keysym.sym == SDLK_KP_ENTER ||
                       event.key.keysym.sym == SDLK_SPACE) {
                resume_player_pause(gs);
            }

        } else if (event.type == SDL_CONTROLLERDEVICEADDED) {
            if (!gs->controller && !gs->controller_init_pending &&
                SDL_WasInit(SDL_INIT_GAMECONTROLLER) != 0 &&
                SDL_IsGameController(event.cdevice.which)) {
                gs->controller = SDL_GameControllerOpen(event.cdevice.which);
            }

        } else if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
            handle_controller_removed(gs, &event.cdevice);

        } else if (event.type == SDL_CONTROLLERBUTTONDOWN) {
            if (terminal_overlay(gs)) {
                if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_UP) {
                    move_terminal(gs, -1);
                } else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) {
                    move_terminal(gs, 1);
                } else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_BACK ||
                           event.cbutton.button == SDL_CONTROLLER_BUTTON_B) {
                    request_terminal_action(gs, GAME_TERMINAL_ACTION_EXIT);
                } else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_A ||
                           event.cbutton.button == SDL_CONTROLLER_BUTTON_START) {
                    confirm_terminal(gs);
                }
            } else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_START) {
                toggle_player_pause(gs);
            }

        } else if (event.type == SDL_WINDOWEVENT) {
            handle_window_event(gs, &event.window);
        }
    }
}
