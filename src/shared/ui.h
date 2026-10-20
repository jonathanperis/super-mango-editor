/*
 * ui.h — Small immediate-mode widgets for the editor and game settings.
 *
 * "Immediate-mode" means the caller issues widget calls every frame rather
 * than constructing a permanent tree of button objects. The caller owns the
 * edited model; UIState remembers only input, an active edit and dropdown
 * state. Typical frame order:
 *
 *   ui_begin_frame → collect input → call widgets → present the frame
 *
 * A field's nonzero ID must remain stable between frames. Its temporary text
 * lives in edit_buf until confirmation, so invalid/incomplete input does not
 * overwrite the document. before_change lets the editor capture undo state.
 */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "text.h"
#include "../levels/level.h"
#include "../../vendor/raygui/raygui_icons.h" /* UIIcon */

typedef void (*UIBeforeChangeFn)(void *context, int widget_id);
typedef int (*UIBeforeCommandFn)(void *context);
typedef enum { UI_EDIT_NONE = 0, UI_EDIT_INT, UI_EDIT_FLOAT, UI_EDIT_TEXT } UIEditType;
/* Caret keys an active field understands (see ui_edit_key). */
typedef enum {
    UI_KEY_LEFT, UI_KEY_RIGHT, UI_KEY_HOME, UI_KEY_END,
    UI_KEY_BACKSPACE, UI_KEY_DELETE,
    UI_KEY_WORD_LEFT, UI_KEY_WORD_RIGHT,  /* Ctrl+Left / Ctrl+Right      */
    UI_KEY_SELECT_ALL,                    /* Ctrl+A                      */
    UI_KEY_UP, UI_KEY_DOWN,               /* lists; step a number field  */
    UI_KEY_PICK                           /* open dropdown lists only    */
} UIEditKey;

#define UI_EDIT_BUFFER_SIZE LEVEL_DESCRIPTION_CAPACITY
#define UI_PENDING_TEXT_SIZE 4096
/* Most editable fields one frame can draw; Tab moves through them in order. */
#define UI_MAX_FIELDS 128

#define UI_BG         (Color){0x2D,0x2D,0x2D,0xFF}
#define UI_TITLE_BG   (Color){0x3D,0x3D,0x3D,0xFF}
#define UI_BTN        (Color){0x4D,0x4D,0x4D,0xFF}
#define UI_BTN_HOT    (Color){0x5D,0x5D,0x5D,0xFF}
#define UI_BTN_ACTIVE (Color){0x4A,0x90,0xD9,0xFF}
#define UI_ACCENT     (Color){0x4A,0x90,0xD9,0xFF}
#define UI_TEXT       (Color){0xE0,0xE0,0xE0,0xFF}
/* Secondary text (hints, notes, "Validation: OK"): 4.5:1 or more (WCAG
 * AA) on the panel, title and input faces it is drawn on. */
#define UI_TEXT_DIM   (Color){0xA8,0xA8,0xA8,0xFF}
#define UI_INPUT_BG   (Color){0x1D,0x1D,0x1D,0xFF}

/*
 * UIStyle — every colour the widgets draw, by role, like raygui's style
 * table. Widgets pick a role by state (a hovered button draws
 * UI_COLOR_HOT, the selected dropdown item UI_COLOR_SELECTED, a disabled
 * button UI_COLOR_TITLE with UI_COLOR_TEXT_DIM), so swapping the table
 * re-themes every widget and every panel that asks ui_color.
 *
 * UI_STYLE_DEFAULT is the editor look (the UI_* colours above);
 * UI_STYLE_HIGH_CONTRAST keeps every text colour at 7:1 or more (WCAG AAA) against
 * its background and outlines buttons, fields and dropdowns.
 */
typedef enum {
    UI_COLOR_PANEL,     /* panel and window background                     */
    UI_COLOR_TITLE,     /* title bars, disabled buttons                    */
    UI_COLOR_INPUT,     /* field background                                */
    UI_COLOR_BUTTON,    /* button and dropdown face                        */
    UI_COLOR_HOT,       /* hovered or keyboard-highlighted face or row     */
    UI_COLOR_SELECTED,  /* chosen item, text selection, current tool       */
    UI_COLOR_TEXT,      /* labels and values                               */
    UI_COLOR_TEXT_DIM,  /* hints, separators, inactive field borders       */
    UI_COLOR_ACCENT,    /* active field border, symbols, highlights        */
    UI_COLOR_WARNING,   /* a problem worth noticing (profile, bindings)    */
    UI_COLOR_ERROR,     /* validation errors                               */
    UI_COLOR_BORDER,    /* control outline, drawn when border_width > 0   */
    UI_COLOR_COUNT
} UIColorRole;

/*
 * UIScroll — one scrolled list or panel, in the manner of raygui's
 * GuiScrollPanel: content taller than its view shows from `offset` units
 * down (pixels for a panel, rows for a list). A trackpad sends fractions
 * of a wheel notch, so the part that is not yet a whole unit waits in
 * `carry` for the next event; truncating each event alone would turn
 * every small swipe into nothing. A zeroed UIScroll is at the top.
 */
typedef struct {
    int   offset;  /* units scrolled past the top, 0 .. content - view */
    float carry;   /* wheel movement not yet a whole unit              */
} UIScroll;

typedef struct {
    Color color[UI_COLOR_COUNT];
    int   border_width;  /* px of UI_COLOR_BORDER around controls; 0 = none */
} UIStyle;

extern const UIStyle UI_STYLE_DEFAULT;
extern const UIStyle UI_STYLE_HIGH_CONTRAST;

typedef struct {
    /* ---- Initialized once ---- */
    TextFont *font; /* borrowed; labels are drawn straight from its atlas */
    UIStyle   style; /* UI_STYLE_DEFAULT after ui_init; replace to re-theme */
    /* The scrollbar thumb being dragged (NULL = none) and where on the
     * thumb the pointer took it, in pixels from its top. */
    UIScroll *scroll_drag;
    int       scroll_grab;
    int       scroll_drag_seen;  /* its scrollbar ran this frame */
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
    /* The active text field keeps line breaks: Shift+Enter (queued as
     * "\n" text) and pasted newlines go in instead of being dropped. */
    int edit_multiline;
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
    /*
     * Tooltips (ui_tooltip).  The owner sets time_ms each frame (any
     * millisecond clock; tests set it by hand) and view_w / view_h to the
     * size of what it draws into, so a tip near an edge stays inside.
     * last_x .. last_h is the widget drawn last, the one ui_tooltip is
     * about.  tooltip_x .. tooltip_h is the widget the pointer has rested
     * on since tooltip_since_ms; tooltip_text is this frame's tip for it,
     * and tooltip_shown says ui_draw_overlays drew it, in tip_box.
     */
    uint32_t time_ms;
    int view_w, view_h;
    int last_x, last_y, last_w, last_h;
    /* The lowest widget edge (y + h) drawn since an owner last set this to
     * 0: how tall a panel's content turned out, for scrolling it. */
    int widgets_bottom;
    const char *tooltip_text;   /* borrowed until ui_draw_overlays */
    int tooltip_x, tooltip_y, tooltip_w, tooltip_h;
    uint32_t tooltip_since_ms;
    int tooltip_seen;           /* a tip was asked for this frame        */
    int tooltip_dismissed;      /* a click on the widget hid it          */
    int tooltip_shown;
    int tip_box[4];             /* x, y, w, h of the box last drawn      */
    /*
     * The mouse cursor (a raylib MouseCursor).  Widgets and the owner
     * say what this frame wants in cursor_wish (ui_begin_frame resets it
     * to the arrow); ui_apply_cursor sets it once at the end of the frame,
     * and only when it differs from cursor_shown.  cursor_changes counts
     * the changes, for tests.
     */
    int cursor_wish, cursor_shown, cursor_changes;
    /* The clip rectangle set with ui_clip_begin (or ui_scroll_begin), so
     * code that has to leave the frame being drawn (an in-window prompt)
     * can put it back afterwards.  raylib keeps no such record. */
    int clip_active;
    int clip_x, clip_y, clip_w, clip_h;
    /* Capture undo before a change; finish/block a field edit before commands. */
    UIBeforeChangeFn before_change;
    void *before_change_context;
    UIBeforeCommandFn before_command;
    void *before_command_context;
} UIState;

/* The style's colour for a role; owners draw their own panels with it too. */
static inline Color ui_color(const UIState *ui, UIColorRole role)
{
    return ui->style.color[role];
}

/* Borrow the already-loaded font and initialize state. The caller owns it. */
void ui_init(UIState *ui, TextFont *font);
/* Pair with ui_init. UIState holds no resources today (labels are drawn from
 * the font's atlas), so this does nothing; owners still call it. */
void ui_cleanup(UIState *ui);
/* Reset one-shot input, not retained edits. Call before processing commands. */
void ui_begin_frame(UIState *ui);
/* Record a left-button press. While a dropdown list is open the press only
 * goes to that list (to pick an option or close it); mouse_clicked stays 0,
 * so no other widget reacts. Returns 1 when the press was taken this way. */
int ui_press(UIState *ui);
/* Draw the open dropdown list, then any tooltip that is due, on top of
 * everything else drawn this frame. */
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
 * text it passes over instead.  In a number field UI_KEY_UP / UI_KEY_DOWN
 * add or take away one step (ten with extend), inside the field's limits:
 * an int field's own step, or 1; a float field 1 from 10 up and 0.1 below.
 * Does nothing when no field is active. */
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

/* Ask for a mouse cursor (MOUSE_CURSOR_*) for this frame; the last ask
 * wins.  Editable fields ask for the I-beam while the pointer is on them. */
void ui_set_cursor(UIState *ui, int cursor);
/* End of frame: show the cursor this frame asked for (the resize arrows
 * while a scrollbar thumb is dragged), calling raylib only when it
 * changes.  (Upstream raylib's GLFW backend made a new system cursor on
 * every SetMouseCursor; the pinned build reuses one per shape, see
 * vendor/raylib/README.md, but there is still no reason to call it every
 * frame.)  A hidden window or the Memory test backend has no cursor to
 * set; there only cursor_shown follows. */
void ui_apply_cursor(UIState *ui);
/* Draw a button and return 1 on its click frame, not every held-mouse frame.
 * An active edit must finish before the caller executes the button command. */
int ui_button(UIState *ui, int x, int y, int w, int h, const char *label);
/* A button for a state that is on or off (a tool, the grid, a view): drawn
 * in the selected colour while `on`, with the accent outline when the style
 * has outlines.  Returns 1 on its click frame, like ui_button; the caller
 * flips the state. */
int ui_toggle(UIState *ui, int x, int y, int w, int h, const char *label, int on);
/* ui_button and ui_toggle with one of raygui's 16 x 16 icons before the
 * label, the two centred together.  UI_ICON_NONE draws the label alone. */
int ui_button_icon(UIState *ui, int x, int y, int w, int h, UIIcon icon, const char *label);
int ui_toggle_icon(UIState *ui, int x, int y, int w, int h, UIIcon icon, const char *label, int on);
/* Width of the icon and the gap after it, before a button's label. */
#define UI_ICON_SIZE 16
#define UI_ICON_GAP  4
/* Draw `icon` with its top-left pixel at (x, y), one window pixel per icon
 * pixel, like raygui's GuiDrawIcon at pixelSize 1. */
void ui_draw_icon(UIIcon icon, int x, int y, Color color);
/* The same button while its command has nothing to act on: drawn darker with
 * a dim label, without a hover highlight, and it never takes a click. */
void ui_button_disabled(UIState *ui, int x, int y, int w, int h, const char *label);
/*
 * Give the widget drawn just before this call a tooltip: once the pointer
 * has rested on it for UI_TOOLTIP_DELAY_MS, ui_draw_overlays shows `text`
 * in a box by the pointer.  Pressing the mouse on the widget hides the tip
 * until the pointer moves to another widget.  The text is borrowed until
 * ui_draw_overlays (a string literal is the usual choice).
 */
#define UI_TOOLTIP_DELAY_MS 500u
void ui_tooltip(UIState *ui, const char *text);
/* UTF-8 labels, drawn from the font atlas with DrawTextEx. Coordinates and
 * measurements are logical canvas pixels. */
void ui_label(UIState *ui, int x, int y, const char *text);
void ui_label_color(UIState *ui, int x, int y, const char *text, Color color);
/* ui_label_color kept to max_w pixels: text that does not fit is cut at a
 * character boundary and ends in "...", and resting the pointer on it shows
 * the whole text as a tooltip (text is borrowed until ui_draw_overlays).
 * Returns 1 when the text was cut. */
int ui_label_fit(UIState *ui, int x, int y, int max_w, const char *text, Color color);
/* A label that explains itself: resting the pointer on it shows `tip`
 * (borrowed until ui_draw_overlays), like a widget's ui_tooltip. */
void ui_label_tip(UIState *ui, int x, int y, const char *text, const char *tip);
/* Decorative background only: panels do not own widgets or handle input. */
void ui_panel(UIState *ui, int x, int y, int w, int h);

/* Editable fields: click activates, Return confirms, Escape discards.
 * Each returns 1 only when a valid commit changes the caller-owned value.
 * A float shows the shortest text that reads back as the same value ("0.08"). */
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
/* ui_text_field showing `hint` (dimmed) while the text is empty.  With
 * multiline, Shift+Enter starts a new line.  Either way a line break is
 * shown as a pilcrow, so the text stays on the field's one row, and text
 * wider than the field ends in "..." until the field is clicked. */
int ui_text_field_hint(UIState *ui, int id, int x, int y, int w, char *buf, int buf_size,
                       const char *hint, int multiline);
/* Open/close the option list on click; update *selected and return 1 only
 * when the choice changes. Labels are borrowed from the caller's array.
 * A *selected outside [0, count) means "not one of these options" (a
 * hand-edited path, say): the header shows "---" and any choice applies. */
int ui_dropdown(UIState *ui, int id, int x, int y, int w,
                const char **options, int count, int *selected);
/* ui_dropdown whose header shows `unlisted` instead of "---" while
 * *selected is none of the options: the file a level names that the list
 * does not offer, say, so the user sees what is set before changing it. */
int ui_dropdown_unlisted(UIState *ui, int id, int x, int y, int w,
                         const char **options, int count, int *selected,
                         const char *unlisted);
/* A horizontal divider, drawn in logical pixels. */
void ui_separator(UIState *ui, int x, int y, int w);


/* Apply a wheel delta in notches (positive = up) at units_per_notch and
 * return how many whole units it moved. The offset never goes below 0;
 * ui_scroll_clamp or ui_scroll_begin limit the other end. */
int ui_scroll_wheel(UIScroll *scroll, float wheel, float units_per_notch);
/* Keep the offset within 0 .. content - view (0 when everything fits). */
void ui_scroll_clamp(UIScroll *scroll, int content, int view);
/* Clip drawing to (x, y, w, h) with raylib's scissor, and remember it in
 * the UIState (clip_*).  Every scissor the owner sets goes through these,
 * so the active clip is always known.  Clips do not nest. */
void ui_clip_begin(UIState *ui, int x, int y, int w, int h);
void ui_clip_end(UIState *ui);
/* For a pixel panel: clamp, clip drawing to the view (x, y, w, view_h)
 * and return where the content's top edge goes. Pair with ui_scroll_end. */
int ui_scroll_begin(UIState *ui, UIScroll *scroll, int x, int y, int w, int view_h,
                    int content_h);
void ui_scroll_end(UIState *ui);
/* Scroll just far enough that [top, bottom) is inside [view_top,
 * view_bottom) with margin to spare; ui_scroll_begin limits the result. */
void ui_scroll_reveal(UIScroll *scroll, int top, int bottom,
                      int view_top, int view_bottom, int margin);

/*
 * A vertical scrollbar in (x, y, w, h) for a view of `view` units over
 * `content` units; nothing when everything fits. The thumb's length shows
 * how much is visible and its place where; dragging it scrolls, and a click
 * on the track above or below it moves one view.
 *
 * Call ui_scrollbar_input BEFORE the scrolled content handles clicks: a
 * click on the bar is used up (mouse_clicked cleared) so the row under it
 * is not picked too. Call ui_scrollbar_draw after the content, outside its
 * clip. Returns 1 when the offset changed.
 */
int ui_scrollbar_input(UIState *ui, UIScroll *scroll, int x, int y, int w, int h,
                       int view, int content);
void ui_scrollbar_draw(UIState *ui, const UIScroll *scroll, int x, int y, int w, int h,
                       int view, int content);
/* Measure with the same font used for drawing, for centered/aligned labels. */
int ui_text_width(UIState *ui, const char *text);
