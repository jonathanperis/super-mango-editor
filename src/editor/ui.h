/*
 * ui.h — Public interface for the editor's immediate-mode UI system.
 *
 * Provides a lightweight set of widget functions (buttons, labels, panels,
 * input fields, dropdowns) built on SDL2 draw primitives and SDL2_ttf.
 *
 * "Immediate mode" means there is no persistent widget tree.  Each frame
 * the caller re-issues every widget call; the UI library decides what to
 * draw and whether the user interacted with the widget on that frame.
 * This makes the code simple and stateless — the caller owns all data.
 *
 * UIState carries the per-frame input snapshot (mouse position, clicks,
 * keyboard events, text input) plus a small amount of retained state for
 * widgets that need multi-frame editing (text fields, open dropdowns).
 *
 * Include this header in any editor module that draws UI.
 */
#pragma once

#include <SDL.h>       /* SDL_Renderer, SDL_Color, SDL_Rect */
#include <SDL_ttf.h>   /* TTF_Font                          */

/* ------------------------------------------------------------------ */
/* Colour palette                                                      */
/* ------------------------------------------------------------------ */

/*
 * Editor UI colour constants — a neutral dark theme.
 *
 * These use C99 compound literals so they can appear in expressions:
 *   draw_rect(renderer, 0, 0, 100, 20, UI_BG);
 *
 * Naming convention:
 *   BG        — panel / window background
 *   TITLE_BG  — slightly lighter header bar
 *   BTN       — button at rest
 *   BTN_HOT   — button while the mouse hovers over it
 *   BTN_ACTIVE— button in its "pressed" or "selected" state
 *   ACCENT    — highlight colour (selection, active input border)
 *   TEXT      — primary text colour (nearly white)
 *   TEXT_DIM  — secondary / disabled text (mid-grey)
 *   INPUT_BG  — background of text / number input fields
 */
#define UI_BG         (SDL_Color){0x2D,0x2D,0x2D,0xFF}
#define UI_TITLE_BG   (SDL_Color){0x3D,0x3D,0x3D,0xFF}
#define UI_BTN        (SDL_Color){0x4D,0x4D,0x4D,0xFF}
#define UI_BTN_HOT    (SDL_Color){0x5D,0x5D,0x5D,0xFF}
#define UI_BTN_ACTIVE (SDL_Color){0x4A,0x90,0xD9,0xFF}
#define UI_ACCENT     (SDL_Color){0x4A,0x90,0xD9,0xFF}
#define UI_TEXT        (SDL_Color){0xE0,0xE0,0xE0,0xFF}
#define UI_TEXT_DIM    (SDL_Color){0x80,0x80,0x80,0xFF}
#define UI_INPUT_BG   (SDL_Color){0x1D,0x1D,0x1D,0xFF}

/* ------------------------------------------------------------------ */
/* Types                                                               */
/* ------------------------------------------------------------------ */

/*
 * UIState — everything the immediate-mode UI needs for one frame.
 *
 * The caller fills in the per-frame input fields (mouse, keyboard, text)
 * from SDL events before calling any widget function.  The retained fields
 * (active_id, edit_buf, edit_cursor, dropdown_open_id) persist across
 * frames so that text editing and dropdown menus work correctly.
 *
 * renderer / font are set once in ui_init and never change.
 */
typedef struct {
    /* --- Set once at init --- */
    SDL_Renderer *renderer;       /* GPU drawing context                     */
    TTF_Font     *font;           /* font used for all UI text               */

    /* --- Per-frame input (caller fills before widget calls) --- */
    int           mouse_x;        /* cursor X in logical pixels              */
    int           mouse_y;        /* cursor Y in logical pixels              */
    int           mouse_clicked;  /* 1 on the frame left button went down    */
    int           mouse_down;     /* 1 while left button is held             */
    int           key_backspace;  /* 1 if backspace pressed this frame       */
    int           key_return;     /* 1 if return/enter pressed this frame    */
    int           key_escape;     /* 1 if escape pressed this frame          */
    char          text_input[32]; /* SDL_TEXTINPUT text received this frame  */
    int           has_text_input; /* 1 if text_input was filled this frame   */

    /* --- Retained state for active widgets --- */
    int           active_id;      /* ID of widget being edited, 0 = none     */
    char          edit_buf[64];   /* buffer for the active text/number input */
    int           edit_cursor;    /* cursor position inside edit_buf         */

    /*
     * dropdown_open_id — tracks which dropdown (if any) is expanded.
     * 0 means no dropdown is open.  When non-zero, the dropdown with
     * this ID draws its option list and captures clicks.
     */
    int           dropdown_open_id;
} UIState;

/* ------------------------------------------------------------------ */
/* Function declarations                                               */
/* ------------------------------------------------------------------ */

/*
 * ui_init — One-time setup: store the renderer and font pointers.
 *
 * Call once after creating the SDL renderer and loading the TTF font.
 * The UIState is zero-initialised except for these two pointers.
 */
void ui_init(UIState *ui, SDL_Renderer *renderer, TTF_Font *font);

/*
 * ui_begin_frame — Reset per-frame input state at the start of each frame.
 *
 * Call this BEFORE processing SDL events.  It clears mouse_clicked,
 * key_backspace, key_return, key_escape, text_input, and has_text_input
 * so that only the current frame's events are seen by widgets.
 */
void ui_begin_frame(UIState *ui);

/* ---- Widgets ----------------------------------------------------- */

/*
 * ui_button — Draw a clickable button and return 1 on the click frame.
 *
 * Draws a filled rectangle in BTN colour (BTN_HOT when the mouse hovers).
 * The label text is centred inside the rectangle.
 * Returns 1 on the single frame the user clicked inside the button, else 0.
 */
int ui_button(UIState *ui, int x, int y, int w, int h, const char *label);

/*
 * ui_label — Draw a single line of text at (x, y) in the default colour.
 *
 * Uses TTF_RenderText_Blended to produce anti-aliased text, creates a
 * one-frame texture, renders it, then destroys the texture immediately.
 * Simple and correct for an editor; a shipped game would cache textures.
 */
void ui_label(UIState *ui, int x, int y, const char *text);

/*
 * ui_label_color — Same as ui_label but with a caller-chosen colour.
 */
void ui_label_color(UIState *ui, int x, int y, const char *text,
                    SDL_Color color);

/*
 * ui_panel — Draw a filled dark rectangle as a visual container.
 *
 * Panels have no interactivity — they just provide a background behind
 * groups of widgets to visually separate sections of the editor UI.
 */
void ui_panel(UIState *ui, int x, int y, int w, int h);

/*
 * ui_int_field — Editable integer field.
 *
 * Displays the current *value as text.  Click to activate (enters edit
 * mode with a blinking cursor).  While active, accepts digit and minus
 * characters via SDL_TEXTINPUT.  Backspace deletes; Return confirms;
 * Escape cancels and restores the original value.
 *
 * id     — unique non-zero integer identifying this field (used for active_id).
 * value  — pointer to the integer being edited; updated on confirm.
 *
 * Returns 1 when the value changes (user pressed Return), else 0.
 */
int ui_int_field(UIState *ui, int id, int x, int y, int w, int *value);

/*
 * ui_float_field — Editable floating-point field (1 decimal place display).
 *
 * Same interaction model as ui_int_field, but also accepts '.' in input.
 * The displayed value is formatted with one decimal place ("123.4").
 *
 * Returns 1 when the value changes (user pressed Return), else 0.
 */
int ui_float_field(UIState *ui, int id, int x, int y, int w, float *value);

/*
 * ui_text_field — Editable single-line text field.
 *
 * Same interaction model as ui_int_field, but accepts any printable
 * character (not just digits).  The buffer is caller-owned with a given
 * maximum size.
 *
 * id       — unique non-zero integer identifying this field.
 * buf      — pointer to the editable char buffer.
 * buf_size — total capacity of buf (including the '\0' terminator).
 *
 * Returns 1 when the value changes (user pressed Return), else 0.
 */
int ui_text_field(UIState *ui, int id, int x, int y, int w,
                  char *buf, int buf_size);

/*
 * ui_dropdown — Selectable dropdown list.
 *
 * Displays the currently selected option in a box.  Clicking the box
 * toggles the dropdown open, showing all options as a vertical list.
 * Clicking an option selects it and closes the dropdown.  Clicking
 * anywhere outside the dropdown also closes it.
 *
 * id       — unique non-zero integer identifying this dropdown.
 * options  — array of C strings (the option labels).
 * count    — number of elements in the options array.
 * selected — pointer to the selected index; updated on selection.
 *
 * Returns 1 when the selection changes, else 0.
 */
int ui_dropdown(UIState *ui, int id, int x, int y, int w,
                const char **options, int count, int *selected);

/*
 * ui_separator — Draw a thin horizontal line to divide sections visually.
 *
 * Draws a 1-pixel-high line in TEXT_DIM colour from (x, y) to (x+w, y).
 */
void ui_separator(UIState *ui, int x, int y, int w);
