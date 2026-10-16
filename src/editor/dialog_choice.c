/*
 * dialog_choice.c — Two- and three-button prompts drawn in the editor window.
 *
 * The editor used to ask its yes/no/cancel questions through zenity,
 * osascript or PowerShell.  Those cost a process launch per question, look
 * like a different program, and are missing on many Linux desktops.  The
 * prompt is now part of the editor window, while keeping the old blocking
 * call: dialog_choice runs a small frame loop of its own and returns the
 * button chosen, so its callers did not change.
 *
 * A prompt can open at two kinds of moment:
 *   - while events are handled, before the frame is drawn (a shortcut, a
 *     dropped file, closing the window), or
 *   - in the middle of drawing (a toolbar button or a field acts when it
 *     is drawn), with the frame target bound and maybe a clip set.
 * The loop leaves whatever was being drawn: it ends the clip and texture
 * mode, draws its own frames into a target of its own, and on the way out
 * binds the frame target and sets the clip again, so the interrupted frame
 * carries on exactly where it was.  Behind the box it shows the last
 * complete frame, which dialog_choice_keep_frame copies after every frame
 * (a half-drawn one would show missing panels).  Nothing in the loop runs
 * editor code: it reads input, draws and returns.
 */

#include "file_dialog.h"    /* dialog_choice */
#include "dialog_choice.h"

#include <string.h>          /* strlen, memcpy */

#include "../shared/graphics.h"      /* display_present */
#include "../input/input_backend.h"  /* input_collect, input_poll, input_push */

static EditorState *s_host;
static int s_open;           /* a prompt is showing: never stack another */
static int s_frames;

static DialogLayout s_last_layout;   /* the last prompt's, for tests */

#ifdef MANGO_TESTING
int dialog_choice_test_frames(void) { return s_frames; }
const DialogLayout *dialog_choice_test_layout(void) { return &s_last_layout; }
#endif

void dialog_choice_attach(EditorState *es)
{
    s_host = es;
}

void dialog_choice_keep_frame(EditorState *es)
{
    if (!es || !IsWindowReady() || !IsRenderTextureValid(es->frame_target) ||
        !IsRenderTextureValid(es->prompt_backdrop)) return;
    EndTextureMode();
    BeginTextureMode(es->prompt_backdrop);
    /* Render textures are stored upside down; a negative height reads the
     * frame the right way up, as display_present does. */
    DrawTextureRec(es->frame_target.texture,
                   (Rectangle){0, 0, (float)es->frame_target.texture.width,
                               -(float)es->frame_target.texture.height},
                   (Vector2){0, 0}, WHITE);
    EndTextureMode();
}

/* ------------------------------------------------------------------ */
/* Layout                                                              */
/* ------------------------------------------------------------------ */

#define BOX_W      520
#define PAD        16
#define TITLE_H    28
#define BUTTON_H   28
#define BUTTON_MIN 96
#define BUTTON_GAP 8

/* Width of the bytes [from, from + length) of text in ui's font. */
static int span_width(UIState *ui, const char *text, int from, int length)
{
    char part[256];
    if (length >= (int)sizeof(part)) length = (int)sizeof(part) - 1;
    memcpy(part, text + from, (size_t)length);
    part[length] = '\0';
    return ui_text_width(ui, part);
}

/* Start of the next UTF-8 character after byte `at`. */
static int next_char(const char *text, int at)
{
    at++;
    while (((unsigned char)text[at] & 0xC0) == 0x80) at++;
    return at;
}

/*
 * wrap_message — Break the message into lines that fit `width`: at the
 * message's own line breaks, at spaces, and inside a word only when the
 * word alone is too wide (a long file path).  Returns the line count.
 */
static int wrap_message(UIState *ui, const char *text, int width, DialogLayout *out)
{
    int count = 0;
    int at = 0;
    int length = (int)strlen(text);

    while (at <= length && count < DIALOG_MAX_LINES) {
        int end = at;      /* the line is [at, end) */
        int fit = at;      /* end of the last whole word that fits */
        for (;;) {
            if (end >= length || text[end] == '\n') {
                if (span_width(ui, text, at, end - at) <= width) fit = end;
                break;
            }
            if (text[end] == ' ') {
                if (span_width(ui, text, at, end - at) > width) break;
                fit = end;
            }
            end++;
        }
        if (fit == at && end > at && span_width(ui, text, at, end - at) > width) {
            /* One word wider than the box: cut it where it stops fitting. */
            fit = next_char(text, at);
            while (fit < end && span_width(ui, text, at, next_char(text, fit) - at) <= width)
                fit = next_char(text, fit);
        }
        out->line_start[count] = at;
        out->line_length[count] = fit - at;
        count++;
        if (fit >= length) break;
        at = fit;
        if (text[at] == ' ' || text[at] == '\n') at++;
    }
    return count;
}

/* Lay out a prompt for `message` and `count` buttons with ui's font,
 * centred in the editor.  Returns 0, or -1 for a count other than 2 or 3. */
static int layout_prompt(UIState *ui, const char *message, const char *const *labels,
                         int count, DialogLayout *out)
{
    int text_h = 0;
    int right;

    if (!ui || !message || !labels || !out || count < 2 || count > 3) return -1;
    memset(out, 0, sizeof(*out));
    font_measure(ui->font, "Ag", NULL, &text_h);
    if (text_h <= 0) text_h = 10;
    out->line_h = text_h + 6;
    out->box_w = BOX_W;
    out->title_h = TITLE_H;
    out->line_count = wrap_message(ui, message, BOX_W - 2 * PAD, out);
    out->button_h = BUTTON_H;
    out->box_h = TITLE_H + PAD + out->line_count * out->line_h + PAD + BUTTON_H + PAD;
    out->box_x = (EDITOR_W - out->box_w) / 2;
    out->box_y = (EDITOR_H - out->box_h) / 2;
    out->text_x = out->box_x + PAD;
    out->text_y = out->box_y + TITLE_H + PAD;
    out->button_y = out->box_y + out->box_h - PAD - BUTTON_H;
    /* Buttons in their given order, the row ending at the right margin. */
    out->button_count = count;
    for (int i = 0; i < count; i++) {
        int w = ui_text_width(ui, labels[i]) + 24;
        out->button_w[i] = w < BUTTON_MIN ? BUTTON_MIN : w;
    }
    right = out->box_x + out->box_w - PAD;
    for (int i = count - 1; i >= 0; i--) {
        right -= out->button_w[i];
        out->button_x[i] = right;
        right -= BUTTON_GAP;
    }
    return 0;
}

static int button_at(const DialogLayout *layout, int x, int y)
{
    if (y < layout->button_y || y >= layout->button_y + layout->button_h) return -1;
    for (int i = 0; i < layout->button_count; i++)
        if (x >= layout->button_x[i] && x < layout->button_x[i] + layout->button_w[i])
            return i;
    return -1;
}

/* ------------------------------------------------------------------ */
/* Drawing                                                             */
/* ------------------------------------------------------------------ */

/* The widgets draw with a UIState of their own, styled like the editor's,
 * so the prompt's hover and clicks never touch the editor's UI state.
 * Static: a UIState holds several kilobytes of edit buffers. */
static UIState s_ui;

static void draw_prompt(EditorState *es, RenderTexture2D target, const char *title,
                        const char *message, const char *const *labels,
                        const DialogLayout *layout, int focus, Vector2 pointer)
{
    UIState *ui = &s_ui;
    char line[512];

    ui_begin_frame(ui);
    ui->mouse_x = (int)pointer.x;
    ui->mouse_y = (int)pointer.y;
    BeginDrawing();
    BeginTextureMode(target);
    ClearBackground(ui_color(ui, UI_COLOR_PANEL));
    /* The last complete frame, dimmed: the editor is still there, waiting. */
    if (IsRenderTextureValid(es->prompt_backdrop))
        DrawTextureRec(es->prompt_backdrop.texture,
                       (Rectangle){0, 0, (float)es->prompt_backdrop.texture.width,
                                   -(float)es->prompt_backdrop.texture.height},
                       (Vector2){0, 0}, WHITE);
    DrawRectangle(0, 0, EDITOR_W, EDITOR_H, ColorAlpha(ui_color(ui, UI_COLOR_PANEL), 0.7f));

    ui_panel(ui, layout->box_x, layout->box_y, layout->box_w, layout->box_h);
    DrawRectangle(layout->box_x, layout->box_y, layout->box_w, layout->title_h,
                  ui_color(ui, UI_COLOR_TITLE));
    ui_label(ui, layout->box_x + PAD, layout->box_y + 8, title);
    /* An outline sets the box off from the dimmed panels behind it. */
    if (ui->style.border_width > 0)
        DrawRectangleLinesEx((Rectangle){(float)layout->box_x, (float)layout->box_y,
                                         (float)layout->box_w, (float)layout->box_h},
                             (float)ui->style.border_width, ui_color(ui, UI_COLOR_BORDER));
    else
        DrawRectangleLines(layout->box_x, layout->box_y, layout->box_w, layout->box_h,
                           ui_color(ui, UI_COLOR_TEXT_DIM));
    for (int i = 0; i < layout->line_count; i++) {
        int length = layout->line_length[i];
        if (length >= (int)sizeof(line)) length = (int)sizeof(line) - 1;
        memcpy(line, message + layout->line_start[i], (size_t)length);
        line[length] = '\0';
        ui_label(ui, layout->text_x, layout->text_y + i * layout->line_h, line);
    }
    /* The highlighted button (Enter presses it) is drawn lit. */
    for (int i = 0; i < layout->button_count; i++)
        (void)ui_toggle(ui, layout->button_x[i], layout->button_y, layout->button_w[i],
                        layout->button_h, labels[i], i == focus);
    display_present(target);   /* ends texture mode, presents, EndDrawing */
    s_frames++;
}

/* ------------------------------------------------------------------ */
/* The prompt                                                          */
/* ------------------------------------------------------------------ */

/* Input the prompt read but does not own: a button released after the
 * press that opened it.  The editor gets it back afterwards, or it would
 * believe the button was still held. */
#define MAX_RETURNED 4

int dialog_choice(const char *title, const char *message, const char *const *labels,
                  int count, int default_index, int cancel_index, int *selected)
{
    EditorState *es = s_host;
    RenderTexture2D target = {0};
    RenderTexture2D *bound;
    DialogLayout layout;
    InputEvent returned[MAX_RETURNED];
    int returned_count = 0;
    int pressed_here[3] = {0, 0, 0};  /* a press began inside the prompt */
    int pressed = -1;                 /* the button the left press is on */
    int focus = default_index;
    int answer = -1;
    int failed = 0;
    int clip = 0, clip_x = 0, clip_y = 0, clip_w = 0, clip_h = 0;

    if (!title || !message || !labels || !selected || count < 2 || count > 3 ||
        default_index < 0 || default_index >= count ||
        cancel_index < 0 || cancel_index >= count) return -1;
    /* Nobody could answer: no editor or no window.  Fail rather than wait
     * forever. */
    if (!es || s_open || !IsWindowReady() || !es->font) return -1;
#if !defined(MANGO_TESTING) && !defined(MANGO_RAYLIB_MEMORY)
    /* Nor in a hidden window (the render smoke test).  Tests use hidden
     * windows too, but answer with queued events (see below). */
    if (IsWindowHidden()) return -1;
#endif

    s_open = 1;
    ui_init(&s_ui, es->font);
    s_ui.style = es->ui.style;
    if (layout_prompt(&s_ui, message, labels, count, &layout) != 0) {
        s_open = 0;
        return -1;
    }
    s_last_layout = layout;

    /* Leave the frame being drawn, if any, remembering how to get back. */
    clip = es->ui.clip_active;
    if (clip) {
        clip_x = es->ui.clip_x; clip_y = es->ui.clip_y;
        clip_w = es->ui.clip_w; clip_h = es->ui.clip_h;
        ui_clip_end(&es->ui);
    }
    bound = es->drawing_target;
    if (bound) EndTextureMode();
    /* The arrow over the box, whatever the editor showed (an I-beam on
     * the field that asked, say); the editor's next frame sets its own. */
    ui_set_cursor(&es->ui, MOUSE_CURSOR_DEFAULT);
    ui_apply_cursor(&es->ui);

    target = LoadRenderTexture(EDITOR_W, EDITOR_H);
    if (!IsRenderTextureValid(target)) failed = 1;
    else SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);

    while (!failed && answer < 0) {
        InputEvent event;
        int got = 0;

        draw_prompt(es, target, title, message, labels, &layout, focus, input_mouse());
        input_collect();
        while (answer < 0 && input_poll(&event)) {
            got = 1;
            switch (event.type) {
            case INPUT_KEY_DOWN:
                if (event.key == KEY_ENTER || event.key == KEY_KP_ENTER ||
                    event.key == KEY_SPACE)
                    answer = focus;
                else if (event.key == KEY_ESCAPE)
                    answer = cancel_index;
                else if (event.key == KEY_TAB)
                    focus = (focus + ((event.mods & INPUT_SHIFT) ? count - 1 : 1)) % count;
                else if ((event.key == KEY_LEFT || event.key == KEY_UP) && focus > 0)
                    focus--;
                else if ((event.key == KEY_RIGHT || event.key == KEY_DOWN) && focus < count - 1)
                    focus++;
                break;
            case INPUT_MOUSE_DOWN:
                if (event.button >= 0 && event.button < 3) pressed_here[event.button] = 1;
                if (event.button == MOUSE_BUTTON_LEFT)
                    pressed = button_at(&layout, event.x, event.y);
                break;
            case INPUT_MOUSE_UP:
                if (event.button < 0 || event.button >= 3 || !pressed_here[event.button]) {
                    if (returned_count < MAX_RETURNED) returned[returned_count++] = event;
                    break;
                }
                pressed_here[event.button] = 0;
                /* Like any button: pressed and released on the same one. */
                if (event.button == MOUSE_BUTTON_LEFT && pressed >= 0 &&
                    button_at(&layout, event.x, event.y) == pressed)
                    answer = pressed;
                if (event.button == MOUSE_BUTTON_LEFT) pressed = -1;
                break;
            case INPUT_QUIT:
                /* Closing the window while asked counts as Cancel, as
                 * closing a native dialog did. */
                answer = cancel_index;
                break;
            default:
                break;   /* typing, the wheel, focus: nothing to do here */
            }
        }
#if defined(MANGO_TESTING) || defined(MANGO_RAYLIB_MEMORY)
        /* Tests and the Memory backend have no user: once the events a
         * test queued are used up, nobody is left to answer. */
        if (answer < 0 && !got) failed = 1;
#else
        (void)got;
#endif
    }

    if (IsRenderTextureValid(target)) UnloadRenderTexture(target);
    for (int i = 0; i < returned_count; i++) (void)input_push(&returned[i]);
    /* Back into the interrupted frame: its target, then its clip. */
    if (bound) BeginTextureMode(*bound);
    if (clip) ui_clip_begin(&es->ui, clip_x, clip_y, clip_w, clip_h);
    s_open = 0;
    if (failed) return -1;
    *selected = answer;
    return 0;
}
