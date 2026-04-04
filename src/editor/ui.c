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
 * Text rendering takes the simple path: each label call creates a
 * TTF texture, renders it, and destroys it.  This allocates GPU memory
 * every frame, which is fine for a level editor (low widget count, not
 * a 60 FPS game).  A production UI would cache glyph atlases.
 *
 * Input fields (int, float) use a tiny retained-state mechanism:
 * UIState.active_id tracks which field is being edited, and edit_buf
 * holds the in-progress text.  Only one field can be active at a time.
 */

#include <SDL.h>       /* SDL_Renderer, SDL_SetRenderDrawColor, SDL_RenderFillRect */
#include <SDL_ttf.h>   /* TTF_RenderText_Blended, TTF_SizeText                    */
#include <stdio.h>     /* snprintf                                                 */
#include <string.h>    /* strlen, strncpy, memset                                  */
#include <stdlib.h>    /* atoi, atof                                               */

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
 * Uses the "create-render-destroy" pattern:
 *   1. TTF_RenderText_Blended  → CPU surface with anti-aliased glyphs
 *   2. SDL_CreateTextureFromSurface → upload to GPU
 *   3. SDL_RenderCopy           → draw the texture
 *   4. SDL_DestroyTexture + SDL_FreeSurface → release memory
 *
 * This is simple but allocates/frees every frame.  Acceptable for a
 * low-widget-count editor; a game HUD should cache textures instead.
 */
static void draw_text(SDL_Renderer *r, TTF_Font *font, int x, int y,
                       const char *text, SDL_Color c);

/*
 * point_in_rect — Test whether point (px, py) lies inside a rectangle.
 *
 * Uses simple axis-aligned comparison:
 *   px must be between rx and rx+rw  (horizontal)
 *   py must be between ry and ry+rh  (vertical)
 */
static int point_in_rect(int px, int py, int rx, int ry, int rw, int rh);

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

static void draw_text(SDL_Renderer *r, TTF_Font *font, int x, int y,
                       const char *text, SDL_Color c)
{
    /*
     * Guard: TTF_RenderText_Blended crashes on NULL or empty strings,
     * so bail out early.
     */
    if (!text || text[0] == '\0') return;

    /*
     * TTF_RenderText_Blended — render UTF-8 text into a 32-bit ARGB surface.
     * "Blended" means full anti-aliasing against a transparent background,
     * which gives the best quality when composited onto any backdrop.
     * The returned surface is allocated on the CPU heap.
     */
    SDL_Surface *surf = TTF_RenderText_Blended(font, text, c);
    if (!surf) return;

    /*
     * SDL_CreateTextureFromSurface — upload the CPU surface to a GPU texture.
     * After this call the surface data lives on the GPU; we free the CPU
     * copy immediately after rendering.
     */
    SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
    if (!tex) {
        SDL_FreeSurface(surf);
        return;
    }

    /*
     * Build the destination rect from the surface dimensions so the text
     * renders at its natural pixel size (no scaling).
     */
    SDL_Rect dst = { x, y, surf->w, surf->h };

    /*
     * SDL_RenderCopy — blit the texture onto the back buffer.
     * NULL source rect means "copy the entire texture".
     */
    SDL_RenderCopy(r, tex, NULL, &dst);

    /*
     * Clean up: destroy the GPU texture first, then free the CPU surface.
     * We recreate both next frame — simple but correct.
     */
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
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
    TTF_SizeText(ui->font, label, &tw, &th);
    int tx = x + (w - tw) / 2;   /* horizontal centre */
    int ty = y + (h - th) / 2;   /* vertical centre   */
    draw_text(ui->renderer, ui->font, tx, ty, label, UI_TEXT);

    /* Return 1 only on the click-down frame while hovering. */
    return (hovered && ui->mouse_clicked) ? 1 : 0;
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
    draw_text(ui->renderer, ui->font, x, y, text, UI_TEXT);
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
    draw_text(ui->renderer, ui->font, x, y, text, color);
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
                                           x, y, w, h)) {
        /*
         * Start editing: copy the current value into edit_buf so the user
         * sees the existing number and can modify it.  snprintf converts
         * the integer to its decimal string representation.
         */
        ui->active_id = id;
        snprintf(ui->edit_buf, sizeof(ui->edit_buf), "%d", *value);
        ui->edit_cursor = (int)strlen(ui->edit_buf);
    }

    /*
     * Deactivate if the user clicks somewhere else (clicked but NOT inside
     * this field, and this field is currently active).  This is the
     * "click outside to cancel" behaviour.
     */
    if (is_active && ui->mouse_clicked &&
        !point_in_rect(ui->mouse_x, ui->mouse_y, x, y, w, h)) {
        ui->active_id = 0;
        is_active = 0;
    }

    /* --- Keyboard handling while active --- */
    if (is_active) {
        /*
         * Append typed text.  SDL_TEXTINPUT events deliver actual characters
         * (respecting the OS keyboard layout and IME), unlike SDL_KEYDOWN
         * which gives raw key codes.  We only accept digits (0-9) and the
         * minus sign for negative numbers.
         */
        if (ui->has_text_input) {
            for (int i = 0; ui->text_input[i] != '\0'; i++) {
                char ch = ui->text_input[i];
                int is_digit = (ch >= '0' && ch <= '9');
                int is_minus = (ch == '-');
                if ((is_digit || is_minus) &&
                    ui->edit_cursor < (int)sizeof(ui->edit_buf) - 1) {
                    ui->edit_buf[ui->edit_cursor++] = ch;
                    ui->edit_buf[ui->edit_cursor]   = '\0';
                }
            }
        }

        /* Backspace — delete the character before the cursor. */
        if (ui->key_backspace && ui->edit_cursor > 0) {
            ui->edit_cursor--;
            ui->edit_buf[ui->edit_cursor] = '\0';
        }

        /*
         * Return — confirm the edit.  Parse the buffer as an integer and
         * write it back to *value.  atoi returns 0 for unparseable strings,
         * which is a safe fallback.
         */
        if (ui->key_return) {
            int new_val = atoi(ui->edit_buf);
            if (new_val != *value) {
                *value  = new_val;
                changed = 1;
            }
            ui->active_id = 0;
            is_active = 0;
        }

        /* Escape — cancel: discard edits and deactivate. */
        if (ui->key_escape) {
            ui->active_id = 0;
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
        draw_text(ui->renderer, ui->font, x + 4, y + 3, display, UI_TEXT);
    } else {
        /* Inactive: show the current value as a plain number. */
        char display[32];
        snprintf(display, sizeof(display), "%d", *value);
        draw_text(ui->renderer, ui->font, x + 4, y + 3, display, UI_TEXT);
    }

    return changed;
}

/* ------------------------------------------------------------------ */

/*
 * ui_float_field — Editable floating-point input field.
 *
 * Identical interaction model to ui_int_field, but:
 *   - Accepts '.' (decimal point) in addition to digits and '-'.
 *   - Displays the value with one decimal place ("123.4").
 *   - Parses the edit buffer with atof instead of atoi.
 *
 * One decimal place is enough for game coordinates and speeds, and keeps
 * the field narrow.  The internal float retains full precision.
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
                                           x, y, w, h)) {
        ui->active_id = id;
        /*
         * Format with 1 decimal place for a clean starting string.
         * The "%.1f" format rounds the float to one digit past the point.
         */
        snprintf(ui->edit_buf, sizeof(ui->edit_buf), "%.1f", *value);
        ui->edit_cursor = (int)strlen(ui->edit_buf);
    }

    /* Deactivate on click outside. */
    if (is_active && ui->mouse_clicked &&
        !point_in_rect(ui->mouse_x, ui->mouse_y, x, y, w, h)) {
        ui->active_id = 0;
        is_active = 0;
    }

    /* --- Keyboard handling while active --- */
    if (is_active) {
        if (ui->has_text_input) {
            for (int i = 0; ui->text_input[i] != '\0'; i++) {
                char ch = ui->text_input[i];
                int is_digit = (ch >= '0' && ch <= '9');
                int is_dot   = (ch == '.');
                int is_minus = (ch == '-');
                if ((is_digit || is_dot || is_minus) &&
                    ui->edit_cursor < (int)sizeof(ui->edit_buf) - 1) {
                    ui->edit_buf[ui->edit_cursor++] = ch;
                    ui->edit_buf[ui->edit_cursor]   = '\0';
                }
            }
        }

        if (ui->key_backspace && ui->edit_cursor > 0) {
            ui->edit_cursor--;
            ui->edit_buf[ui->edit_cursor] = '\0';
        }

        /*
         * Return — parse with atof (ASCII to float).  atof handles decimal
         * points, negative signs, and returns 0.0 for invalid input.
         * We cast to float because atof returns double.
         */
        if (ui->key_return) {
            float new_val = (float)atof(ui->edit_buf);
            if (new_val != *value) {
                *value  = new_val;
                changed = 1;
            }
            ui->active_id = 0;
            is_active = 0;
        }

        if (ui->key_escape) {
            ui->active_id = 0;
            is_active = 0;
        }
    }

    /* --- Draw display text --- */
    if (is_active) {
        char display[80];
        int blink = (SDL_GetTicks() / 500) % 2;
        snprintf(display, sizeof(display), "%s%s",
                 ui->edit_buf, blink ? "|" : "");
        draw_text(ui->renderer, ui->font, x + 4, y + 3, display, UI_TEXT);
    } else {
        char display[32];
        snprintf(display, sizeof(display), "%.1f", *value);
        draw_text(ui->renderer, ui->font, x + 4, y + 3, display, UI_TEXT);
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
                                           x, y, w, h)) {
        ui->active_id = id;
        /*
         * Copy the current buffer contents into edit_buf so the user
         * sees the existing text and can modify it.
         */
        strncpy(ui->edit_buf, buf, sizeof(ui->edit_buf) - 1);
        ui->edit_buf[sizeof(ui->edit_buf) - 1] = '\0';
        ui->edit_cursor = (int)strlen(ui->edit_buf);
    }

    /* Deactivate on click outside. */
    if (is_active && ui->mouse_clicked &&
        !point_in_rect(ui->mouse_x, ui->mouse_y, x, y, w, h)) {
        ui->active_id = 0;
        is_active = 0;
    }

    /* --- Keyboard handling while active --- */
    if (is_active) {
        /*
         * Accept any printable character (>= space).  The edit_buf capacity
         * (64 chars) and the caller's buf_size both limit the length.
         */
        if (ui->has_text_input) {
            int max_len = buf_size - 1;
            if (max_len > (int)sizeof(ui->edit_buf) - 1)
                max_len = (int)sizeof(ui->edit_buf) - 1;
            for (int i = 0; ui->text_input[i] != '\0'; i++) {
                char ch = ui->text_input[i];
                if (ch >= ' ' && ui->edit_cursor < max_len) {
                    ui->edit_buf[ui->edit_cursor++] = ch;
                    ui->edit_buf[ui->edit_cursor]   = '\0';
                }
            }
        }

        if (ui->key_backspace && ui->edit_cursor > 0) {
            ui->edit_cursor--;
            ui->edit_buf[ui->edit_cursor] = '\0';
        }

        /*
         * Return — confirm the edit.  Copy edit_buf back into the caller's
         * buffer.  Only signal "changed" if the text actually differs.
         */
        if (ui->key_return) {
            if (strcmp(ui->edit_buf, buf) != 0) {
                strncpy(buf, ui->edit_buf, (size_t)(buf_size - 1));
                buf[buf_size - 1] = '\0';
                changed = 1;
            }
            ui->active_id = 0;
            is_active = 0;
        }

        if (ui->key_escape) {
            ui->active_id = 0;
            is_active = 0;
        }
    }

    /* --- Draw display text --- */
    if (is_active) {
        char display[80];
        int blink = (SDL_GetTicks() / 500) % 2;
        snprintf(display, sizeof(display), "%s%s",
                 ui->edit_buf, blink ? "|" : "");
        draw_text(ui->renderer, ui->font, x + 4, y + 3, display, UI_TEXT);
    } else {
        draw_text(ui->renderer, ui->font, x + 4, y + 3,
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
    draw_text(ui->renderer, ui->font, x + 4, y + 3, current, UI_TEXT);

    /*
     * Draw a small "▼" indicator on the right side of the header to signal
     * that this is a dropdown.  We use the "v" character as a simple stand-in;
     * the font may or may not have a real triangle glyph.
     */
    draw_text(ui->renderer, ui->font, x + w - 14, y + 3, "v", UI_TEXT_DIM);

    /* --- Toggle open/closed on header click --- */
    if (ui->mouse_clicked && hovered_header) {
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
            draw_text(ui->renderer, ui->font, x + 4, oy + 3,
                      options[i], UI_TEXT);

            /* Select this option on click. */
            if (ui->mouse_clicked && hovered_opt) {
                if (i != *selected) {
                    *selected = i;
                    changed   = 1;
                }
                ui->dropdown_open_id = 0;   /* close after selection */
                is_open = 0;
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
