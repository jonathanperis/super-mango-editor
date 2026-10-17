/* No desktop needed: exercise the production callback adapter against a small
 * public-GLFW registration fixture, including chaining and restoration. */
#include "input/input_backend.h"
#include "shared/graphics.h"  /* display_viewport */
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <string.h>

static int window_ready, prior_keys, prior_chars, prior_buttons;
static int screen_w = 800, screen_h = 600;
static GLFWkeyfun key_handler;
static GLFWcharfun char_handler;
static GLFWmousebuttonfun button_handler;
static GLFWcursorposfun cursor_handler;
static GLFWscrollfun scroll_handler;

/* Joysticks GLFW sees, and which of those have a gamepad mapping. */
static int joystick_present[4], joystick_mapped[4];
bool test_input_gamepad_available(int index) { return index >= 0 && index < 4 && joystick_present[index]; }
int test_input_joystick_is_gamepad(int index) { return index >= 0 && index < 4 && joystick_mapped[index]; }

bool test_input_window_ready(void) { return window_ready != 0; }
int test_input_screen_width(void) { return screen_w; }
int test_input_screen_height(void) { return screen_h; }
GLFWwindow *test_input_current_context(void) { return (GLFWwindow *)(uintptr_t)1; }
/* input_collect clears a reported close request; nothing to record here. */
void test_input_set_should_close(GLFWwindow *window, int value)
{
    (void)window; (void)value;
}
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

static int viewport_is(const char *name, int window_w, int window_h, int canvas_w, int canvas_h,
                       float x, float y, float w, float h)
{
    Rectangle got = display_viewport(window_w, window_h, canvas_w, canvas_h);
    if (got.x == x && got.y == y && got.width == w && got.height == h) return 0;
    fprintf(stderr, "display viewport %s: got %g,%g %gx%g, expected %g,%g %gx%g\n",
            name, got.x, got.y, got.width, got.height, x, y, w, h);
    return 1;
}

static int pointer_is(const char *name, float px, float py, float x, float y)
{
    Vector2 got = input_pointer_to_logical((Vector2){px, py});
    if (got.x == x && got.y == y) return 0;
    fprintf(stderr, "pointer %s: got %g,%g, expected %g,%g\n", name, got.x, got.y, x, y);
    return 1;
}

/* display_present and input_pointer_to_logical share display_viewport, so
 * the picture and the clicks scale by the same whole number. */
int display_viewport_test(void)
{
    int failed = 0;
    /* Game canvas: 1080p fits 3x (1200x900), centred on whole pixels. */
    failed |= viewport_is("1080p game", 1920, 1080, 400, 300, 360, 90, 1200, 900);
    /* The default 2x window; a window just short of 2x drops to 1x rather
     * than sampling at 1.9975x. */
    failed |= viewport_is("2x game", 800, 600, 400, 300, 0, 0, 800, 600);
    failed |= viewport_is("almost 2x", 799, 600, 400, 300, 199, 150, 400, 300);
    /* Odd margins round down instead of landing on half pixels. */
    failed |= viewport_is("odd margin", 1001, 701, 400, 300, 100, 50, 800, 600);
    /* Editor canvas: its own size is exact; 1440p is exactly 2x. */
    failed |= viewport_is("editor 1x", 1280, 720, 1280, 720, 0, 0, 1280, 720);
    failed |= viewport_is("editor 1x + 1", 1281, 721, 1280, 720, 0, 0, 1280, 720);
    failed |= viewport_is("editor 1440p", 2560, 1440, 1280, 720, 0, 0, 2560, 1440);
    failed |= viewport_is("editor ultrawide", 3840, 1600, 1280, 720, 640, 80, 2560, 1440);
    /* Below 1x the whole canvas still shows, at a fractional fit that keeps
     * its aspect ratio (offsets may be fractional there). */
    failed |= viewport_is("tiny game", 200, 150, 400, 300, 0, 0, 200, 150);
    failed |= viewport_is("narrow game", 300, 300, 400, 300, 0, 37.5f, 300, 225);
    failed |= viewport_is("short editor", 1280, 360, 1280, 720, 320, 0, 640, 360);
    /* A minimised window reports no size: nothing is drawn or hit. */
    failed |= viewport_is("empty window", 0, 0, 400, 300, 0, 0, 0, 0);
    failed |= viewport_is("empty canvas", 800, 600, 0, 0, 0, 0, 0, 0);

    window_ready = 1;
    input_open(400, 300);
    screen_w = 1920; screen_h = 1080;
    failed |= pointer_is("1080p top-left", 360, 90, 0, 0);
    failed |= pointer_is("1080p centre", 960, 540, 200, 150);
    failed |= pointer_is("1080p left margin", 0, 90, -120, 0);
    screen_w = 200; screen_h = 150;
    failed |= pointer_is("half size", 100, 75, 200, 150);
    screen_w = 0; screen_h = 0;
    failed |= pointer_is("no window", 10, 10, -1, -1);
    screen_w = 800; screen_h = 600;
    input_close();
    window_ready = 0;
    return failed;
}

static int fit_is(const char *name, int requested, int monitor_w, int monitor_h, int expected)
{
    int got = display_scale_fit(requested, 400, 300, monitor_w, monitor_h);
    if (got == expected) return 0;
    fprintf(stderr, "window scale %s: got %dx, expected %dx\n", name, got, expected);
    return 1;
}

static int moved_is(const char *name, IntRect window, IntRect monitor, int x, int y)
{
    IntRect got = display_keep_on_monitor(window, monitor);
    if (got.x == x && got.y == y && got.w == window.w && got.h == window.h) return 0;
    fprintf(stderr, "keep on monitor %s: got %d,%d, expected %d,%d\n", name, got.x, got.y, x, y);
    return 1;
}

/* The game's window_scale setting is capped to what the monitor can show,
 * with room for the title bar and taskbar, and the grown window is moved
 * back onto the monitor. */
int display_window_fit_test(void)
{
    int failed = 0;
    failed |= fit_is("4x on 1080p", 4, 1920, 1080, 3);   /* 1600x1200 is too tall */
    failed |= fit_is("3x on 1080p", 3, 1920, 1080, 3);   /* 1200x900 leaves 180 px */
    failed |= fit_is("2x on 1080p", 2, 1920, 1080, 2);   /* never raised */
    failed |= fit_is("4x on 1440p", 4, 2560, 1440, 4);
    failed |= fit_is("3x on 1366x768", 3, 1366, 768, 2);
    failed |= fit_is("2x on 1024x768", 2, 1024, 768, 2);
    failed |= fit_is("2x on 800x600", 2, 800, 600, 1);
    failed |= fit_is("tiny monitor", 4, 320, 200, 1);    /* never below 1x */
    failed |= fit_is("unknown monitor", 3, 0, 0, 3);     /* Memory backend */
    failed |= fit_is("bad setting", 0, 1920, 1080, 1);

    IntRect hd = {0, 0, 1920, 1080};
    failed |= moved_is("already on", (IntRect){100, 100, 1200, 900}, hd, 100, 100);
    failed |= moved_is("hanging off bottom right", (IntRect){900, 400, 1200, 900}, hd, 720, 180);
    failed |= moved_is("under the title bar", (IntRect){-50, 0, 800, 600}, hd, 0, 40);
    /* A second monitor to the right of the first, at x 1920. */
    IntRect right = {1920, 0, 1280, 1024};
    failed |= moved_is("second monitor", (IntRect){2500, 300, 1200, 900}, right, 2000, 124);
    /* Larger than the monitor: keep the top-left corner on it. */
    failed |= moved_is("too large", (IntRect){300, 300, 1600, 1200}, (IntRect){0, 0, 1280, 720}, 0, 40);
    failed |= moved_is("unknown monitor", (IntRect){-500, -500, 800, 600}, (IntRect){0}, -500, -500);
    return failed;
}

/* On the desktop the controller is the first joystick GLFW has a gamepad
 * mapping for: an unmapped device (a wheel, a flight stick) never reports
 * a button, so it must not be chosen ahead of a real gamepad. */
int input_gamepad_mapping_test(void)
{
    int failed = 0;
    window_ready = 1;
    input_open(400, 300);
    joystick_present[0] = 1;                       /* unmapped joystick only */
    if (input_first_gamepad() != 0) failed = 1;
    joystick_present[2] = joystick_mapped[2] = 1;  /* plus a real gamepad */
    if (input_first_gamepad() != 3) failed = 1;
    joystick_mapped[0] = 1;                        /* the first one gains a mapping */
    if (input_first_gamepad() != 1) failed = 1;
    memset(joystick_present, 0, sizeof(joystick_present));
    memset(joystick_mapped, 0, sizeof(joystick_mapped));
    input_close();
    window_ready = 0;
    if (failed) fprintf(stderr, "input backend: unmapped joystick chosen as the controller\n");
    return failed;
}
