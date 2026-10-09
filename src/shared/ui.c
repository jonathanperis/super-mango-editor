/*
 * ui.c — Immediate-mode UI widget implementation for the level editor.
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
#include "utf8.h"    /* utf8_sequence_length */

/* ------------------------------------------------------------------ */
/* Internal helper — forward declarations                              */
/* ------------------------------------------------------------------ */

/*
 * draw_rect — Fill a rectangle with a solid colour.
 *
 * A font-less UI model can process edits without a graphics context.
 */
static void draw_rect(int x, int y, int w, int h, Color c);

/*
 * draw_text — Render a single line of text at (x, y).
 *
 * Short labels reuse a bounded renderer-owned texture cache keyed by text
 * and color. Cache misses rasterize UTF-8 and upload once; long
 * strings use transient textures. ui_cleanup releases the retained textures.
 */
static void draw_text(UIState *ui, int x, int y,
                       const char *text, Color c);

/*
 * point_in_rect — Test whether point (px, py) lies inside a rectangle.
 *
 * Uses simple axis-aligned comparison:
 *   px must be between rx and rx+rw  (horizontal)
 *   py must be between ry and ry+rh  (vertical)
 */
static int point_in_rect(int px, int py, int rx, int ry, int rw, int rh);

static void notify_before_change(UIState *ui, int id)
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
    ui->edit_target = NULL;
    ui->edit_target_size = 0;
    ui->edit_int_min = INT_MIN;
    ui->edit_int_max = INT_MAX;
    ui->edit_int_step = 1;
    ui->edit_float_min = -FLT_MAX;
    ui->edit_float_max = FLT_MAX;
    ui->edit_float_nonzero = 0;
    ui->edit_cursor = 0;
    ui->edit_buf[0] = '\0';
    ui->pending_text_length = 0;
    ui->pending_text_input[0] = '\0';
    ui->tab_request = 0;
}

static void apply_pending_text_input(UIState *ui)
{
    size_t i;
    int max_len;

    if (!ui || ui->active_id == 0 || ui->edit_target == NULL) return;
    max_len = UI_EDIT_BUFFER_SIZE - 1;
    if (ui->edit_type == UI_EDIT_TEXT) {
        max_len = ui->edit_target_size - 1;
        if (max_len > UI_EDIT_BUFFER_SIZE - 1) max_len = UI_EDIT_BUFFER_SIZE - 1;
    }
    if (max_len < 0) max_len = 0;

    for (i = 0; i < ui->pending_text_length;) {
        unsigned char ch = (unsigned char)ui->pending_text_input[i];
        int accepted = ch >= ' ';
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
static int is_continuation_byte(char c)
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

void ui_edit_key(UIState *ui, UIEditKey key)
{
    int length;

    if (!ui || ui->active_id == 0) return;
    /* Text typed before this key belongs in front of the caret's move. */
    apply_pending_text_input(ui);
    length = (int)strlen(ui->edit_buf);
    if (ui->edit_cursor > length) ui->edit_cursor = length;
    switch (key) {
    case UI_KEY_LEFT:
        ui->edit_cursor = previous_char_start(ui->edit_buf, ui->edit_cursor);
        break;
    case UI_KEY_RIGHT:
        ui->edit_cursor = next_char_start(ui->edit_buf, ui->edit_cursor);
        break;
    case UI_KEY_HOME:
        ui->edit_cursor = 0;
        break;
    case UI_KEY_END:
        ui->edit_cursor = length;
        break;
    case UI_KEY_BACKSPACE:
        erase_edit_range(ui, previous_char_start(ui->edit_buf, ui->edit_cursor),
                         ui->edit_cursor);
        break;
    case UI_KEY_DELETE:
        erase_edit_range(ui, ui->edit_cursor,
                         next_char_start(ui->edit_buf, ui->edit_cursor));
        break;
    case UI_KEY_UP: case UI_KEY_DOWN: case UI_KEY_PICK:
        break;   /* list keys: see ui_dropdown_key */
    }
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
static int neighbour_field(const UIState *ui, int id, int direction)
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
static int field_drawn(UIState *ui, int id, int y)
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
static int handle_tab(UIState *ui, int id)
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

static int command_allows_activation(UIState *ui, int id)
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
        if (memcmp(&value, target, sizeof(value)) != 0 &&
            ui->edit_float_min <= ui->edit_float_max) {
            /* As for integers, name the end that was hit (the other may
             * be -FLT_MAX or FLT_MAX). */
            if (value < ui->edit_float_min)
                snprintf(ui->edit_note, sizeof(ui->edit_note),
                         "%.9g is below %.9g, the smallest this field takes; it was limited",
                         value, ui->edit_float_min);
            else if (value > ui->edit_float_max)
                snprintf(ui->edit_note, sizeof(ui->edit_note),
                         "%.9g is above %.9g, the largest this field takes; it was limited",
                         value, ui->edit_float_max);
            if (value < ui->edit_float_min) value = ui->edit_float_min;
            if (value > ui->edit_float_max) value = ui->edit_float_max;
        }
        /* A field that must not hold 0 treats a typed 0 like text that is
         * not a number: nothing is stored and the field stays active. */
        if (ui->edit_float_nonzero && value == 0.0f &&
            memcmp(&value, target, sizeof(value)) != 0) {
            snprintf(ui->edit_note, sizeof(ui->edit_note),
                     "0 is not allowed here; type a value other than 0");
            return 0;
        }
        /* "Did the stored value change?" is a question about the stored
         * bits, not about numbers being close: any edit, however small, is
         * a change worth an undo step. Comparing the bytes says exactly that
         * (parse_float_value already rejects NaN and infinity). */
        if (memcmp(&value, target, sizeof(value)) != 0) {
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

static void draw_rect(int x, int y, int w, int h, Color c)
{
    if (IsWindowReady()) DrawRectangle(x, y, w, h, c);
}

static void draw_text(UIState *ui, int x, int y,
                       const char *text, Color c)
{
    if (!ui->font || !text || !text[0]) return;
    UITextCacheEntry *entry = NULL;
    uint32_t color = (uint32_t)c.r << 24 | (uint32_t)c.g << 16 | (uint32_t)c.b << 8 | c.a;
    if (strlen(text) < UI_TEXT_CACHE_BYTES) {
        entry = &ui->text_cache[0];
        for (int i = 0; i < UI_TEXT_CACHE_COUNT; i++) {
            UITextCacheEntry *candidate = &ui->text_cache[i];
            if (candidate->texture && candidate->color == color && !strcmp(candidate->text, text)) {
                candidate->used = ++ui->text_clock;
                IntRect dst = {x, y, candidate->w, candidate->h};
                sprite_draw(candidate->texture, NULL, &dst, 0, SPRITE_NORMAL, WHITE);
                return;
            }
            if (candidate->used < entry->used) entry = candidate;
        }
    }
    Texture2D *texture = font_texture(ui->font, text, c);
    if (!texture) return;
    IntRect dst = {x, y, texture->width, texture->height};
    sprite_draw(texture, NULL, &dst, 0, SPRITE_NORMAL, WHITE);
    if (entry) {
        texture_unload(entry->texture);
        entry->texture = texture;
        memcpy(entry->text, text, strlen(text) + 1);
        entry->color = color;
        entry->used = ++ui->text_clock;
        entry->w = dst.w;
        entry->h = dst.h;
    } else texture_unload(texture);
}

/*
 * text_width_of — Width in pixels of the bytes [from, to) of text.
 */
static int text_width_of(UIState *ui, const char *text, size_t from, size_t to)
{
    char part[UI_EDIT_BUFFER_SIZE];
    size_t length = to > from ? to - from : 0;

    if (length >= sizeof(part)) length = sizeof(part) - 1;
    memcpy(part, text + from, length);
    part[length] = '\0';
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
static void draw_active_edit(UIState *ui, int x, int y, int w)
{
    char display[UI_EDIT_BUFFER_SIZE];
    const char *text = ui->edit_buf;
    const size_t length = strlen(text);
    const int max_width = w - 8;             /* 4 px padding on each side */
    size_t cursor = (size_t)ui->edit_cursor;
    size_t low, high;
    size_t start, end;

    if (cursor > length) cursor = length;

    /* start: the smallest offset whose text up to the caret still fits. */
    low = 0;
    high = cursor;
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        if (text_width_of(ui, text, mid, cursor) <= max_width) high = mid;
        else low = mid + 1;
    }
    start = low;
    /* Never start drawing in the middle of a multi-byte UTF-8 character. */
    while (start < cursor && is_continuation_byte(text[start])) start++;

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

    memcpy(display, text + start, end - start);
    display[end - start] = '\0';
    draw_text(ui, x + 4, y + 3, display, UI_TEXT);

    /*
     * The monotonic clock returns milliseconds. Dividing by 500 and checking
     * odd/even gives a half-second blink.
     */
    if ((clock_millis() / 500) % 2) {
        int caret_x = x + 4 + text_width_of(ui, text, start, cursor);
        draw_rect(caret_x, y + 3, 1, 14, UI_TEXT);
    }
}

static int point_in_rect(int px, int py, int rx, int ry, int rw, int rh)
{
    return px >= rx && px < rx + rw &&
           py >= ry && py < ry + rh;
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
void ui_init(UIState *ui, TextFont *font)
{
    memset(ui, 0, sizeof(*ui));
    ui->font     = font;
    clear_active_edit(ui);
}

void ui_cleanup(UIState *ui)
{
    for (int i = 0; i < UI_TEXT_CACHE_COUNT; i++) {
        texture_unload(ui->text_cache[i].texture);
        ui->text_cache[i] = (UITextCacheEntry){0};
    }
    ui->text_clock = 0;
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

/* ------------------------------------------------------------------ */
/* Widgets                                                             */
/* ------------------------------------------------------------------ */

/*
 * ui_button — Draw a clickable rectangle with centred text.
 *
 * Visual states:
 *   Default — BTN colour (medium grey)
 *   Hover   — BTN_HOT colour (slightly lighter) when cursor is inside
 *
 * Returns 1 on the single frame the user clicks inside the button.
 * "Click" = mouse_clicked (button-down edge), not mouse_down (held).
 * This prevents repeated firing while the user holds the button.
 */
int ui_button(UIState *ui, int x, int y, int w, int h, const char *label)
{
    int hovered = point_in_rect(ui->mouse_x, ui->mouse_y, x, y, w, h);

    /* Choose background colour based on hover state. */
    Color bg = hovered ? UI_BTN_HOT : UI_BTN;
    draw_rect(x, y, w, h, bg);

    /*
     * Centre the label text inside the button rectangle.
     *
     * Measure the pixel width and height that this string
     * would occupy when rendered with the given font.  We use the width
     * to compute a horizontal offset that centres the text.
     */
    int tw = 0, th = 0;
    font_measure(ui->font, label, &tw, &th);
    int tx = x + (w - tw) / 2;   /* horizontal centre */
    int ty = y + (h - th) / 2;   /* vertical centre   */
    draw_text(ui, tx, ty, label, UI_TEXT);

    /* Return 1 only on the click-down frame while hovering. */
    if (hovered && ui->mouse_clicked) {
        if (ui->active_id != 0 && ui->before_command &&
            !ui->before_command(ui->before_command_context)) return 0;
        if (ui->active_id != 0) return 0;
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */

/*
 * ui_label — Draw text at (x, y) in the default UI_TEXT colour.
 *
 * This is the most common widget — just a text string, no background,
 * no interactivity.  Used for field labels, section headers, status text.
 */
void ui_label(UIState *ui, int x, int y, const char *text)
{
    draw_text(ui, x, y, text, UI_TEXT);
}

/* ------------------------------------------------------------------ */

/*
 * ui_label_color — Draw text at (x, y) in a caller-specified colour.
 *
 * Useful for emphasis (red for warnings, dim grey for hints, accent
 * colour for active selections).
 */
void ui_label_color(UIState *ui, int x, int y, const char *text,
                    Color color)
{
    draw_text(ui, x, y, text, color);
}

/* ------------------------------------------------------------------ */

/*
 * ui_panel — Draw a filled dark rectangle as a visual group container.
 *
 * No interactivity — panels are purely decorative backgrounds that
 * help the user distinguish sections of the editor interface.
 */
void ui_panel(UIState *ui, int x, int y, int w, int h)
{
    (void)ui;
    draw_rect(x, y, w, h, UI_BG);
}

/* ------------------------------------------------------------------ */

/*
 * ui_int_field — Editable integer input field.
 *
 * Interaction model (state machine):
 *
 *   [Inactive] ─── click on field ───► [Active / editing]
 *        ▲                                    │
 *        │   Return: parse edit_buf, write *value, deactivate
 *        │   Escape: discard edit_buf, deactivate
 *        └────────────────────────────────────┘
 *
 * While active:
 *   - Text-input characters are appended if they are digits or '-'.
 *   - Backspace deletes the last character.
 *   - A monotonic-clock blinking cursor provides visual feedback.
 *
 * The field height is fixed at 20 logical pixels (fits the 13 px font
 * with some padding).
 */
int ui_int_field(UIState *ui, int id, int x, int y, int w, int *value)
{
    return ui_int_field_limited(ui, id, x, y, w, value, INT_MIN, INT_MAX, 1);
}

int ui_int_field_limited(UIState *ui, int id, int x, int y, int w, int *value,
                         int min, int max, int step)
{
    int h        = 20;             /* fixed field height in logical px   */
    int is_active = (ui->active_id == id);
    int changed   = 0;

    /* --- Background and border --- */
    draw_rect(x, y, w, h, UI_INPUT_BG);

    /*
     * Draw a 1-pixel border around the field.  The accent colour indicates
     * the active (editing) field; dim grey marks inactive fields.
     *
     * Only the outline is drawn; the field background stays intact.
     */
    Color border = is_active ? UI_ACCENT : UI_TEXT_DIM;
    if (IsWindowReady()) DrawRectangleLines(x, y, w, h, border);

    /* --- Activation on click, or by Tab from the previous field --- */
    if (field_drawn(ui, id, y) ||
        (ui->mouse_clicked && point_in_rect(ui->mouse_x, ui->mouse_y,
                                            x, y, w, h) &&
         command_allows_activation(ui, id))) {
        /*
         * Start editing: copy the current value into edit_buf so the user
         * sees the existing number and can modify it.  snprintf converts
         * the integer to its decimal string representation.
         */
        ui->active_id = id;
        ui->edit_type = UI_EDIT_INT;
        ui->edit_target = value;
        ui->edit_target_size = sizeof(*value);
        ui->edit_int_min = min;
        ui->edit_int_max = max;
        ui->edit_int_step = step;
        snprintf(ui->edit_buf, sizeof(ui->edit_buf), "%d", *value);
        ui->edit_cursor = (int)strlen(ui->edit_buf);
        is_active = 1;
    }

    /*
     * Deactivate if the user clicks somewhere else (clicked but NOT inside
     * this field, and this field is currently active).  This is the
     * "click outside to cancel" behaviour.
     */
    /* --- Keyboard handling while active --- */
    if (is_active) {
        /*
         * Append typed text. Text events deliver actual characters
         * (respecting the OS keyboard layout), unlike key-down commands
         * which gives raw key codes.  We only accept digits (0-9) and the
         * minus sign for negative numbers.
         */
        apply_pending_text_input(ui);

        /* Backspace — delete the character before the caret. */
        if (ui->key_backspace) ui_edit_key(ui, UI_KEY_BACKSPACE);

        /*
         * Return — confirm only a complete, in-range integer.  Invalid input
         * leaves the caller's value unchanged.
         */
        if (ui->key_return) {
            int result = ui_apply_active_edit(ui);
            if (result != 0) {
                changed = result == 2;
                is_active = 0;
            }
        } else if (ui->tab_request) {
            /* Tab commits like Return, then focuses the neighbour. */
            int result = handle_tab(ui, id);
            if (result != 0) {
                changed = result == 2;
                is_active = 0;
            }
        }

        /* Escape — cancel: discard edits and deactivate. */
        if (ui->key_escape) {
            ui_cancel_active_edit(ui);
            is_active = 0;
        }
    }

    /* --- Draw the display text --- */
    if (is_active) {
        /* Active: show the edit buffer with a blinking cursor. */
        draw_active_edit(ui, x, y, w);
    } else {
        /* Inactive: show the current value as a plain number. */
        char display[32];
        snprintf(display, sizeof(display), "%d", *value);
        draw_text(ui, x + 4, y + 3, display, UI_TEXT);
    }

    return changed;
}

/* ------------------------------------------------------------------ */

/*
 * ui_float_field — Editable floating-point input field.
 *
 * Identical interaction model to ui_int_field, but:
 *   - Accepts '.' (decimal point) in addition to digits and '-'.
 *   - Displays the value with nine significant digits ("%.9g").
 *   - Parses the edit buffer with complete-input validation.
 */
static int float_field(UIState *ui, int id, int x, int y, int w,
                       float *value, float min, float max, int nonzero);

int ui_float_field(UIState *ui, int id, int x, int y, int w, float *value)
{
    return float_field(ui, id, x, y, w, value, -FLT_MAX, FLT_MAX, 0);
}

int ui_float_field_limited(UIState *ui, int id, int x, int y, int w,
                           float *value, float min, float max)
{
    return float_field(ui, id, x, y, w, value, min, max, 0);
}

int ui_float_field_nonzero(UIState *ui, int id, int x, int y, int w,
                           float *value, float limit)
{
    return float_field(ui, id, x, y, w, value, -limit, limit, 1);
}

/* The float field itself; nonzero is ui_float_field_nonzero's extra rule. */
static int float_field(UIState *ui, int id, int x, int y, int w,
                       float *value, float min, float max, int nonzero)
{
    int h         = 20;
    int is_active = (ui->active_id == id);
    int changed   = 0;

    /* --- Background and border --- */
    draw_rect(x, y, w, h, UI_INPUT_BG);

    Color border = is_active ? UI_ACCENT : UI_TEXT_DIM;
    if (IsWindowReady()) DrawRectangleLines(x, y, w, h, border);

    /* --- Activation on click, or by Tab from the previous field --- */
    if (field_drawn(ui, id, y) ||
        (ui->mouse_clicked && point_in_rect(ui->mouse_x, ui->mouse_y,
                                            x, y, w, h) &&
         command_allows_activation(ui, id))) {
        ui->active_id = id;
        ui->edit_type = UI_EDIT_FLOAT;
        ui->edit_target = value;
        ui->edit_target_size = sizeof(*value);
        ui->edit_float_min = min;
        ui->edit_float_max = max;
        ui->edit_float_nonzero = nonzero;
        /*
         * Keep enough significant digits for a float to survive activation
         * and a no-op Return unchanged.
         */
        snprintf(ui->edit_buf, sizeof(ui->edit_buf), "%.9g", *value);
        ui->edit_cursor = (int)strlen(ui->edit_buf);
        is_active = 1;
    }

    /* Deactivate on click outside. */
    /* --- Keyboard handling while active --- */
    if (is_active) {
        apply_pending_text_input(ui);

        if (ui->key_backspace) ui_edit_key(ui, UI_KEY_BACKSPACE);

        /*
         * Return — parse only a complete, finite float.  Invalid input leaves
         * the caller's value unchanged.
         */
        if (ui->key_return) {
            int result = ui_apply_active_edit(ui);
            if (result != 0) {
                changed = result == 2;
                is_active = 0;
            }
        } else if (ui->tab_request) {
            /* Tab commits like Return, then focuses the neighbour. */
            int result = handle_tab(ui, id);
            if (result != 0) {
                changed = result == 2;
                is_active = 0;
            }
        }

        if (ui->key_escape) {
            ui_cancel_active_edit(ui);
            is_active = 0;
        }
    }

    /* --- Draw display text --- */
    if (is_active) {
        draw_active_edit(ui, x, y, w);
    } else {
        char display[32];
        snprintf(display, sizeof(display), "%.9g", *value);
        draw_text(ui, x + 4, y + 3, display, UI_TEXT);
    }

    return changed;
}

/* ------------------------------------------------------------------ */

/*
 * ui_text_field — Editable single-line text field.
 *
 * Identical interaction model to ui_int_field but accepts any printable
 * character, not just digits.  The caller owns the buffer; this widget
 * copies the edit result back on Return.
 *
 * buf      — pointer to the editable char array (e.g. LevelDef.name).
 * buf_size — total capacity including the '\0' terminator.
 */
int ui_text_field(UIState *ui, int id, int x, int y, int w,
                  char *buf, int buf_size)
{
    int h         = 20;
    int is_active = (ui->active_id == id);
    int changed   = 0;

    /* --- Background and border --- */
    draw_rect(x, y, w, h, UI_INPUT_BG);

    Color border = is_active ? UI_ACCENT : UI_TEXT_DIM;
    if (IsWindowReady()) DrawRectangleLines(x, y, w, h, border);

    /* --- Activation on click, or by Tab from the previous field --- */
    if (field_drawn(ui, id, y) ||
        (ui->mouse_clicked && point_in_rect(ui->mouse_x, ui->mouse_y,
                                            x, y, w, h) &&
         command_allows_activation(ui, id))) {
        ui->active_id = id;
        ui->edit_type = UI_EDIT_TEXT;
        ui->edit_target = buf;
        ui->edit_target_size = buf_size;
        /*
         * Copy the current buffer contents into edit_buf so the user
         * sees the existing text and can modify it.
         */
        {
            size_t source_limit = buf_size > 0 ? (size_t)buf_size - 1 : 0;
            size_t copy_len = 0;
            while (copy_len < source_limit &&
                   copy_len < sizeof(ui->edit_buf) - 1 &&
                   buf[copy_len] != '\0') {
                copy_len++;
            }
            memcpy(ui->edit_buf, buf, copy_len);
            ui->edit_buf[copy_len] = '\0';
        }
        ui->edit_cursor = (int)strlen(ui->edit_buf);
        is_active = 1;
    }

    /* Deactivate on click outside. */
    /* --- Keyboard handling while active --- */
    if (is_active) {
        /*
         * Accept any printable character (>= space).  The edit_buf capacity
         * and caller's buf_size both limit the length.
         */
        apply_pending_text_input(ui);

        /* Backspace removes the whole UTF-8 character before the caret. */
        if (ui->key_backspace) ui_edit_key(ui, UI_KEY_BACKSPACE);

        /*
         * Return — confirm the edit.  Copy edit_buf back into the caller's
         * buffer.  Only signal "changed" if the text actually differs.
         */
        if (ui->key_return) {
            int result = ui_apply_active_edit(ui);
            if (result != 0) {
                changed = result == 2;
                is_active = 0;
            }
        } else if (ui->tab_request) {
            /* Tab commits like Return, then focuses the neighbour. */
            int result = handle_tab(ui, id);
            if (result != 0) {
                changed = result == 2;
                is_active = 0;
            }
        }

        if (ui->key_escape) {
            ui_cancel_active_edit(ui);
            is_active = 0;
        }
    }

    /* --- Draw display text --- */
    if (is_active) {
        draw_active_edit(ui, x, y, w);
    } else {
        draw_text(ui, x + 4, y + 3,
                  buf[0] ? buf : "Untitled", UI_TEXT_DIM);
    }

    return changed;
}

/* ------------------------------------------------------------------ */

/*
 * ui_dropdown — Selectable dropdown (combo box).
 *
 * Visual layout:
 *
 *   ┌──────────────────────┐
 *   │ Current option   ▼   │  ← header: always visible
 *   ├──────────────────────┤
 *   │ Option A              │  ← option list: only visible when open
 *   │ Option B (hover)      │
 *   │ Option C              │
 *   └──────────────────────┘
 *
 * Interaction:
 *   - Click the header to open the list.
 *   - While the list is open, the next press belongs to it: on an option it
 *     picks that option, anywhere else (the header included) it just closes
 *     the list.  Either way nothing underneath reacts to that press.
 *
 * The list hangs over whatever is below the header: the canvas, palette
 * rows, other fields.  Two things keep it on top.  ui_press routes a press
 * to the open list instead of setting mouse_clicked, so widgets drawn
 * earlier in the frame never see it, and the list clears both flags once it
 * has used the press, so widgets drawn later do not either.  The list itself
 * is drawn by ui_draw_overlays after everything else, so a field drawn later
 * cannot paint over it.
 *
 * Only one dropdown can be open at a time.  dropdown_open_id in UIState
 * tracks which one is expanded.
 *
 * The keyboard reaches it too: a dropdown has its place in the Tab order
 * like a field, and Tab onto it opens its list.  While open, Up / Down
 * move a highlight (ui_dropdown_key), Enter picks the highlighted option
 * like a click, Esc closes the list, and Tab closes it and moves on.
 */
int ui_dropdown(UIState *ui, int id, int x, int y, int w,
                const char **options, int count, int *selected)
{
    int h       = 20;              /* height of the header row             */
    int is_open = (ui->dropdown_open_id == id);
    int changed = 0;

    /* A place in the Tab order, the order on screen. */
    if (ui->field_order_count < UI_MAX_FIELDS)
        ui->field_order[ui->field_order_count++] = id;

    /* --- Draw the header (always visible) --- */
    int hovered_header = point_in_rect(ui->mouse_x, ui->mouse_y,
                                       x, y, w, h);
    Color header_bg = hovered_header ? UI_BTN_HOT : UI_BTN;
    draw_rect(x, y, w, h, header_bg);

    /* Show the currently selected option text (or "---" if out of range). */
    const char *current = (*selected >= 0 && *selected < count)
                          ? options[*selected]
                          : "---";
    draw_text(ui, x + 4, y + 3, current, UI_TEXT);

    /*
     * Draw a small "▼" indicator on the right side of the header to signal
     * that this is a dropdown.  We use the "v" character as a simple stand-in;
     * the font may or may not have a real triangle glyph.
     */
    draw_text(ui, x + w - 14, y + 3, "v", UI_TEXT_DIM);

    /* A focus request (ui_focus_field, e.g. a jump from a validation
     * message) opens the list: for a dropdown, that is taking the focus.
     * Like a click, it waits until no text field is being edited. */
    if (!is_open && ui->focus_request_id == id && ui->active_id == 0) {
        ui->focus_request_id = 0;
        ui->focus_landed_id = id;
        ui->focus_landed_y = y;
        ui->dropdown_open_id = id;
        ui->dropdown_highlight = *selected >= 0 && *selected < count ? *selected : 0;
        is_open = 1;
    }

    if (!is_open) {
        /* --- Open on header click --- */
        if (ui->mouse_clicked && hovered_header) {
            if (ui->active_id != 0 && ui->before_command &&
                !ui->before_command(ui->before_command_context)) {
                return 0;
            }
            if (ui->active_id != 0) return 0;
            ui->dropdown_open_id = id;   /* implicitly closes any other */
            ui->mouse_clicked = 0;       /* the press opened the list    */
            ui->dropdown_highlight = *selected >= 0 && *selected < count ? *selected : 0;
            is_open = 1;
        }
    } else if (ui->dropdown_key_pick || ui->dropdown_key_tab) {
        /* --- Enter picks the highlighted option; Tab moves on --- */
        int pick = ui->dropdown_key_pick ? ui->dropdown_highlight + ui->dropdown_key_move : -1;
        if (pick >= count) pick = count - 1;
        if (ui->dropdown_key_pick && pick < 0) pick = 0;
        if (pick >= 0 && pick != *selected) {
            notify_before_change(ui, id);
            *selected = pick;
            changed = 1;
        }
        if (ui->dropdown_key_tab) {
            int next = neighbour_field(ui, id, ui->dropdown_key_tab);
            if (next && next != id) {
                ui->focus_request_id = next;
                ui->focus_request_frames = 0;
            }
        }
        ui->dropdown_open_id = 0;
        ui->dropdown_key_move = ui->dropdown_key_pick = ui->dropdown_key_tab = 0;
        is_open = 0;
    } else if (ui->mouse_clicked || ui->dropdown_click) {
        /* --- The press while open: pick an option or just close --- */
        int list_y = y + h;
        if (count > 0 &&
            point_in_rect(ui->mouse_x, ui->mouse_y, x, list_y, w, count * h)) {
            int i = (ui->mouse_y - list_y) / h;
            if (ui->active_id != 0 && ui->before_command &&
                !ui->before_command(ui->before_command_context)) {
                return 0;
            }
            if (ui->active_id != 0) return 0;
            /* An unrecognised current value (*selected out of range) never
             * equals i, so every option, the first included, applies. */
            if (i != *selected) {
                notify_before_change(ui, id);
                *selected = i;
                changed   = 1;
            }
        }
        ui->dropdown_open_id = 0;
        ui->dropdown_click = 0;
        ui->mouse_clicked = 0;
        is_open = 0;
    }

    /* --- Up / Down move the highlight, stopping at either end --- */
    if (is_open && ui->dropdown_key_move) {
        ui->dropdown_highlight += ui->dropdown_key_move;
        if (ui->dropdown_highlight >= count) ui->dropdown_highlight = count - 1;
        if (ui->dropdown_highlight < 0) ui->dropdown_highlight = 0;
        ui->dropdown_key_move = 0;
    }

    /* --- Remember the open list; ui_draw_overlays draws it last --- */
    if (is_open) {
        ui->dropdown_seen = 1;
        ui->dropdown_options = options;
        ui->dropdown_count = count;
        ui->dropdown_x = x;
        ui->dropdown_y = y + h;
        ui->dropdown_w = w;
        ui->dropdown_selected = *selected;
    }

    return changed;
}

void ui_draw_overlays(UIState *ui)
{
    int h = 20;

    if (!ui || ui->dropdown_open_id == 0 || !ui->dropdown_options) return;
    for (int i = 0; i < ui->dropdown_count; i++) {
        int oy = ui->dropdown_y + i * h;   /* Y of this option row */
        int hovered = point_in_rect(ui->mouse_x, ui->mouse_y,
                                    ui->dropdown_x, oy, ui->dropdown_w, h);

        /* Highlight: accent colour for the selected item, hot for hover
         * and for the row the keyboard highlights. */
        Color bg = i == ui->dropdown_selected                 ? UI_BTN_ACTIVE
                 : hovered || i == ui->dropdown_highlight ? UI_BTN_HOT
                                                          : UI_BTN;
        draw_rect(ui->dropdown_x, oy, ui->dropdown_w, h, bg);
        draw_text(ui, ui->dropdown_x + 4, oy + 3,
                  ui->dropdown_options[i], UI_TEXT);
    }
}

/* ------------------------------------------------------------------ */

/*
 * ui_separator — Draw a thin horizontal dividing line.
 *
 * A 1-pixel line from (x, y) to (x+w, y) in the dim text colour.
 * Helps visually group related widgets within a panel.
 */
void ui_separator(UIState *ui, int x, int y, int w)
{
    (void)ui;
    if (IsWindowReady()) DrawLine(x, y, x + w, y, UI_TEXT_DIM);
}

/* ------------------------------------------------------------------ */

/*
 * ui_text_width — Return the rendered pixel width of a string in the UI font.
 *
 * Measures glyph advances without drawing anything.
 * Returns 0 if the font is NULL or the string is empty.
 */
int ui_text_width(UIState *ui, const char *text)
{
    if (!ui->font || !text || text[0] == '\0') return 0;
    int w = 0;
    font_measure(ui->font, text, &w, NULL);
    return w;
}
