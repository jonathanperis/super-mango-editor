/*
 * ui.h — Small immediate-mode widgets for the editor and game settings.
 *
 * "Immediate-mode" means the caller issues widget calls every frame rather
 * than constructing a permanent tree of button objects. The caller owns the
 * edited model; UIState remembers only input, an active edit, dropdown state
 * and reusable text textures. Typical frame order:
 *
 *   ui_begin_frame → collect input → call widgets → present the frame
 *
 * A field's nonzero ID must remain stable between frames. Its temporary text
 * lives in edit_buf until confirmation, so invalid/incomplete input does not
 * overwrite the document. before_change lets the editor capture undo state.
 */
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
    /* Each cache entry owns its texture. "used" is a recency counter; a full
     * cache can evict an old label without growing memory for every string. */
    Texture2D *texture;
    char text[UI_TEXT_CACHE_BYTES];
    uint32_t color;
    uint64_t used;
    int w, h;
} UITextCacheEntry;

typedef struct {
    /* ---- Initialized once; cache entries retain state across frames ---- */
    TextFont *font; /* borrowed; cache is released before the owning font */
    UITextCacheEntry text_cache[UI_TEXT_CACHE_COUNT];
    uint64_t text_clock;
    /* ---- Input for this frame, filled after ui_begin_frame ---- */
    int mouse_x, mouse_y,       /* logical canvas coordinates */
        mouse_clicked,         /* press edge: a button acts once */
        mouse_down;            /* held state: useful for dragging */
    int key_backspace, key_return, key_escape;
    char text_input[32];
    int has_text_input;
    /* Append UTF-8 events instead of overwriting earlier same-frame typing. */
    char pending_text_input[UI_PENDING_TEXT_SIZE];
    size_t pending_text_length;
    /* ---- Retained edit state; zero active_id means no active field ---- */
    int active_id;
    char edit_buf[UI_EDIT_BUFFER_SIZE];
    int edit_cursor;           /* byte offset; UTF-8 deletion respects codepoints */
    UIEditType edit_type;
    void *edit_target;          /* borrowed destination, interpreted by edit_type */
    int edit_target_size;
    int dropdown_open_id;      /* zero = closed; otherwise the stable widget ID */
    /* Capture undo before a change; finish/block a field edit before commands. */
    UIBeforeChangeFn before_change;
    void *before_change_context;
    UIBeforeCommandFn before_command;
    void *before_command_context;
} UIState;

/* Borrow the already-loaded font and initialize state. The caller owns it. */
void ui_init(UIState *ui, TextFont *font);
/* Free cached label textures before the font and graphics context are closed. */
void ui_cleanup(UIState *ui);
/* Reset one-shot input, not retained edits. Call before processing commands. */
void ui_begin_frame(UIState *ui);
/* Append UTF-8 input without splitting a codepoint if the queue fills. */
void ui_queue_text_input(UIState *ui, const char *text);
/* Apply at command boundaries: 0 invalid/retained, 1 valid no-op, 2 changed. */
int ui_apply_active_edit(UIState *ui);
/* Discard the staging buffer; the uncommitted model value was never replaced. */
void ui_cancel_active_edit(UIState *ui);

/* Draw a button and return 1 on its click frame, not every held-mouse frame.
 * An active edit must finish before the caller executes the button command. */
int ui_button(UIState *ui, int x, int y, int w, int h, const char *label);
/* UTF-8 labels use a bounded cache for short strings and transient textures
 * for long ones. Coordinates and measurements are logical canvas pixels. */
void ui_label(UIState *ui, int x, int y, const char *text);
void ui_label_color(UIState *ui, int x, int y, const char *text, Color color);
/* Decorative background only: panels do not own widgets or handle input. */
void ui_panel(UIState *ui, int x, int y, int w, int h);

/* Editable fields: click activates, Return confirms, Escape discards.
 * Each returns 1 only when a valid commit changes the caller-owned value.
 * Float display uses nine significant digits for round-trip-safe editing. */
int ui_int_field(UIState *ui, int id, int x, int y, int w, int *value);
int ui_float_field(UIState *ui, int id, int x, int y, int w, float *value);
/* buf_size includes the final NUL byte. Paste/backspace preserve UTF-8 units. */
int ui_text_field(UIState *ui, int id, int x, int y, int w, char *buf, int buf_size);
/* Open/close the option list on click; update *selected and return 1 only
 * when the choice changes. Labels are borrowed from the caller's array. */
int ui_dropdown(UIState *ui, int id, int x, int y, int w,
                const char **options, int count, int *selected);
/* A horizontal divider, drawn in logical pixels. */
void ui_separator(UIState *ui, int x, int y, int w);
/* Measure with the same font used for drawing, for centered/aligned labels. */
int ui_text_width(UIState *ui, const char *text);
