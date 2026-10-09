/*
 * game_input.c — Input system implementation.
 *
 * Converts device samples into remappable gameplay action masks.
 */

#include "game_input.h"
#include "../game.h"  /* GameState: this file reads its fields */
#include "game_web_input.h"

#include "input_backend.h"

static int s_test_physical_override;
static GameInputPhysicalState s_test_physical_state;

#ifdef MANGO_TESTING
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
#endif

unsigned int game_input_keyboard_mask(const uint8_t *keys, const GameSettings *settings)
{
    static const GameSettings defaults = GAME_SETTINGS_DEFAULTS;
    const GameSettings *s = settings ? settings : &defaults;
    unsigned int mask = 0;
    /* Action order matches PLAYER_INPUT_* bit positions. Arrows remain fixed
     * alternatives so remapping can never strand menu/navigation controls. */
    for (int i = 0; i < PROFILE_ACTION_COUNT; i++) if (keys[s->keys[i]]) mask |= 1u << i;
    if (keys[BINDING_KEY_LEFT]) mask |= PLAYER_INPUT_LEFT;
    if (keys[BINDING_KEY_RIGHT]) mask |= PLAYER_INPUT_RIGHT;
    if (keys[BINDING_KEY_UP]) mask |= PLAYER_INPUT_UP;
    if (keys[BINDING_KEY_DOWN]) mask |= PLAYER_INPUT_DOWN;
    if (keys[BINDING_KEY_RIGHT_SHIFT]) mask |= PLAYER_INPUT_RUN;
    if (keys[BINDING_KEY_SPACE] || keys[BINDING_KEY_ENTER] || keys[BINDING_KEY_KP_ENTER])
        mask |= GAME_INPUT_CONFIRM;
    return mask;
}

unsigned int game_input_controller_mask(const uint8_t *buttons, int16_t x, int16_t y,
                                         const GameSettings *settings)
{
    static const GameSettings defaults = GAME_SETTINGS_DEFAULTS;
    const GameSettings *s = settings ? settings : &defaults;
    unsigned int mask = 0;
    for (int i = 0; i < PROFILE_ACTION_COUNT; i++) if (buttons[s->buttons[i]]) mask |= 1u << i;
    if (buttons[PAD_A] || buttons[PAD_START]) mask |= GAME_INPUT_CONFIRM;
    if (x < -s->dead_zone) mask |= PLAYER_INPUT_LEFT;
    if (x > s->dead_zone) mask |= PLAYER_INPUT_RIGHT;
    if (y < -s->dead_zone) mask |= PLAYER_INPUT_UP;
    if (y > s->dead_zone) mask |= PLAYER_INPUT_DOWN;
    return mask;
}

void game_input_read_bound(int controller, const GameSettings *settings,
                            GameInputPhysicalState *state)
{
    uint8_t keys[512] = {0};

    if (!state) return;
    state->keyboard_mask = 0;
    state->controller_mask = 0;

    if (s_test_physical_override) {
        *state = s_test_physical_state;
        return;
    }

    for (int binding = 4; binding <= 290; binding++) keys[binding] = (uint8_t)input_key_down(binding);
    state->keyboard_mask = game_input_keyboard_mask(keys, settings);

    if (!controller || !IsWindowReady() || !IsGamepadAvailable(controller-1)) return;
    uint8_t buttons[PAD_COUNT] = {0};
    for (int i = 0; i < PAD_COUNT; i++) {
        int button = input_pad_button(i);
        if (button) buttons[i] = IsGamepadButtonDown(controller-1, button);
    }
    float x = GetGamepadAxisMovement(controller-1, GAMEPAD_AXIS_LEFT_X);
    float y = GetGamepadAxisMovement(controller-1, GAMEPAD_AXIS_LEFT_Y);
    state->controller_mask = game_input_controller_mask(buttons,
        (int16_t)(x*(x < 0 ? 32768.0f : 32767.0f)),
        (int16_t)(y*(y < 0 ? 32768.0f : 32767.0f)), settings);
}

void game_input_read_physical(int controller, GameInputPhysicalState *state)
{
    game_input_read_bound(controller, NULL, state);
}

void game_input_arm_release_latch(GameState *gs,
                                  const GameInputPhysicalState *inherited)
{
    GameInputPhysicalState current;

    if (!gs) return;
    game_web_input_clear_touch();
    game_input_read_bound(gs->screen.controller, gs->screen.profile ? &gs->screen.profile->data.settings : NULL, &current);
    gs->screen.input_release_keyboard_mask = current.keyboard_mask;
    gs->screen.input_release_controller_mask = current.controller_mask;
    if (inherited) {
        gs->screen.input_release_keyboard_mask |= inherited->keyboard_mask;
        gs->screen.input_release_controller_mask |= inherited->controller_mask;
    }
    gs->screen.input_release_latched =
        gs->screen.input_release_keyboard_mask != 0 ||
        gs->screen.input_release_controller_mask != 0;
}

unsigned int game_input_sample(GameState *gs)
{
    GameInputPhysicalState current;
    int keyboard_held;
    int controller_held;

    if (!gs) return 0;
    game_input_read_bound(gs->screen.controller, gs->screen.profile ? &gs->screen.profile->data.settings : NULL, &current);
    if (!gs->screen.input_release_latched)
        return current.keyboard_mask | current.controller_mask;

    keyboard_held = (current.keyboard_mask & gs->screen.input_release_keyboard_mask) != 0;
    controller_held = (current.controller_mask & gs->screen.input_release_controller_mask) != 0;

    if (keyboard_held || controller_held) return 0;

    gs->screen.input_release_keyboard_mask = 0;
    gs->screen.input_release_controller_mask = 0;
    gs->screen.input_release_latched = 0;
    return current.keyboard_mask | current.controller_mask;
}

void game_input_clear_controller_latch(GameState *gs)
{
    if (!gs) return;
    gs->screen.input_release_controller_mask = 0;
    gs->screen.input_release_latched = gs->screen.input_release_keyboard_mask != 0;
}

void gamepad_adopt_controller(GameState *gs, int device)
{
    /* Keep the controller already in use; removal commands clear the old
     * device and its latch before another can be adopted. */
    if (gs && !gs->screen.controller && device) gs->screen.controller = device;
}

void gamepad_refresh_controller(GameState *gs)
{
    gamepad_adopt_controller(gs, input_first_gamepad());
}

int game_input_event_confirms(const InputEvent *event)
{
    if (!event) return 0;
    if (event->type == INPUT_KEY_DOWN)
        return event->key == KEY_ENTER || event->key == KEY_KP_ENTER || event->key == KEY_SPACE;
    /* Checking the type matters: a mouse event's button 0 is not PAD_A. */
    if (event->type == INPUT_PAD_DOWN)
        return event->button == PAD_A || event->button == PAD_START;
    return 0;
}

void gamepad_close_controller(GameState *gs)
{
    if (!gs) return;
    gs->screen.controller = 0;
}
