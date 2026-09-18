/* No desktop needed: exercise the production callback adapter against a small
 * public-GLFW registration fixture, including chaining and restoration. */
#include "input/input_backend.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <string.h>

static int window_ready, prior_keys, prior_chars, prior_buttons;
static GLFWkeyfun key_handler;
static GLFWcharfun char_handler;
static GLFWmousebuttonfun button_handler;
static GLFWcursorposfun cursor_handler;
static GLFWscrollfun scroll_handler;

bool test_input_window_ready(void) { return window_ready != 0; }
int test_input_screen_width(void) { return 800; }
int test_input_screen_height(void) { return 600; }
GLFWwindow *test_input_current_context(void) { return (GLFWwindow *)(uintptr_t)1; }
void test_input_cursor_pos(GLFWwindow *window, double *x, double *y)
{
    (void)window; *x = 400; *y = 368;
}
#define SET_CALLBACK(name, type, slot) \
    type name(GLFWwindow *window, type callback) { \
        (void)window; type old = slot; slot = callback; return old; \
    }
SET_CALLBACK(test_input_set_key, GLFWkeyfun, key_handler)
SET_CALLBACK(test_input_set_char, GLFWcharfun, char_handler)
SET_CALLBACK(test_input_set_button, GLFWmousebuttonfun, button_handler)
SET_CALLBACK(test_input_set_cursor, GLFWcursorposfun, cursor_handler)
SET_CALLBACK(test_input_set_scroll, GLFWscrollfun, scroll_handler)
#undef SET_CALLBACK

static void original_key(GLFWwindow *w, int key, int scan, int action, int mods)
{
    (void)w; (void)key; (void)scan; (void)action; (void)mods; prior_keys++;
}
static void original_char(GLFWwindow *w, unsigned int codepoint)
{
    (void)w; (void)codepoint; prior_chars++;
}
static void original_button(GLFWwindow *w, int button, int action, int mods)
{
    (void)w; (void)button; (void)action; (void)mods; prior_buttons++;
}

int input_backend_contract_test(void)
{
    window_ready = 1;
    key_handler = original_key;
    char_handler = original_char;
    button_handler = original_button;
    input_open(400, 300);
    input_open(400, 300); /* Reinitialization must not chain a callback to itself. */
    GLFWwindow *window = test_input_current_context();
    key_handler(window, KEY_SEVEN, 0, GLFW_PRESS, 0);
    char_handler(window, '7');
    key_handler(window, KEY_LEFT_CONTROL, 0, GLFW_PRESS, GLFW_MOD_CONTROL);
    key_handler(window, KEY_S, 0, GLFW_PRESS, GLFW_MOD_CONTROL);
    key_handler(window, KEY_LEFT_CONTROL, 0, GLFW_RELEASE, 0);
    button_handler(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
    button_handler(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE, 0);
    InputEvent expected[] = {
        {.type=INPUT_KEY_DOWN,.key=KEY_SEVEN}, {.type=INPUT_TEXT},
        {.type=INPUT_KEY_DOWN,.key=KEY_LEFT_CONTROL,.mods=INPUT_CTRL},
        {.type=INPUT_KEY_DOWN,.key=KEY_S,.mods=INPUT_CTRL},
        {.type=INPUT_KEY_UP,.key=KEY_LEFT_CONTROL},
        {.type=INPUT_MOUSE_DOWN,.button=MOUSE_BUTTON_LEFT,.x=200,.y=184},
        {.type=INPUT_MOUSE_UP,.button=MOUSE_BUTTON_LEFT,.x=200,.y=184}
    };
    int failed = prior_keys != 4 || prior_chars != 1 || prior_buttons != 2;
    for (size_t i = 0; i < sizeof(expected)/sizeof(expected[0]); i++) {
        InputEvent actual;
        if (!input_poll(&actual) || actual.type != expected[i].type ||
            actual.key != expected[i].key || actual.mods != expected[i].mods ||
            actual.x != expected[i].x || actual.y != expected[i].y ||
            (actual.type == INPUT_TEXT && strcmp(actual.text, "7"))) failed = 1;
    }
    InputEvent extra;
    if (input_poll(&extra)) failed = 1;
    input_close();
    if (key_handler != original_key || char_handler != original_char || button_handler != original_button) failed = 1;
    window_ready = 0;
    if (failed) fprintf(stderr, "input backend: ordered callbacks/modifiers/quick click failed\n");
    return failed;
}
