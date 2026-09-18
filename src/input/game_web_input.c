/*
 * game_web_input.c — Semantic touch input and browser keyboard-state repair.
 */

#include "game_web_input.h"

#include "input_backend.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>  /* emscripten_run_script — run JavaScript on canvas */
#endif

static unsigned int touch_buttons;
static unsigned int touch_pressed;

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
int game_web_input_touch(int action, int pressed)
{
    static const int keys[GAME_TOUCH_COUNT] = {
        KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN,
        KEY_SPACE, KEY_LEFT_SHIFT, KEY_ESCAPE, KEY_F1
    };
    if (action < 0 || action >= GAME_TOUCH_COUNT || (pressed != 0 && pressed != 1)) return 0;
    unsigned int bit = 1u << action;
    if (!!(touch_buttons & bit) == pressed) return 1;
    /* Releases must clear holds even after the input owner has shut down. */
    if (!pressed) touch_buttons &= ~bit;
    if (!input_ready()) return 0;
    InputEvent event = {.type = pressed ? INPUT_KEY_DOWN : INPUT_KEY_UP,
                        .key = keys[action], .binding = input_binding_from_key(keys[action])};
    if (!input_push(&event)) return 0;
    if (pressed) {
        touch_buttons |= bit;
        if (action <= GAME_TOUCH_RUN) touch_pressed |= bit;
    }
    return 1;
}

unsigned int game_web_input_take_touch_mask(void)
{
    unsigned int result = (touch_buttons | touch_pressed) & ((1u << (GAME_TOUCH_RUN + 1)) - 1);
    touch_pressed = 0;
    return result;
}

void game_web_input_clear_touch(void)
{
    for (int action = 0; action < GAME_TOUCH_COUNT; action++)
        if (touch_buttons & (1u << action)) (void)game_web_input_touch(action, 0);
    touch_buttons = touch_pressed = 0;
#ifdef __EMSCRIPTEN__
    emscripten_run_script("if (Module.clearTouchInput) Module.clearTouchInput();");
#endif
}

/*
 * game_web_input_flush_stale_keys — Clear stuck movement keys on startup.
 *
 * Browser focus changes can miss a keyup. Reset all held keys before a new
 * scene, not just the default bindings. Touch holds and
 * buffered taps also belong to their originating scene and are released here.
 */
void game_web_input_flush_stale_keys(void)
{
    game_web_input_clear_touch();
    input_clear_keys();
}
