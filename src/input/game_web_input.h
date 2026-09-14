/*
 * game_web_input.h — WebAssembly browser input helpers.
 *
 * Native builds include this header too, but the functions become no-ops
 * outside Emscripten so callers do not need platform-specific branches.
 */
#pragma once

enum {
    GAME_TOUCH_LEFT, GAME_TOUCH_RIGHT, GAME_TOUCH_UP, GAME_TOUCH_DOWN,
    GAME_TOUCH_JUMP, GAME_TOUCH_RUN, GAME_TOUCH_PAUSE, GAME_TOUCH_SETTINGS,
    GAME_TOUCH_COUNT
};

/* Semantic touch actions plus matching SDL events for menu controls. */
int game_web_input_touch(int action, int pressed);
unsigned int game_web_input_take_touch_mask(void);
void game_web_input_clear_touch(void);

/* Flush stale browser key state before the WebAssembly main loop starts. */
void game_web_input_flush_stale_keys(void);
