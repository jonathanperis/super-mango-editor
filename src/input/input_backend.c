/*
 * input_backend.c — Adapt raylib/GLFW input without moving gameplay into it.
 *
 * Callbacks preserve ordered commands (including text and quick clicks).
 * Sampled state answers "is this control still held?". Saved bindings use a
 * separate legacy number space, translated explicitly below. Read the header
 * for queue lifecycle; gameplay routing lives in game_events/game_input.
 */
#include "input_backend.h"
#include <stdio.h>
#include <string.h>
#include "../shared/platform.h"
#ifndef MANGO_RAYLIB_MEMORY
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
EM_JS(void, input_web_scope_open, (void), {
    if (Module.__mangoKeyboardCleanup) Module.__mangoKeyboardCleanup();
    Module.__mangoKeyboardCleanup = globalThis.SuperMangoKeyboard.bind(window, Module.canvas, GLFW);
});
EM_JS(void, input_web_scope_close, (void), {
    if (Module.__mangoKeyboardCleanup) Module.__mangoKeyboardCleanup();
    Module.__mangoKeyboardCleanup = null;
});
#endif

#define INPUT_CAPACITY 16384
static InputEvent events[INPUT_CAPACITY];
static unsigned int first, count;
static int ready, width, height, focused = 1, gamepad;
static unsigned char suppressed[512];

#ifndef MANGO_RAYLIB_MEMORY
/* raylib exposes key and character queues separately. Chaining its backend's
 * public callbacks preserves text-before-Ctrl+S and per-event modifiers, as
 * well as mouse clicks pressed and released within one frame. The original
 * callbacks still maintain raylib's sampled key/mouse state. */
static GLFWwindow *input_window;
static GLFWkeyfun prior_key;
static GLFWcharfun prior_char;
static GLFWmousebuttonfun prior_button;
static GLFWcursorposfun prior_cursor;
static GLFWscrollfun prior_scroll;

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    /* Let raylib update its held-key state first. Our additional command keeps
     * the event's modifiers/repeat bit instead of sampling them later. */
    if (prior_key) prior_key(window, key, scancode, action, mods);
    if (key <= 0 || key >= 512) return;
    if (action == GLFW_RELEASE) suppressed[key] = 0;
    else if (suppressed[key]) return;
    input_push(&(InputEvent){.type=action == GLFW_RELEASE ? INPUT_KEY_UP : INPUT_KEY_DOWN,
        .key=key, .binding=input_binding_from_key(key), .mods=mods,
        .repeat=action == GLFW_REPEAT});
}

static void char_callback(GLFWwindow *window, unsigned int codepoint)
{
    if (prior_char) prior_char(window, codepoint);
    /* Key identity and typed text differ on non-US layouts. GLFW delivers a
     * Unicode codepoint; encode it as UTF-8 bytes for the editor's text queue.
     * Aggregate initialization supplies the trailing NUL in the empty bytes. */
    InputEvent event = {.type = INPUT_TEXT};
    int bytes = 0;
    const char *text = CodepointToUTF8((int)codepoint, &bytes);
    if (bytes > 0 && bytes < (int)sizeof(event.text)) {
        memcpy(event.text, text, (size_t)bytes);
        input_push(&event);
    }
}

static InputEvent pointer_event(GLFWwindow *window, InputKind type)
{
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    Vector2 point = input_pointer_to_logical((Vector2){(float)x,(float)y});
    return (InputEvent){.type=type,.x=(int)point.x,.y=(int)point.y,.mods=input_modifiers()};
}

static void button_callback(GLFWwindow *window, int button, int action, int mods)
{
    if (prior_button) prior_button(window, button, action, mods);
    InputEvent event = pointer_event(window, action == GLFW_PRESS ? INPUT_MOUSE_DOWN : INPUT_MOUSE_UP);
    event.button = button;
    event.mods = mods;
    input_push(&event);
}

static void cursor_callback(GLFWwindow *window, double x, double y)
{
    if (prior_cursor) prior_cursor(window, x, y);
    Vector2 point = input_pointer_to_logical((Vector2){(float)x,(float)y});
    input_push(&(InputEvent){.type=INPUT_MOUSE_MOVE,.x=(int)point.x,.y=(int)point.y,.mods=input_modifiers()});
}

static void scroll_callback(GLFWwindow *window, double x, double y)
{
    if (prior_scroll) prior_scroll(window, x, y);
    InputEvent event = pointer_event(window, INPUT_WHEEL);
    event.wheel = (float)y;
    input_push(&event);
}

static void callbacks_close(void)
{
    if (!input_window) return;
    glfwSetKeyCallback(input_window, prior_key);
    glfwSetCharCallback(input_window, prior_char);
    glfwSetMouseButtonCallback(input_window, prior_button);
    glfwSetCursorPosCallback(input_window, prior_cursor);
    glfwSetScrollCallback(input_window, prior_scroll);
    input_window = NULL;
}

static void callbacks_open(void)
{
    callbacks_close();
    if (!IsWindowReady()) return;
    input_window = glfwGetCurrentContext();
    prior_key = glfwSetKeyCallback(input_window, key_callback);
    prior_char = glfwSetCharCallback(input_window, char_callback);
    prior_button = glfwSetMouseButtonCallback(input_window, button_callback);
    prior_cursor = glfwSetCursorPosCallback(input_window, cursor_callback);
    prior_scroll = glfwSetScrollCallback(input_window, scroll_callback);
}
#endif

int input_binding_known(int b)
{
    /* These are version-1 profile wire IDs, not raylib key constants. Keep
     * each valid range explicit: holes are invalid, but a saved key may be
     * valid even when the current backend cannot deliver it. The settings
     * screen warns about those unavailable keys instead of resetting them. */
    if (b >= 4 && b <= 129) return 1;
    if (b >= 133 && b <= 164) return 1;
    if (b >= 176 && b <= 221) return 1;
    if (b >= 224 && b <= 231) return 1;
    if (b >= 257 && b <= 290) return 1;
    return 0;
}

int input_key_from_binding(int b)
{
    if (b >= 4 && b <= 29) return KEY_A + b - 4;
    if (b >= 30 && b <= 38) return KEY_ONE + b - 30;
    if (b >= 58 && b <= 69) return KEY_F1 + b - 58;
    if (b >= 89 && b <= 97) return KEY_KP_1 + b - 89;
    /* GLFW reports F13-F24 through raylib's key queue/state arrays too. */
    if (b >= 104 && b <= 115) return 302 + b - 104;
    switch (b) {
    case 39: return KEY_ZERO; case 40: return KEY_ENTER; case 41: return KEY_ESCAPE;
    case 42: return KEY_BACKSPACE; case 43: return KEY_TAB; case 44: return KEY_SPACE;
    case 45: return KEY_MINUS; case 46: return KEY_EQUAL; case 47: return KEY_LEFT_BRACKET;
    case 48: return KEY_RIGHT_BRACKET; case 49: return KEY_BACKSLASH;
    case 51: return KEY_SEMICOLON; case 52: return KEY_APOSTROPHE; case 53: return KEY_GRAVE;
    case 54: return KEY_COMMA; case 55: return KEY_PERIOD; case 56: return KEY_SLASH;
    case 57: return KEY_CAPS_LOCK; case 70: return KEY_PRINT_SCREEN; case 71: return KEY_SCROLL_LOCK;
    case 72: return KEY_PAUSE; case 73: return KEY_INSERT; case 74: return KEY_HOME;
    case 75: return KEY_PAGE_UP; case 76: return KEY_DELETE; case 77: return KEY_END;
    case 78: return KEY_PAGE_DOWN; case 79: return KEY_RIGHT; case 80: return KEY_LEFT;
    case 81: return KEY_DOWN; case 82: return KEY_UP; case 83: return KEY_NUM_LOCK;
    case 84: return KEY_KP_DIVIDE; case 85: return KEY_KP_MULTIPLY; case 86: return KEY_KP_SUBTRACT;
    case 87: return KEY_KP_ADD; case 88: return KEY_KP_ENTER; case 98: return KEY_KP_0;
    case 99: return KEY_KP_DECIMAL; case 101: return KEY_KB_MENU; case 103: return KEY_KP_EQUAL;
    case 224: return KEY_LEFT_CONTROL; case 225: return KEY_LEFT_SHIFT;
    case 226: return KEY_LEFT_ALT; case 227: return KEY_LEFT_SUPER;
    case 228: return KEY_RIGHT_CONTROL; case 229: return KEY_RIGHT_SHIFT;
    case 230: return KEY_RIGHT_ALT; case 231: return KEY_RIGHT_SUPER;
    default: return KEY_NULL;
    }
}

int input_binding_from_key(int key)
{
    if (!key) return 0;
    for (int b = 4; b <= 290; b++) if (input_key_from_binding(b) == key) return b;
    return 0;
}

const char *input_binding_name(int binding)
{
    static char label[64];
    int key = input_key_from_binding(binding);
    if (!key) { snprintf(label, sizeof(label), "Legacy key %d (unavailable)", binding); return label; }
    if (binding >= 4 && binding <= 29) { label[0] = (char)('A'+binding-4); label[1] = 0; return label; }
    if (key >= KEY_F1 && key <= 313) { snprintf(label, sizeof(label), "F%d", key-KEY_F1+1); return label; }
    switch (key) {
    case KEY_SPACE: return "Space"; case KEY_LEFT_SHIFT: return "Left Shift";
    case KEY_RIGHT_SHIFT: return "Right Shift"; case KEY_LEFT_CONTROL: return "Left Ctrl";
    case KEY_RIGHT_CONTROL: return "Right Ctrl"; case KEY_LEFT_ALT: return "Left Alt";
    case KEY_RIGHT_ALT: return "Right Alt"; case KEY_LEFT_SUPER: return "Left Super";
    case KEY_RIGHT_SUPER: return "Right Super"; case KEY_BACKSPACE: return "Backspace";
    default: break;
    }
    if (IsWindowReady()) {
        const char *name = GetKeyName(key);
        if (name && name[0]) return name;
    }
    snprintf(label, sizeof(label), "Key %d", binding);
    return label;
}

int input_pad_button(int b)
{
    static const int buttons[PAD_COUNT] = {
        GAMEPAD_BUTTON_RIGHT_FACE_DOWN, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT,
        GAMEPAD_BUTTON_RIGHT_FACE_LEFT, GAMEPAD_BUTTON_RIGHT_FACE_UP,
        GAMEPAD_BUTTON_MIDDLE_LEFT, GAMEPAD_BUTTON_MIDDLE, GAMEPAD_BUTTON_MIDDLE_RIGHT,
        GAMEPAD_BUTTON_LEFT_THUMB, GAMEPAD_BUTTON_RIGHT_THUMB,
        GAMEPAD_BUTTON_LEFT_TRIGGER_1, GAMEPAD_BUTTON_RIGHT_TRIGGER_1,
        GAMEPAD_BUTTON_LEFT_FACE_UP, GAMEPAD_BUTTON_LEFT_FACE_DOWN,
        GAMEPAD_BUTTON_LEFT_FACE_LEFT, GAMEPAD_BUTTON_LEFT_FACE_RIGHT
    };
    return b >= 0 && b < PAD_COUNT ? buttons[b] : 0;
}

const char *input_pad_name(int b)
{
    static const char *const names[PAD_COUNT] = {
        "A", "B", "X", "Y", "Back", "Guide", "Start", "Left stick", "Right stick",
        "Left shoulder", "Right shoulder", "D-pad up", "D-pad down", "D-pad left", "D-pad right",
        "Misc (unavailable)", "Paddle 1 (unavailable)", "Paddle 2 (unavailable)",
        "Paddle 3 (unavailable)", "Paddle 4 (unavailable)", "Touchpad (unavailable)"
    };
    return b >= 0 && b < PAD_COUNT ? names[b] : "?";
}

int input_ready(void)
{
    return ready;
}

void input_clear(void)
{
    first = count = 0;
}

void input_open(int w, int h)
{
    width = w;
    height = h;
    ready = 1;
    focused = 1;
    gamepad = 0;
    memset(suppressed, 0, sizeof(suppressed));
    input_clear();
#ifndef MANGO_RAYLIB_MEMORY
    callbacks_open();
#endif
#ifdef __EMSCRIPTEN__
    input_web_scope_open();
#endif
}
void input_close(void)
{
#ifndef MANGO_RAYLIB_MEMORY
    callbacks_close();
#endif
#ifdef __EMSCRIPTEN__
    input_web_scope_close();
#endif
    input_clear();
    ready = 0;
    gamepad = 0;
}
int input_push(const InputEvent *event)
{
    /* Copy the event by value: callers can safely pass a stack-local event.
     * Modulo wraps around the fixed ring; no allocation occurs per command. */
    if (!ready || !event || count == INPUT_CAPACITY) return 0;
    events[(first+count++) % INPUT_CAPACITY] = *event;
    return 1;
}
int input_poll(InputEvent *event)
{
    if (!count) return 0;
    *event = events[first];
    first = (first+1) % INPUT_CAPACITY;
    count--;
    return 1;
}

void input_clear_keys(void)
{
    input_clear();
    if (!IsWindowReady()) return;
    for (int key = 1; key < 512; key++) suppressed[key] = IsKeyDown(key);
    while (GetKeyPressed()) {}
    while (GetCharPressed()) {}
}

int input_key_down(int binding)
{
    int key = input_key_from_binding(binding);
    if (!key || !IsWindowReady()) return 0;
    if (suppressed[key]) {
        if (!IsKeyDown(key)) suppressed[key] = 0;
        return 0;
    }
    return IsKeyDown(key);
}

Vector2 input_mouse(void)
{
    if (!IsWindowReady()) return (Vector2){0};
    return input_pointer_to_logical(GetMousePosition());
}

Vector2 input_pointer_to_logical(Vector2 mouse)
{
    /* Invert display_present: remove letterbox margins, then divide by the
     * smaller scale ratio. Do this once before widgets/world hit testing. */
    float sx = (float)GetScreenWidth()/width, sy = (float)GetScreenHeight()/height;
    float scale = sx < sy ? sx : sy;
    if (scale <= 0) return (Vector2){-1, -1};
    return (Vector2){(mouse.x-(GetScreenWidth()-width*scale)/2)/scale,
                     (mouse.y-(GetScreenHeight()-height*scale)/2)/scale};
}

int input_modifiers(void)
{
    return ((IsKeyDown(KEY_LEFT_SHIFT)||IsKeyDown(KEY_RIGHT_SHIFT)) ? INPUT_SHIFT : 0) |
        ((IsKeyDown(KEY_LEFT_CONTROL)||IsKeyDown(KEY_RIGHT_CONTROL)) ? INPUT_CTRL : 0) |
        ((IsKeyDown(KEY_LEFT_ALT)||IsKeyDown(KEY_RIGHT_ALT)) ? INPUT_ALT : 0) |
        ((IsKeyDown(KEY_LEFT_SUPER)||IsKeyDown(KEY_RIGHT_SUPER)) ? INPUT_SUPER : 0);
}

int input_first_gamepad(void)
{
    /* Keep the selected controller while it exists. A lower-index device
     * appearing later must not steal the current player's controls. */
    if (!IsWindowReady()) return 0;
    if (gamepad && IsGamepadAvailable(gamepad-1)) return gamepad;
    for (int i = 0; i < 4; i++) if (IsGamepadAvailable(i)) return i+1;
    return 0;
}

void input_collect(void)
{
    if (!ready || !IsWindowReady()) return;
    /* Bounded hidden-window smoke runs have no desktop focus to acquire. */
    int current_focus = IsWindowHidden() ? 1 : IsWindowFocused();
    if (current_focus != focused) {
        focused = current_focus;
        if (!focused)
            for (int key = 1; key < 512; key++)
                suppressed[key] = IsKeyDown(key);
        input_push(&(InputEvent){.type=INPUT_FOCUS, .focused=focused});
    }
    if (WindowShouldClose()) input_push(&(InputEvent){.type=INPUT_QUIT});
    int device = input_first_gamepad();
    if (device != gamepad) {
        if (gamepad) input_push(&(InputEvent){.type=INPUT_PAD_REMOVED,.device=gamepad});
        gamepad = device;
        if (device) input_push(&(InputEvent){.type=INPUT_PAD_ADDED,.device=device});
    }
    /* Commands were captured in callback order during EndDrawing's poll. */
    while (GetKeyPressed()) {}
    while (GetCharPressed()) {}
    for (int key = 1; key < 512; key++) if (!IsKeyDown(key)) suppressed[key] = 0;
    if (device) for (int b = 0; b < PAD_COUNT; b++) {
        int button = input_pad_button(b);
        if (!button) continue;
        if (IsGamepadButtonPressed(device-1, button)) input_push(&(InputEvent){.type=INPUT_PAD_DOWN,.button=b,.device=device});
        if (IsGamepadButtonReleased(device-1, button)) input_push(&(InputEvent){.type=INPUT_PAD_UP,.button=b,.device=device});
    }
}
