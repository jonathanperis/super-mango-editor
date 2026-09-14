/*
 * game_web_input.c — Semantic touch input and browser keyboard-state repair.
 */

#include "game_web_input.h"

#include <SDL.h>  /* SDL_FlushEvents — discard synthetic keyup events */

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
    static const SDL_Keycode keys[GAME_TOUCH_COUNT] = {
        SDLK_LEFT, SDLK_RIGHT, SDLK_UP, SDLK_DOWN,
        SDLK_SPACE, SDLK_LSHIFT, SDLK_ESCAPE, SDLK_F1
    };
    if (action < 0 || action >= GAME_TOUCH_COUNT || (pressed != 0 && pressed != 1)) return 0;
    unsigned int bit = 1u << action;
    if (!!(touch_buttons & bit) == pressed) return 1;
    /* Releases must clear holds even if SDL has already shut down. */
    if (!pressed) touch_buttons &= ~bit;
    if (!SDL_WasInit(SDL_INIT_EVENTS)) return 0;
    SDL_Event event;
    SDL_zero(event);
    event.type = pressed ? SDL_KEYDOWN : SDL_KEYUP;
    event.key.state = pressed ? SDL_PRESSED : SDL_RELEASED;
    event.key.keysym.sym = keys[action];
    event.key.keysym.scancode = SDL_GetScancodeFromKey(keys[action]);
    SDL_Window *window = SDL_GetKeyboardFocus();
    if (window) event.key.windowID = SDL_GetWindowID(window);
    if (SDL_PushEvent(&event) <= 0) return 0;
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
 * SDL2's Emscripten backend can miss a keyup when focus moves away. Reset all
 * scancodes before a new scene, not just the default bindings. Touch holds and
 * buffered taps also belong to their originating scene and are released here.
 */
void game_web_input_flush_stale_keys(void)
{
    game_web_input_clear_touch();
#ifdef __EMSCRIPTEN__
    /* Reset every scancode, including user-remapped keys. The pinned SDL port
     * supports SDL_ResetKeyboard; no DOM-code translation table is needed. */
    SDL_ResetKeyboard();
    SDL_FlushEvents(SDL_KEYDOWN, SDL_KEYUP);
#endif
}
