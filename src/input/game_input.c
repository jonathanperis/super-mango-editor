/*
 * game_input.c — Input system implementation.
 *
 * Handles background gamepad initialization and input-related utilities.
 */

#include "game_input.h"
#include "game_web_input.h"

#include <SDL.h>

static int s_test_physical_override;
static GameInputPhysicalState s_test_physical_state;

void game_input_test_set_physical_state(unsigned int keyboard_mask,
                                         unsigned int controller_mask)
{
    s_test_physical_override = 1;
    s_test_physical_state.keyboard_mask = keyboard_mask;
    s_test_physical_state.controller_mask = controller_mask;
}

void game_input_test_clear_physical_state(void)
{
    s_test_physical_override = 0;
    s_test_physical_state.keyboard_mask = 0;
    s_test_physical_state.controller_mask = 0;
}

unsigned int game_input_keyboard_mask(const Uint8 *keys, const GameSettings *settings)
{
    static const GameSettings defaults = GAME_SETTINGS_DEFAULTS;
    const GameSettings *s = settings ? settings : &defaults;
    unsigned int mask = 0;
    /* Action order matches PLAYER_INPUT_* bit positions. Arrows remain fixed
     * alternatives so remapping can never strand menu/navigation controls. */
    for (int i = 0; i < PROFILE_ACTION_COUNT; i++) if (keys[s->keys[i]]) mask |= 1u << i;
    if (keys[SDL_SCANCODE_LEFT]) mask |= PLAYER_INPUT_LEFT;
    if (keys[SDL_SCANCODE_RIGHT]) mask |= PLAYER_INPUT_RIGHT;
    if (keys[SDL_SCANCODE_UP]) mask |= PLAYER_INPUT_UP;
    if (keys[SDL_SCANCODE_DOWN]) mask |= PLAYER_INPUT_DOWN;
    if (keys[SDL_SCANCODE_RSHIFT]) mask |= PLAYER_INPUT_RUN;
    if (keys[SDL_SCANCODE_SPACE] || keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_KP_ENTER]) mask |= GAME_INPUT_CONFIRM;
    return mask;
}

unsigned int game_input_controller_mask(const Uint8 *buttons, Sint16 x, Sint16 y,
                                         const GameSettings *settings)
{
    static const GameSettings defaults = GAME_SETTINGS_DEFAULTS;
    const GameSettings *s = settings ? settings : &defaults;
    unsigned int mask = 0;
    for (int i = 0; i < PROFILE_ACTION_COUNT; i++) if (buttons[s->buttons[i]]) mask |= 1u << i;
    if (buttons[SDL_CONTROLLER_BUTTON_A] || buttons[SDL_CONTROLLER_BUTTON_START]) mask |= GAME_INPUT_CONFIRM;
    if (x < -s->dead_zone) mask |= PLAYER_INPUT_LEFT;
    if (x > s->dead_zone) mask |= PLAYER_INPUT_RIGHT;
    if (y < -s->dead_zone) mask |= PLAYER_INPUT_UP;
    if (y > s->dead_zone) mask |= PLAYER_INPUT_DOWN;
    return mask;
}

void game_input_read_bound(SDL_GameController *controller, const GameSettings *settings,
                            GameInputPhysicalState *state)
{
    const Uint8 *keys;

    if (!state) return;
    state->keyboard_mask = 0;
    state->controller_mask = 0;

    if (s_test_physical_override) {
        *state = s_test_physical_state;
        return;
    }

    keys = SDL_GetKeyboardState(NULL);
    state->keyboard_mask = game_input_keyboard_mask(keys, settings);

    if (!controller) return;
    Uint8 buttons[SDL_CONTROLLER_BUTTON_MAX];
    for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++)
        buttons[i] = SDL_GameControllerGetButton(controller, (SDL_GameControllerButton)i);
    state->controller_mask = game_input_controller_mask(buttons,
        SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX),
        SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY), settings);
}

void game_input_read_physical(SDL_GameController *controller, GameInputPhysicalState *state)
{
    game_input_read_bound(controller, NULL, state);
}

void game_input_arm_release_latch(GameState *gs,
                                  const GameInputPhysicalState *inherited)
{
    GameInputPhysicalState current;

    if (!gs) return;
    game_web_input_clear_touch();
    game_input_read_bound(gs->controller, gs->profile ? &gs->profile->data.settings : NULL, &current);
    gs->input_release_keyboard_mask = current.keyboard_mask;
    gs->input_release_controller_mask = current.controller_mask;
    if (inherited) {
        gs->input_release_keyboard_mask |= inherited->keyboard_mask;
        gs->input_release_controller_mask |= inherited->controller_mask;
    }
    gs->input_release_latched =
        gs->input_release_keyboard_mask != 0 ||
        gs->input_release_controller_mask != 0;
}

unsigned int game_input_sample(GameState *gs)
{
    GameInputPhysicalState current;
    int keyboard_held;
    int controller_held;

    if (!gs) return 0;
    game_input_read_bound(gs->controller, gs->profile ? &gs->profile->data.settings : NULL, &current);
    if (!gs->input_release_latched)
        return current.keyboard_mask | current.controller_mask;

    keyboard_held = (current.keyboard_mask & gs->input_release_keyboard_mask) != 0;
    if (s_test_physical_override) {
        controller_held =
            (current.controller_mask & gs->input_release_controller_mask) != 0;
    } else if (gs->input_release_controller_mask && !gs->controller) {
        /* Keep the gate closed until AppSession publishes controller readiness. */
        controller_held = gs->controller_init_pending != 0;
    } else {
        controller_held =
            (current.controller_mask & gs->input_release_controller_mask) != 0;
    }

    if (keyboard_held || controller_held) return 0;

    gs->input_release_keyboard_mask = 0;
    gs->input_release_controller_mask = 0;
    gs->input_release_latched = 0;
    return current.keyboard_mask | current.controller_mask;
}

void game_input_clear_controller_latch(GameState *gs)
{
    if (!gs) return;
    gs->input_release_controller_mask = 0;
    gs->input_release_latched = gs->input_release_keyboard_mask != 0;
}

void game_input_set_controller_init_pending(GameState *gs, int pending)
{
    if (gs) gs->controller_init_pending = pending != 0;
}

void gamepad_refresh_controller(GameState *gs)
{
    if (!gs || gs->controller || gs->controller_init_pending ||
        SDL_WasInit(SDL_INIT_GAMECONTROLLER) == 0)
        return;
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            gs->controller = SDL_GameControllerOpen(i);
            if (gs->controller) break;
        }
    }
}

void gamepad_close_controller(GameState *gs)
{
    if (!gs) return;
    if (gs->controller) {
        SDL_GameControllerClose(gs->controller);
        gs->controller = NULL;
    }
}
