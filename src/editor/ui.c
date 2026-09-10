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

#include <SDL.h>       /* SDL_Renderer, SDL_SetRenderDrawColor, SDL_RenderFillRect */
#include <SDL_ttf.h>   /* TTF_RenderText_Blended, TTF_SizeText                    */
#include <stdio.h>     /* snprintf                                                 */
#include <string.h>    /* strlen, strncpy, memset                                  */
#include <stdlib.h>    /* strtol, strtof                                           */
#include <errno.h>     /* errno, ERANGE                                             */
#include <limits.h>    /* INT_MIN, INT_MAX                                           */
#include <math.h>      /* isfinite                                                   */

#include "ui.h"

/* ------------------------------------------------------------------ */
/* Internal helper — forward declarations                              */
/* ------------------------------------------------------------------ */

/*
 * draw_rect — Fill a rectangle with a solid colour.
 *
 * SDL_SetRenderDrawColor sets the colour for subsequent draw calls.
 * SDL_RenderFillRect draws a filled rectangle using that colour.
 * We wrap both into one call because every widget needs this combo.
 */
static void draw_rect(SDL_Renderer *r, int x, int y, int w, int h,
                       SDL_Color c);

/*
 * draw_text — Render a single line of text at (x, y).
 *
 * Short labels reuse a bounded renderer-owned texture cache keyed by text
 * and color. Cache misses use TTF_RenderUTF8_Blended and upload once; long
 * strings use transient textures. ui_cleanup releases the retained textures.
 */
static void draw_text(UIState *ui, int x, int y,
                       const char *text, SDL_Color c);

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

static void clear_active_edit(UIState *ui)
{
    ui->active_id = 0;
    ui->edit_type = UI_EDIT_NONE;
    ui->edit_target = NULL;
    ui->edit_target_size = 0;
    ui->edit_cursor = 0;
    ui->edit_buf[0] = '\0';
    ui->pending_text_length = 0;
    ui->pending_text_input[0] = '\0';
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
            while (i + bytes < ui->pending_text_length &&
                   ((unsigned char)ui->pending_text_input[i + bytes] & 0xc0) == 0x80) bytes++;
        }

        if (ui->edit_type == UI_EDIT_INT) {
            accepted = (ch >= '0' && ch <= '9') || ch == '-';
        } else if (ui->edit_type == UI_EDIT_FLOAT) {
            accepted = (ch >= '0' && ch <= '9') || ch == '.' || ch == '-';
        }
        if (accepted && bytes <= (size_t)(max_len - ui->edit_cursor)) {
            memcpy(ui->edit_buf + ui->edit_cursor, ui->pending_text_input + i, bytes);
            ui->edit_cursor += (int)bytes;
            ui->edit_buf[ui->edit_cursor] = '\0';
        }
        i += bytes;
    }
    ui->pending_text_length = 0;
    ui->pending_text_input[0] = '\0';
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
        if (value != *target) {
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

static void draw_rect(SDL_Renderer *r, int x, int y, int w, int h,
                       SDL_Color c)
{
    /*
     * SDL_SetRenderDrawColor — set the draw colour used by subsequent
     * SDL_RenderFillRect / SDL_RenderDrawRect / SDL_RenderDrawLine calls.
     * The four values are red, green, blue, alpha (0–255 each).
     */
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);

    /*
     * SDL_RenderFillRect — fill a rectangle on the current render target.
     * The SDL_Rect uses integer coordinates; positions are in logical space
     * because SDL_RenderSetLogicalSize handles the scaling for us.
     */
    SDL_Rect rect = { x, y, w, h };
    SDL_RenderFillRect(r, &rect);
}

static void draw_text(UIState *ui, int x, int y,
                       const char *text, SDL_Color c)
{
    if (!ui->font || !ui->renderer || !text || !text[0]) return;
    UITextCacheEntry *entry = NULL;
    Uint32 color = (Uint32)c.r << 24 | (Uint32)c.g << 16 | (Uint32)c.b << 8 | c.a;
    if (strlen(text) < UI_TEXT_CACHE_BYTES) {
        entry = &ui->text_cache[0];
        for (int i = 0; i < UI_TEXT_CACHE_COUNT; i++) {
            UITextCacheEntry *candidate = &ui->text_cache[i];
            if (candidate->texture && candidate->color == color && !strcmp(candidate->text, text)) {
                candidate->used = ++ui->text_clock;
                SDL_Rect dst = {x, y, candidate->w, candidate->h};
                SDL_RenderCopy(ui->renderer, candidate->texture, NULL, &dst);
                return;
            }
            if (candidate->used < entry->used) entry = candidate;
        }
    }
    /* UTF-8 is the encoding supplied by SDL_TEXTINPUT and native paths. */
    SDL_Surface *surface = TTF_RenderUTF8_Blended(ui->font, text, c);
    if (!surface) return;
    SDL_Texture *texture = SDL_CreateTextureFromSurface(ui->renderer, surface);
    SDL_Rect dst = {x, y, surface->w, surface->h};
    SDL_FreeSurface(surface);
    if (!texture) return;
    SDL_RenderCopy(ui->renderer, texture, NULL, &dst);
    if (entry) {
        if (entry->texture) SDL_DestroyTexture(entry->texture);
        entry->texture = texture;
        memcpy(entry->text, text, strlen(text) + 1);
        entry->color = color;
        entry->used = ++ui->text_clock;
        entry->w = dst.w;
        entry->h = dst.h;
    } else SDL_DestroyTexture(texture);
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
 * ui_init — Store the renderer and font; zero-initialise everything else.
 *
 * memset ensures all per-frame and retained fields start at zero/NULL.
 * Then we set the two persistent pointers that every widget call needs.
 */
void ui_init(UIState *ui, SDL_Renderer *renderer, TTF_Font *font)
{
    memset(ui, 0, sizeof(*ui));
    ui->renderer = renderer;
    ui->font     = font;
}

void ui_cleanup(UIState *ui)
{
    for (int i = 0; i < UI_TEXT_CACHE_COUNT; i++) {
        if (ui->text_cache[i].texture) SDL_DestroyTexture(ui->text_cache[i].texture);
        ui->text_cache[i] = (UITextCacheEntry){0};
    }
    ui->text_clock = 0;
}

/* ------------------------------------------------------------------ */

/*
 * ui_begin_frame — Clear per-frame input so stale events don't linger.
 *
 * Called at the very start of each frame, BEFORE the SDL event loop.
 * Only the per-frame flags are cleared; retained fields (active_id,
 * edit_buf, edit_cursor, dropdown_open_id) persist across frames.
 */
void ui_begin_frame(UIState *ui)
{
    ui->mouse_clicked  = 0;
    ui->mouse_down     = 0;
    ui->key_backspace  = 0;
    ui->key_return     = 0;
    ui->key_escape     = 0;
    ui->has_text_input = 0;
    memset(ui->text_input, 0, sizeof(ui->text_input));
    ui->pending_text_length = 0;
    memset(ui->pending_text_input, 0, sizeof(ui->pending_text_input));
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
    SDL_Color bg = hovered ? UI_BTN_HOT : UI_BTN;
    draw_rect(ui->renderer, x, y, w, h, bg);

    /*
     * Centre the label text inside the button rectangle.
     *
     * TTF_SizeText — measure the pixel width and height that this string
     * would occupy when rendered with the given font.  We use the width
     * to compute a horizontal offset that centres the text.
     */
    int tw = 0, th = 0;
    TTF_SizeUTF8(ui->font, label, &tw, &th);
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
                    SDL_Color color)
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
    draw_rect(ui->renderer, x, y, w, h, UI_BG);
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
 *   - SDL_TEXTINPUT characters are appended if they are digits or '-'.
 *   - Backspace deletes the last character.
 *   - A blinking cursor (using SDL_GetTicks) provides visual feedback.
 *
 * The field height is fixed at 20 logical pixels (fits the 13 px font
 * with some padding).
 */
int ui_int_field(UIState *ui, int id, int x, int y, int w, int *value)
{
    int h        = 20;             /* fixed field height in logical px   */
    int is_active = (ui->active_id == id);
    int changed   = 0;

    /* --- Background and border --- */
    draw_rect(ui->renderer, x, y, w, h, UI_INPUT_BG);

    /*
     * Draw a 1-pixel border around the field.  The accent colour indicates
     * the active (editing) field; dim grey marks inactive fields.
     *
     * SDL_RenderDrawRect — draw the outline of a rectangle (no fill).
     */
    SDL_Color border = is_active ? UI_ACCENT : UI_TEXT_DIM;
    SDL_SetRenderDrawColor(ui->renderer, border.r, border.g, border.b,
                           border.a);
    SDL_Rect outline = { x, y, w, h };
    SDL_RenderDrawRect(ui->renderer, &outline);

    /* --- Activation on click --- */
    if (ui->mouse_clicked && point_in_rect(ui->mouse_x, ui->mouse_y,
                                           x, y, w, h) &&
        command_allows_activation(ui, id)) {
        /*
         * Start editing: copy the current value into edit_buf so the user
         * sees the existing number and can modify it.  snprintf converts
         * the integer to its decimal string representation.
         */
        ui->active_id = id;
        ui->edit_type = UI_EDIT_INT;
        ui->edit_target = value;
        ui->edit_target_size = sizeof(*value);
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
         * Append typed text.  SDL_TEXTINPUT events deliver actual characters
         * (respecting the OS keyboard layout and IME), unlike SDL_KEYDOWN
         * which gives raw key codes.  We only accept digits (0-9) and the
         * minus sign for negative numbers.
         */
        apply_pending_text_input(ui);

        /* Backspace — delete the character before the cursor. */
        if (ui->key_backspace && ui->edit_cursor > 0) {
            ui->edit_cursor--;
            ui->edit_buf[ui->edit_cursor] = '\0';
        }

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
        }

        /* Escape — cancel: discard edits and deactivate. */
        if (ui->key_escape) {
            ui_cancel_active_edit(ui);
            is_active = 0;
        }
    }

    /* --- Draw the display text --- */
    if (is_active) {
        /*
         * Active: show the edit buffer with a blinking cursor.
         *
         * SDL_GetTicks returns milliseconds since SDL_Init.  By dividing
         * by 500 and checking odd/even we get a half-second blink rate.
         * The cursor is drawn as a "|" appended to the display string.
         */
        char display[80];
        int blink = (SDL_GetTicks() / 500) % 2;  /* 0 or 1 every 500 ms */
        snprintf(display, sizeof(display), "%s%s",
                 ui->edit_buf, blink ? "|" : "");
        draw_text(ui, x + 4, y + 3, display, UI_TEXT);
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
int ui_float_field(UIState *ui, int id, int x, int y, int w, float *value)
{
    int h         = 20;
    int is_active = (ui->active_id == id);
    int changed   = 0;

    /* --- Background and border --- */
    draw_rect(ui->renderer, x, y, w, h, UI_INPUT_BG);

    SDL_Color border = is_active ? UI_ACCENT : UI_TEXT_DIM;
    SDL_SetRenderDrawColor(ui->renderer, border.r, border.g, border.b,
                           border.a);
    SDL_Rect outline = { x, y, w, h };
    SDL_RenderDrawRect(ui->renderer, &outline);

    /* --- Activation on click --- */
    if (ui->mouse_clicked && point_in_rect(ui->mouse_x, ui->mouse_y,
                                           x, y, w, h) &&
        command_allows_activation(ui, id)) {
        ui->active_id = id;
        ui->edit_type = UI_EDIT_FLOAT;
        ui->edit_target = value;
        ui->edit_target_size = sizeof(*value);
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

        if (ui->key_backspace && ui->edit_cursor > 0) {
            ui->edit_cursor--;
            ui->edit_buf[ui->edit_cursor] = '\0';
        }

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
        }

        if (ui->key_escape) {
            ui_cancel_active_edit(ui);
            is_active = 0;
        }
    }

    /* --- Draw display text --- */
    if (is_active) {
        char display[80];
        int blink = (SDL_GetTicks() / 500) % 2;
        size_t length = strlen(ui->edit_buf);
        const char *visible = ui->edit_buf + (length > sizeof(display)-2 ? length-(sizeof(display)-2) : 0);
        while (((unsigned char)*visible & 0xc0) == 0x80) visible++;
        while (*visible && ui_text_width(ui, visible) > w - 16) {
            visible++;
            while (((unsigned char)*visible & 0xc0) == 0x80) visible++;
        }
        snprintf(display, sizeof(display), "%s%s",
                 visible, blink ? "|" : "");
        draw_text(ui, x + 4, y + 3, display, UI_TEXT);
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
    draw_rect(ui->renderer, x, y, w, h, UI_INPUT_BG);

    SDL_Color border = is_active ? UI_ACCENT : UI_TEXT_DIM;
    SDL_SetRenderDrawColor(ui->renderer, border.r, border.g, border.b,
                           border.a);
    SDL_Rect outline = { x, y, w, h };
    SDL_RenderDrawRect(ui->renderer, &outline);

    /* --- Activation on click --- */
    if (ui->mouse_clicked && point_in_rect(ui->mouse_x, ui->mouse_y,
                                           x, y, w, h) &&
        command_allows_activation(ui, id)) {
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

        if (ui->key_backspace && ui->edit_cursor > 0) {
            ui->edit_cursor--;
            while (ui->edit_cursor > 0 &&
                   ((unsigned char)ui->edit_buf[ui->edit_cursor] & 0xc0) == 0x80) ui->edit_cursor--;
            ui->edit_buf[ui->edit_cursor] = '\0';
        }

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
        }

        if (ui->key_escape) {
            ui_cancel_active_edit(ui);
            is_active = 0;
        }
    }

    /* --- Draw display text --- */
    if (is_active) {
        char display[80];
        int blink = (SDL_GetTicks() / 500) % 2;
        snprintf(display, sizeof(display), "%s%s",
                 ui->edit_buf, blink ? "|" : "");
        draw_text(ui, x + 4, y + 3, display, UI_TEXT);
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
 *   - Click the header to toggle open/closed.
 *   - Click an option to select it (closes the dropdown).
 *   - Click anywhere outside to close without changing selection.
 *
 * Only one dropdown can be open at a time; opening one closes any other.
 * The dropdown_open_id in UIState tracks which one is expanded.
 */
int ui_dropdown(UIState *ui, int id, int x, int y, int w,
                const char **options, int count, int *selected)
{
    int h       = 20;              /* height of the header row             */
    int is_open = (ui->dropdown_open_id == id);
    int changed = 0;

    /* --- Draw the header (always visible) --- */
    int hovered_header = point_in_rect(ui->mouse_x, ui->mouse_y,
                                       x, y, w, h);
    SDL_Color header_bg = hovered_header ? UI_BTN_HOT : UI_BTN;
    draw_rect(ui->renderer, x, y, w, h, header_bg);

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

    /* --- Toggle open/closed on header click --- */
    if (ui->mouse_clicked && hovered_header) {
        if (ui->active_id != 0 && ui->before_command &&
            !ui->before_command(ui->before_command_context)) {
            return 0;
        }
        if (ui->active_id != 0) return 0;
        if (is_open) {
            /* Already open — close it. */
            ui->dropdown_open_id = 0;
            is_open = 0;
        } else {
            /* Open this dropdown (and implicitly close any other). */
            ui->dropdown_open_id = id;
            is_open = 1;
        }
    }

    /* --- Draw the option list when open --- */
    if (is_open) {
        /*
         * Draw each option as a row directly below the header.
         * Hovered rows get a lighter background for visual feedback.
         */
        for (int i = 0; i < count; i++) {
            int oy = y + h + i * h;   /* Y of this option row */

            int hovered_opt = point_in_rect(ui->mouse_x, ui->mouse_y,
                                            x, oy, w, h);

            /* Highlight: accent colour for the selected item, hot for hover. */
            SDL_Color opt_bg;
            if (i == *selected) {
                opt_bg = UI_BTN_ACTIVE;
            } else if (hovered_opt) {
                opt_bg = UI_BTN_HOT;
            } else {
                opt_bg = UI_BTN;
            }
            draw_rect(ui->renderer, x, oy, w, h, opt_bg);
            draw_text(ui, x + 4, oy + 3,
                      options[i], UI_TEXT);

            /* Select this option on click. */
            if (ui->mouse_clicked && hovered_opt) {
                if (ui->active_id != 0 && ui->before_command &&
                    !ui->before_command(ui->before_command_context)) {
                    return 0;
                }
                if (ui->active_id != 0) return 0;
                if (i != *selected) {
                    notify_before_change(ui, id);
                    *selected = i;
                    changed   = 1;
                }
                ui->dropdown_open_id = 0;   /* close after selection */
                break;   /* stop processing further options this frame */
            }
        }

        /*
         * Close on click outside: if the user clicked but not on the header
         * and not on any option row, close the dropdown.  We check whether
         * the click landed inside the combined header + list rectangle.
         */
        if (ui->mouse_clicked && !changed) {
            int total_h = h + count * h;  /* header + all option rows */
            if (!point_in_rect(ui->mouse_x, ui->mouse_y,
                               x, y, w, total_h)) {
                ui->dropdown_open_id = 0;
            }
        }
    }

    return changed;
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
    SDL_Color c = UI_TEXT_DIM;
    SDL_SetRenderDrawColor(ui->renderer, c.r, c.g, c.b, c.a);

    /*
     * SDL_RenderDrawLine — draw a single-pixel line between two points.
     * Both endpoints are in logical coordinates.
     */
    SDL_RenderDrawLine(ui->renderer, x, y, x + w, y);
}

/* ------------------------------------------------------------------ */

/*
 * ui_text_width — Return the rendered pixel width of a string in the UI font.
 *
 * Uses TTF_SizeText to measure without actually drawing anything.
 * Returns 0 if the font is NULL or the string is empty.
 */
int ui_text_width(UIState *ui, const char *text)
{
    if (!ui->font || !text || text[0] == '\0') return 0;
    int w = 0;
    TTF_SizeUTF8(ui->font, text, &w, NULL);
    return w;
}
