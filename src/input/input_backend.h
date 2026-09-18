/* Input adapter: backend callbacks create ordered commands; gameplay also
 * samples held state. Keeping the two concepts separate preserves quick taps
 * and text/shortcut order without requiring a command for every held frame. */
#pragma once
#include <raylib.h>
#include <stdint.h>

/* Button IDs are the existing profile wire format, not raylib enum values. */
enum {
    PAD_A, PAD_B, PAD_X, PAD_Y, PAD_BACK, PAD_GUIDE, PAD_START,
    PAD_LEFT_STICK, PAD_RIGHT_STICK, PAD_LEFT_SHOULDER, PAD_RIGHT_SHOULDER,
    PAD_UP, PAD_DOWN, PAD_LEFT, PAD_RIGHT, PAD_MISC,
    PAD_PADDLE1, PAD_PADDLE2, PAD_PADDLE3, PAD_PADDLE4, PAD_TOUCHPAD, PAD_COUNT
};
enum { INPUT_SHIFT = 1, INPUT_CTRL = 2, INPUT_ALT = 4, INPUT_SUPER = 8 };
typedef enum {
    INPUT_QUIT, INPUT_KEY_DOWN, INPUT_KEY_UP, INPUT_TEXT,
    INPUT_MOUSE_DOWN, INPUT_MOUSE_UP, INPUT_MOUSE_MOVE, INPUT_WHEEL,
    INPUT_FOCUS, INPUT_PAD_ADDED, INPUT_PAD_REMOVED, INPUT_PAD_DOWN, INPUT_PAD_UP
} InputKind;
typedef struct {
    InputKind type;
    /* key is raylib/GLFW's key; binding is the saved profile wire ID.
     * mods/repeat belong to this event, not a later sample of the keyboard. */
    int key, binding, mods, repeat;
    int button, device, focused;
    int x, y;                 /* already converted to logical canvas pixels */
    float wheel;
    char text[8];              /* one NUL-terminated UTF-8 codepoint */
} InputEvent;

/* Register after opening a window. Width/height describe the logical target,
 * not necessarily the physical window. Close before destroying that window. */
void input_open(int width, int height);
void input_close(void);
int input_ready(void);
/* Collect focus/gamepad changes once per frame. EndDrawing has already polled
 * backend callbacks; this function must not poll the OS a second time. */
void input_collect(void);
/* Copy a command into the bounded queue; returns 0 when unavailable/full. */
int input_push(const InputEvent *event);
/* Copy/dequeue the next command; returns 0 when the queue is empty. */
int input_poll(InputEvent *event);
void input_clear(void);
/* Clear queued commands and suppress currently held keys until released.
 * A key held while changing screens must not trigger the next screen. */
void input_clear_keys(void);
Vector2 input_mouse(void);
Vector2 input_pointer_to_logical(Vector2 position);
int input_modifiers(void);
int input_first_gamepad(void); /* device index + 1, zero when unavailable */
/* Translation is explicit: profile numbers are never cast to raylib enums.
 * A known legacy binding may translate to KEY_NULL (preserve it and warn). */
int input_key_from_binding(int binding);
int input_binding_from_key(int key);
int input_binding_known(int binding);
const char *input_binding_name(int binding);
int input_pad_button(int binding);
const char *input_pad_name(int binding);
int input_key_down(int binding);
