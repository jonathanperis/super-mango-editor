/* Immediate-mode widgets: callers issue draw calls every frame and own the
 * edited model. UIState retains only active edits, dropdowns and a bounded
 * text cache. Input must be collected after ui_begin_frame, before widgets. */
#pragma once
#include <stddef.h>
#include "text.h"
#include "../levels/level.h"

typedef void (*UIBeforeChangeFn)(void *context, int widget_id);
typedef int (*UIBeforeCommandFn)(void *context);
typedef enum { UI_EDIT_NONE = 0, UI_EDIT_INT, UI_EDIT_FLOAT, UI_EDIT_TEXT } UIEditType;

#define UI_EDIT_BUFFER_SIZE LEVEL_DESCRIPTION_CAPACITY
#define UI_PENDING_TEXT_SIZE 4096
#define UI_TEXT_CACHE_COUNT 64
#define UI_TEXT_CACHE_BYTES 128

#define UI_BG         (Color){0x2D,0x2D,0x2D,0xFF}
#define UI_TITLE_BG   (Color){0x3D,0x3D,0x3D,0xFF}
#define UI_BTN        (Color){0x4D,0x4D,0x4D,0xFF}
#define UI_BTN_HOT    (Color){0x5D,0x5D,0x5D,0xFF}
#define UI_BTN_ACTIVE (Color){0x4A,0x90,0xD9,0xFF}
#define UI_ACCENT     (Color){0x4A,0x90,0xD9,0xFF}
#define UI_TEXT       (Color){0xE0,0xE0,0xE0,0xFF}
#define UI_TEXT_DIM   (Color){0x80,0x80,0x80,0xFF}
#define UI_INPUT_BG   (Color){0x1D,0x1D,0x1D,0xFF}

typedef struct {
    Texture2D *texture;
    char text[UI_TEXT_CACHE_BYTES];
    uint32_t color;
    uint64_t used;
    int w, h;
} UITextCacheEntry;

typedef struct {
    TextFont *font; /* borrowed; cache is released before the owning font */
    UITextCacheEntry text_cache[UI_TEXT_CACHE_COUNT];
    uint64_t text_clock;
    int mouse_x, mouse_y, mouse_clicked, mouse_down;
    int key_backspace, key_return, key_escape;
    char text_input[32];
    int has_text_input;
    char pending_text_input[UI_PENDING_TEXT_SIZE];
    size_t pending_text_length;
    int active_id;
    char edit_buf[UI_EDIT_BUFFER_SIZE];
    int edit_cursor;
    UIEditType edit_type;
    void *edit_target;
    int edit_target_size;
    int dropdown_open_id;
    UIBeforeChangeFn before_change;
    void *before_change_context;
    UIBeforeCommandFn before_command;
    void *before_command_context;
} UIState;

void ui_init(UIState *ui, TextFont *font);
void ui_cleanup(UIState *ui);
void ui_begin_frame(UIState *ui);
void ui_queue_text_input(UIState *ui, const char *text);
/* Apply at command boundaries: 0 invalid/retained, 1 valid no-op, 2 changed. */
int ui_apply_active_edit(UIState *ui);
void ui_cancel_active_edit(UIState *ui);
int ui_button(UIState *ui, int x, int y, int w, int h, const char *label);
void ui_label(UIState *ui, int x, int y, const char *text);
void ui_label_color(UIState *ui, int x, int y, const char *text, Color color);
void ui_panel(UIState *ui, int x, int y, int w, int h);
int ui_int_field(UIState *ui, int id, int x, int y, int w, int *value);
int ui_float_field(UIState *ui, int id, int x, int y, int w, float *value);
int ui_text_field(UIState *ui, int id, int x, int y, int w, char *buf, int buf_size);
int ui_dropdown(UIState *ui, int id, int x, int y, int w,
                const char **options, int count, int *selected);
void ui_separator(UIState *ui, int x, int y, int w);
int ui_text_width(UIState *ui, const char *text);
