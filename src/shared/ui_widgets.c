/*
 * ui_widgets.c — Immediate-mode widgets (buttons, fields, dropdowns, labels).
 *
 * Drawn and hit-tested in one call, with no retained widget tree. The edit
 * buffer and caret they share with the keyboard path live in ui.c.
 */

#include "platform.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>   /* INT_MIN, INT_MAX */
#include <float.h>    /* FLT_MAX */

#include "ui.h"
#include "ui_internal.h"
#include "geometry.h"

/* ------------------------------------------------------------------ */
/* Widgets                                                             */
/* ------------------------------------------------------------------ */

/*
 * button_body — What ui_button and ui_toggle share: a face in role `face`,
 * the style's outline (in `border` instead when that is not
 * UI_COLOR_BORDER), the label centred, and the click.
 *
 * Returns 1 on the single frame the user clicks inside the button.
 * "Click" = mouse_clicked (button-down edge), not mouse_down (held).
 * This prevents repeated firing while the user holds the button.
 */
static int button_body(UIState *ui, int x, int y, int w, int h, UIIcon icon,
                       const char *label, UIColorRole face, UIColorRole border)
{
    int hovered = pointer_over(ui, x, y, w, h);

    note_widget(ui, x, y, w, h);
    draw_rect(x, y, w, h, ui_color(ui, face));
    if (border == UI_COLOR_BORDER) draw_control_border(ui, x, y, w, h);
    else if (ui->style.border_width > 0 && IsWindowReady())
        DrawRectangleLinesEx((Rectangle){(float)x, (float)y, (float)w, (float)h},
                             (float)ui->style.border_width, ui_color(ui, border));

    /*
     * Centre the label text inside the button rectangle.
     *
     * Measure the pixel width and height that this string
     * would occupy when rendered with the given font.  We use the width
     * to compute a horizontal offset that centres the text.
     */
    int tw = 0, th = 0;
    font_measure(ui->font, label, &tw, &th);
    /* An icon goes before the label, and the pair is centred as one. */
    int iw = icon != UI_ICON_NONE ? UI_ICON_SIZE + (tw > 0 ? UI_ICON_GAP : 0) : 0;
    int tx = x + (w - iw - tw) / 2;   /* horizontal centre */
    int ty = y + (h - th) / 2;        /* vertical centre   */
    if (iw > 0) ui_draw_icon(icon, tx, y + (h - UI_ICON_SIZE) / 2, ui_color(ui, UI_COLOR_TEXT));
    draw_text(ui, tx + iw, ty, label, ui_color(ui, UI_COLOR_TEXT));

    /* Return 1 only on the click-down frame while hovering. */
    if (hovered && ui->mouse_clicked) {
        if (ui->active_id != 0 && ui->before_command &&
            !ui->before_command(ui->before_command_context)) return 0;
        if (ui->active_id != 0) return 0;
        return 1;
    }
    return 0;
}

/*
 * ui_button — Draw a clickable rectangle with centred text.
 *
 * Visual states:
 *   Default — BTN colour (medium grey)
 *   Hover   — BTN_HOT colour (slightly lighter) when cursor is inside
 */
int ui_button(UIState *ui, int x, int y, int w, int h, const char *label)
{
    return ui_button_icon(ui, x, y, w, h, UI_ICON_NONE, label);
}

int ui_button_icon(UIState *ui, int x, int y, int w, int h, UIIcon icon, const char *label)
{
    int hovered = pointer_over(ui, x, y, w, h);
    return button_body(ui, x, y, w, h, icon, label,
                       hovered ? UI_COLOR_HOT : UI_COLOR_BUTTON, UI_COLOR_BORDER);
}

/*
 * ui_toggle — A button that stays down while `on`.
 *
 * On, the face is UI_COLOR_SELECTED (the current tool, a shown grid),
 * and a style with outlines draws it in the accent colour, so high
 * contrast marks the state with more than a shade of blue.  Off, it
 * looks and hovers like ui_button.
 */
int ui_toggle(UIState *ui, int x, int y, int w, int h, const char *label, int on)
{
    return ui_toggle_icon(ui, x, y, w, h, UI_ICON_NONE, label, on);
}

int ui_toggle_icon(UIState *ui, int x, int y, int w, int h, UIIcon icon,
                   const char *label, int on)
{
    int hovered = pointer_over(ui, x, y, w, h);
    UIColorRole face = on ? UI_COLOR_SELECTED : hovered ? UI_COLOR_HOT : UI_COLOR_BUTTON;
    return button_body(ui, x, y, w, h, icon, label, face,
                       on ? UI_COLOR_ACCENT : UI_COLOR_BORDER);
}

/*
 * ui_draw_icon — Draw one raygui icon, a pixel per set bit.
 *
 * Bit k of word w is the pixel at (k % 16, 2 * w + k / 16).  Rows of set
 * bits are drawn as one rectangle each, so a solid icon costs a few dozen
 * rectangles in the shared batch rather than one per pixel.
 */
void ui_draw_icon(UIIcon icon, int x, int y, Color color)
{
    if (icon <= UI_ICON_NONE || icon >= UI_ICON_COUNT) return;
    for (int row = 0; row < UI_ICON_SIZE; row++) {
        uint32_t bits = raygui_icon_bits[icon][row / 2] >> (row % 2 * 16);
        for (int col = 0; col < UI_ICON_SIZE;) {
            if (!(bits >> col & 1u)) { col++; continue; }
            int start = col;
            while (col < UI_ICON_SIZE && bits >> col & 1u) col++;
            draw_rect(x + start, y + row, col - start, 1, color);
        }
    }
}

/* ------------------------------------------------------------------ */

/*
 * ui_button_disabled — Draw a button that cannot be used right now.
 *
 * It keeps its place and its label, so the row it sits in does not jump
 * around as it comes and goes, but it looks switched off: a darker box, a
 * dim label and no hover highlight.  It takes no click, which is why it
 * returns nothing: the caller has no command to run.
 */
void ui_button_disabled(UIState *ui, int x, int y, int w, int h, const char *label)
{
    int tw = 0, th = 0;

    note_widget(ui, x, y, w, h);
    draw_rect(x, y, w, h, ui_color(ui, UI_COLOR_TITLE));
    draw_control_border(ui, x, y, w, h);
    font_measure(ui->font, label, &tw, &th);
    draw_text(ui, x + (w - tw) / 2, y + (h - th) / 2, label, ui_color(ui, UI_COLOR_TEXT_DIM));
}

/* ------------------------------------------------------------------ */

void ui_set_cursor(UIState *ui, int cursor)
{
    if (ui) ui->cursor_wish = cursor;
}

void ui_apply_cursor(UIState *ui)
{
    if (!ui) return;
    int want = ui->scroll_drag ? MOUSE_CURSOR_RESIZE_NS : ui->cursor_wish;
    if (want == ui->cursor_shown) return;
    ui->cursor_shown = want;
    ui->cursor_changes++;
#ifndef MANGO_RAYLIB_MEMORY
    /* Memory has no pointer and warns on every call; a hidden window
     * (render smoke) has nothing to show it on. */
    if (IsWindowReady() && !IsWindowHidden()) SetMouseCursor(want);
#endif
}

/* ------------------------------------------------------------------ */

/*
 * ui_label — Draw text at (x, y) in the style's text colour.
 *
 * This is the most common widget — just a text string, no background,
 * no interactivity.  Used for field labels, section headers, status text.
 */
void ui_label(UIState *ui, int x, int y, const char *text)
{
    draw_text(ui, x, y, text, ui_color(ui, UI_COLOR_TEXT));
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

int ui_label_fit(UIState *ui, int x, int y, int max_w, const char *text, Color color)
{
    if (!text || !text[0]) return 0;
    /* A label that was cut short, or has line breaks shown as pilcrows,
     * offers its full text as a tooltip. */
    if (!draw_field_text(ui, x, y, max_w, text, color) && !strchr(text, '\n')) return 0;
    /* In a scrolled panel, a row moved out of the view (under the panel's
     * title, say) is not what the pointer is on. */
    if (ui->clip_active &&
        !point_in_rect(ui->mouse_x, ui->mouse_y, ui->clip_x, ui->clip_y, ui->clip_w, ui->clip_h))
        return 1;
    note_widget(ui, x, y, max_w, 14);
    ui_tooltip(ui, text);
    return 1;
}

void ui_label_tip(UIState *ui, int x, int y, const char *text, const char *tip)
{
    ui_label(ui, x, y, text);
    if (!text || !text[0]) return;
    note_widget(ui, x, y - 2, ui_text_width(ui, text), 16);
    ui_tooltip(ui, tip);
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
    draw_rect(x, y, w, h, ui_color(ui, UI_COLOR_PANEL));
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
    note_widget(ui, x, y, w, h);
    draw_rect(x, y, w, h, ui_color(ui, UI_COLOR_INPUT));

    /*
     * Draw a 1-pixel border around the field.  The accent colour indicates
     * the active (editing) field; dim grey marks inactive fields.
     *
     * Only the outline is drawn; the field background stays intact.
     */
    draw_field_border(ui, x, y, w, h, is_active);

    /* --- Activation on click, or by Tab from the previous field --- */
    int clicked_on = !is_active && ui->mouse_clicked &&
                     pointer_over(ui, x, y, w, h);
    if (field_drawn(ui, id, y) ||
        (clicked_on && command_allows_activation(ui, id))) {
        /*
         * Start editing: copy the current value into edit_buf so the user
         * sees the existing number and can modify it.  snprintf converts
         * the integer to its decimal string representation.
         */
        ui->active_id = id;
        ui->edit_type = UI_EDIT_INT;
        ui->edit_multiline = 0;
        ui->edit_target = value;
        ui->edit_target_size = sizeof(*value);
        ui->edit_int_min = min;
        ui->edit_int_max = max;
        ui->edit_int_step = step;
        snprintf(ui->edit_buf, sizeof(ui->edit_buf), "%d", *value);
        ui->edit_cursor = (int)strlen(ui->edit_buf);
        ui->edit_anchor = -1;
        is_active = 1;
    }
    /* The mouse places the caret and drags out a selection. */
    if (is_active) edit_mouse(ui, x, y, w, h, clicked_on);
    field_cursor(ui, x, y, w, h, is_active);

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
        if (ui->key_backspace) ui_edit_key(ui, UI_KEY_BACKSPACE, 0);

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
        draw_text(ui, x + 4, y + 3, display, ui_color(ui, UI_COLOR_TEXT));
    }

    return changed;
}

/* ------------------------------------------------------------------ */

/*
 * ui_float_field — Editable floating-point input field.
 *
 * Identical interaction model to ui_int_field, but:
 *   - Accepts '.' (decimal point) in addition to digits and '-'.
 *   - Shows the shortest text that reads back as the same float ("0.08").
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
    note_widget(ui, x, y, w, h);
    draw_rect(x, y, w, h, ui_color(ui, UI_COLOR_INPUT));

    draw_field_border(ui, x, y, w, h, is_active);

    /* --- Activation on click, or by Tab from the previous field --- */
    int clicked_on = !is_active && ui->mouse_clicked &&
                     pointer_over(ui, x, y, w, h);
    if (field_drawn(ui, id, y) ||
        (clicked_on && command_allows_activation(ui, id))) {
        ui->active_id = id;
        ui->edit_type = UI_EDIT_FLOAT;
        ui->edit_multiline = 0;
        ui->edit_target = value;
        ui->edit_target_size = sizeof(*value);
        ui->edit_float_min = min;
        ui->edit_float_max = max;
        ui->edit_float_nonzero = nonzero;
        /*
         * Keep enough significant digits for a float to survive activation
         * and a no-op Return unchanged.
         */
        format_float(ui->edit_buf, sizeof(ui->edit_buf), *value);
        ui->edit_cursor = (int)strlen(ui->edit_buf);
        ui->edit_anchor = -1;
        is_active = 1;
    }
    /* The mouse places the caret and drags out a selection. */
    if (is_active) edit_mouse(ui, x, y, w, h, clicked_on);
    field_cursor(ui, x, y, w, h, is_active);

    /* Deactivate on click outside. */
    /* --- Keyboard handling while active --- */
    if (is_active) {
        apply_pending_text_input(ui);

        if (ui->key_backspace) ui_edit_key(ui, UI_KEY_BACKSPACE, 0);

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
        format_float(display, sizeof(display), *value);
        draw_text(ui, x + 4, y + 3, display, ui_color(ui, UI_COLOR_TEXT));
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
    return ui_text_field_hint(ui, id, x, y, w, buf, buf_size, NULL, 0);
}

int ui_text_field_hint(UIState *ui, int id, int x, int y, int w, char *buf, int buf_size,
                       const char *hint, int multiline)
{
    int h         = 20;
    int is_active = (ui->active_id == id);
    int changed   = 0;

    /* --- Background and border --- */
    note_widget(ui, x, y, w, h);
    draw_rect(x, y, w, h, ui_color(ui, UI_COLOR_INPUT));

    draw_field_border(ui, x, y, w, h, is_active);

    /* --- Activation on click, or by Tab from the previous field --- */
    int clicked_on = !is_active && ui->mouse_clicked &&
                     pointer_over(ui, x, y, w, h);
    if (field_drawn(ui, id, y) ||
        (clicked_on && command_allows_activation(ui, id))) {
        ui->active_id = id;
        ui->edit_type = UI_EDIT_TEXT;
        ui->edit_multiline = multiline;
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
        ui->edit_anchor = -1;
        is_active = 1;
    }
    /* The mouse places the caret and drags out a selection. */
    if (is_active) edit_mouse(ui, x, y, w, h, clicked_on);
    field_cursor(ui, x, y, w, h, is_active);

    /* Deactivate on click outside. */
    /* --- Keyboard handling while active --- */
    if (is_active) {
        /*
         * Accept any printable character (>= space).  The edit_buf capacity
         * and caller's buf_size both limit the length.
         */
        apply_pending_text_input(ui);

        /* Backspace removes the whole UTF-8 character before the caret. */
        if (ui->key_backspace) ui_edit_key(ui, UI_KEY_BACKSPACE, 0);

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
    } else if (buf[0]) {
        draw_field_text(ui, x + 4, y + 3, w - 8, buf, ui_color(ui, UI_COLOR_TEXT));
    } else if (hint) {
        draw_field_text(ui, x + 4, y + 3, w - 8, hint, ui_color(ui, UI_COLOR_TEXT_DIM));
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
    return ui_dropdown_unlisted(ui, id, x, y, w, options, count, selected, "---");
}

int ui_dropdown_unlisted(UIState *ui, int id, int x, int y, int w,
                         const char **options, int count, int *selected,
                         const char *unlisted)
{
    int h       = 20;              /* height of the header row             */
    int is_open = (ui->dropdown_open_id == id);
    int changed = 0;

    /* A place in the Tab order, the order on screen. */
    if (ui->field_order_count < UI_MAX_FIELDS)
        ui->field_order[ui->field_order_count++] = id;

    /* --- Draw the header (always visible) --- */
    note_widget(ui, x, y, w, h);
    int hovered_header = pointer_over(ui,
                                       x, y, w, h);
    Color header_bg = ui_color(ui, hovered_header ? UI_COLOR_HOT : UI_COLOR_BUTTON);
    draw_rect(x, y, w, h, header_bg);
    draw_control_border(ui, x, y, w, h);

    /* Show the currently selected option text (or `unlisted` if out of range). */
    const char *current = (*selected >= 0 && *selected < count)
                          ? options[*selected]
                          : unlisted ? unlisted : "---";
    /* Room for the "v" arrow at the right. */
    draw_field_text(ui, x + 4, y + 3, w - 22, current, ui_color(ui, UI_COLOR_TEXT));

    /*
     * Draw a small "▼" indicator on the right side of the header to signal
     * that this is a dropdown.  We use the "v" character as a simple stand-in;
     * the font may or may not have a real triangle glyph.
     */
    draw_text(ui, x + w - 14, y + 3, "v", ui_color(ui, UI_COLOR_TEXT_DIM));

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

void ui_tooltip(UIState *ui, const char *text)
{
    if (!ui || !text || !text[0] || ui->last_w <= 0 || ui->last_h <= 0) return;
    if (!pointer_over(ui, ui->last_x, ui->last_y, ui->last_w, ui->last_h)) return;
    /* A new widget under the pointer starts the delay again. */
    if (ui->tooltip_x != ui->last_x || ui->tooltip_y != ui->last_y ||
        ui->tooltip_w != ui->last_w || ui->tooltip_h != ui->last_h) {
        ui->tooltip_x = ui->last_x;
        ui->tooltip_y = ui->last_y;
        ui->tooltip_w = ui->last_w;
        ui->tooltip_h = ui->last_h;
        ui->tooltip_since_ms = ui->time_ms;
        ui->tooltip_dismissed = 0;
    }
    /* Using the widget answers the question the tip was for. */
    if (ui->mouse_clicked || ui->mouse_down) ui->tooltip_dismissed = 1;
    ui->tooltip_seen = 1;
    ui->tooltip_text = text;
}

/*
 * draw_tooltip — The tip asked for this frame, once the pointer has rested
 * long enough: a title-coloured box below and right of the pointer, moved
 * back inside the view (or above the pointer) near an edge.
 */
/*
 * wrap_tooltip — Copy text into out with line breaks so no line is wider
 * than max_w: a line ends at the last space or '/' that fits, or, in a
 * run with neither (a long file name), at the last character that fits.
 */
#define TOOLTIP_MAX_W 480
static void wrap_tooltip(UIState *ui, const char *text, char *out, size_t out_size, int max_w)
{
    size_t used = 0, len = strlen(text);
    size_t i = 0;

    if (out_size == 0) return;
    while (i < len && used + 2 < out_size) {
        /* How far this line can go from text[i]. */
        size_t end = i, last_break = 0;
        while (end < len) {
            size_t next = end + 1;
            while (next < len && is_continuation_byte(text[next])) next++;
            if (text_width_of(ui, text, i, next) > max_w) break;
            if (text[end] == ' ' || text[end] == '/') last_break = next;
            end = next;
        }
        if (end < len && last_break > i) end = last_break;
        if (end == i) {                  /* one character wider than max_w */
            end = i + 1;
            while (end < len && is_continuation_byte(text[end])) end++;
        }
        size_t n = end - i;
        if (used + n + 2 > out_size) n = out_size - used - 2;
        memcpy(out + used, text + i, n);
        used += n;
        i = end;
        while (i < len && text[i] == ' ') i++;   /* no space starts a line */
        if (i < len) out[used++] = '\n';
    }
    out[used] = '\0';
}

static void draw_tooltip(UIState *ui)
{
    const int pad = 6;
    int tw = 0, th = 0, w, h, x, y;
    int view_w = ui->view_w > 0 ? ui->view_w : GetScreenWidth();
    int view_h = ui->view_h > 0 ? ui->view_h : GetScreenHeight();

    if (!ui->tooltip_text || ui->tooltip_dismissed || ui->dropdown_open_id != 0) return;
    /* Unsigned subtraction: right across a wrap of the clock too. */
    if ((uint32_t)(ui->time_ms - ui->tooltip_since_ms) < UI_TOOLTIP_DELAY_MS) return;
    /* Long tips (a validation message with a full path) wrap to lines no
     * wider than TOOLTIP_MAX_W or the window. */
    char wrapped[UI_EDIT_BUFFER_SIZE];
    int max_text_w = view_w - 2 * pad - 8 < TOOLTIP_MAX_W ? view_w - 2 * pad - 8 : TOOLTIP_MAX_W;
    wrap_tooltip(ui, ui->tooltip_text, wrapped, sizeof(wrapped), max_text_w);
    font_measure(ui->font, wrapped, &tw, &th);
    w = tw + 2 * pad;
    h = th + 2 * pad;
    /* Clear of the pointer's arrow, which hangs down and to the right. */
    x = ui->mouse_x + 12;
    y = ui->mouse_y + 20;
    if (x + w > view_w) x = view_w - w;
    if (y + h > view_h) y = ui->mouse_y - h - 4;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    draw_rect(x, y, w, h, ui_color(ui, UI_COLOR_TITLE));
    draw_control_border(ui, x, y, w, h);
    draw_text(ui, x + pad, y + pad, wrapped, ui_color(ui, UI_COLOR_TEXT));
    ui->tooltip_shown = 1;
    ui->tip_box[0] = x;
    ui->tip_box[1] = y;
    ui->tip_box[2] = w;
    ui->tip_box[3] = h;
}

void ui_draw_overlays(UIState *ui)
{
    int h = 20;

    if (!ui) return;
    /* A tip never shows over an open list (draw_tooltip checks), so the
     * two cannot overlap; the list goes first anyway, the tip last. */
    for (int i = 0; ui->dropdown_open_id != 0 && ui->dropdown_options &&
                    i < ui->dropdown_count; i++) {
        int oy = ui->dropdown_y + i * h;   /* Y of this option row */
        int hovered = point_in_rect(ui->mouse_x, ui->mouse_y,
                                    ui->dropdown_x, oy, ui->dropdown_w, h);

        /* Highlight: accent colour for the selected item, hot for hover
         * and for the row the keyboard highlights. */
        UIColorRole face = i == ui->dropdown_selected                 ? UI_COLOR_SELECTED
                         : hovered || i == ui->dropdown_highlight ? UI_COLOR_HOT
                                                                  : UI_COLOR_BUTTON;
        draw_rect(ui->dropdown_x, oy, ui->dropdown_w, h, ui_color(ui, face));
        draw_text(ui, ui->dropdown_x + 4, oy + 3,
                  ui->dropdown_options[i], ui_color(ui, UI_COLOR_TEXT));
    }
    draw_tooltip(ui);
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
    if (IsWindowReady()) DrawLine(x, y, x + w, y, ui_color(ui, UI_COLOR_TEXT_DIM));
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
