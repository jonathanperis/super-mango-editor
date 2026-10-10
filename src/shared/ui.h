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
/* Caret keys an active field understands (see ui_edit_key). */
typedef enum {
    UI_KEY_LEFT, UI_KEY_RIGHT, UI_KEY_HOME, UI_KEY_END,
    UI_KEY_BACKSPACE, UI_KEY_DELETE,
    UI_KEY_WORD_LEFT, UI_KEY_WORD_RIGHT,  /* Ctrl+Left / Ctrl+Right      */
    UI_KEY_SELECT_ALL,                    /* Ctrl+A                      */
    UI_KEY_UP, UI_KEY_DOWN, UI_KEY_PICK   /* open dropdown lists only */
} UIEditKey;

#define UI_EDIT_BUFFER_SIZE LEVEL_DESCRIPTION_CAPACITY
#define UI_PENDING_TEXT_SIZE 4096
#define UI_TEXT_CACHE_COUNT 64
#define UI_TEXT_CACHE_BYTES 128
/* Most editable fields one frame can draw; Tab moves through them in order. */
#define UI_MAX_FIELDS 128

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
    /* The caret, as a byte offset into edit_buf.  It always sits on a
     * character boundary: moving and deleting step over whole UTF-8
     * characters, and typing inserts at the caret. */
    int edit_cursor;
    /* The other end of the selection, a byte offset like the caret; -1
     * when nothing is selected.  The selected text is what lies between
     * the two, and typing, Backspace, Delete or a paste replaces it. */
    int edit_anchor;
    /* A press inside the active field: while the button stays down, the
     * caret follows the pointer and selects from where the press landed.
     * mouse_down must be filled in each frame for this (after events). */
    int edit_drag, edit_drag_from;
    UIEditType edit_type;
    void *edit_target;          /* borrowed destination, interpreted by edit_type */
    int edit_target_size;
    /* Limits of the active integer field. Every commit path (Return, or the
     * editor applying a staged edit before another command) clamps to them. */
    int edit_int_min, edit_int_max, edit_int_step;
    float edit_float_min, edit_float_max;   /* same, for a float field */
    int edit_float_nonzero;    /* 1: 0 is not a value this field accepts  */
    /* Set when a commit changed or refused what was typed ("900 is outside
     * -960..960, so it became 960"); the caller shows it and clears it. */
    char edit_note[128];
    int dropdown_open_id;      /* zero = closed; otherwise the stable widget ID */
    /* A press while a list is open belongs to that list alone (ui_press), so
     * it can never reach a widget or the canvas drawn underneath the list. */
    int dropdown_click;
    int dropdown_seen;         /* the open dropdown was drawn this frame */
    /* The open list is drawn last (ui_draw_overlays) so later widgets cannot
     * paint over it. Options are borrowed from the caller's static array. */
    const char **dropdown_options;
    int dropdown_count, dropdown_x, dropdown_y, dropdown_w, dropdown_selected;
    /* The keyboard on an open list: the highlighted option, and what this
     * frame's keys asked for (ui_dropdown_key, ui_focus_next), done when
     * the dropdown is next drawn. */
    int dropdown_highlight;
    int dropdown_key_move;     /* rows to move the highlight            */
    int dropdown_key_pick;     /* Enter: choose the highlighted option  */
    int dropdown_key_tab;      /* Tab +1 / Shift+Tab -1: close and move */
    /*
     * Tab / Shift+Tab focus.  Every field records its id in field_order as it
     * is drawn, so the order is simply the order on screen; ui_begin_frame
     * keeps the last complete frame's list in prev_field_order.  A Tab
     * (tab_request = +1 or -1) commits the active field like Return and asks
     * the neighbouring field, focus_request_id, to activate itself when it
     * is next drawn.
     */
    int field_order[UI_MAX_FIELDS], field_order_count;
    int prev_field_order[UI_MAX_FIELDS], prev_field_order_count;
    int tab_request;
    int focus_request_id, focus_request_frames;
    /* The field a focus request (Tab, or a jump from elsewhere) activated
     * and the y it was drawn at, so a scrolling panel can bring it into
     * view.  The panel clears focus_landed_id once it has scrolled. */
    int focus_landed_id, focus_landed_y;
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
/* Record a left-button press. While a dropdown list is open the press only
 * goes to that list (to pick an option or close it); mouse_clicked stays 0,
 * so no other widget reacts. Returns 1 when the press was taken this way. */
int ui_press(UIState *ui);
/* Draw the open dropdown list on top of everything else drawn this frame. */
void ui_draw_overlays(UIState *ui);
/* Append UTF-8 input without splitting a codepoint if the queue fills. */
void ui_queue_text_input(UIState *ui, const char *text);
/* Apply at command boundaries: 0 invalid/retained, 1 valid no-op, 2 changed. */
int ui_apply_active_edit(UIState *ui);
/* Discard the staging buffer; the uncommitted model value was never replaced. */
void ui_cancel_active_edit(UIState *ui);
/* Move the caret or delete a character in the active field, right away.
 * Typing queued earlier is inserted first, so keys and text keep the order
 * they were pressed in.  With extend (Shift held) a caret move selects the
 * text it passes over instead.  Does nothing when no field is active. */
void ui_edit_key(UIState *ui, UIEditKey key, int extend);
/* Copy the active field's selected text into out (cut short to fit, never
 * inside a UTF-8 character) and return its length in bytes; 0 when
 * nothing is selected. */
int ui_edit_selected_text(UIState *ui, char *out, size_t out_size);
/* Delete the active field's selected text (Ctrl+X, after copying it). */
void ui_edit_delete_selection(UIState *ui);
/* Tab (+1) / Shift+Tab (-1): when the active field is next drawn it commits
 * like Return and the next / previous field on screen becomes active.  An
 * invalid value keeps the focus where it is.  Dropdowns take part in the
 * order: Tab onto one opens its list, Tab from an open list closes it. */
void ui_focus_next(UIState *ui, int direction);
/* Keys for the open dropdown list: UI_KEY_UP / UI_KEY_DOWN move the
 * highlight, UI_KEY_HOME / UI_KEY_END jump to the first / last option and
 * UI_KEY_PICK chooses the highlighted one (like clicking it).  Does
 * nothing when no list is open. */
void ui_dropdown_key(UIState *ui, UIEditKey key);
/* Activate field `id` the next time it is drawn, as if it had been clicked
 * (dropped if it is not drawn within two frames).  For a dropdown id this
 * opens its list. */
void ui_focus_field(UIState *ui, int id);

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
/* Like ui_int_field, but a committed value is clamped to [min, max] and, when
 * step > 1, rounded to the nearest multiple of step (from 0) inside them. */
int ui_int_field_limited(UIState *ui, int id, int x, int y, int w, int *value,
                         int min, int max, int step);
int ui_float_field(UIState *ui, int id, int x, int y, int w, float *value);
/* Like ui_float_field, but a committed value is clamped to [min, max]. */
int ui_float_field_limited(UIState *ui, int id, int x, int y, int w,
                           float *value, float min, float max);
/* A speed that must keep moving: clamped to [-limit, limit] like the field
 * above, and a typed 0 is refused like text that is not a number (the
 * field stays active and edit_note says why). */
int ui_float_field_nonzero(UIState *ui, int id, int x, int y, int w,
                           float *value, float limit);
/* buf_size includes the final NUL byte. Paste/backspace preserve UTF-8 units. */
int ui_text_field(UIState *ui, int id, int x, int y, int w, char *buf, int buf_size);
/* Open/close the option list on click; update *selected and return 1 only
 * when the choice changes. Labels are borrowed from the caller's array.
 * A *selected outside [0, count) means "not one of these options" (a
 * hand-edited path, say): the header shows "---" and any choice applies. */
int ui_dropdown(UIState *ui, int id, int x, int y, int w,
                const char **options, int count, int *selected);
/* A horizontal divider, drawn in logical pixels. */
void ui_separator(UIState *ui, int x, int y, int w);
/* Measure with the same font used for drawing, for centered/aligned labels. */
int ui_text_width(UIState *ui, const char *text);
