/*
 * ui.c — Immediate-mode UI shared by the editor and the game settings.
 *
 * The widget calls live in ui_widgets.c and scrolling in ui_scroll.c.
 * This file keeps the edit buffer, caret movement and the helpers both
 * of those call (ui_internal.h). The public entry points stay ui_*.
 *
 * Each widget function draws itself and checks for interaction in a single
 * call.  There is no retained widget tree — the caller decides every frame
 * which widgets exist and where they go.  This "immediate mode" pattern
 * (popularised by Dear ImGui) keeps editor UI code extremely simple:
 *
 *   if (ui_button(&ui, 10, 10, 80, 24, "Save")) { save_level(); }
 *
 * Repeated short labels reuse a bounded per-UI texture cache. Long strings
 * remain transient so document paths cannot grow the cache without bound.
 *
 * Input fields (int, float) use a tiny retained-state mechanism:
 * UIState.active_id tracks which field is being edited, and edit_buf
 * holds the in-progress text.  Only one field can be active at a time.
 */

#include "platform.h"
#include <stdio.h>     /* snprintf                                                 */
#include <string.h>    /* strlen, strncpy, memset                                  */
#include <stdlib.h>    /* strtol, strtof                                           */
#include <errno.h>     /* errno, ERANGE                                             */
#include <float.h>     /* FLT_MAX                                                   */
#include <limits.h>    /* INT_MIN, INT_MAX                                           */
#include <math.h>      /* isfinite                                                   */

#include "ui.h"
#include "ui_internal.h"
#include "utf8.h"    /* utf8_sequence_length */
#include "geometry.h" /* float_same_value */

/* ------------------------------------------------------------------ */
/* Internal helper — forward declarations                              */
/* ------------------------------------------------------------------ */

/*
 * draw_rect — Fill a rectangle with a solid colour.
 *
 * A font-less UI model can process edits without a graphics context.
 */
void draw_rect(int x, int y, int w, int h, Color c);

/*
 * draw_text — Render a single line of text at (x, y).
 *
 * Short labels reuse a bounded renderer-owned texture cache keyed by text
 * and color. Cache misses rasterize UTF-8 and upload once; long
 * strings use transient textures. ui_cleanup releases the retained textures.
 */
void draw_text(UIState *ui, int x, int y,
                       const char *text, Color c);

/*
 * point_in_rect — Test whether point (px, py) lies inside a rectangle.
 *
 * Uses simple axis-aligned comparison:
 *   px must be between rx and rx+rw  (horizontal)
 *   py must be between ry and ry+rh  (vertical)
 */
int point_in_rect(int px, int py, int rx, int ry, int rw, int rh);
void note_widget(UIState *ui, int x, int y, int w, int h);
void field_cursor(UIState *ui, int x, int y, int w, int h, int is_active);
int pointer_over(const UIState *ui, int x, int y, int w, int h);

void notify_before_change(UIState *ui, int id)
{
    if (ui->before_change) {
        ui->before_change(ui->before_change_context, id);
    }
}

static int parse_int_value(const char *text, int *value)
{
    char *end;
    long parsed;

    if (!text || !value || text[0] == '\0') return 0;
    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0' ||
        parsed < INT_MIN || parsed > INT_MAX) return 0;
    *value = (int)parsed;
    return 1;
}

static int parse_float_value(const char *text, float *value)
{
    char *end;
    float parsed;

    if (!text || !value || text[0] == '\0') return 0;
    errno = 0;
    parsed = strtof(text, &end);
    if (errno == ERANGE || end == text || *end != '\0' ||
        !isfinite(parsed)) return 0;
    *value = parsed;
    return 1;
}

/*
 * format_float — The shortest text that reads back as exactly this float:
 * "0.08", not the "0.0799999982" that %.9g prints for the nearest float.
 * Exponent forms are passed over ("100", not "1e+02") unless the value
 * needs one; nine significant digits always read back, so the search
 * stops there.
 */
void format_float(char *out, size_t out_size, float value)
{
    for (int digits = 1; digits <= 9; digits++) {
        float back;
        snprintf(out, out_size, "%.*g", digits, value);
        if (strchr(out, 'e')) continue;
        if (parse_float_value(out, &back) && float_same_value(back, value)) return;
    }
    snprintf(out, out_size, "%.9g", value);
}

/*
 * limit_float_edit — Apply the active float field's limits to a typed value.
 *
 * The float twin of limit_int_edit below, without the step: a value outside
 * the limits is moved to the nearer end, and the note says which end was hit
 * (the other may be -FLT_MAX or FLT_MAX, which would read badly as a range).
 */
static float limit_float_edit(UIState *ui, float value)
{
    if (ui->edit_float_min > ui->edit_float_max) return value;   /* no usable limits */
    if (value < ui->edit_float_min) {
        char typed[32], limit[32];
        format_float(typed, sizeof(typed), value);
        format_float(limit, sizeof(limit), ui->edit_float_min);
        snprintf(ui->edit_note, sizeof(ui->edit_note),
                 "%s is below %s, the smallest this field takes; it was limited",
                 typed, limit);
        return ui->edit_float_min;
    }
    if (value > ui->edit_float_max) {
        char typed[32], limit[32];
        format_float(typed, sizeof(typed), value);
        format_float(limit, sizeof(limit), ui->edit_float_max);
        snprintf(ui->edit_note, sizeof(ui->edit_note),
                 "%s is above %s, the largest this field takes; it was limited",
                 typed, limit);
        return ui->edit_float_max;
    }
    return value;
}

/*
 * limit_int_edit — Apply the active integer field's limits to a typed value.
 *
 * The limits belong to the edit, not to the code that draws the field, so
 * Return and every other way of committing (the editor applies a staged edit
 * before Save, a canvas click, a button) store the same clamped value.
 * long long keeps the rounding below from overflowing near INT_MAX.
 */
static int limit_int_edit(UIState *ui, int value)
{
    long long v = value;
    long long lo = ui->edit_int_min;
    long long hi = ui->edit_int_max;
    long long step = ui->edit_int_step;

    if (lo > hi) return value;            /* no usable limits */
    /* Say which end was hit: one-sided limits use INT_MIN / INT_MAX for
     * the other end, which would read badly as a range. */
    if (v < lo)
        snprintf(ui->edit_note, sizeof(ui->edit_note),
                 "%d is below %lld, the smallest this field takes; it was limited",
                 value, lo);
    else if (v > hi)
        snprintf(ui->edit_note, sizeof(ui->edit_note),
                 "%d is above %lld, the largest this field takes; it was limited",
                 value, hi);
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    if (step > 1) {
        /* Nearest multiple of step, halves rounded up; then step back
         * inside the limits if rounding crossed one of them. */
        long long r = v >= 0 ? (v + step / 2) / step * step
                             : -((-v + step / 2 - 1) / step * step);
        if (r > hi) r -= step;
        if (r < lo) r += step;
        if (r >= lo && r <= hi) v = r;
    }
    return (int)v;
}

static void clear_active_edit(UIState *ui)
{
    ui->active_id = 0;
    ui->edit_type = UI_EDIT_NONE;
    ui->edit_multiline = 0;
    ui->edit_target = NULL;
    ui->edit_target_size = 0;
    ui->edit_int_min = INT_MIN;
    ui->edit_int_max = INT_MAX;
    ui->edit_int_step = 1;
    ui->edit_float_min = -FLT_MAX;
    ui->edit_float_max = FLT_MAX;
    ui->edit_float_nonzero = 0;
    ui->edit_cursor = 0;
    ui->edit_anchor = -1;
    ui->edit_drag = 0;
    ui->edit_buf[0] = '\0';
    ui->pending_text_length = 0;
    ui->pending_text_input[0] = '\0';
    ui->tab_request = 0;
}

/* Is some text selected in the active field?  [*from, *to) is it. */
static int selection_range(const UIState *ui, int *from, int *to)
{
    int length = (int)strlen(ui->edit_buf);
    int anchor = ui->edit_anchor;
    int cursor = ui->edit_cursor;

    if (anchor < 0 || anchor == cursor || anchor > length || cursor > length) return 0;
    *from = anchor < cursor ? anchor : cursor;
    *to = anchor < cursor ? cursor : anchor;
    return 1;
}

/* Remove the selected text, leaving the caret where it was; 1 when there
 * was a selection to remove. */
static int erase_selection(UIState *ui)
{
    int from, to;
    if (!selection_range(ui, &from, &to)) {
        ui->edit_anchor = -1;
        return 0;
    }
    memmove(ui->edit_buf + from, ui->edit_buf + to, strlen(ui->edit_buf) - (size_t)to + 1);
    ui->edit_cursor = from;
    ui->edit_anchor = -1;
    return 1;
}

void apply_pending_text_input(UIState *ui)
{
    size_t i;
    int max_len;

    if (!ui || ui->active_id == 0 || ui->edit_target == NULL) return;
    /* Typed or pasted text replaces the selection. */
    if (ui->pending_text_length > 0) (void)erase_selection(ui);
    max_len = UI_EDIT_BUFFER_SIZE - 1;
    if (ui->edit_type == UI_EDIT_TEXT) {
        max_len = ui->edit_target_size - 1;
        if (max_len > UI_EDIT_BUFFER_SIZE - 1) max_len = UI_EDIT_BUFFER_SIZE - 1;
    }
    if (max_len < 0) max_len = 0;

    for (i = 0; i < ui->pending_text_length;) {
        unsigned char ch = (unsigned char)ui->pending_text_input[i];
        int accepted = ch >= ' ' || (ch == '\n' && ui->edit_multiline);
        size_t bytes = 1;
        if (ui->edit_type == UI_EDIT_TEXT) {
            /* Take one whole character.  Pasted text, or a lone surrogate
             * from a text event, can carry bytes that are not valid UTF-8;
             * dropping such a byte keeps the field from holding text that
             * the level file could not load back. */
            bytes = utf8_sequence_length(ui->pending_text_input + i,
                                         ui->pending_text_length - i);
            if (bytes == 0) {
                accepted = 0;
                bytes = 1;
            }
        }

        if (ui->edit_type == UI_EDIT_INT) {
            accepted = (ch >= '0' && ch <= '9') || ch == '-';
        } else if (ui->edit_type == UI_EDIT_FLOAT) {
            accepted = (ch >= '0' && ch <= '9') || ch == '.' || ch == '-';
        }
        /* Insert at the caret: the text after it moves right to make room.
         * The whole field, not just the part before the caret, must still
         * fit the target buffer. */
        size_t length = strlen(ui->edit_buf);
        if (accepted && length + bytes <= (size_t)max_len) {
            memmove(ui->edit_buf + ui->edit_cursor + bytes,
                    ui->edit_buf + ui->edit_cursor,
                    length - (size_t)ui->edit_cursor + 1);   /* + 1: the NUL */
            memcpy(ui->edit_buf + ui->edit_cursor, ui->pending_text_input + i, bytes);
            ui->edit_cursor += (int)bytes;
        }
        i += bytes;
    }
    ui->pending_text_length = 0;
    ui->pending_text_input[0] = '\0';
}

/* ------------------------------------------------------------------ */
/* Caret movement                                                      */
/* ------------------------------------------------------------------ */

/* UTF-8 continuation bytes look like 10xxxxxx; a character starts anywhere
 * else.  Stepping over them moves by whole characters. */
int is_continuation_byte(char c)
{
    return ((unsigned char)c & 0xC0) == 0x80;
}

/* Byte offset of the character before / after offset `at` in text. */
static int previous_char_start(const char *text, int at)
{
    if (at <= 0) return 0;
    at--;
    while (at > 0 && is_continuation_byte(text[at])) at--;
    return at;
}

static int next_char_start(const char *text, int at)
{
    int length = (int)strlen(text);
    if (at >= length) return length;
    at++;
    while (at < length && is_continuation_byte(text[at])) at++;
    return at;
}

/* Remove the bytes [from, to) of the edit buffer and put the caret at from. */
static void erase_edit_range(UIState *ui, int from, int to)
{
    size_t length = strlen(ui->edit_buf);
    if (from < 0 || to <= from || (size_t)to > length) return;
    memmove(ui->edit_buf + from, ui->edit_buf + to, length - (size_t)to + 1);
    ui->edit_cursor = from;
}

/*
 * Word jumps.  A word is a run of letters, digits, '_' or non-ASCII
 * characters (every byte of a multi-byte UTF-8 character is >= 0x80, so a
 * jump never stops inside one).  Ctrl+Right goes to the end of the next
 * word, Ctrl+Left to the start of the previous one.
 */
static int is_word_byte(char c)
{
    unsigned char b = (unsigned char)c;
    return b >= 0x80 || b == '_' || (b >= '0' && b <= '9') ||
           (b >= 'a' && b <= 'z') || (b >= 'A' && b <= 'Z');
}

static int next_word_end(const char *text, int at)
{
    int length = (int)strlen(text);
    while (at < length && !is_word_byte(text[at])) at++;
    while (at < length && is_word_byte(text[at])) at++;
    return at;
}

static int previous_word_start(const char *text, int at)
{
    while (at > 0 && !is_word_byte(text[at - 1])) at--;
    while (at > 0 && is_word_byte(text[at - 1])) at--;
    return at;
}

/*
 * step_number — Up / Down in a number field: add one step (ten with
 * Shift) to the value being typed, or to the stored value when what is
 * typed is not a number yet, and keep it inside the field's limits.  The
 * result stays a staged edit like typing; Return, Tab or a click commits
 * it.
 *
 * An integer field steps by its own step (1 when it has none).  A float
 * field has no step of its own, so it uses one that suits its size: 1
 * (a pixel, a pixel per second) for values of 10 or more, and 0.1 below
 * that, where the fractions matter (parallax speeds, camera factors).
 */
static void step_number(UIState *ui, int direction, int big)
{
    int times = big ? 10 : 1;

    if (ui->edit_type == UI_EDIT_INT) {
        int value;
        long long step = ui->edit_int_step > 1 ? ui->edit_int_step : 1;
        if (!parse_int_value(ui->edit_buf, &value)) value = *(const int *)ui->edit_target;
        long long next = (long long)value + (long long)direction * times * step;
        if (ui->edit_int_min <= ui->edit_int_max) {
            if (next < ui->edit_int_min) next = ui->edit_int_min;
            if (next > ui->edit_int_max) next = ui->edit_int_max;
        }
        if (next < INT_MIN) next = INT_MIN;
        if (next > INT_MAX) next = INT_MAX;
        snprintf(ui->edit_buf, sizeof(ui->edit_buf), "%d", (int)next);
    } else if (ui->edit_type == UI_EDIT_FLOAT) {
        float value;
        if (!parse_float_value(ui->edit_buf, &value)) value = *(const float *)ui->edit_target;
        double step = fabs((double)value) >= 10.0 ? 1.0 : 0.1;
        double next = (double)value + direction * times * step;
        /* A speed that must keep moving steps over 0. */
        if (ui->edit_float_nonzero && fabs(next) < step / 2) next += direction * step;
        if (ui->edit_float_min <= ui->edit_float_max) {
            if (next < ui->edit_float_min) next = ui->edit_float_min;
            if (next > ui->edit_float_max) next = ui->edit_float_max;
        }
        float stored = (float)next;
        /* "0.3", not "0.300000012", so steps of 0.1 do not show float
         * error piling up. */
        format_float(ui->edit_buf, sizeof(ui->edit_buf), stored);
    } else {
        return;   /* text and dropdowns: nothing to step */
    }
    ui->edit_cursor = (int)strlen(ui->edit_buf);
    ui->edit_anchor = -1;
}

void ui_edit_key(UIState *ui, UIEditKey key, int extend)
{
    int length;
    int from, to;
    int selected;
    int cursor;

    if (!ui || ui->active_id == 0) return;
    /* Text typed before this key belongs in front of the caret's move. */
    apply_pending_text_input(ui);
    length = (int)strlen(ui->edit_buf);
    if (ui->edit_cursor > length) ui->edit_cursor = length;
    selected = selection_range(ui, &from, &to);
    cursor = ui->edit_cursor;
    switch (key) {
    case UI_KEY_LEFT:
        /* Without Shift, Left first collapses a selection to its start. */
        cursor = selected && !extend ? from : previous_char_start(ui->edit_buf, cursor);
        break;
    case UI_KEY_RIGHT:
        cursor = selected && !extend ? to : next_char_start(ui->edit_buf, cursor);
        break;
    case UI_KEY_WORD_LEFT:  cursor = previous_word_start(ui->edit_buf, cursor); break;
    case UI_KEY_WORD_RIGHT: cursor = next_word_end(ui->edit_buf, cursor);       break;
    case UI_KEY_HOME:       cursor = 0;      break;
    case UI_KEY_END:        cursor = length; break;
    case UI_KEY_SELECT_ALL:
        ui->edit_anchor = length > 0 ? 0 : -1;
        ui->edit_cursor = length;
        return;
    case UI_KEY_BACKSPACE:
        if (!erase_selection(ui))
            erase_edit_range(ui, previous_char_start(ui->edit_buf, ui->edit_cursor),
                             ui->edit_cursor);
        return;
    case UI_KEY_DELETE:
        if (!erase_selection(ui))
            erase_edit_range(ui, ui->edit_cursor,
                             next_char_start(ui->edit_buf, ui->edit_cursor));
        return;
    case UI_KEY_UP: case UI_KEY_DOWN:
        step_number(ui, key == UI_KEY_UP ? 1 : -1, extend);
        return;
    case UI_KEY_PICK:
        return;   /* list keys: see ui_dropdown_key */
    }
    /* A caret move: with Shift it stretches the selection from where it
     * started (the anchor stays put); without, any selection ends. */
    if (extend && ui->edit_anchor < 0) ui->edit_anchor = ui->edit_cursor;
    if (!extend) ui->edit_anchor = -1;
    ui->edit_cursor = cursor;
    if (ui->edit_anchor == ui->edit_cursor) ui->edit_anchor = -1;
}

int ui_edit_selected_text(UIState *ui, char *out, size_t out_size)
{
    int from, to;
    size_t length;

    if (!ui || !out || out_size == 0) return 0;
    out[0] = '\0';
    if (ui->active_id == 0) return 0;
    apply_pending_text_input(ui);
    if (!selection_range(ui, &from, &to)) return 0;
    length = (size_t)(to - from);
    if (length >= out_size) {
        length = out_size - 1;
        while (length > 0 && is_continuation_byte(ui->edit_buf[from + (int)length])) length--;
    }
    memcpy(out, ui->edit_buf + from, length);
    out[length] = '\0';
    return (int)length;
}

void ui_edit_delete_selection(UIState *ui)
{
    if (!ui || ui->active_id == 0) return;
    apply_pending_text_input(ui);
    (void)erase_selection(ui);
}

void ui_focus_next(UIState *ui, int direction)
{
    if (!ui || direction == 0) return;
    if (ui->active_id == 0 && ui->dropdown_open_id != 0) {
        ui->dropdown_key_tab = direction > 0 ? 1 : -1;
        return;
    }
    if (ui->active_id == 0) return;
    ui->tab_request = direction > 0 ? 1 : -1;
}

void ui_dropdown_key(UIState *ui, UIEditKey key)
{
    if (!ui || ui->dropdown_open_id == 0) return;
    switch (key) {
    case UI_KEY_UP:   ui->dropdown_key_move--; break;
    case UI_KEY_DOWN: ui->dropdown_key_move++; break;
    /* Far past either end; ui_dropdown stops at the first / last row. */
    case UI_KEY_HOME: ui->dropdown_key_move = -100000; break;
    case UI_KEY_END:  ui->dropdown_key_move = 100000; break;
    case UI_KEY_PICK: ui->dropdown_key_pick = 1; break;
    default: break;
    }
}

/* The field `direction` places after / before `id` in last frame's
 * on-screen order (wrapping around), or 0 when id was not drawn. */
int neighbour_field(const UIState *ui, int id, int direction)
{
    int count = ui->prev_field_order_count;
    for (int i = 0; i < count; i++)
        if (ui->prev_field_order[i] == id)
            return ui->prev_field_order[(i + direction + count) % count];
    return 0;
}

/*
 * field_drawn — Bookkeeping every editable field does when it is drawn:
 * note its place in the on-screen order, and say whether a Tab asked this
 * field to become active.
 */
int field_drawn(UIState *ui, int id, int y)
{
    if (ui->field_order_count < UI_MAX_FIELDS)
        ui->field_order[ui->field_order_count++] = id;
    if (ui->focus_request_id == id && ui->active_id == 0) {
        ui->focus_request_id = 0;
        ui->focus_landed_id = id;
        ui->focus_landed_y = y;
        return 1;
    }
    return 0;
}

void ui_focus_field(UIState *ui, int id)
{
    if (!ui || id == 0) return;
    ui->focus_request_id = id;
    ui->focus_request_frames = 0;
}

/*
 * handle_tab — The active field `id` saw a Tab: commit it like Return, then
 * ask its neighbour in last frame's on-screen order to take the focus
 * (wrapping around at either end).  Returns ui_apply_active_edit's result:
 * 0 (invalid value: the field stays active and nothing moves), 1 or 2.
 */
int handle_tab(UIState *ui, int id)
{
    int direction = ui->tab_request;
    int next;
    int result;

    ui->tab_request = 0;
    result = ui_apply_active_edit(ui);
    if (result == 0) return 0;
    next = neighbour_field(ui, id, direction);
    if (next) {
        ui->focus_request_id = next;
        ui->focus_request_frames = 0;
    }
    return result;
}

int command_allows_activation(UIState *ui, int id)
{
    if (ui->active_id == 0 || ui->active_id == id) return 1;
    if (!ui->before_command) return 0;
    return ui->before_command(ui->before_command_context) != 0;
}

int ui_apply_active_edit(UIState *ui)
{
    int changed = 0;

    if (!ui || ui->active_id == 0 || !ui->edit_target) return 1;

    apply_pending_text_input(ui);

    switch (ui->edit_type) {
    case UI_EDIT_INT: {
        int value;
        int *target = (int *)ui->edit_target;
        if (!parse_int_value(ui->edit_buf, &value)) return 0;
        /* Limits apply to what the user typed. A stored value that is
         * already outside them (a hand-edited file's screen_count = 0,
         * meaning "default") survives a no-op Return unchanged. */
        if (value != *target) value = limit_int_edit(ui, value);
        if (value != *target) {
            notify_before_change(ui, ui->active_id);
            *target = value;
            changed = 1;
        }
        break;
    }
    case UI_EDIT_FLOAT: {
        float value;
        float *target = (float *)ui->edit_target;
        if (!parse_float_value(ui->edit_buf, &value)) return 0;
        /* As for integers: limits apply to a value the user changed. */
        /* Bits, not numeric equality: -0 and a new fraction must count as edits.
         * parse_float_value already rejects NaN and infinity. */
        if (memcmp(&value, target, sizeof(value)) != 0) /* NOLINT(bugprone-suspicious-memory-comparison) */
            value = limit_float_edit(ui, value);
        /* A field that must not hold 0 treats a typed 0 like text that is
         * not a number: nothing is stored and the field stays active. */
        if (ui->edit_float_nonzero && value == 0.0f &&
            memcmp(&value, target, sizeof(value)) != 0) { /* NOLINT(bugprone-suspicious-memory-comparison) */
            snprintf(ui->edit_note, sizeof(ui->edit_note),
                     "0 is not allowed here; type a value other than 0");
            return 0;
        }
        /* "Did the stored value change?" is a question about the stored
         * bits, not about numbers being close: any edit, however small, is
         * a change worth an undo step. Comparing the bytes says exactly that
         * (parse_float_value already rejects NaN and infinity). */
        if (memcmp(&value, target, sizeof(value)) != 0) { /* NOLINT(bugprone-suspicious-memory-comparison) */
            notify_before_change(ui, ui->active_id);
            *target = value;
            changed = 1;
        }
        break;
    }
    case UI_EDIT_TEXT: {
        char *target = (char *)ui->edit_target;
        if (ui->edit_target_size <= 0) return 0;
        if (strcmp(ui->edit_buf, target) != 0) {
            notify_before_change(ui, ui->active_id);
            strncpy(target, ui->edit_buf, (size_t)ui->edit_target_size - 1);
            target[ui->edit_target_size - 1] = '\0';
            changed = 1;
        }
        break;
    }
    case UI_EDIT_NONE:
        break;
    }

    clear_active_edit(ui);
    return changed ? 2 : 1;
}

void ui_cancel_active_edit(UIState *ui)
{
    if (ui) clear_active_edit(ui);
}

/* ------------------------------------------------------------------ */
/* Helper implementations                                              */
/* ------------------------------------------------------------------ */

void draw_rect(int x, int y, int w, int h, Color c)
{
    if (IsWindowReady()) DrawRectangle(x, y, w, h, c);
}

void draw_text(UIState *ui, int x, int y,
                       const char *text, Color c)
{
    if (!ui->font || !text || !text[0] || !IsWindowReady()) return;
    /* raylib's default font is one atlas texture, so each label is drawn
     * glyph by glyph from it in the shared batch: nothing to cache. */
    font_draw(ui->font, text, x, y, c);
}

/* Outline a button or dropdown face when the style asks for one. */
void draw_control_border(UIState *ui, int x, int y, int w, int h)
{
    if (ui->style.border_width <= 0 || !IsWindowReady()) return;
    DrawRectangleLinesEx((Rectangle){(float)x, (float)y, (float)w, (float)h},
                         (float)ui->style.border_width, ui_color(ui, UI_COLOR_BORDER));
}

/* A field's border: accent while editing, dim otherwise. The default style
 * draws a 1 px line; a style with border_width draws that many pixels. */
void draw_field_border(UIState *ui, int x, int y, int w, int h, int is_active)
{
    if (!IsWindowReady()) return;
    Color border = ui_color(ui, is_active ? UI_COLOR_ACCENT : UI_COLOR_TEXT_DIM);
    if (ui->style.border_width > 0)
        DrawRectangleLinesEx((Rectangle){(float)x, (float)y, (float)w, (float)h},
                             (float)ui->style.border_width, border);
    else DrawRectangleLines(x, y, w, h, border);
}

/*
 * field_display_copy — Copy up to `length` bytes of a field's text into
 * out as it is shown: each line break becomes a pilcrow (U+00B6, in the
 * default font), since DrawTextEx would start a new line and draw over the
 * fields below.  Stops early rather than split a pilcrow; returns the
 * number of source bytes copied.
 */
#define FIELD_PILCROW "\xC2\xB6"
int text_width_of(UIState *ui, const char *text, size_t from, size_t to);
static size_t field_display_copy(char *out, size_t out_size, const char *text, size_t length)
{
    size_t used = 0, i = 0;

    if (out_size == 0) return 0;
    for (; i < length && text[i] != '\0'; i++) {
        if (text[i] == '\n') {
            if (used + 2 >= out_size) break;
            memcpy(out + used, FIELD_PILCROW, 2);
            used += 2;
        } else {
            if (used + 1 >= out_size) break;
            out[used++] = text[i];
        }
    }
    out[used] = '\0';
    return i;
}

/* The start of the character that byte offset `at` of text falls in. */
static size_t char_start_at(const char *text, size_t at)
{
    while (at > 0 && is_continuation_byte(text[at])) at--;
    return at;
}

/*
 * draw_field_text — Draw a field's idle text from x: line breaks shown as
 * pilcrows, and text wider than max_width cut at a character boundary and
 * ended with "..." so it never runs past the field.  Returns 1 when the
 * text was cut.
 *
 * Every idle field is drawn each frame, and a description may hold 4 KiB.
 * Measuring one character shorter at a time cost a measurement of the
 * whole remaining text per character: millions of glyphs a frame for a
 * long description.  Width only grows with the prefix, so a binary search
 * finds the longest prefix that fits in about 12 measurements.
 */
int draw_field_text(UIState *ui, int x, int y, int max_width,
                           const char *text, Color color)
{
    char shown[UI_EDIT_BUFFER_SIZE * 2];
    size_t length = strlen(text);

    (void)field_display_copy(shown, sizeof(shown), text, length);
    int cut = ui_text_width(ui, shown) > max_width;
    if (cut) {
        int room = max_width - ui_text_width(ui, "...");
        /* low ends as the largest offset whose whole characters fit; the
         * full text does not fit, so the cut always drops something. */
        size_t low = 0, high = length > 0 ? length - 1 : 0;
        while (low < high) {
            size_t mid = low + (high - low + 1) / 2;
            if (text_width_of(ui, text, 0, char_start_at(text, mid)) <= room) low = mid;
            else high = mid - 1;
        }
        size_t keep = char_start_at(text, low);
        (void)field_display_copy(shown, sizeof(shown) - 3, text, keep);
        strcat(shown, "...");
    }
    draw_text(ui, x, y, shown, color);
    return cut;
}

/*
 * text_width_of — Width in pixels of the bytes [from, to) of text.
 */
int text_width_of(UIState *ui, const char *text, size_t from, size_t to)
{
    char part[UI_EDIT_BUFFER_SIZE * 2];

    (void)field_display_copy(part, sizeof(part), text + from, to > from ? to - from : 0);
    return ui_text_width(ui, part);
}

/*
 * draw_active_edit — Draw the edit buffer of the active field with a
 * blinking caret, scrolled so the caret stays visible.
 *
 * The edit buffer can hold a 4 KiB description, but a field is only a few
 * hundred pixels wide, so only a window of the text is drawn: it starts at
 * the first character that still lets the text up to the caret fit, and
 * ends where the field runs out of room.  Text width only grows as a piece
 * of text gets longer, so a binary search finds each end of the window with
 * about 12 measurements instead of one per character.
 *
 * The caret is a 1-pixel line drawn between two characters, so blinking
 * never shifts the text around it.
 */
/* The first byte of the active field's text a field of width w shows:
 * the smallest offset whose text up to the caret still fits. */
static size_t edit_window_start(UIState *ui, int w)
{
    const char *text = ui->edit_buf;
    const size_t length = strlen(text);
    const int max_width = w - 8;             /* 4 px padding on each side */
    size_t cursor = (size_t)ui->edit_cursor;
    size_t low = 0, high;

    if (cursor > length) cursor = length;
    high = cursor;
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        if (text_width_of(ui, text, mid, cursor) <= max_width) high = mid;
        else low = mid + 1;
    }
    /* Never start drawing in the middle of a multi-byte UTF-8 character. */
    while (low < cursor && is_continuation_byte(text[low])) low++;
    return low;
}

/*
 * edit_offset_at — The character boundary of the active field's text
 * nearest to window x `px`, for a field at x whose text is drawn from byte
 * `start` on.  Walks the visible characters one by one, which is cheap for
 * the few dozen a field shows.
 */
static int edit_offset_at(UIState *ui, int x, size_t start, int px)
{
    const char *text = ui->edit_buf;
    int length = (int)strlen(text);
    int target = px - (x + 4);
    int at = (int)start;
    int at_width = 0;

    if (target <= 0) return at;
    while (at < length) {
        int next = next_char_start(text, at);
        int width = text_width_of(ui, text, start, (size_t)next);
        if (width >= target) return width - target < target - at_width ? next : at;
        at = next;
        at_width = width;
    }
    return length;
}

/*
 * edit_mouse — A press inside the field that is already active puts the
 * caret where it landed; while the button stays down the caret follows
 * the pointer and the text between them is selected.  The press that
 * activates a field (`activated`) leaves the caret at the end, ready to
 * append, as it always did.
 */
void edit_mouse(UIState *ui, int x, int y, int w, int h, int activated)
{
    if (activated) return;
    if (ui->mouse_clicked && pointer_over(ui, x, y, w, h)) {
        apply_pending_text_input(ui);
        ui->edit_cursor = edit_offset_at(ui, x, edit_window_start(ui, w), ui->mouse_x);
        ui->edit_anchor = -1;
        ui->edit_drag = 1;
        ui->edit_drag_from = ui->edit_cursor;
    } else if (ui->edit_drag && ui->mouse_down) {
        ui->edit_cursor = edit_offset_at(ui, x, edit_window_start(ui, w), ui->mouse_x);
        ui->edit_anchor = ui->edit_cursor != ui->edit_drag_from ? ui->edit_drag_from : -1;
    } else {
        ui->edit_drag = 0;
    }
}

void draw_active_edit(UIState *ui, int x, int y, int w)
{
    char display[UI_EDIT_BUFFER_SIZE * 2];
    const char *text = ui->edit_buf;
    const size_t length = strlen(text);
    const int max_width = w - 8;             /* 4 px padding on each side */
    size_t cursor = (size_t)ui->edit_cursor;
    size_t low, high;
    size_t start, end;
    int from, to;

    if (cursor > length) cursor = length;
    start = edit_window_start(ui, w);

    /* end: the largest offset whose text from start still fits. */
    low = cursor;
    high = length;
    while (low < high) {
        size_t mid = low + (high - low + 1) / 2;
        if (text_width_of(ui, text, start, mid) <= max_width) low = mid;
        else high = mid - 1;
    }
    end = low;
    while (end > cursor && end < length && is_continuation_byte(text[end])) end--;

    /* The selection, as far as it is in view, behind the text. */
    if (selection_range(ui, &from, &to) && (size_t)to > start && (size_t)from < end) {
        size_t lo = (size_t)from > start ? (size_t)from : start;
        size_t hi = (size_t)to < end ? (size_t)to : end;
        int left = x + 4 + text_width_of(ui, text, start, lo);
        draw_rect(left, y + 2, text_width_of(ui, text, lo, hi), 16, ui_color(ui, UI_COLOR_SELECTED));
    }
    (void)field_display_copy(display, sizeof(display), text + start, end - start);
    draw_text(ui, x + 4, y + 3, display, ui_color(ui, UI_COLOR_TEXT));

    /*
     * The monotonic clock returns milliseconds. Dividing by 500 and checking
     * odd/even gives a half-second blink.
     */
    if ((clock_millis() / 500) % 2) {
        int caret_x = x + 4 + text_width_of(ui, text, start, cursor);
        draw_rect(caret_x, y + 3, 1, 14, ui_color(ui, UI_COLOR_TEXT));
    }
}

int point_in_rect(int px, int py, int rx, int ry, int rw, int rh)
{
    return px >= rx && px < rx + rw &&
           py >= ry && py < ry + rh;
}

/*
 * pointer_over — Is the pointer on (x, y, w, h) and, inside a clipped
 * (scrolled) panel, on the part of the panel that shows?  A panel still
 * calls the widgets it has scrolled out of view; they must not take a
 * click or a hover meant for what is drawn there instead.
 */
int pointer_over(const UIState *ui, int x, int y, int w, int h)
{
    if (!point_in_rect(ui->mouse_x, ui->mouse_y, x, y, w, h)) return 0;
    return !ui->clip_active ||
           point_in_rect(ui->mouse_x, ui->mouse_y, ui->clip_x, ui->clip_y,
                         ui->clip_w, ui->clip_h);
}

/* An editable field shows the I-beam while the pointer is on it (and on
 * the visible part of its scrolled panel), or while a press in it drags out
 * a selection; not under an open list, which takes the next press itself. */
void field_cursor(UIState *ui, int x, int y, int w, int h, int is_active)
{
    if (ui->dropdown_open_id != 0) return;
    /* A clipped (scrolled) panel still calls fields scrolled out of its
     * view; they must not claim the pointer over what is drawn there. */
    if (pointer_over(ui, x, y, w, h) ||
        (is_active && ui->edit_drag && ui->mouse_down))
        ui->cursor_wish = MOUSE_CURSOR_IBEAM;
}

/* Remember the widget just drawn, for a ui_tooltip call that follows it. */
void note_widget(UIState *ui, int x, int y, int w, int h)
{
    ui->last_x = x;
    ui->last_y = y;
    ui->last_w = w;
    ui->last_h = h;
    if (y + h > ui->widgets_bottom) ui->widgets_bottom = y + h;
}

/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

/*
 * ui_init — Borrow the font; zero-initialise everything else.
 *
 * memset ensures all per-frame and retained fields start at zero/NULL.
 * The caller releases any previous cache before reinitializing.
 */
/* The UI_* colours of ui.h, as constants a static table can hold. */
const UIStyle UI_STYLE_DEFAULT = {{
    [UI_COLOR_PANEL]    = {0x2D, 0x2D, 0x2D, 0xFF},
    [UI_COLOR_TITLE]    = {0x3D, 0x3D, 0x3D, 0xFF},
    [UI_COLOR_INPUT]    = {0x1D, 0x1D, 0x1D, 0xFF},
    [UI_COLOR_BUTTON]   = {0x4D, 0x4D, 0x4D, 0xFF},
    [UI_COLOR_HOT]      = {0x5D, 0x5D, 0x5D, 0xFF},
    [UI_COLOR_SELECTED] = {0x4A, 0x90, 0xD9, 0xFF},
    [UI_COLOR_TEXT]     = {0xE0, 0xE0, 0xE0, 0xFF},
    [UI_COLOR_TEXT_DIM] = {0xA8, 0xA8, 0xA8, 0xFF},
    [UI_COLOR_ACCENT]   = {0x4A, 0x90, 0xD9, 0xFF},
    [UI_COLOR_WARNING]  = {0xFF, 0xBE, 0x5A, 0xFF},
    [UI_COLOR_ERROR]    = {0xFF, 0x70, 0x70, 0xFF},
    [UI_COLOR_BORDER]   = {0x4D, 0x4D, 0x4D, 0xFF},
}, 0};

/* Black faces with white outlines and text; every text colour is at least
 * 7:1 against every face it is drawn on (WCAG AAA); editor_ui_test checks. */
const UIStyle UI_STYLE_HIGH_CONTRAST = {{
    [UI_COLOR_PANEL]    = {0x00, 0x00, 0x00, 0xFF},
    [UI_COLOR_TITLE]    = {0x20, 0x20, 0x20, 0xFF},
    [UI_COLOR_INPUT]    = {0x00, 0x00, 0x00, 0xFF},
    [UI_COLOR_BUTTON]   = {0x00, 0x00, 0x00, 0xFF},
    [UI_COLOR_HOT]      = {0x3A, 0x3A, 0x3A, 0xFF},
    [UI_COLOR_SELECTED] = {0x00, 0x30, 0x7A, 0xFF},
    [UI_COLOR_TEXT]     = {0xFF, 0xFF, 0xFF, 0xFF},
    [UI_COLOR_TEXT_DIM] = {0xD8, 0xD8, 0xD8, 0xFF},
    [UI_COLOR_ACCENT]   = {0xFF, 0xD4, 0x00, 0xFF},
    [UI_COLOR_WARNING]  = {0xFF, 0xC0, 0x60, 0xFF},
    [UI_COLOR_ERROR]    = {0xFF, 0x90, 0x90, 0xFF},
    [UI_COLOR_BORDER]   = {0xFF, 0xFF, 0xFF, 0xFF},
}, 1};

void ui_init(UIState *ui, TextFont *font)
{
    memset(ui, 0, sizeof(*ui));
    ui->font     = font;
    ui->style    = UI_STYLE_DEFAULT;
    clear_active_edit(ui);
}

void ui_cleanup(UIState *ui)
{
    /* UIState owns no textures: labels are drawn from the font's atlas. */
    (void)ui;
}

/* ------------------------------------------------------------------ */

/*
 * ui_begin_frame — Clear per-frame input so stale events don't linger.
 *
 * Called at the very start of each frame, BEFORE consuming input commands.
 * Only the per-frame flags are cleared; retained fields (active_id,
 * edit_buf, edit_cursor, dropdown_open_id) persist across frames.
 */
void ui_begin_frame(UIState *ui)
{
    /*
     * A list stays open only while its dropdown is still drawn. If the
     * widget went away (its entity was deleted, its panel collapsed), close
     * the list; otherwise ui_press would keep handing every click to a list
     * nobody can see.
     */
    if (!ui->dropdown_seen) ui->dropdown_open_id = 0;
    ui->dropdown_seen    = 0;
    /* Likewise a scrollbar drag ends with the button (mouse_down still
     * holds the last frame's state here; it is reset below) or with its
     * scrollbar: a panel that stopped being drawn (a playtest took the
     * window, say) never sees the release, and the up-down cursor would
     * stay. */
    if (!ui->scroll_drag_seen || !ui->mouse_down) ui->scroll_drag = NULL;
    ui->scroll_drag_seen = 0;
    /* The pointer left every widget with a tip: the next one it rests on
     * starts its delay afresh. */
    if (!ui->tooltip_seen) ui->tooltip_w = ui->tooltip_h = 0;
    ui->tooltip_seen = 0;
    ui->tooltip_text = NULL;
    ui->tooltip_shown = 0;
    ui->last_w = ui->last_h = 0;
    ui->cursor_wish = MOUSE_CURSOR_DEFAULT;
    ui->dropdown_click   = 0;
    ui->dropdown_options = NULL;
    ui->dropdown_key_move = 0;
    ui->dropdown_key_pick = 0;
    ui->dropdown_key_tab  = 0;
    ui->mouse_clicked  = 0;
    ui->mouse_down     = 0;
    ui->key_backspace  = 0;
    ui->key_return     = 0;
    ui->key_escape     = 0;
    ui->has_text_input = 0;
    memset(ui->text_input, 0, sizeof(ui->text_input));
    ui->pending_text_length = 0;
    memset(ui->pending_text_input, 0, sizeof(ui->pending_text_input));
    /* The last frame's fields, in the order they were drawn, are the order
     * Tab walks through.  A focus request that no field took within a
     * frame (its panel closed, say) is dropped. */
    memcpy(ui->prev_field_order, ui->field_order,
           (size_t)ui->field_order_count * sizeof(ui->field_order[0]));
    ui->prev_field_order_count = ui->field_order_count;
    ui->field_order_count = 0;
    if (ui->focus_request_id && ++ui->focus_request_frames > 2)
        ui->focus_request_id = 0;
}

int ui_press(UIState *ui)
{
    if (!ui) return 0;
    if (ui->dropdown_open_id != 0) {
        ui->dropdown_click = 1;
        return 1;
    }
    ui->mouse_clicked = 1;
    return 0;
}

void ui_queue_text_input(UIState *ui, const char *text)
{
    size_t length;
    size_t available;

    if (!ui || !text || text[0] == '\0') return;
    length = strlen(text);
    available = sizeof(ui->pending_text_input) - 1 - ui->pending_text_length;
    if (length > available) {
        length = available;
        while (length > 0 && ((unsigned char)text[length] & 0xc0) == 0x80) length--;
    }
    if (length == 0) return;
    memcpy(ui->pending_text_input + ui->pending_text_length, text, length);
    ui->pending_text_length += length;
    ui->pending_text_input[ui->pending_text_length] = '\0';
    ui->has_text_input = 1;
}

