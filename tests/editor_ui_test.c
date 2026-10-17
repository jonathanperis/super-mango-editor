/*
 * editor_ui_test.c — The editor driven the way a designer drives it.
 *
 * Every interaction is an InputEvent (mouse, wheel, key) handled by
 * editor_handle_event, followed by one immediate-mode frame. Assertions read
 * the resulting document, selection, undo history and status line, never
 * pixel colours, so they hold while the panels' code is reorganised.
 *
 * The frames draw into an 8x8 render target: every widget and canvas path
 * still runs (hit tests, hover, clicks, text), but software rendering of a
 * full 1280x720 frame would cost a hundred times more. Two real
 * editor_run_frame calls cover the full-size frame path as well.
 *
 * Panel positions are found by probing, not hard-coded: a probe is one click
 * at a point, and its effect (tool, selection, active field) says what was
 * there. Native dialogs can never open: their test seams are armed before
 * every probe, and on POSIX fake pickers that answer "Cancel" come first on
 * PATH.
 */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L /* fork, setenv, mkdir, chmod under -std=c11 */
#endif

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "editor/canvas.h"
#include "editor/dialog_choice.h"
#include "editor/editor_campaign.h"
#include "editor/editor.h"
#include "editor/editor_chrome.h"
#include "editor/editor_events.h"
#include "editor/editor_files.h"
#include "editor/editor_frame.h"
#include "editor/editor_layout.h"
#include "editor/editor_minimap.h"
#include "editor/editor_panels.h"
#include "editor/editor_playtest.h"
#include "editor/editor_session.h"
#include "editor/editor_shortcuts.h"
#include "editor/entity_meta.h"
#include "editor/file_dialog.h"
#include "editor/properties.h"
#include "editor/tools.h"
#include "levels/level_loader.h"
#include "shared/serializer.h"
#include "test_folders.h"
#include "test_paths.h"

#include <rlgl.h>  /* rlGetActiveFramebuffer: which target draws land in */

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "editor_ui_test:%d: %s\n", __LINE__, #test); \
    failed = 1; goto done; } } while (0)

/* A spot in the status bar: drawn every frame, but nothing there to click. */
#define NEUTRAL_X (CANVAS_W / 2)
#define NEUTRAL_Y (EDITOR_H - STATUS_H / 2)
/* Probe columns in the right panel: labels on the left, fields on the right. */
#define PANEL_LABEL_X (CANVAS_W + 40)
#define PANEL_FIELD_X (CANVAS_W + 140)
#define PANEL_FAR_X   (CANVAS_W + 300)
/* Level Config rows (the panel is open and unscrolled at start). */
#define CFG_NAME_Y    (TOOLBAR_H + 64 + 4)
#define CFG_SCREENS_Y (TOOLBAR_H + 136 + 4)

/* A dropped path this long fills raylib's MAX_FILEPATH_LENGTH buffer
 * (260 on Windows, 4096 elsewhere, less the NUL). */
#ifdef _WIN32
#define EDITOR_DROP_TEST_LONG 259
#else
#define EDITOR_DROP_TEST_LONG 4095
#endif

static RenderTexture2D tiny_target;

/* ------------------------------------------------------------------ */
/* Input and frames                                                    */
/* ------------------------------------------------------------------ */

static void push_event(InputKind type, int key_or_button, int mods, int x, int y)
{
    InputEvent event = {0};
    event.type = type;
    if (type == INPUT_KEY_DOWN || type == INPUT_KEY_UP) event.key = key_or_button;
    else event.button = key_or_button;
    event.mods = mods;
    event.x = x;
    event.y = y;
    input_push(&event);
}

static void push_key(int key, int mods)
{
    push_event(INPUT_KEY_DOWN, key, mods, 0, 0);
    push_event(INPUT_KEY_UP, key, mods, 0, 0);
}

static void push_click(int x, int y)
{
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, 0, x, y);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, 0, x, y);
}

static void push_wheel(int x, int y, float wheel, int mods)
{
    InputEvent event = {0};
    event.type = INPUT_WHEEL;
    event.x = x;
    event.y = y;
    event.wheel = wheel;
    event.mods = mods;
    input_push(&event);
}

static void push_text(const char *text)
{
    for (; *text; text++) {
        InputEvent event = {0};
        event.type = INPUT_TEXT;
        event.text[0] = *text;
        input_push(&event);
    }
}

/* What the "Skip this undo step?" prompt is answered with: "Keep the
 * step", the dialog's own default (what Enter chooses), unless a test sets
 * another answer for the frames that follow.  clear_dialog_seams puts it
 * back. */
static int skip_step_answer = EDITOR_SKIP_STEP_KEEP;

/* A dialog that would block the run instead returns its safe answer. */
static void arm_dialog_seams(void)
{
    editor_test_set_finish_field_choice(2);   /* Discard the field edit */
    editor_test_set_skip_step_choice(skip_step_answer);
    file_dialog_test_set_open_result(FILE_DIALOG_CANCELLED, NULL);
    file_dialog_test_set_save_result(FILE_DIALOG_CANCELLED, NULL);
}

static void clear_dialog_seams(void)
{
    skip_step_answer = EDITOR_SKIP_STEP_KEEP;
    editor_test_set_skip_step_choice(-1);
    editor_test_set_finish_field_choice(-1);
    file_dialog_test_set_open_result(-1, NULL);
    file_dialog_test_set_save_result(-1, NULL);
}

/*
 * One editor frame with the pointer at (mx, my): queued events, hover and
 * drawing in editor_run_frame's order, into the tiny target.
 */
static void ui_frame(EditorState *es, int mx, int my)
{
    InputEvent event;
    arm_dialog_seams();
    ui_begin_frame(&es->ui);
    while (input_poll(&event)) editor_handle_event(es, &event);
    editor_handle_dropped_files(es);
    es->mouse_x = es->ui.mouse_x = mx;
    es->mouse_y = es->ui.mouse_y = my;
    es->ui.mouse_down = es->mouse_down;
    if (es->playing) editor_check_play_status(es);
    /* Like editor_run_frame, revalidate (which also checks asset files on
     * disk) only when the document changed. */
    uint64_t hash = editor_document_hash(&es->level);
    if (!es->validation_cache_valid || hash != es->validated_document_hash) {
        editor_validate_level(&es->level, &es->validation_report);
        es->validated_document_hash = hash;
        es->validation_cache_valid = 1;
    }
    canvas_clamp_camera(es);
    BeginDrawing();
    BeginTextureMode(tiny_target);
    es->drawing_target = &tiny_target;
    if (es->playing) {
        editor_render_play_overlay(es);
    } else {
        canvas_render(es);
        editor_render_toolbar(es);
        editor_render_side_panels(es);
        editor_render_status_bar(es);
        ui_draw_overlays(&es->ui);
        editor_shortcut_sheet_render(es);
    }
    editor_update_cursor(es);
    EndTextureMode();
    es->drawing_target = NULL;
    EndDrawing();
}

/* Click at (x, y) and run the frame that sees it. */
static void click_frame(EditorState *es, int x, int y)
{
    push_click(x, y);
    ui_frame(es, x, y);
}

static void key_frame(EditorState *es, int key, int mods)
{
    push_key(key, mods);
    ui_frame(es, NEUTRAL_X, NEUTRAL_Y);
}

static int level_is_valid(const EditorState *es)
{
    return level_validate_runtime(&es->level, NULL, 0) == 0;
}

static uint64_t doc_hash(const EditorState *es)
{
    return editor_document_hash(&es->level);
}

static int open_editor(EditorState *es, const char *level_path)
{
    memset(es, 0, sizeof(*es));
    if (editor_init(es, 1) != 0) return -1;
    /* Each editor owns its window, so the tiny target lives with it. */
    tiny_target = LoadRenderTexture(8, 8);
    if (!IsRenderTextureValid(tiny_target)) return -1;
    if (level_path) {
        if (level_load_toml(level_path, &es->level) != 0) return -1;
        editor_sync_config_resources(es);
        editor_set_document_save_point(es);
    }
    input_clear();
    return 0;
}

static void close_editor(EditorState *es)
{
    if (IsRenderTextureValid(tiny_target)) UnloadRenderTexture(tiny_target);
    tiny_target = (RenderTexture2D){0};
    editor_cleanup(es);
}

/*
 * Finish whatever a probe click at (x, y) started: a text edit gets a digit
 * appended and Enter; an open dropdown gets the entry below the field
 * picked. If the document changed, that must be exactly one undo step:
 * undo restores the exact document, redo brings the edit back, and a
 * final undo leaves the document as it was before the probe.
 * Returns 0 on success.
 */
static int settle_probe(EditorState *es, int x, int y, uint64_t before,
                        int undo_before, int *changes)
{
    if (es->ui.active_id) {
        push_text("7");
        key_frame(es, KEY_ENTER, 0);
        if (es->ui.active_id) key_frame(es, KEY_ESCAPE, 0);
    }
    if (es->ui.dropdown_open_id) {
        click_frame(es, x, y + 26);
        if (es->ui.dropdown_open_id) click_frame(es, NEUTRAL_X, NEUTRAL_Y);
    }
    if (es->ui.active_id || es->ui.dropdown_open_id) return -1;
    if (doc_hash(es) == before) return es->undo->top == undo_before ? 0 : -1;
    if (es->undo->top != undo_before + 1) return -1;
    (*changes)++;
    uint64_t after = doc_hash(es);
    key_frame(es, KEY_Z, INPUT_CTRL);
    if (doc_hash(es) != before || es->undo->top != undo_before) return -1;
    key_frame(es, KEY_Y, INPUT_CTRL);
    if (doc_hash(es) != after) return -1;
    key_frame(es, KEY_Z, INPUT_CTRL);
    return doc_hash(es) == before ? 0 : -1;
}

/* ------------------------------------------------------------------ */
/* Palette                                                             */
/* ------------------------------------------------------------------ */

/*
 * Click down the palette column. A click that switches to the Place tool
 * hit an entry; any other click hit a header (which opens or closes its
 * category), so the next probe skips past it. Every probe starts from the
 * Select tool so each pick is visible.
 */
static int scan_palette(EditorState *es, int top, int bottom, int picked[ENT_COUNT])
{
    int found = 0;
    for (int y = top; y < bottom; ) {
        push_key(KEY_ONE, 0);
        click_frame(es, PANEL_LABEL_X, y);
        if (es->tool == TOOL_PLACE) {
            if (es->palette_type < 0 || es->palette_type >= ENT_COUNT) return -1;
            if (!picked[es->palette_type]) found++;
            picked[es->palette_type] = 1;
            y += 9;
        } else {
            y += 30;
        }
    }
    return found;
}

/* UIScroll: trackpad fractions add up to whole units, the offset stays
 * within the content, and reveal moves just far enough. */
static int ui_scroll_keeps_fractions_and_stays_in_range(void)
{
    int failed = 0;
    UIScroll rows = {0};
    for (int i = 0; i < 3; i++) CHECK(ui_scroll_wheel(&rows, -0.25f, 1.0f) == 0);
    CHECK(rows.offset == 0);
    CHECK(ui_scroll_wheel(&rows, -0.25f, 1.0f) == 1 && rows.offset == 1);
    (void)ui_scroll_wheel(&rows, 5.0f, 1.0f);           /* far past the top */
    CHECK(rows.offset == 0);

    UIScroll pixels = {0};
    CHECK(ui_scroll_wheel(&pixels, -0.1f, 20.0f) == 2 && pixels.offset == 2);
    pixels.offset = 500;
    ui_scroll_clamp(&pixels, 300, 100);                 /* 200 px can scroll */
    CHECK(pixels.offset == 200);
    ui_scroll_clamp(&pixels, 50, 100);                  /* everything fits */
    CHECK(pixels.offset == 0);

    ui_scroll_reveal(&pixels, 300, 320, 100, 200, 8);   /* below the view */
    CHECK(pixels.offset == 128);
    ui_scroll_reveal(&pixels, 90, 110, 100, 200, 8);    /* above the view */
    CHECK(pixels.offset == 110);
done:
    return failed;
}

/* A scrollbar: a track click moves one view, dragging the thumb scrolls in
 * proportion, its clicks are used up, and with nothing to scroll it is
 * absent and lets clicks through. */
static int ui_scrollbar_pages_drags_and_uses_its_clicks(void)
{
    int failed = 0;
    UIState ui;
    UIScroll scroll = {0};
    ui_init(&ui, NULL);

    /* view 100 of 400 on a 100 px track: a 25 px thumb at the top. */
    ui.mouse_x = 2; ui.mouse_y = 80; ui.mouse_clicked = 1;
    CHECK(ui_scrollbar_input(&ui, &scroll, 0, 0, 5, 100, 100, 400) == 1);
    CHECK(scroll.offset == 100 && ui.mouse_clicked == 0 && ui.scroll_drag == NULL);

    /* The thumb now spans y 25..50: take it at 30 and drag to the bottom. */
    ui.mouse_y = 30; ui.mouse_clicked = 1; ui.mouse_down = 1;
    (void)ui_scrollbar_input(&ui, &scroll, 0, 0, 5, 100, 100, 400);
    CHECK(ui.scroll_drag == &scroll && ui.scroll_grab == 5 && ui.mouse_clicked == 0);
    ui.mouse_clicked = 0; ui.mouse_y = 200;
    CHECK(ui_scrollbar_input(&ui, &scroll, 0, 0, 5, 100, 100, 400) == 1);
    CHECK(scroll.offset == 300);
    ui.mouse_y = 5 + 37;              /* thumb top at 37 of 75: half way */
    (void)ui_scrollbar_input(&ui, &scroll, 0, 0, 5, 100, 100, 400);
    CHECK(scroll.offset == 148);
    ui.mouse_down = 0;
    (void)ui_scrollbar_input(&ui, &scroll, 0, 0, 5, 100, 100, 400);
    CHECK(ui.scroll_drag == NULL && scroll.offset == 148);

    /* Everything fits: no bar, the click is left for the content. */
    UIScroll fits = {0};
    ui.mouse_y = 50; ui.mouse_clicked = 1;
    CHECK(ui_scrollbar_input(&ui, &fits, 0, 0, 5, 100, 100, 80) == 0);
    CHECK(ui.mouse_clicked == 1 && fits.offset == 0);
done:
    return failed;
}

/*
 * A thumb drag ends at the next frame once its scrollbar is no longer run
 * (F5 hands the window to the playtest overlay, and the panel's release
 * is never seen) or the button was up, so the up-down cursor goes back.
 */
static int scrollbar_drag_ends_with_its_bar(void)
{
    int failed = 0;
    UIState ui;
    UIScroll scroll = {0};
    ui_init(&ui, NULL);

    /* Take the 25 px thumb at the top of a 100 px track. */
    ui_begin_frame(&ui);
    ui.mouse_x = 2; ui.mouse_y = 10; ui.mouse_clicked = 1; ui.mouse_down = 1;
    (void)ui_scrollbar_input(&ui, &scroll, 0, 0, 5, 100, 100, 400);
    CHECK(ui.scroll_drag == &scroll);
    ui_apply_cursor(&ui);
    CHECK(ui.cursor_shown == MOUSE_CURSOR_RESIZE_NS);
    /* Held, and the bar runs again: the drag goes on. */
    ui_begin_frame(&ui);
    ui.mouse_down = 1;
    (void)ui_scrollbar_input(&ui, &scroll, 0, 0, 5, 100, 100, 400);
    CHECK(ui.scroll_drag == &scroll);
    /* Still held, but the panel is not drawn: the next frame lets go. */
    ui_begin_frame(&ui);
    ui.mouse_down = 1;
    ui_begin_frame(&ui);
    ui.mouse_down = 1;
    CHECK(ui.scroll_drag == NULL);
    ui_apply_cursor(&ui);
    CHECK(ui.cursor_shown == MOUSE_CURSOR_DEFAULT);

    /* Released in a frame the bar did not see the release in. */
    ui_begin_frame(&ui);
    ui.mouse_y = 10; ui.mouse_clicked = 1; ui.mouse_down = 1;
    (void)ui_scrollbar_input(&ui, &scroll, 0, 0, 5, 100, 100, 400);
    CHECK(ui.scroll_drag == &scroll);
    ui.mouse_down = 0;
    ui_begin_frame(&ui);
    CHECK(ui.scroll_drag == NULL);
done:
    return failed;
}

/* A scrolled panel still draws its fields scrolled out of view; those must
 * not show the I-beam over whatever is under the pointer there. */
static int scrolled_out_field_wants_no_ibeam(void)
{
    int failed = 0;
    UIState ui;
    UIScroll scroll = {0};
    int value = 5;
    ui_init(&ui, NULL);
    ui_begin_frame(&ui);
    ui.mouse_x = 20; ui.mouse_y = 150;   /* below the 100 px view */
    int top = ui_scroll_begin(&ui, &scroll, 0, 0, 200, 100, 400);
    (void)ui_int_field(&ui, 1, 10, top + 145, 80, &value);
    ui_scroll_end(&ui);
    CHECK(ui.cursor_wish != MOUSE_CURSOR_IBEAM);
    /* The same field inside the view does want it. */
    ui_begin_frame(&ui);
    ui.mouse_x = 20; ui.mouse_y = 50;
    top = ui_scroll_begin(&ui, &scroll, 0, 0, 200, 100, 400);
    (void)ui_int_field(&ui, 1, 10, top + 45, 80, &value);
    ui_scroll_end(&ui);
    CHECK(ui.cursor_wish == MOUSE_CURSOR_IBEAM);
done:
    return failed;
}

/* WCAG relative luminance of an sRGB colour. */
static double luminance(Color c)
{
    double channel[3] = {c.r / 255.0, c.g / 255.0, c.b / 255.0};
    for (int i = 0; i < 3; i++)
        channel[i] = channel[i] <= 0.03928 ? channel[i] / 12.92
                                           : pow((channel[i] + 0.055) / 1.055, 2.4);
    return 0.2126 * channel[0] + 0.7152 * channel[1] + 0.0722 * channel[2];
}

static double contrast(Color a, Color b)
{
    double la = luminance(a), lb = luminance(b);
    return la > lb ? (la + 0.05) / (lb + 0.05) : (lb + 0.05) / (la + 0.05);
}

static int same_color(Color a, Color b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

/* ------------------------------------------------------------------ */
/* Canvas                                                              */
/* ------------------------------------------------------------------ */

/*
 * The floor is drawn only where the canvas shows it, so check the one
 * thing culling could break: the strip still reaches both canvas edges,
 * scrolled anywhere and at any zoom, with gaps left open.  This renders
 * the canvas alone into the window and reads it back.
 */
static int floor_reaches_both_canvas_edges_at_any_scroll(void)
{
    int failed = 0;
    EditorState es;
    Image shot = {0};
    const Color backdrop = {255, 0, 255, 255};
    const float zooms[] = {1.0f, 2.0f, 3.0f, 5.0f};  /* the zoom menu */
    CHECK(open_editor(&es, NULL) == 0);
    /* A new level has no sky image, so the canvas starts as flat sky blue:
     * a stretch the floor failed to cover shows that blue. */
    const Color sky = {0x87, 0xCE, 0xEB, 255};
    const Color sky_swapped = {0xEB, 0xCE, 0x87, 255};   /* Memory reads BGRA */
    CHECK(es.textures.sky == NULL);
    es.level.screen_count = 12;
    es.level.floor_gaps[0] = 400;
    es.level.floor_gaps[1] = 2000;
    es.level.floor_gap_count = 2;
    const float world_w = (float)(es.level.screen_count * GAME_W);
    for (size_t z = 0; z < sizeof(zooms) / sizeof(zooms[0]); z++) {
        for (int step = 0; step <= 8; step++) {
            es.camera.zoom = zooms[z];
            /* Start, end and odd fractional positions in between. */
            es.camera.x = step * (world_w / 8.0f) + (step % 3) * 7.3f;
            /* Centre the dirt row vertically so it is on screen at every
             * zoom (at 3x and 5x the world is taller than the canvas). */
            es.camera.y = (float)(FLOOR_Y + TILE_SIZE / 2) - (CANVAS_H / 2) / es.camera.zoom;
            canvas_clamp_camera(&es);
            /* Drawn straight to the window: the Memory backend reads back
             * the framebuffer but not textures. */
            BeginDrawing();
            ClearBackground(backdrop);
            canvas_render(&es);
            rlDrawRenderBatchActive();
            shot = LoadImageFromScreen();
            EndDrawing();
            CHECK(IsImageValid(shot) && shot.width == EDITOR_W && shot.height == EDITOR_H);
#ifdef MANGO_MEMORY_TESTS
            /* Memory readback is bottom-origin (and BGRA: the magenta
             * backdrop does not notice, the sky is checked both ways). */
            ImageFlipVertical(&shot);
#endif
            /* Middle of the dirt row, in screen pixels. */
            int sy = (int)((FLOOR_Y + TILE_SIZE / 2 - es.camera.y) * es.camera.zoom) + TOOLBAR_H;
            CHECK(sy >= TOOLBAR_H && sy < TOOLBAR_H + CANVAS_H);
            int checked = 0;
            for (int sx = 0; sx < CANVAS_W; sx++) {
                float wx = sx / es.camera.zoom + es.camera.x;
                int near_gap = 0;
                for (int g = 0; g < es.level.floor_gap_count; g++)
                    near_gap |= wx > es.level.floor_gaps[g] - 2.0f &&
                                wx < es.level.floor_gaps[g] + FLOOR_GAP_W + 2.0f;
                if (near_gap || wx >= world_w - 1.0f) continue;
                Color c = GetImageColor(shot, sx, sy);
                checked++;
                if (same_color(c, backdrop) || same_color(c, sky) || same_color(c, sky_swapped)) {
                    fprintf(stderr, "floor missing at zoom %.1f camera %.1f sx %d\n",
                            es.camera.zoom, es.camera.x, sx);
                    CHECK(0);
                }
            }
            /* Most of the row is floor (only the gaps and the world's end
             * are skipped), so the check above really ran. */
            CHECK(checked > CANVAS_W / 2);
            UnloadImage(shot);
            shot = (Image){0};
        }
    }
done:
    if (shot.data) UnloadImage(shot);
    close_editor(&es);
    return failed;
}

/* Rest the pointer at (x, y) until time_ms, and run that frame. */
static void hover_frame(EditorState *es, int x, int y, uint32_t time_ms)
{
    es->ui.time_ms = time_ms;
    ui_frame(es, x, y);
}

/*
 * Toolbar tooltips: shown once the pointer has rested on a button for
 * UI_TOOLTIP_DELAY_MS, restarted by moving to another button, hidden by a
 * click until the pointer leaves, and kept inside the window at its edge.
 */
static int toolbar_tooltips_wait_then_name_the_shortcut(void)
{
    int failed = 0;
    EditorState es;
    const int select_x = 30, grid_x = 4 + 3 * (64 + 4) + 16 + 20, bar_y = 15;
    CHECK(open_editor(&es, NULL) == 0);

    hover_frame(&es, select_x, bar_y, 1000);
    CHECK(es.ui.tooltip_text && strstr(es.ui.tooltip_text, "(1)") != NULL);
    CHECK(!es.ui.tooltip_shown);
    hover_frame(&es, select_x + 3, bar_y, 1000 + UI_TOOLTIP_DELAY_MS - 1);
    CHECK(!es.ui.tooltip_shown);
    hover_frame(&es, select_x, bar_y, 1000 + UI_TOOLTIP_DELAY_MS);
    CHECK(es.ui.tooltip_shown);
    /* Below and right of the pointer, inside the window. */
    CHECK(es.ui.tip_box[0] > select_x && es.ui.tip_box[1] > bar_y);

    /* Another button starts its own wait. */
    hover_frame(&es, grid_x, bar_y, 2000);
    CHECK(es.ui.tooltip_text && strcmp(es.ui.tooltip_text, "Show the grid (G)") == 0);
    CHECK(!es.ui.tooltip_shown);
    hover_frame(&es, grid_x, bar_y, 2600);
    CHECK(es.ui.tooltip_shown);

    /* A click hides it while the pointer stays... */
    CHECK(es.show_grid);
    push_click(grid_x, bar_y);
    hover_frame(&es, grid_x, bar_y, 2700);
    CHECK(!es.show_grid);
    hover_frame(&es, grid_x, bar_y, 5000);
    CHECK(!es.ui.tooltip_shown);
    /* ...and leaving the button resets it. */
    hover_frame(&es, NEUTRAL_X, NEUTRAL_Y, 5100);
    CHECK(es.ui.tooltip_text == NULL && !es.ui.tooltip_shown);
    hover_frame(&es, grid_x, bar_y, 5200);
    CHECK(!es.ui.tooltip_shown);
    hover_frame(&es, grid_x, bar_y, 5700);
    CHECK(es.ui.tooltip_shown);

    /* Play sits at the right edge: its tip moves left to stay inside. */
    hover_frame(&es, EDITOR_W - 10, bar_y, 6000);
    CHECK(es.ui.tooltip_text && strstr(es.ui.tooltip_text, "F5") != NULL);
    hover_frame(&es, EDITOR_W - 10, bar_y, 7000);
    CHECK(es.ui.tooltip_shown);
    CHECK(es.ui.tip_box[0] + es.ui.tip_box[2] <= EDITOR_W);
    CHECK(es.ui.tip_box[0] < EDITOR_W - 10);
done:
    close_editor(&es);
    return failed;
}

/* The default style is the editor look the UI_* colours describe, with no
 * control outlines; the high-contrast style keeps every text colour at the
 * WCAG AAA ratio (7:1) or more on every face it is drawn on. */
/* A shown editor slows to 10 frames a second while another window has
 * focus and returns to 60 with it; a hidden (smoke) window is left alone. */
static int unfocused_editor_slows_its_frames(void)
{
    int failed = 0;
    CHECK(editor_frame_rate_for(0, 1) == EDITOR_FPS_FOCUSED && EDITOR_FPS_FOCUSED == 60);
    CHECK(editor_frame_rate_for(0, 0) == EDITOR_FPS_UNFOCUSED && EDITOR_FPS_UNFOCUSED == 10);
    CHECK(editor_frame_rate_for(1, 0) == 0 && editor_frame_rate_for(1, 1) == 0);
done:
    return failed;
}

static int ui_styles_keep_the_default_look_and_high_contrast(void)
{
    int failed = 0;
    const UIStyle *d = &UI_STYLE_DEFAULT, *h = &UI_STYLE_HIGH_CONTRAST;
    CHECK(same_color(d->color[UI_COLOR_PANEL], UI_BG) && same_color(d->color[UI_COLOR_TITLE], UI_TITLE_BG));
    CHECK(same_color(d->color[UI_COLOR_INPUT], UI_INPUT_BG) && same_color(d->color[UI_COLOR_BUTTON], UI_BTN));
    CHECK(same_color(d->color[UI_COLOR_HOT], UI_BTN_HOT) && same_color(d->color[UI_COLOR_SELECTED], UI_BTN_ACTIVE));
    CHECK(same_color(d->color[UI_COLOR_TEXT], UI_TEXT) && same_color(d->color[UI_COLOR_TEXT_DIM], UI_TEXT_DIM));
    CHECK(same_color(d->color[UI_COLOR_ACCENT], UI_ACCENT) && d->border_width == 0);
    CHECK(h->border_width > 0);
    const UIColorRole faces[] = {UI_COLOR_PANEL, UI_COLOR_TITLE, UI_COLOR_INPUT,
                                 UI_COLOR_BUTTON, UI_COLOR_HOT, UI_COLOR_SELECTED};
    const UIColorRole inks[] = {UI_COLOR_TEXT, UI_COLOR_TEXT_DIM};
    for (size_t f = 0; f < sizeof(faces) / sizeof(faces[0]); f++)
        for (size_t i = 0; i < sizeof(inks) / sizeof(inks[0]); i++)
            CHECK(contrast(h->color[inks[i]], h->color[faces[f]]) >= 7.0);
    const UIColorRole on_panels[] = {UI_COLOR_ACCENT, UI_COLOR_WARNING, UI_COLOR_ERROR, UI_COLOR_BORDER};
    for (size_t i = 0; i < sizeof(on_panels) / sizeof(on_panels[0]); i++) {
        CHECK(contrast(h->color[on_panels[i]], h->color[UI_COLOR_PANEL]) >= 7.0);
        CHECK(contrast(h->color[on_panels[i]], h->color[UI_COLOR_TITLE]) >= 7.0);
    }
done:
    return failed;
}

/* The toolbar's Contrast button switches every widget to the high-contrast
 * style and back. */
static int toolbar_contrast_toggles_the_high_contrast_style(void)
{
    int failed = 0;
    EditorState es;
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(es.high_contrast == 0 && es.ui.style.border_width == 0);
    const int x = 520 + 42, y = 4 + 12;   /* centre of the Contrast button */
    ui_frame(&es, x, y);
    click_frame(&es, x, y);
    CHECK(es.high_contrast == 1);
    CHECK(es.ui.style.border_width == UI_STYLE_HIGH_CONTRAST.border_width);
    CHECK(same_color(ui_color(&es.ui, UI_COLOR_TEXT), UI_STYLE_HIGH_CONTRAST.color[UI_COLOR_TEXT]));
    ui_frame(&es, x, y);
    click_frame(&es, x, y);
    CHECK(es.high_contrast == 0 && es.ui.style.border_width == 0);
    CHECK(same_color(ui_color(&es.ui, UI_COLOR_TEXT), UI_TEXT));
done:
    close_editor(&es);
    return failed;
}

/*
 * The colour at (x, y) of the toolbar alone, drawn into the window with
 * the pointer parked in the status bar.  Memory readback is bottom-origin
 * and BGRA (see session_test.c); this puts it back to RGBA top-down.
 */
static Color toolbar_pixel(EditorState *es, int x, int y)
{
    Color c = {0};
    es->ui.mouse_x = NEUTRAL_X;
    es->ui.mouse_y = NEUTRAL_Y;
    BeginDrawing();
    ClearBackground(BLACK);
    editor_render_toolbar(es);
    rlDrawRenderBatchActive();
    Image shot = LoadImageFromScreen();
    EndDrawing();
    if (!IsImageValid(shot)) return c;
#ifdef MANGO_MEMORY_TESTS
    c = GetImageColor(shot, x, shot.height - 1 - y);
    unsigned char red = c.r;
    c.r = c.b;
    c.b = red;
#else
    c = GetImageColor(shot, x, y);
#endif
    UnloadImage(shot);
    return c;
}

/*
 * The tools and the on/off buttons are toggles: a click switches them,
 * and the one that is on is drawn in the selected colour, outlined in the
 * accent colour under high contrast, instead of a bracketed label.
 */
static int toolbar_toggles_show_what_is_on(void)
{
    int failed = 0;
    EditorState es;
    const int by = 4 + 12, select_x = 4 + 32, place_x = 72 + 32, delete_x = 140 + 32;
    const int grid_x = 224 + 28, debug_x = 284 + 28;
    Color selected, button, accent;
    CHECK(open_editor(&es, NULL) == 0);
    selected = ui_color(&es.ui, UI_COLOR_SELECTED);
    button = ui_color(&es.ui, UI_COLOR_BUTTON);

    /* Select is the starting tool, and the grid starts shown. */
    CHECK(es.tool == TOOL_SELECT && es.show_grid && !es.debug_play);
    CHECK(same_color(toolbar_pixel(&es, select_x, by - 8), selected));
    CHECK(same_color(toolbar_pixel(&es, place_x, by - 8), button));
    CHECK(same_color(toolbar_pixel(&es, grid_x, by - 8), selected));
    CHECK(same_color(toolbar_pixel(&es, debug_x, by - 8), button));

    click_frame(&es, place_x, by);
    CHECK(es.tool == TOOL_PLACE);
    click_frame(&es, delete_x, by);
    CHECK(es.tool == TOOL_DELETE);
    CHECK(same_color(toolbar_pixel(&es, delete_x, by - 8), selected));
    CHECK(same_color(toolbar_pixel(&es, select_x, by - 8), button));
    click_frame(&es, select_x, by);
    CHECK(es.tool == TOOL_SELECT);

    click_frame(&es, grid_x, by);
    CHECK(!es.show_grid);
    CHECK(same_color(toolbar_pixel(&es, grid_x, by - 8), button));
    click_frame(&es, debug_x, by);
    CHECK(es.debug_play);
    CHECK(same_color(toolbar_pixel(&es, debug_x, by - 8), selected));

    /* High contrast outlines the lit toggle in the accent colour, and the
     * others in the plain border colour. */
    editor_set_high_contrast(&es, 1);
    accent = ui_color(&es.ui, UI_COLOR_ACCENT);
    CHECK(same_color(toolbar_pixel(&es, debug_x, 4), accent));
    CHECK(same_color(toolbar_pixel(&es, grid_x, 4), ui_color(&es.ui, UI_COLOR_BORDER)));
    CHECK(same_color(toolbar_pixel(&es, debug_x, by - 4),
                     ui_color(&es.ui, UI_COLOR_SELECTED)));
done:
    close_editor(&es);
    return failed;
}

static int palette_clicks_choose_what_the_place_tool_adds(void)
{
    int failed = 0;
    EditorState es;
    int picked[ENT_COUNT] = {0};
    CHECK(open_editor(&es, NULL) == 0);

    /* Level Config is taller than its part of the column: a click low on
     * its scrollbar's track scrolls it a page and changes nothing else. */
    {
        uint64_t hash_before = doc_hash(&es);
        CHECK(es.config_open == 1 && es.config_scroll.offset == 0);
        const int bar_x = CANVAS_W + PANEL_W - 2, bar_y = TOOLBAR_H + 28 + 290;
        ui_frame(&es, bar_x, bar_y);
        click_frame(&es, bar_x, bar_y);
        CHECK(es.config_scroll.offset > 0 && es.config_open == 1);
        CHECK(doc_hash(&es) == hash_before && es.ui.active_id == 0);
        push_wheel(PANEL_LABEL_X, TOOLBAR_H + 100, 200.0f, 0);   /* back to the top */
        ui_frame(&es, PANEL_LABEL_X, TOOLBAR_H + 100);
        CHECK(es.config_scroll.offset == 0);
    }

    /* Collapse Level Config by its header so the palette gets the column. */
    CHECK(es.config_open == 1);
    click_frame(&es, PANEL_LABEL_X, TOOLBAR_H + 8);
    CHECK(es.config_open == 0);
    const int column_bottom = EDITOR_H - STATUS_H;

    /* Find the palette title just below the folded config header: the
     * click that folds the palette away. Clicking it again unfolds it. */
    int title_y = -1;
    for (int y = TOOLBAR_H + 20; y < TOOLBAR_H + 120 && title_y < 0; y += 4) {
        click_frame(&es, PANEL_LABEL_X, y);
        if (!es.palette_open) title_y = y;
    }
    CHECK(title_y > 0 && es.config_open == 0);
    /* Nothing is under a folded palette. */
    push_key(KEY_ONE, 0);
    click_frame(&es, PANEL_LABEL_X, title_y + 60);
    CHECK(es.tool == TOOL_SELECT);
    click_frame(&es, PANEL_LABEL_X, title_y);
    CHECK(es.palette_open == 1);
    /* Rows start below the title, which is no taller than a header. */
    const int palette_top = title_y + 30;

    /* Pass 1 opens categories top-down; the list then outgrows the column,
     * so scroll to the end and pass again for the rest. */
    int found = scan_palette(&es, palette_top, column_bottom - 4, picked);
    CHECK(found >= 4);
    for (int pass = 0; pass < 3; pass++) {
        push_wheel(PANEL_LABEL_X, palette_top + 100, -40.0f, 0);
        ui_frame(&es, PANEL_LABEL_X, palette_top + 100);
        int more = scan_palette(&es, palette_top, column_bottom - 4, picked);
        CHECK(more >= 0);
        found += more;
    }
    push_wheel(PANEL_LABEL_X, palette_top + 100, 80.0f, 0);   /* back to the top */
    ui_frame(&es, PANEL_LABEL_X, palette_top + 100);
    CHECK(found >= 15);

    CHECK(!picked[ENT_PLAYER_SPAWN] || editor_entity_type_is_singleton(ENT_PLAYER_SPAWN));
    CHECK(doc_hash(&es) == es.saved_document_hash && es.undo->top == 0);

    /* Each picked type is what a canvas click then places, with its ghost
     * drawn under the cursor first. Some need something to attach to (a
     * rail, a gap); those refuse with a status message. Either way the
     * level stays valid and every placement is one undo step. Each new
     * array placement also adds one to the status bar's "Entities: N". */
    int placed = 0, refused = 0;
    for (int type = 0; type < ENT_COUNT; type++) {
        if (!picked[type]) continue;
        es.palette_type = (EntityType)type;
        es.tool = TOOL_PLACE;
        int x = 40 + (placed * 53) % (CANVAS_W - 120);
        int y = TOOLBAR_H + 120 + (placed % 3) * 60;
        ui_frame(&es, x, y);                       /* ghost preview */
        int count = editor_entity_count(&es.level, (EntityType)type);
        int total = editor_placed_entity_total(&es.level);
        int undo_top = es.undo->top;
        click_frame(&es, x, y);
        CHECK(level_is_valid(&es));
        if (editor_entity_count(&es.level, (EntityType)type) == count + 1 ||
            (editor_entity_type_is_singleton((EntityType)type) && es.undo->top == undo_top + 1)) {
            CHECK(es.undo->top == undo_top + 1);
            if (!editor_entity_type_is_singleton((EntityType)type))
                CHECK(editor_placed_entity_total(&es.level) == total + 1);
            placed++;
        } else {
            CHECK(es.undo->top == undo_top && es.status_message[0] != '\0');
            refused++;
        }
    }
    CHECK(placed >= 12 && placed + refused == found);

    /* Checkpoints count toward "Entities: N" too (the status bar used to
     * leave them out). One must sit right of the player start, which the
     * sweep above may not reach, so place one near the canvas's right edge. */
    {
        const int cx = CANVAS_W - 60, cy = TOOLBAR_H + 200;
        float wx, wy;
        canvas_screen_to_world(&es, cx, cy, &wx, &wy);
        CHECK(wx > es.level.player_start_x);
        int total = editor_placed_entity_total(&es.level);
        int count = es.level.checkpoint_count;
        es.palette_type = ENT_CHECKPOINT;
        es.tool = TOOL_PLACE;
        ui_frame(&es, cx, cy);
        click_frame(&es, cx, cy);
        CHECK(es.level.checkpoint_count == count + 1 && level_is_valid(&es));
        CHECK(editor_placed_entity_total(&es.level) == total + 1);
    }
    CHECK(es.modified == 1);

    /* Esc leaves the Place tool; the grid key toggles the overlay. */
    key_frame(&es, KEY_ESCAPE, 0);
    CHECK(es.tool == TOOL_SELECT);
    int grid = es.show_grid;
    key_frame(&es, KEY_G, 0);
    CHECK(es.show_grid == !grid);
    key_frame(&es, KEY_G, 0);
done:
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/* ------------------------------------------------------------------ */
/* Canvas                                                              */
/* ------------------------------------------------------------------ */

/* Undo and Redo say what they did in the status bar, and bring the entity
 * they changed back into view when the canvas has scrolled away from it. */
static int history_reports_and_reveals_the_step(void)
{
    int failed = 0;
    EditorState es;
    EditorRect r;
    CHECK(open_editor(&es, NULL) == 0);
    es.level.screen_count = 8;
    es.palette_type = ENT_COIN;
    key_frame(&es, KEY_TWO, 0);
    ui_frame(&es, 300, 300);
    click_frame(&es, 300, 300);
    CHECK(es.level.coin_count == 1);
    CHECK(editor_entity_bounds(&es.level, ENT_COIN, 0, &r) && canvas_shows(&es, &r));

    es.camera.x = 2000.0f;
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(!canvas_shows(&es, &r));
    key_frame(&es, KEY_Z, INPUT_CTRL);
    /* The coin is gone, so there is nothing to scroll to. */
    CHECK(es.level.coin_count == 0 && es.camera.x == 2000.0f);
    CHECK(strcmp(es.status_message, "Undid: placed Coin") == 0);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(strcmp(es.status_message, "Nothing to undo") == 0);
    key_frame(&es, KEY_Z, INPUT_CTRL | INPUT_SHIFT);
    CHECK(es.level.coin_count == 1);
    CHECK(strcmp(es.status_message, "Redid: placed Coin") == 0);
    CHECK(editor_entity_bounds(&es.level, ENT_COIN, 0, &r) && canvas_shows(&es, &r));
    /* Already in view: a step leaves the scroll alone. */
    float shown_x = es.camera.x;
    key_frame(&es, KEY_Z, INPUT_CTRL);
    key_frame(&es, KEY_Z, INPUT_CTRL | INPUT_SHIFT);
    CHECK(es.camera.x == shown_x);
    key_frame(&es, KEY_Z, INPUT_CTRL | INPUT_SHIFT);
    CHECK(strcmp(es.status_message, "Nothing to redo") == 0);
done:
    close_editor(&es);
    return failed;
}

/* A paste that includes the one Last Star moves it and places the rest as
 * one step. Undoing that step reports a change, not "placed", and shows
 * the star, the one entity the undo leaves in the level. */
static int mixed_step_reports_change_and_shows_survivor(void)
{
    int failed = 0;
    EditorState es;
    EditorRect r;
    CHECK(open_editor(&es, NULL) == 0);
    es.level.screen_count = 8;
    es.level.coin_count = 1;
    es.level.coins[0].x = 100.0f;
    es.level.coins[0].y = 200.0f;
    es.level.last_star = (LastStarPlacement){3000.0f, 200.0f};
    CHECK(undo_group_begin(es.undo, 2) != 0);
    Command place = {0};
    place.type = CMD_PLACE;
    place.entity_type = ENT_COIN;
    place.entity_index = 0;
    place.after.coin = es.level.coins[0];
    CHECK(undo_push(es.undo, &place));
    Command move = {0};
    move.type = CMD_MOVE;
    move.entity_type = ENT_LAST_STAR;
    move.before.last_star = (LastStarPlacement){2900.0f, 200.0f};
    move.after.last_star = es.level.last_star;
    CHECK(undo_push(es.undo, &move));
    undo_group_end(es.undo);
    es.camera.x = 0.0f;
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coin_count == 0 && es.level.last_star.x == 2900.0f);
    CHECK(strcmp(es.status_message, "Undid: changed 2 entities") == 0);
    CHECK(editor_entity_bounds(&es.level, ENT_LAST_STAR, 0, &r) && canvas_shows(&es, &r));
done:
    close_editor(&es);
    return failed;
}

/* The overview strip under the canvas spans the whole level: a click
 * centres the view there, a drag slides it, and the release lets go. */
static int minimap_jumps_and_drags_the_view(void)
{
    int failed = 0;
    EditorState es;
    CHECK(open_editor(&es, NULL) == 0);
    es.level.screen_count = 8;
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    uint64_t before = doc_hash(&es);
    float world_w = editor_world_width(&es.level);
    CHECK(editor_minimap_world_x(&es, EDITOR_MINIMAP_X) == 0.0f);
    CHECK(editor_minimap_world_x(&es, EDITOR_MINIMAP_X + EDITOR_MINIMAP_WIDTH) == world_w);
    CHECK(editor_minimap_contains(EDITOR_MINIMAP_X, EDITOR_MINIMAP_Y + EDITOR_MINIMAP_HEIGHT - 1) &&
          !editor_minimap_contains(EDITOR_MINIMAP_X, EDITOR_H - STATUS_H + 8));

    const int y = EDITOR_MINIMAP_Y + 3, mid = EDITOR_MINIMAP_X + EDITOR_MINIMAP_WIDTH / 2;
    float view_w = (float)CANVAS_W / es.camera.zoom;
    CHECK(es.camera.x == 0.0f);
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, 0, mid, y);
    ui_frame(&es, mid, y);
    CHECK(fabsf(es.camera.x + view_w / 2.0f - editor_minimap_world_x(&es, mid)) < 0.5f);
    float clicked_x = es.camera.x;
    push_event(INPUT_MOUSE_MOVE, 0, 0, mid + 100, y);
    ui_frame(&es, mid + 100, y);
    CHECK(es.minimap_dragging && es.camera.x > clicked_x);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, 0, mid + 100, y);
    ui_frame(&es, mid + 100, y);
    CHECK(!es.minimap_dragging);
    /* Released, the pointer passing over the strip moves nothing. */
    float dropped_x = es.camera.x;
    ui_frame(&es, EDITOR_MINIMAP_X, y);
    CHECK(es.camera.x == dropped_x);
    CHECK(doc_hash(&es) == before && es.level.coin_count == 0);
    /* A click pressed and released within one frame still jumps. */
    es.camera.x = 0.0f;
    click_frame(&es, mid, y);
    CHECK(es.camera.x > 0.0f && !es.minimap_dragging);
    /* A hand-edited level can hold any number: the strip clips it as a
     * float, before it becomes a pixel, and draws nothing outside itself. */
    int l, t, w, h;
    CHECK(editor_minimap_strip_rect(world_w, 0.0f, 0.0f, 1e30f, 1e30f, &l, &t, &w, &h) &&
          l == EDITOR_MINIMAP_X && t == EDITOR_MINIMAP_Y &&
          w == EDITOR_MINIMAP_WIDTH && h == EDITOR_MINIMAP_HEIGHT);
    CHECK(!editor_minimap_strip_rect(world_w, 1e30f, 1e30f, 16.0f, 16.0f, &l, &t, &w, &h));
    CHECK(!editor_minimap_strip_rect(world_w, -1e30f, -1e30f, 16.0f, 16.0f, &l, &t, &w, &h));
    CHECK(editor_minimap_strip_rect(world_w, -1e30f, -1e30f, INFINITY, INFINITY, &l, &t, &w, &h) &&
          l == EDITOR_MINIMAP_X && w == EDITOR_MINIMAP_WIDTH);
    CHECK(!editor_minimap_strip_rect(world_w, NAN, NAN, 16.0f, 16.0f, &l, &t, &w, &h));
    /* Inside the level, a small entity is still a pixel where it stands. */
    CHECK(editor_minimap_strip_rect(world_w, world_w / 2.0f, 0.0f, 1.0f, 1.0f, &l, &t, &w, &h) &&
          l == EDITOR_MINIMAP_X + EDITOR_MINIMAP_WIDTH / 2 && w == 1 && h == 1);
done:
    close_editor(&es);
    return failed;
}

/* F1 shows the shortcut list over the editor; while it is up, keys and
 * clicks change nothing beneath it, and F1, Esc or a click close it. The
 * toolbar's tooltips come from the same table. */
static int f1_sheet_lists_the_shortcuts(void)
{
    int failed = 0;
    EditorState es;
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(strcmp(editor_shortcut_tooltip(EDITOR_SHORTCUT_SAVE), "Save (Ctrl+S)") == 0);
    CHECK(strcmp(editor_shortcut_tooltip(EDITOR_SHORTCUT_PLAYTEST), "Playtest (F5; Shift+F5 from here)") == 0);
    CHECK(editor_shortcut_tooltip(EDITOR_SHORTCUT_UNDO) == NULL);
    CHECK(ui_text_width(&es.ui, "Undo") > 0);  /* the font measures, so the widths below mean something */
    for (int i = 0; i < EDITOR_SHORTCUT_COUNT; i++) {
        const EditorShortcut *s = editor_shortcut((EditorShortcutId)i);
        CHECK(s && s->keys && s->keys[0] && s->action && s->action[0]);
        /* Each column fits the 640 px panel (keys 190 px, then the action). */
        if (ui_text_width(&es.ui, s->keys) > 180 || ui_text_width(&es.ui, s->action) > 426)
            fprintf(stderr, "shortcut row too wide: %s / %s (%d px)\n", s->keys, s->action,
                    ui_text_width(&es.ui, s->action));
        CHECK(ui_text_width(&es.ui, s->keys) <= 180 && ui_text_width(&es.ui, s->action) <= 426);
    }

    key_frame(&es, KEY_F1, 0);
    CHECK(es.shortcut_sheet_open);
    /* Keys under the sheet do nothing: 2 would pick the Place tool. */
    key_frame(&es, KEY_TWO, 0);
    CHECK(es.tool == TOOL_SELECT && es.shortcut_sheet_open);
    key_frame(&es, KEY_ESCAPE, 0);
    CHECK(!es.shortcut_sheet_open);
    key_frame(&es, KEY_F1, 0);
    key_frame(&es, KEY_F1, 0);
    CHECK(!es.shortcut_sheet_open);
    /* A click closes it without placing, selecting or pressing anything. */
    key_frame(&es, KEY_TWO, 0);
    es.palette_type = ENT_COIN;
    key_frame(&es, KEY_F1, 0);
    click_frame(&es, 300, 300);
    CHECK(!es.shortcut_sheet_open && es.level.coin_count == 0 && !es.mouse_down);
    click_frame(&es, 300, 300);
    CHECK(es.level.coin_count == 1);
    /* Once closed, F1 and Esc are ordinary keys again: Esc leaves Place. */
    key_frame(&es, KEY_ESCAPE, 0);
    CHECK(es.tool == TOOL_SELECT);
done:
    close_editor(&es);
    return failed;
}

static int canvas_place_select_drag_delete_and_undo(void)
{
    int failed = 0;
    EditorState es;
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(es.level.coin_count == 0 && es.camera.zoom == 2.0f);

    /* Place a coin with the Place tool (key 2). */
    es.palette_type = ENT_COIN;
    key_frame(&es, KEY_TWO, 0);
    CHECK(es.tool == TOOL_PLACE);
    const int sx = 300, sy = 300;
    ui_frame(&es, sx, sy);
    click_frame(&es, sx, sy);
    CHECK(es.level.coin_count == 1 && es.undo->top == 1 && es.modified);
    /* A click outside the canvas never places. */
    click_frame(&es, CANVAS_W + 10, EDITOR_H - STATUS_H - 10);
    CHECK(es.level.coin_count == 1);

    /* Select it where it was placed (key 1 = Select tool). */
    key_frame(&es, KEY_ONE, 0);
    click_frame(&es, sx, sy);
    CHECK(es.selection.type == ENT_COIN && es.selection.index == 0);
    const float x0 = es.level.coins[0].x, y0 = es.level.coins[0].y;

    /* Drag it: 60 x 20 screen pixels at zoom 2 is 30 x 10 world pixels, and
     * the point that was grabbed stays under the cursor. */
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, 0, sx, sy);
    push_event(INPUT_MOUSE_MOVE, 0, 0, sx + 30, sy + 10);
    push_event(INPUT_MOUSE_MOVE, 0, 0, sx + 60, sy + 20);
    ui_frame(&es, sx + 60, sy + 20);
    /* Mid-drag, history keys are refused with an explanation. */
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(strstr(es.status_message, "Release the mouse") != NULL);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, 0, sx + 60, sy + 20);
    ui_frame(&es, sx + 60, sy + 20);
    CHECK(es.level.coins[0].x == x0 + 30.0f && es.level.coins[0].y == y0 + 10.0f);
    CHECK(es.undo->top == 2);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coins[0].x == x0 && es.level.coins[0].y == y0);
    key_frame(&es, KEY_Z, INPUT_CTRL | INPUT_SHIFT);   /* Ctrl+Shift+Z redoes */
    CHECK(es.level.coins[0].x == x0 + 30.0f);
    key_frame(&es, KEY_Z, INPUT_CTRL);

    /* Esc during a drag puts the coin back and records nothing. */
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, 0, sx, sy);
    push_event(INPUT_MOUSE_MOVE, 0, 0, sx + 80, sy);
    ui_frame(&es, sx + 80, sy);
    key_frame(&es, KEY_ESCAPE, 0);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, 0, sx + 80, sy);
    ui_frame(&es, sx + 80, sy);
    CHECK(es.level.coins[0].x == x0 && es.undo->top == 1);

    /* Copy and paste makes a second coin; undo takes it away again. */
    click_frame(&es, sx, sy);
    key_frame(&es, KEY_C, INPUT_CTRL);
    key_frame(&es, KEY_V, INPUT_CTRL);
    CHECK(es.level.coin_count == 2 && level_is_valid(&es));
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coin_count == 1);

    /* Three ways to delete, each undoable: Delete key, right-click, and
     * the Delete tool (key 3). */
    click_frame(&es, sx, sy);
    CHECK(es.selection.index == 0);
    key_frame(&es, KEY_DELETE, 0);
    CHECK(es.level.coin_count == 0 && es.selection.index < 0);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coin_count == 1);
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_RIGHT, 0, sx, sy);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_RIGHT, 0, sx, sy);
    ui_frame(&es, sx, sy);
    CHECK(es.level.coin_count == 0);
    key_frame(&es, KEY_Y, INPUT_CTRL);   /* redo of nothing: still deleted */
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coin_count == 1);
    key_frame(&es, KEY_THREE, 0);
    CHECK(es.tool == TOOL_DELETE);
    click_frame(&es, sx, sy);
    CHECK(es.level.coin_count == 0);
    key_frame(&es, KEY_ESCAPE, 0);
    CHECK(es.tool == TOOL_SELECT);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coin_count == 1);

    /* Esc with a selection clears it. */
    click_frame(&es, sx, sy);
    CHECK(es.selection.index == 0);
    key_frame(&es, KEY_ESCAPE, 0);
    CHECK(es.selection.index < 0);

    /* The wheel pans; Ctrl+wheel steps through the 1/2/3/5 zoom presets
     * around the cursor; Shift+wheel pans vertically when the world is
     * taller than the view. The camera never leaves the world. */
    push_wheel(400, 300, -2.0f, 0);
    ui_frame(&es, 400, 300);
    CHECK(es.camera.x == 2 * 48.0f / 2.0f);
    push_wheel(400, 300, 100.0f, 0);
    ui_frame(&es, 400, 300);
    CHECK(es.camera.x == 0.0f);
    /* Zoom stops at 5x and at 1x: it used to wrap from 5x to 1x. */
    static const float up[] = {3.0f, 5.0f, 5.0f};
    for (int i = 0; i < 3; i++) {
        push_wheel(400, 300, 1.0f, INPUT_CTRL);
        ui_frame(&es, 400, 300);
        CHECK(es.camera.zoom == up[i]);
    }
    static const float down[] = {3.0f, 2.0f, 1.0f, 1.0f};
    for (int i = 0; i < 4; i++) {
        push_wheel(400, 300, -1.0f, INPUT_CTRL);
        ui_frame(&es, 400, 300);
        CHECK(es.camera.zoom == down[i]);
    }
    /* A trackpad pinch sends fractions: four quarter notches make one step. */
    for (int i = 0; i < 3; i++) push_wheel(400, 300, 0.25f, INPUT_CTRL);
    ui_frame(&es, 400, 300);
    CHECK(es.camera.zoom == 1.0f);
    push_wheel(400, 300, 0.25f, INPUT_CTRL);
    ui_frame(&es, 400, 300);
    CHECK(es.camera.zoom == 2.0f);
    push_wheel(400, 300, -1.0f, INPUT_CTRL);
    ui_frame(&es, 400, 300);
    CHECK(es.camera.zoom == 1.0f);
    push_wheel(400, 300, 1.0f, INPUT_CTRL);
    push_wheel(400, 300, 1.0f, INPUT_CTRL);
    push_wheel(400, 300, 1.0f, INPUT_CTRL);
    ui_frame(&es, 400, 300);
    CHECK(es.camera.zoom == 5.0f);
    push_wheel(400, 300, -10.0f, INPUT_SHIFT);
    ui_frame(&es, 400, 300);
    CHECK(es.camera.y > 0.0f && es.camera.y <= GAME_H - CANVAS_H / 5.0f + 0.01f);
    ui_frame(&es, 400, 300);   /* draw the grid at 5x */
    for (int i = 0; i < 3; i++) push_wheel(400, 300, -1.0f, INPUT_CTRL);
    ui_frame(&es, 400, 300);
    CHECK(es.camera.zoom == 1.0f && es.camera.y == 0.0f);

    /* Typing into the selected coin's first field (x) and pressing Enter
     * moves it; undo restores it. The field is found by probing. */
    push_wheel(400, 300, 1.0f, INPUT_CTRL);
    ui_frame(&es, 400, 300);
    CHECK(es.camera.zoom == 2.0f);
    push_wheel(400, 300, 100.0f, 0);   /* pan back to the left edge */
    ui_frame(&es, 400, 300);
    CHECK(es.camera.x == 0.0f && es.camera.y == 0.0f);
    click_frame(&es, sx, sy);
    CHECK(es.selection.type == ENT_COIN && es.selection.index == 0);
    int field_y = -1;
    for (int y = EDITOR_H - STATUS_H - 200 + 30; y < EDITOR_H - STATUS_H && field_y < 0; y += 6) {
        click_frame(&es, PANEL_FIELD_X, y);
        if (es.ui.active_id) field_y = y;
    }
    CHECK(field_y > 0);
    for (int i = 0; i < 16; i++) key_frame(&es, KEY_BACKSPACE, 0);
    push_text("123");
    ui_frame(&es, PANEL_FIELD_X, field_y);
    int undo_top = es.undo->top;
    key_frame(&es, KEY_ENTER, 0);
    CHECK(es.ui.active_id == 0);
    CHECK(es.level.coins[0].x == 123.0f || es.level.coins[0].y == 123.0f);
    CHECK(es.undo->top == undo_top + 1);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coins[0].x == x0 && es.level.coins[0].y == y0);

    /* The properties header folds the panel; the canvas keeps the selection. */
    for (int y = EDITOR_H - STATUS_H - 200; y < EDITOR_H - STATUS_H - 200 + 24; y += 6) {
        click_frame(&es, PANEL_LABEL_X, y);
        if (!es.panel_open) break;
    }
    CHECK(es.panel_open == 0 && es.selection.index == 0);
    for (int y = EDITOR_H - STATUS_H - 30; y < EDITOR_H - STATUS_H; y += 4) {
        click_frame(&es, PANEL_LABEL_X, y);
        if (es.panel_open) break;
    }
    CHECK(es.panel_open == 1);

    /* Two full-size frames through editor_run_frame itself. */
    editor_run_frame(&es);
    editor_run_frame(&es);
    CHECK(es.running && es.validation_cache_valid);
done:
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/* ------------------------------------------------------------------ */
/* Properties and level config                                         */
/* ------------------------------------------------------------------ */

/*
 * Select the first entity of every type in a real level, draw its
 * properties, and click every row of the panel. Whatever a click does —
 * start a text edit, open a dropdown, flip a toggle — must leave the level
 * valid, and any change must be exactly one undo step.
 */
static int probe_properties_of_every_type(const char *level_path, int seen[ENT_COUNT],
                                          int *changes)
{
    int failed = 0;
    EditorState es;
    CHECK(open_editor(&es, level_path) == 0);
    const int panel_top = EDITOR_H - STATUS_H - 200;
    for (int type = 0; type < ENT_COUNT; type++) {
        if (editor_entity_count(&es.level, (EntityType)type) == 0) continue;
        es.selection.type = (EntityType)type;
        es.selection.index = 0;
        es.tool = TOOL_SELECT;
        ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
        CHECK(es.selection.index == 0);
        if (seen[type]) continue;   /* already probed in an earlier level */
        for (int y = panel_top + 36; y < EDITOR_H - STATUS_H; y += 24) {
            uint64_t before = doc_hash(&es);
            int undo_before = es.undo->top;
            click_frame(&es, PANEL_FIELD_X, y);
            CHECK(level_is_valid(&es));
            if (settle_probe(&es, PANEL_FIELD_X, y, before, undo_before, changes) != 0 ||
                !level_is_valid(&es)) {
                fprintf(stderr, "editor_ui_test: %s probe at y=%d broke undo\n",
                        level_path, y);
                CHECK(0);
            }
            es.selection.type = (EntityType)type;
            es.selection.index = 0;
        }
        seen[type] = 1;
    }
done:
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

static int properties_panel_handles_every_entity_type(void)
{
    int failed = 0, changes = 0;
    int seen[ENT_COUNT] = {0};
    CHECK(probe_properties_of_every_type("levels/02_lugio_02.toml", seen, &changes) == 0);
    CHECK(probe_properties_of_every_type("levels/00_sandbox_01.toml", seen, &changes) == 0);
    int types = 0;
    for (int i = 0; i < ENT_COUNT; i++) types += seen[i];
    CHECK(types >= 25);
    /* Toggles and dropdown picks really edit the document. */
    CHECK(changes >= 3);
done:
    return failed;
}

static int level_config_sections_resize_the_panel(void)
{
    int failed = 0, changes = 0;
    EditorState es;
    CHECK(open_editor(&es, "levels/00_sandbox_01.toml") == 0);
    CHECK(es.config_open == 1);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);

    /* Validation messages and recent files take rows of their own. */
    int base = editor_config_total_height(&es);
    es.recent_file_count = 2;
    CHECK(editor_config_total_height(&es) > base);
    es.recent_file_count = 0;
    int messages = es.validation_report.message_count;
    es.validation_report.message_count = messages + 3;
    CHECK(editor_config_total_height(&es) > base);
    es.validation_report.message_count = messages;
    CHECK(editor_config_total_height(&es) == base);

    /* Click down the visible config panel, scrolling it as we go. Section
     * headers open (the panel grows); fields edit or open lists. Every
     * change is one undo step and the level stays valid. */
    int grew = 0, max_height = base;
    for (int pass = 0; pass < 3; pass++) {
        for (int y = TOOLBAR_H + 30; y < TOOLBAR_H + (EDITOR_H - STATUS_H - TOOLBAR_H) / 2; y += 11) {
            for (int column = 0; column < 2; column++) {
                int x = column ? PANEL_FIELD_X : PANEL_LABEL_X;
                uint64_t before = doc_hash(&es);
                int undo_before = es.undo->top;
                int height = editor_config_total_height(&es);
                click_frame(&es, x, y);
                if (editor_config_total_height(&es) > height) grew = 1;
                if (editor_config_total_height(&es) > max_height)
                    max_height = editor_config_total_height(&es);
                CHECK(es.config_open == 1 || y < TOOLBAR_H + 28);
                CHECK(settle_probe(&es, x, y, before, undo_before, &changes) == 0);
                if (!es.config_open) click_frame(&es, PANEL_LABEL_X, TOOLBAR_H + 8);
            }
        }
        /* Scroll the config panel down for the next pass. */
        push_wheel(PANEL_LABEL_X, TOOLBAR_H + 100, -6.0f, 0);
        ui_frame(&es, PANEL_LABEL_X, TOOLBAR_H + 100);
    }
    CHECK(grew && max_height > base);
    CHECK(level_is_valid(&es));
    /* Scrolling and section toggles are view state, not document edits. */
    push_wheel(PANEL_LABEL_X, TOOLBAR_H + 100, 50.0f, 0);
    ui_frame(&es, PANEL_LABEL_X, TOOLBAR_H + 100);
    /* A trackpad's small fractions add up to a scroll instead of each
     * being cut to nothing: ten 0.1-notch swipes scroll one notch. */
    {
        /* The top edge of one field (found by its id), before and after. */
        int top_y = -1, scrolled_y = -1, field_id = 0;
        for (int y = TOOLBAR_H + 150; y < TOOLBAR_H + 300 && top_y < 0; y += 2) {
            click_frame(&es, PANEL_FIELD_X, y);
            if (es.ui.active_id) {
                top_y = y;
                field_id = es.ui.active_id;
            }
            key_frame(&es, KEY_ESCAPE, 0);
        }
        CHECK(top_y > 0);
        for (int i = 0; i < 10; i++) push_wheel(PANEL_LABEL_X, TOOLBAR_H + 100, -0.1f, 0);
        ui_frame(&es, PANEL_LABEL_X, TOOLBAR_H + 100);
        for (int y = top_y - 40; y <= top_y && scrolled_y < 0; y += 2) {
            click_frame(&es, PANEL_FIELD_X, y);
            if (es.ui.active_id == field_id) scrolled_y = y;
            key_frame(&es, KEY_ESCAPE, 0);
        }
        CHECK(scrolled_y > 0 && scrolled_y < top_y);
        push_wheel(PANEL_LABEL_X, TOOLBAR_H + 100, 50.0f, 0);
        ui_frame(&es, PANEL_LABEL_X, TOOLBAR_H + 100);
    }
    /* The header folds the whole panel to one row. */
    click_frame(&es, PANEL_LABEL_X, TOOLBAR_H + 8);
    CHECK(es.config_open == 0);
    click_frame(&es, PANEL_LABEL_X, TOOLBAR_H + 8);
    CHECK(es.config_open == 1);
done:
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/* ------------------------------------------------------------------ */
/* Keyboard editing of the selection                                   */
/* ------------------------------------------------------------------ */

/* Place a coin at screen (sx, sy) with the Place tool, then select it. */
static int place_and_select_coin(EditorState *es, int sx, int sy)
{
    int before = es->level.coin_count;
    es->palette_type = ENT_COIN;
    es->tool = TOOL_PLACE;
    ui_frame(es, sx, sy);
    click_frame(es, sx, sy);
    es->tool = TOOL_SELECT;
    click_frame(es, sx, sy);
    return es->level.coin_count == before + 1 && es->selection.type == ENT_COIN ? 0 : -1;
}

/*
 * The mouse cursor follows what is under it: the I-beam on a field, the
 * four-way arrows while a coin is dragged, the arrow elsewhere, and the
 * system cursor is touched only when that changes.  (A scrollbar drag's
 * up-down arrows are checked on a bare UIState below.)
 */
static int mouse_cursor_follows_fields_and_drags(void)
{
    int failed = 0;
    EditorState es;
    const int sx = 300, sy = 300;
    CHECK(open_editor(&es, NULL) == 0);

    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(es.ui.cursor_shown == MOUSE_CURSOR_DEFAULT);
    int changes = es.ui.cursor_changes;
    ui_frame(&es, CANVAS_W + 90, CFG_SCREENS_Y);
    CHECK(es.ui.cursor_shown == MOUSE_CURSOR_IBEAM);
    CHECK(es.ui.cursor_changes == changes + 1);
    ui_frame(&es, CANVAS_W + 92, CFG_SCREENS_Y + 2);   /* still on it */
    CHECK(es.ui.cursor_changes == changes + 1);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(es.ui.cursor_shown == MOUSE_CURSOR_DEFAULT && es.ui.cursor_changes == changes + 2);

    CHECK(place_and_select_coin(&es, sx, sy) == 0);
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, 0, sx, sy);
    push_event(INPUT_MOUSE_MOVE, 0, 0, sx + 30, sy + 10);
    ui_frame(&es, sx + 30, sy + 10);
    CHECK(es.dragging && es.ui.cursor_shown == MOUSE_CURSOR_RESIZE_ALL);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, 0, sx + 30, sy + 10);
    ui_frame(&es, sx + 30, sy + 10);
    CHECK(!es.dragging && es.ui.cursor_shown == MOUSE_CURSOR_DEFAULT);

    /* A dragged scrollbar thumb shows the up-down arrows. */
    UIState ui;
    UIScroll scroll = {0};
    ui_init(&ui, NULL);
    ui_begin_frame(&ui);
    ui.mouse_x = 2; ui.mouse_y = 10; ui.mouse_clicked = 1; ui.mouse_down = 1;
    (void)ui_scrollbar_input(&ui, &scroll, 0, 0, 5, 100, 100, 400);
    CHECK(ui.scroll_drag == &scroll);
    ui_apply_cursor(&ui);
    CHECK(ui.cursor_shown == MOUSE_CURSOR_RESIZE_NS);
    ui_begin_frame(&ui);
    ui.mouse_down = 0;
    (void)ui_scrollbar_input(&ui, &scroll, 0, 0, 5, 100, 100, 400);
    ui_apply_cursor(&ui);
    CHECK(ui.cursor_shown == MOUSE_CURSOR_DEFAULT && ui.cursor_changes == 2);
done:
    close_editor(&es);
    return failed;
}

/*
 * Arrow keys nudge the selection (1 px, Shift = 16 px).  A quick run of
 * nudges is one undo step; a pause starts a new one.  Backspace deletes
 * the selection like Delete (Mac laptops have no Delete key).
 */
static int arrow_keys_nudge_and_backspace_deletes(void)
{
    int failed = 0;
    EditorState es;
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(place_and_select_coin(&es, 300, 300) == 0);
    const int coin = es.selection.index;
    const float x0 = es.level.coins[coin].x, y0 = es.level.coins[coin].y;
    const int undo_top = es.undo->top;

    key_frame(&es, KEY_RIGHT, 0);
    key_frame(&es, KEY_RIGHT, 0);
    key_frame(&es, KEY_RIGHT, 0);
    key_frame(&es, KEY_DOWN, INPUT_SHIFT);
    CHECK(es.level.coins[coin].x == x0 + 3.0f && es.level.coins[coin].y == y0 + 16.0f);
    CHECK(es.undo->top == undo_top + 1);          /* one step for the run */
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coins[coin].x == x0 && es.level.coins[coin].y == y0);
    key_frame(&es, KEY_Y, INPUT_CTRL);
    CHECK(es.level.coins[coin].x == x0 + 3.0f && es.level.coins[coin].y == y0 + 16.0f);

    /* After a pause, the next nudge is a step of its own. */
    es.nudge_ms -= 2 * NUDGE_COALESCE_MS;
    key_frame(&es, KEY_LEFT, INPUT_SHIFT);
    key_frame(&es, KEY_UP, 0);
    CHECK(es.level.coins[coin].x == x0 + 3.0f - 16.0f && es.level.coins[coin].y == y0 + 15.0f);
    CHECK(es.undo->top == undo_top + 2);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coins[coin].x == x0 + 3.0f && es.level.coins[coin].y == y0 + 16.0f);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coins[coin].x == x0 && es.level.coins[coin].y == y0);

    /* A nudge never leaves the world: at the left edge nothing moves. */
    es.level.coins[coin].x = 0.0f;
    key_frame(&es, KEY_LEFT, 0);
    CHECK(es.level.coins[coin].x == 0.0f && strstr(es.status_message, "Nothing moved"));
    es.level.coins[coin].x = x0;

    /* Backspace deletes the selection; undo brings it back. */
    const int coins = es.level.coin_count;
    key_frame(&es, KEY_BACKSPACE, 0);
    CHECK(es.level.coin_count == coins - 1 && es.selection.index < 0);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coin_count == coins);

    /* After a field commit with Enter the panel has the focus: a Backspace
     * still meant for the field deletes nothing.  Delete still does, and a
     * click on the canvas gives Backspace back. */
    CHECK(place_and_select_coin(&es, 360, 300) == 0);
    click_frame(&es, CANVAS_W + 60, CFG_NAME_Y);
    CHECK(es.ui.active_id != 0);
    key_frame(&es, KEY_ENTER, 0);
    CHECK(es.ui.active_id == 0 && es.selection.index >= 0);
    const int before_backspace = es.level.coin_count;
    key_frame(&es, KEY_BACKSPACE, 0);
    CHECK(es.level.coin_count == before_backspace && es.selection.index >= 0);
    CHECK(strstr(es.status_message, "Backspace deletes on the canvas") != NULL);
    key_frame(&es, KEY_DELETE, 0);
    CHECK(es.level.coin_count == before_backspace - 1);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(place_and_select_coin(&es, 420, 300) == 0);
    key_frame(&es, KEY_BACKSPACE, 0);
    CHECK(es.level.coin_count == before_backspace);
done:
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/*
 * Ctrl+D copies the selection one paste offset along and selects the copy,
 * so repeating it lays out a row.  The clipboard is not touched, and the
 * one-per-level entities refuse.
 */
static int ctrl_d_duplicates_with_a_stepping_offset(void)
{
    int failed = 0;
    EditorState es;
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(place_and_select_coin(&es, 300, 300) == 0);
    const float x0 = es.level.coins[0].x, y0 = es.level.coins[0].y;
    const int undo_top = es.undo->top;

    key_frame(&es, KEY_D, INPUT_CTRL);
    CHECK(es.level.coin_count == 2 && es.selection.index == 1);
    CHECK(es.level.coins[1].x == x0 + 24.0f && es.level.coins[1].y == y0 + 24.0f);
    key_frame(&es, KEY_D, INPUT_CTRL);
    CHECK(es.level.coin_count == 3 && es.selection.index == 2);
    CHECK(es.level.coins[2].x == x0 + 48.0f && es.level.coins[2].y == y0 + 48.0f);
    CHECK(es.undo->top == undo_top + 2 && es.clipboard_count == 0 && level_is_valid(&es));
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coin_count == 2);

    /* The player spawn is unique: nothing is added. */
    es.selection.type = ENT_PLAYER_SPAWN;
    es.selection.index = 0;
    key_frame(&es, KEY_D, INPUT_CTRL);
    CHECK(strstr(es.status_message, "cannot be duplicated") != NULL);
    CHECK(es.undo->top == undo_top + 1);
done:
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/* The screen pixel showing world point (wx, wy): canvas_screen_to_world
 * backwards. */
static void world_to_screen(const EditorState *es, float wx, float wy, float *sx, float *sy)
{
    float zoom = es->camera.zoom > 0.0f ? es->camera.zoom : 1.0f;
    *sx = (wx - es->camera.x) * zoom;
    *sy = (wy - es->camera.y) * zoom + (float)TOOLBAR_H;
}

/*
 * Snap to grid (key S) applies to placing as well as dragging; Shift
 * inverts it for one click or drag.  The status bar shows whether it is on.
 */
static int snap_toggle_applies_to_placing_and_dragging(void)
{
    int failed = 0;
    EditorState es;
    float wx, wy;
    CHECK(open_editor(&es, NULL) == 0);
    /* The status-bar labels around the indicator do not overlap. */
    CHECK(8 + ui_text_width(&es.ui, "Mouse: (1600, 300)") < 150);
    CHECK(150 + ui_text_width(&es.ui, "Snap: off") < 228);
    CHECK(228 + ui_text_width(&es.ui, "Tool: Delete") < 330);

    CHECK(es.snap_to_grid == 0);
    key_frame(&es, KEY_S, 0);
    CHECK(es.snap_to_grid == 1 && strstr(es.status_message, "Snap to grid on"));

    /* A placement lands on the grid cell's corner... */
    es.palette_type = ENT_COIN;
    es.tool = TOOL_PLACE;
    const int sx = 317, sy = 251;
    canvas_screen_to_world(&es, sx, sy, &wx, &wy);
    CHECK(fmodf(wx, (float)TILE_SIZE) != 0.0f);
    ui_frame(&es, sx, sy);
    click_frame(&es, sx, sy);
    CHECK(es.level.coin_count == 1);
    CHECK(es.level.coins[0].x == floorf(wx / TILE_SIZE) * TILE_SIZE);
    CHECK(es.level.coins[0].y == floorf(wy / TILE_SIZE) * TILE_SIZE);
    /* ...unless Shift is held, which places freely. */
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, INPUT_SHIFT, sx, sy);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, INPUT_SHIFT, sx, sy);
    ui_frame(&es, sx, sy);
    CHECK(es.level.coin_count == 2 && es.level.coins[1].x == wx);

    /* A drag of the free coin snaps its corner onto the grid. */
    es.tool = TOOL_SELECT;
    int cx = sx + 2, cy = sy + 2;
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, 0, cx, cy);
    push_event(INPUT_MOUSE_MOVE, 0, 0, cx + 70, cy + 9);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, 0, cx + 70, cy + 9);
    ui_frame(&es, cx + 70, cy + 9);
    CHECK(es.selection.type == ENT_COIN);
    {
        const CoinPlacement *moved = &es.level.coins[es.selection.index];
        CHECK(fmodf(moved->x, (float)TILE_SIZE) == 0.0f);
        CHECK(fmodf(moved->y, (float)TILE_SIZE) == 0.0f);
    }

    /* Select: Shift pressed once the drag is under way moves freely, read
     * at each motion event; the position at release is the one kept. */
    {
        const int index = es.selection.index;
        float gx, gy, before_x = es.level.coins[index].x;
        world_to_screen(&es, es.level.coins[index].x + 4.0f,
                        es.level.coins[index].y + 4.0f, &gx, &gy);
        push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, 0, (int)gx, (int)gy);
        push_event(INPUT_MOUSE_MOVE, 0, INPUT_SHIFT, (int)gx + 31, (int)gy + 5);
        push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, INPUT_SHIFT, (int)gx + 31, (int)gy + 5);
        ui_frame(&es, (int)gx + 31, (int)gy + 5);
        CHECK(es.selection.index == index && es.level.coins[index].x != before_x);
        CHECK(fmodf(es.level.coins[index].x, (float)TILE_SIZE) != 0.0f);
        /* Shift at the press does not drag at all: it takes the coin out
         * of the selection. */
        before_x = es.level.coins[index].x;
        world_to_screen(&es, es.level.coins[index].x + 4.0f,
                        es.level.coins[index].y + 4.0f, &gx, &gy);
        push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, INPUT_SHIFT, (int)gx, (int)gy);
        push_event(INPUT_MOUSE_MOVE, 0, INPUT_SHIFT, (int)gx + 60, (int)gy);
        push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, INPUT_SHIFT, (int)gx + 60, (int)gy);
        ui_frame(&es, (int)gx + 60, (int)gy);
        CHECK(es.level.coins[index].x == before_x && editor_selection_count(&es) == 0);
    }

    key_frame(&es, KEY_S, 0);
    CHECK(es.snap_to_grid == 0);
done:
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/*
 * Overlapping entities: a click picks the topmost; Alt+click, or a second
 * click on the very same spot, steps to the next one underneath and wraps.
 */
static int alt_click_cycles_through_overlapping_entities(void)
{
    int failed = 0;
    EditorState es;
    const int sx = 300, sy = 300;
    CHECK(open_editor(&es, NULL) == 0);
    es.tool = TOOL_PLACE;
    es.palette_type = ENT_COIN;
    click_frame(&es, sx, sy);
    es.palette_type = ENT_STAR_YELLOW;            /* drawn above coins */
    click_frame(&es, sx, sy);
    CHECK(es.level.coin_count == 1 && es.level.star_yellow_count == 1);
    es.tool = TOOL_SELECT;

    click_frame(&es, sx + 2, sy + 2);
    CHECK(es.selection.type == ENT_STAR_YELLOW);
    CHECK(strstr(es.status_message, "1 of 2") != NULL);
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, INPUT_ALT, sx + 5, sy + 5);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, INPUT_ALT, sx + 5, sy + 5);
    ui_frame(&es, sx + 5, sy + 5);
    CHECK(es.selection.type == ENT_COIN && strstr(es.status_message, "2 of 2"));
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, INPUT_ALT, sx + 5, sy + 5);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, INPUT_ALT, sx + 5, sy + 5);
    ui_frame(&es, sx + 5, sy + 5);
    CHECK(es.selection.type == ENT_STAR_YELLOW);   /* wrapped to the top */

    /* A second plain click on the same spot steps down too... */
    click_frame(&es, sx + 5, sy + 5);
    CHECK(es.selection.type == ENT_COIN);
    /* ...but a click somewhere else starts again from the top. */
    click_frame(&es, sx + 3, sy + 1);
    CHECK(es.selection.type == ENT_STAR_YELLOW);
    /* Nothing changed in the document: picking is not an edit. */
    CHECK(es.undo->top == 2);
done:
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/*
 * Clickable validation messages (spec N-001): clicking a message selects
 * the entity it is about and pans the canvas to it, or focuses the Level
 * Config field.  The status-bar summary goes to the first message.
 */
static int validation_messages_take_you_to_the_problem(void)
{
    int failed = 0;
    EditorState es;
    /* First message row: below the Level Config title and its summary. */
    const int row_x = CANVAS_W + 60, row_y = TOOLBAR_H + 28 + 8 + 20 + 9;
    CHECK(open_editor(&es, NULL) == 0);
    es.level.screen_count = 20;
    es.level.coin_count = 3;
    for (int i = 0; i < 3; i++)
        es.level.coins[i] = (CoinPlacement){200.0f + 40.0f * (float)i, 100.0f};
    es.level.checkpoint_count = 1;
    es.level.checkpoints[0] = (CheckpointPlacement){6000.0f, 100.0f};
    CHECK(level_is_valid(&es));

    /* A checkpoint far off to the right goes out of the world. */
    es.level.screen_count = 4;
    es.camera.x = 0.0f;
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(es.validation_report.error_count > 0);
    CHECK(strcmp(es.validation_report.locations[0].path, "checkpoints") == 0);
    click_frame(&es, row_x, row_y);
    CHECK(es.selection.type == ENT_CHECKPOINT && es.selection.index == 0);
    CHECK(es.tool == TOOL_SELECT && es.camera.x > 0.0f);
    CHECK(strstr(es.status_message, "Checkpoint 0") != NULL);
    /* ...and its x field takes the caret (properties.c numbers entity
     * fields type * 100 + field + 1; x is the checkpoint's field 0). */
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(es.ui.active_id == (int)ENT_CHECKPOINT * 100 + 1);
    key_frame(&es, KEY_ESCAPE, 0);
    es.level.checkpoints[0].x = 900.0f;

    /* A bad config value focuses its field. */
    es.level.coin_score = -5;
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(strcmp(es.validation_report.locations[0].path, "coin_score") == 0);
    click_frame(&es, row_x, row_y);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(es.ui.active_id == 9012);
    key_frame(&es, KEY_ESCAPE, 0);
    es.level.coin_score = 10;

    /* The status-bar summary jumps to the first message's entity. */
    es.level.coins[2].y = 9999.0f;
    es.selection.index = -1;
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    click_frame(&es, 335, EDITOR_H - STATUS_H / 2);
    CHECK(es.selection.type == ENT_COIN && es.selection.index == 2);
    /* The next frame draws the coin's fields with y holding the caret. */
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(es.ui.active_id == (int)ENT_COIN * 100 + 2);
    key_frame(&es, KEY_ESCAPE, 0);
    es.level.coins[2].y = 100.0f;

    /* An enemy's bad vx focuses its vx field (spider field 1). */
    es.level.spider_count = 1;
    es.level.spiders[0] = (SpiderPlacement){300.0f, 0.0f, 250.0f, 350.0f, 0};
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(strcmp(es.validation_report.locations[0].field, "vx") == 0);
    CHECK(editor_focus_validation_issue(&es, 0) == 1);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(es.selection.type == ENT_SPIDER && es.ui.active_id == (int)ENT_SPIDER * 100 + 2);
    key_frame(&es, KEY_ESCAPE, 0);
    es.level.spiders[0].vx = 50.0f;

    /* A dropdown takes the focus by opening: the axe's mode (field 1)... */
    es.level.axe_trap_count = 1;
    es.level.axe_traps[0] = (AxeTrapPlacement){.pillar_x = 500.0f, .mode = (AxeTrapMode)9};
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(editor_focus_validation_issue(&es, 0) == 1);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(es.ui.dropdown_open_id == (int)ENT_AXE_TRAP * 100 + 2);
    click_frame(&es, NEUTRAL_X, NEUTRAL_Y);          /* closes the list */
    es.level.axe_trap_count = 0;

    /* ...and a Level Config list row: layer 1's asset dropdown unfolds
     * its group and opens, its speed field takes the caret. */
    g_plx_open = 0;
    es.level.background_layer_count = 2;
    snprintf(es.level.background_layers[0].path, sizeof(es.level.background_layers[0].path),
             "assets/sprites/backgrounds/sky_blue.png");
    snprintf(es.level.background_layers[1].path, sizeof(es.level.background_layers[1].path),
             "assets/sprites/backgrounds/sky.txt");
    es.level.background_layers[1].speed = 0.5f;
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(strcmp(es.validation_report.locations[0].path, "background_layers") == 0);
    CHECK(editor_focus_validation_issue(&es, 0) == 1);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(g_plx_open == 1 && es.ui.dropdown_open_id == 9201);
    click_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    {
        LevelIssueLocation speed = {"background_layers", 1, "speed"};
        CHECK(properties_focus_config(&es, &speed) == 1);
        ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
        CHECK(es.ui.active_id == 9101);
        key_frame(&es, KEY_ESCAPE, 0);
    }
done:
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/*
 * Every validation row can be reached: more rows than fit scroll with the
 * wheel over them, each one is clickable, and the row clicked last is
 * what Ctrl+C copies (until a click elsewhere).
 */
static int validation_rows_scroll_and_copy(void)
{
    int failed = 0;
    EditorState es;
    const int row_x = CANVAS_W + 60, row_y = TOOLBAR_H + 28 + 8 + 20 + 9;
    char first[EDITOR_VALIDATION_MESSAGE_LEN];
    CHECK(open_editor(&es, NULL) == 0);

    /* Twelve coins below the world: twelve errors, eight rows shown. */
    es.level.coin_count = 12;
    for (int i = 0; i < 12; i++)
        es.level.coins[i] = (CoinPlacement){100.0f + 30.0f * (float)i, 9999.0f};
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(es.validation_report.message_count == 12);
    CHECK(es.message_scroll.offset == 0);
    snprintf(first, sizeof(first), "%s", es.validation_report.messages[0]);

    /* The first row takes you to coin 0, and Ctrl+C copies the row. */
    click_frame(&es, row_x, row_y);
    CHECK(es.selection.type == ENT_COIN && es.selection.index == 0);
    CHECK(es.focus_area == EDITOR_FOCUS_MESSAGES);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    key_frame(&es, KEY_C, INPUT_CTRL);
    CHECK(strcmp(editor_test_clipboard(), first) == 0);
    CHECK(strncmp(es.status_message, "Copied: coins[0]", 16) == 0);
    /* Text selected in the field the row opened is the newer choice:
     * Ctrl+C now copies that, as Ctrl+X would cut it. */
    CHECK(es.ui.active_id != 0);
    key_frame(&es, KEY_A, INPUT_CTRL);
    key_frame(&es, KEY_C, INPUT_CTRL);
    CHECK(es.ui.edit_buf[0] != '\0');
    CHECK(strcmp(editor_test_clipboard(), es.ui.edit_buf) == 0);
    key_frame(&es, KEY_ESCAPE, 0);

    /* The wheel over the rows scrolls them, not the panel; the first row
     * shown is now message 4, and clicking it goes to coin 4. */
    push_wheel(row_x, row_y, -4.0f, 0);
    ui_frame(&es, row_x, row_y);
    CHECK(es.message_scroll.offset == 4);
    click_frame(&es, row_x, row_y);
    CHECK(es.selection.type == ENT_COIN && es.selection.index == 4);
    key_frame(&es, KEY_ESCAPE, 0);
    /* It stops at the last full window: rows 5-12. */
    push_wheel(row_x, row_y, -20.0f, 0);
    ui_frame(&es, row_x, row_y);
    CHECK(es.message_scroll.offset == 12 - EDITOR_VALIDATION_VISIBLE_ROWS);
    click_frame(&es, row_x, row_y + 18 * (EDITOR_VALIDATION_VISIBLE_ROWS - 1));
    CHECK(es.selection.type == ENT_COIN && es.selection.index == 11);
    key_frame(&es, KEY_ESCAPE, 0);

    /* A click on the canvas takes the focus away: Ctrl+C copies entities
     * again, not the row. */
    editor_test_set_clipboard("");
    click_frame(&es, NEUTRAL_X, TOOLBAR_H + 200);
    CHECK(es.focus_area == EDITOR_FOCUS_CANVAS);
    key_frame(&es, KEY_C, INPUT_CTRL);
    CHECK(editor_test_clipboard()[0] == '\0');

    /* Folded, Level Config draws no rows, so the wheel over that spot must
     * not be taken for the hidden list: the palette is there now. */
    CHECK(properties_message_list_contains(row_x, row_y));
    es.config_open = 0;
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(!properties_message_list_contains(row_x, row_y));
done:
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/* Write `text` to `path`; 0 on success. */
static int write_text_file(const char *path, const char *text)
{
    FILE *file = fopen(path, "wb");
    if (!file) return -1;
    if (fputs(text, file) == EOF) { fclose(file); return -1; }
    return fclose(file) == 0 ? 0 : -1;
}

/*
 * A file that will not open says why, in the Level Config panel: the TOML
 * syntax error with its line, or every runtime rule the file breaks with
 * the line of each value.  The document stays as it was, and clicking the
 * heading hides the list.
 */
static int files_that_will_not_open_list_why(void)
{
    int failed = 0;
    EditorState es;
    const char *broken = TEST_OUT "ui_broken_syntax.toml";
    const char *invalid = TEST_OUT "ui_invalid_rules.toml";
    /* The list starts the Level Config content, above the summary. */
    const int heading_x = CANVAS_W + 60, heading_y = TOOLBAR_H + 28 + 8 + 9;
    CHECK(open_editor(&es, NULL) == 0);
    uint64_t before = doc_hash(&es);

    /* Line 3 is not "key = value". */
    CHECK(write_text_file(broken,
                          "format_version = 1\n"
                          "name = \"Broken\"\n"
                          "screen_count 4\n") == 0);
    CHECK(editor_load_level(&es, broken) != 0);
    CHECK(doc_hash(&es) == before);
    CHECK(es.load_report.count == 1);
    CHECK(strncmp(es.load_report.messages[0], "line 3: TOML syntax", 19) == 0);
    CHECK(strstr(es.status_message, "line 3") != NULL);

    /* Well-formed TOML that breaks two runtime rules: both are listed,
     * each with the line of its value. */
    CHECK(write_text_file(invalid,
                          "format_version = 1\n"
                          "name = \"Invalid\"\n"
                          "screen_count = 4\n"
                          "\n"
                          "[[coins]]\n"
                          "x = 99999.0\n"
                          "y = 100.0\n"
                          "\n"
                          "[[spiders]]\n"
                          "x = 300.0\n"
                          "vx = 0.0\n"
                          "patrol_x0 = 250.0\n"
                          "patrol_x1 = 350.0\n"
                          "frame_index = 0\n") == 0);
    CHECK(editor_load_level(&es, invalid) != 0);
    CHECK(es.load_report.count == 2);
    CHECK(strncmp(es.load_report.messages[0], "line 11: spiders[0].vx", 22) == 0);
    CHECK(strncmp(es.load_report.messages[1], "line 6: coins[0].x", 18) == 0);
    CHECK(strstr(es.load_report.file, "ui_invalid_rules.toml") != NULL);

    /* The panel shows them; a row can be selected and copied... */
    CHECK(es.config_open == 1);
    int with_list = editor_config_total_height(&es);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    click_frame(&es, heading_x, heading_y + 18);
    CHECK(es.focus_area == EDITOR_FOCUS_MESSAGES && es.load_report.count == 2);
    key_frame(&es, KEY_C, INPUT_CTRL);
    CHECK(strncmp(editor_test_clipboard(), "line 11: spiders[0].vx", 22) == 0);
    /* ...and clicking the heading hides them. */
    click_frame(&es, heading_x, heading_y);
    CHECK(es.load_report.count == 0);
    CHECK(editor_config_total_height(&es) < with_list);
    CHECK(doc_hash(&es) == before);
done:
    clear_dialog_seams();
    close_editor(&es);
    (void)remove(broken);
    (void)remove(invalid);
    return failed;
}

/* A scratch game folder: levels/a, b, c chained and listed in order, and
 * levels/d not listed (see enter_ui_campaign). */
#define UI_CAMPAIGN_ROOT TEST_OUT "ui-campaign-root"
static int write_ui_campaign_level(const char *path, const char *name, const char *next)
{
    char text[512];
    snprintf(text, sizeof(text),
             "format_version = 1\nname = \"%s\"\nscreen_count = 4\n\n"
             "[last_star]\nx = 300.0\ny = 200.0\n%s%s%s", name,
             next[0] ? "next_phase = \"" : "", next, next[0] ? "\"\n" : "");
    return write_text_file(path, text);
}

/*
 * Write the scratch campaign afresh, so each test starts from the same
 * files whatever the one before it saved, and make it the working folder
 * (the view names levels relative to it, as the game does).  The folder
 * the test came from goes in cwd, to return to.  Returns 0, or -1 with the
 * working folder unchanged.
 */
static int enter_ui_campaign(char *cwd, size_t cwd_size)
{
    test_make_folder(UI_CAMPAIGN_ROOT);
    test_make_folder(UI_CAMPAIGN_ROOT "/levels");
    test_make_folder(UI_CAMPAIGN_ROOT "/levels/campaigns");
    if (write_ui_campaign_level(UI_CAMPAIGN_ROOT "/levels/a.toml", "Alpha", "levels/b.toml") ||
        write_ui_campaign_level(UI_CAMPAIGN_ROOT "/levels/b.toml", "Bravo", "levels/c.toml") ||
        write_ui_campaign_level(UI_CAMPAIGN_ROOT "/levels/c.toml", "Charlie", "") ||
        write_ui_campaign_level(UI_CAMPAIGN_ROOT "/levels/d.toml", "Delta", "") ||
        write_text_file(UI_CAMPAIGN_ROOT "/levels/campaigns/main.toml",
                        "format_version = 1\nlevels = [\"levels/a.toml\", "
                        "\"levels/b.toml\", \"levels/c.toml\"]\n"))
        return -1;
    if (test_working_folder(cwd, cwd_size) != 0) return -1;
    return test_change_folder(UI_CAMPAIGN_ROOT);
}

/* The view's rows as their files' letters, in order: "abc" for a.toml,
 * b.toml and c.toml ("" while the view is closed). */
static const char *campaign_rows(const EditorState *es)
{
    static char letters[16];
    const CampaignCatalog *view = editor_campaign_entries(es);
    size_t count = 0;

    for (; view && count < view->count && count < sizeof(letters) - 1; count++)
        letters[count] = view->levels[count].path[sizeof("levels/") - 1];
    letters[count] = '\0';
    return letters;
}

/* Drop `count` paths on the window and run the frame that sees them. */
static void drop_frame(EditorState *es, const char *const *paths, unsigned int count)
{
    editor_test_drop_files(paths, count);
    ui_frame(es, NEUTRAL_X, NEUTRAL_Y);
}

/*
 * A level file dropped on the window opens like a recent file: it asks
 * about unsaved changes, closes the Campaign view first (whose unsaved
 * edits need a second drop to discard), and anything but exactly one
 * .toml file is turned away with a status message.
 */
static int dropped_level_opens_like_a_recent_file(void)
{
    int failed = 0;
    EditorState es;
    char cwd[4096];
    int moved = 0;
    static char long_path[EDITOR_DROP_TEST_LONG + 1];
    const char *one[] = {"levels/a.toml"};
    /* No file of this name exists in any letter case: Windows and macOS
     * would open levels/b.toml for "levels/B.TOML". */
    const char *upper[] = {"levels/missing.TOML"};
    const char *two[] = {"levels/a.toml", "levels/b.toml"};
    const char *art[] = {"levels/a.png"};
    const char *delta[] = {"levels/d.toml"};
    const char *too_long[] = {long_path};
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(enter_ui_campaign(cwd, sizeof(cwd)) == 0);
    moved = 1;

    drop_frame(&es, one, 1);
    CHECK(strcmp(es.file_path, "levels/a.toml") == 0);
    CHECK(strcmp(es.level.name, "Alpha") == 0);
    CHECK(strstr(es.status_message, "Loaded levels/a.toml") != NULL);

    /* The extension is matched in any case: this one reached the loader,
     * which then could not find it. */
    drop_frame(&es, upper, 1);
    CHECK(strcmp(es.file_path, "levels/a.toml") == 0);
    CHECK(strstr(es.status_message, "Load failed") != NULL);

    /* Two files, or one that is not a level, change nothing. */
    drop_frame(&es, two, 2);
    CHECK(strcmp(es.status_message, "Drop one .toml level to open it") == 0);
    drop_frame(&es, art, 1);
    CHECK(strcmp(es.status_message, "Drop one .toml level to open it") == 0);
    CHECK(strcmp(es.file_path, "levels/a.toml") == 0);

    /* A path as long as raylib's buffer may have been cut short. */
    memset(long_path, 'x', sizeof(long_path) - 1);
    memcpy(long_path + sizeof(long_path) - 1 - 5, ".toml", 5);
    drop_frame(&es, too_long, 1);
    CHECK(strstr(es.status_message, "too long") != NULL);
    CHECK(strcmp(es.file_path, "levels/a.toml") == 0);

    /* Unsaved changes ask first: Cancel keeps them, Discard opens. */
    es.level.screen_count = 5;
    editor_refresh_dirty(&es);
    CHECK(es.modified);
    editor_test_set_discard_choice(0);
    drop_frame(&es, delta, 1);
    CHECK(strcmp(es.file_path, "levels/a.toml") == 0 && es.level.screen_count == 5);
    CHECK(strstr(es.status_message, "cancelled") != NULL);
    editor_test_set_discard_choice(1);
    drop_frame(&es, delta, 1);
    CHECK(strcmp(es.file_path, "levels/d.toml") == 0 && !es.modified);
    CHECK(strcmp(es.level.name, "Delta") == 0);

    /* The Campaign view closes for the dropped level... */
    CHECK(editor_campaign_open(&es, NULL) == 0 && es.campaign != NULL);
    drop_frame(&es, one, 1);
    CHECK(es.campaign == NULL && strcmp(es.file_path, "levels/a.toml") == 0);
    /* ...but with unsaved campaign edits only on the second drop. */
    CHECK(editor_campaign_open(&es, NULL) == 0);
    CHECK(editor_campaign_move(&es, 2, -1) == 0);
    drop_frame(&es, delta, 1);
    CHECK(es.campaign != NULL && strcmp(es.file_path, "levels/a.toml") == 0);
    CHECK(strstr(es.status_message, "drop the level again") != NULL);
    drop_frame(&es, delta, 1);
    CHECK(es.campaign == NULL && strcmp(es.file_path, "levels/d.toml") == 0);

    /* A playtest owns the editor until it ends. */
    es.playing = 1;
    editor_test_drop_files(one, 1);
    editor_handle_dropped_files(&es);
    es.playing = 0;
    CHECK(strcmp(es.file_path, "levels/d.toml") == 0);
    CHECK(strstr(es.status_message, "Playtest running") != NULL);
done:
    editor_test_drop_files(NULL, 0);
    editor_test_set_discard_choice(-1);
    if (moved && test_change_folder(cwd) != 0) failed = 1;
    close_editor(&es);
    return failed;
}

/*
 * A shipped level still names its old WAV loop: its bytes key players'
 * ghosts and Continue points.  The editor opens it with the QOA file the
 * game plays and without unsaved changes, so nothing asks to save it and
 * the file only changes (to the .qoa name) when someone edits and saves.
 */
static int shipped_wav_loop_opens_unmodified(void)
{
    int failed = 0;
    EditorState es;
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(editor_load_level(&es, "levels/01_lugio_01.toml") == 0);
    CHECK(strcmp(es.level.music_path, "assets/sounds/levels/lava.qoa") == 0);
    CHECK(!es.modified);
    editor_refresh_dirty(&es);
    CHECK(!es.modified);
done:
    close_editor(&es);
    return failed;
}

/* A second level dropped while the unsaved-changes prompt is up, then
 * Discard chosen in that prompt (Left from Save, Enter). */
static void drop_bravo_during_the_prompt(void)
{
    const char *bravo[] = {"levels/b.toml"};
    editor_test_drop_files(bravo, 1);
    push_key(KEY_LEFT, 0);
    push_key(KEY_ENTER, 0);
    dialog_choice_test_set_poll_hook(NULL);
}

/*
 * raylib lends its own list of dropped paths, and the next drop frees and
 * replaces it.  The unsaved-changes prompt polls the window every frame,
 * so a drop can land while it is up: the level dropped first still opens,
 * the list is not released twice, and the later drop opens next frame.
 */
static int drop_during_the_prompt_waits_its_turn(void)
{
    int failed = 0;
    EditorState es;
    char cwd[4096];
    int moved = 0;
    int misuses = editor_test_drop_misuses();
    const char *delta[] = {"levels/d.toml"};
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(enter_ui_campaign(cwd, sizeof(cwd)) == 0);
    moved = 1;
    CHECK(editor_load_level(&es, "levels/a.toml") == 0);

    es.level.screen_count = 5;
    editor_refresh_dirty(&es);
    CHECK(es.modified);
    editor_test_set_discard_choice(-1);   /* the real prompt answers */
    dialog_choice_test_set_poll_hook(drop_bravo_during_the_prompt);
    drop_frame(&es, delta, 1);
    CHECK(strcmp(es.file_path, "levels/d.toml") == 0 && !es.modified);
    CHECK(editor_test_drop_misuses() == misuses);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(strcmp(es.file_path, "levels/b.toml") == 0);
    CHECK(editor_test_drop_misuses() == misuses);
done:
    dialog_choice_test_set_poll_hook(NULL);
    editor_test_drop_files(NULL, 0);
    input_clear();
    if (moved && test_change_folder(cwd) != 0) failed = 1;
    close_editor(&es);
    return failed;
}

/* Click button `index` (a CampaignButton) of the Campaign view's bottom row. */
static void click_campaign_button(EditorState *es, int index)
{
    click_frame(es, CAMPAIGN_VIEW_X + index * CAMPAIGN_BUTTON_STEP + 10,
                CAMPAIGN_BUTTONS_Y + 10);
}

/*
 * The Campaign view as a designer uses it: Ctrl+M covers the canvas (a
 * click there places nothing), a row click and Up reorder, typing in a
 * name field renames the level, Link in order and Save write the files
 * the game's own loader then reads, and Esc returns to the level.
 */
static int campaign_view_reorders_renames_and_saves(void)
{
    int failed = 0;
    EditorState es;
    char cwd[4096];
    int moved = 0;
    CampaignCatalog reloaded = {0};
    const CampaignCatalog *view;
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(enter_ui_campaign(cwd, sizeof(cwd)) == 0);
    moved = 1;

    es.tool = TOOL_PLACE;
    es.palette_type = ENT_COIN;
    key_frame(&es, KEY_M, INPUT_CTRL);
    CHECK(es.campaign != NULL);
    view = editor_campaign_entries(&es);
    CHECK(view && view->count == 3);
    click_frame(&es, 400, 500);                      /* the canvas sleeps */
    CHECK(es.level.coin_count == 0);
    key_frame(&es, KEY_DELETE, 0);                   /* level keys too */
    CHECK(strstr(es.status_message, "Campaign view") != NULL);

    /* Select row 3 (c) and move it up. */
    click_frame(&es, CAMPAIGN_VIEW_X + 20, CAMPAIGN_ROWS_Y + 2 * CAMPAIGN_ROW_H + 8);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UP);
    CHECK(strcmp(view->levels[1].path, "levels/c.toml") == 0);
    CHECK(editor_campaign_problem(&es)[0] != '\0' || !view->levels[0].available);

    /* Rename the first level: the field holds its name, typing appends. */
    click_frame(&es, CAMPAIGN_NAME_X + 10, CAMPAIGN_ROWS_Y + 8);
    CHECK(es.ui.active_id == CAMPAIGN_NAME_FIELD_ID);
    push_text("!");
    key_frame(&es, KEY_ENTER, 0);
    CHECK(strcmp(view->levels[0].level.name, "Alpha!") == 0);
    CHECK(strcmp(view->levels[0].display_name, "Alpha!") == 0);

    /* Link in order, then Save. */
    click_campaign_button(&es, CAMPAIGN_BUTTON_LINK);
    CHECK(editor_campaign_problem(&es)[0] == '\0');
    click_campaign_button(&es, CAMPAIGN_BUTTON_SAVE);
    CHECK(strstr(es.status_message, "Campaign saved") != NULL);
    CHECK(campaign_catalog_load(CAMPAIGN_MANIFEST_PATH, &reloaded) == 0);
    CHECK(reloaded.count == 3 && strcmp(reloaded.levels[1].path, "levels/c.toml") == 0);
    CHECK(strcmp(reloaded.levels[0].display_name, "Alpha!") == 0);
    CHECK(reloaded.levels[0].available && reloaded.levels[1].available &&
          reloaded.levels[2].available);

    /* Ctrl+S saves the campaign even while a name is being typed (the
     * name is applied first), and never saves the level instead. */
    click_frame(&es, CAMPAIGN_NAME_X + 10, CAMPAIGN_ROWS_Y + CAMPAIGN_ROW_H + 8);
    CHECK(es.ui.active_id == CAMPAIGN_NAME_FIELD_ID + 1);
    push_text("?");
    key_frame(&es, KEY_S, INPUT_CTRL);
    CHECK(es.ui.active_id == 0);
    CHECK(strstr(es.status_message, "Campaign saved") != NULL);
    CHECK(campaign_catalog_load(CAMPAIGN_MANIFEST_PATH, &reloaded) == 0);
    CHECK(strcmp(reloaded.levels[1].display_name, "Charlie?") == 0);
    /* Esc first cancels a name being typed, then closes. */
    click_frame(&es, CAMPAIGN_NAME_X + 10, CAMPAIGN_ROWS_Y + 8);
    CHECK(es.ui.active_id == CAMPAIGN_NAME_FIELD_ID);
    key_frame(&es, KEY_ESCAPE, 0);
    CHECK(es.ui.active_id == 0 && es.campaign != NULL);

    /* A name typed but not applied is unsaved work: quitting only warns. */
    click_frame(&es, CAMPAIGN_NAME_X + 10, CAMPAIGN_ROWS_Y + 8);
    push_text("#");
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(editor_campaign_unsaved(&es) == 1);
    push_event(INPUT_QUIT, 0, 0, 0, 0);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(es.running && es.campaign != NULL);
    CHECK(strstr(es.status_message, "Unsaved campaign changes") != NULL);
    key_frame(&es, KEY_ESCAPE, 0);                   /* drop the typing */
    CHECK(editor_campaign_unsaved(&es) == 0);

    /* Revert (asked twice, like Close) puts back what is on disk. */
    click_frame(&es, CAMPAIGN_VIEW_X + 20, CAMPAIGN_ROWS_Y + 2 * CAMPAIGN_ROW_H + 8);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UP);  /* b above c... */
    CHECK(strcmp(view->levels[1].path, "levels/b.toml") == 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_REVERT);
    CHECK(strstr(es.status_message, "Revert again") != NULL);
    view = editor_campaign_entries(&es);
    CHECK(strcmp(view->levels[1].path, "levels/b.toml") == 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_REVERT);
    view = editor_campaign_entries(&es);
    CHECK(view && strcmp(view->levels[1].path, "levels/c.toml") == 0);
    CHECK(editor_campaign_unsaved(&es) == 0);
    CHECK(strstr(es.status_message, "reverted") != NULL);

    /* Esc goes back to the level. */
    key_frame(&es, KEY_ESCAPE, 0);
    CHECK(es.campaign == NULL);
    click_frame(&es, 400, 500);
    CHECK(es.level.coin_count == 1);
done:
    campaign_catalog_cleanup(&reloaded);
    if (moved && test_change_folder(cwd) != 0) failed = 1;
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/* Click row `row` of the Campaign view on its file name, which selects it. */
static void click_campaign_row(EditorState *es, int row)
{
    click_frame(es, CAMPAIGN_VIEW_X + 20, CAMPAIGN_ROWS_Y + row * CAMPAIGN_ROW_H + 8);
}

/* Click the name field of row `row`, which starts typing in it. */
static void click_campaign_name(EditorState *es, int row)
{
    click_frame(es, CAMPAIGN_NAME_X + 10, CAMPAIGN_ROWS_Y + row * CAMPAIGN_ROW_H + 8);
}

/*
 * The Campaign view has a history of its own: every edit made there (move,
 * remove, add, relink) is one step that Ctrl+Z takes back and Ctrl+Y or
 * Ctrl+Shift+Z makes again, and a new edit forgets what was undone.
 */
static int campaign_undo_and_redo_replay_each_edit(void)
{
    int failed = 0;
    EditorState es;
    char cwd[4096];
    int moved = 0;
    const CampaignCatalog *view;
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(enter_ui_campaign(cwd, sizeof(cwd)) == 0);
    moved = 1;
    key_frame(&es, KEY_M, INPUT_CTRL);
    view = editor_campaign_entries(&es);
    CHECK(view && strcmp(campaign_rows(&es), "abc") == 0);

    /* Nothing done yet: the keys say so and change nothing. */
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(strstr(es.status_message, "nothing to undo") != NULL);
    key_frame(&es, KEY_Y, INPUT_CTRL);
    CHECK(strstr(es.status_message, "nothing to redo") != NULL);
    CHECK(strcmp(campaign_rows(&es), "abc") == 0);

    /* Move: select c and press Up; each of the three keys does its part. */
    click_campaign_row(&es, 2);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UP);
    CHECK(strcmp(campaign_rows(&es), "acb") == 0);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(strcmp(campaign_rows(&es), "abc") == 0);
    CHECK(strstr(es.status_message, "Undid: moved levels/c.toml") != NULL);
    key_frame(&es, KEY_Y, INPUT_CTRL);
    CHECK(strcmp(campaign_rows(&es), "acb") == 0);
    CHECK(strstr(es.status_message, "Redid: moved levels/c.toml") != NULL);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    key_frame(&es, KEY_Z, INPUT_CTRL | INPUT_SHIFT);
    CHECK(strcmp(campaign_rows(&es), "acb") == 0);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(strcmp(campaign_rows(&es), "abc") == 0);
    CHECK(editor_campaign_undo_steps(&es) == 0 && editor_campaign_redo_steps(&es) == 1);

    /* Remove b: undo reads its file again and puts it back where it was. */
    click_campaign_row(&es, 1);
    click_campaign_button(&es, CAMPAIGN_BUTTON_REMOVE);
    CHECK(strcmp(campaign_rows(&es), "ac") == 0);
    CHECK(editor_campaign_redo_steps(&es) == 0);      /* a new edit: no redo */
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(strcmp(campaign_rows(&es), "abc") == 0);
    CHECK(view->levels[1].loaded && strcmp(view->levels[1].level.name, "Bravo") == 0);
    CHECK(strcmp(view->levels[1].level.next_phase, "levels/c.toml") == 0);
    CHECK(view->levels[0].available && view->levels[1].available && view->levels[2].available);
    key_frame(&es, KEY_Y, INPUT_CTRL);
    CHECK(strcmp(campaign_rows(&es), "ac") == 0);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(strcmp(campaign_rows(&es), "abc") == 0);

    /* Add d (the picker's answer, given straight to the view). */
    CHECK(editor_campaign_add(&es, "levels/d.toml") == 0);
    CHECK(strcmp(campaign_rows(&es), "abcd") == 0);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(strcmp(campaign_rows(&es), "abc") == 0);
    CHECK(editor_campaign_unsaved(&es) == 0);
    key_frame(&es, KEY_Y, INPUT_CTRL);
    CHECK(strcmp(campaign_rows(&es), "abcd") == 0);
    CHECK(view->levels[3].loaded && strcmp(view->levels[3].level.name, "Delta") == 0);

    /* Link in order must remember the old links to be undone.  Without
     * the memory for them it changes none, and records nothing. */
    editor_campaign_test_fail_history_allocation(1);
    click_campaign_button(&es, CAMPAIGN_BUTTON_LINK);
    editor_campaign_test_fail_history_allocation(0);
    CHECK(strstr(es.status_message, "Not linked: out of memory") != NULL);
    CHECK(view->levels[2].level.next_phase[0] == '\0');
    CHECK(editor_campaign_undo_steps(&es) == 1);
    /* With it, c is linked to d, and that is one step too. */
    click_campaign_button(&es, CAMPAIGN_BUTTON_LINK);
    CHECK(strcmp(view->levels[2].level.next_phase, "levels/d.toml") == 0);
    CHECK(editor_campaign_undo_steps(&es) == 2);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(view->levels[2].level.next_phase[0] == '\0');
    CHECK(!view->levels[2].available);
    key_frame(&es, KEY_Y, INPUT_CTRL);
    CHECK(strcmp(view->levels[2].level.next_phase, "levels/d.toml") == 0);
    CHECK(view->levels[2].available);

    /* A new edit after an undo forgets the undone step. */
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(editor_campaign_redo_steps(&es) == 1);
    click_campaign_row(&es, 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_DOWN);
    CHECK(strcmp(campaign_rows(&es), "bacd") == 0);
    CHECK(editor_campaign_redo_steps(&es) == 0);
    key_frame(&es, KEY_Y, INPUT_CTRL);
    CHECK(strstr(es.status_message, "nothing to redo") != NULL);
    CHECK(strcmp(campaign_rows(&es), "bacd") == 0);

    /* All the way back is the campaign the files hold. */
    key_frame(&es, KEY_Z, INPUT_CTRL);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(strcmp(campaign_rows(&es), "abc") == 0);
    CHECK(editor_campaign_undo_steps(&es) == 0);
    CHECK(editor_campaign_unsaved(&es) == 0);
done:
    editor_campaign_test_fail_history_allocation(0);
    if (moved && test_change_folder(cwd) != 0) failed = 1;
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/*
 * A rename is one step for each name applied, however many keys it took
 * and however it was applied (Enter, a click elsewhere, or the undo key
 * itself).  Typing alone is not a step.
 */
static int campaign_rename_is_one_undo_step(void)
{
    int failed = 0;
    EditorState es;
    char cwd[4096];
    int moved = 0;
    const CampaignCatalog *view;
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(enter_ui_campaign(cwd, sizeof(cwd)) == 0);
    moved = 1;
    key_frame(&es, KEY_M, INPUT_CTRL);
    view = editor_campaign_entries(&es);
    CHECK(view && view->count == 3);

    /* Three keystrokes in three frames, then Enter. */
    click_campaign_name(&es, 0);
    CHECK(es.ui.active_id == CAMPAIGN_NAME_FIELD_ID);
    push_text("1");
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    push_text("2");
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(editor_campaign_undo_steps(&es) == 0);
    push_text("3");
    key_frame(&es, KEY_ENTER, 0);
    CHECK(strcmp(view->levels[0].level.name, "Alpha123") == 0);
    CHECK(editor_campaign_undo_steps(&es) == 1);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(strcmp(view->levels[0].level.name, "Alpha") == 0);
    CHECK(strcmp(view->levels[0].display_name, "Alpha") == 0);
    CHECK(editor_campaign_unsaved(&es) == 0);
    key_frame(&es, KEY_Y, INPUT_CTRL);
    CHECK(strcmp(view->levels[0].level.name, "Alpha123") == 0);
    CHECK(strcmp(view->levels[0].display_name, "Alpha123") == 0);
    CHECK(editor_campaign_unsaved(&es) == 1);

    /* Enter on a name left as it was is no edit, so no step. */
    click_campaign_name(&es, 2);
    key_frame(&es, KEY_ENTER, 0);
    CHECK(es.ui.active_id == 0 && editor_campaign_undo_steps(&es) == 1);

    /* A click in another row's field applies the name being typed. */
    click_campaign_name(&es, 1);
    push_text("?");
    click_campaign_name(&es, 2);
    CHECK(es.ui.active_id == CAMPAIGN_NAME_FIELD_ID + 2);
    CHECK(strcmp(view->levels[1].level.name, "Bravo?") == 0);
    CHECK(editor_campaign_undo_steps(&es) == 2);

    /* Ctrl+Z while typing applies the typed name as a step and takes that
     * step back at once: the name is as before, and Ctrl+Y brings the
     * typed one. */
    push_text("#");
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.ui.active_id == 0);
    CHECK(strcmp(view->levels[2].level.name, "Charlie") == 0);
    CHECK(editor_campaign_undo_steps(&es) == 2 && editor_campaign_redo_steps(&es) == 1);
    key_frame(&es, KEY_Y, INPUT_CTRL);
    CHECK(strcmp(view->levels[2].level.name, "Charlie#") == 0);

    /* Each name comes back in turn, and none of this was a level edit. */
    key_frame(&es, KEY_Z, INPUT_CTRL);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(strcmp(view->levels[1].level.name, "Bravo") == 0);
    CHECK(strcmp(view->levels[0].level.name, "Alpha123") == 0);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(strcmp(view->levels[0].level.name, "Alpha") == 0);
    CHECK(editor_campaign_unsaved(&es) == 0);
    CHECK(es.undo->top == 0 && es.undo->redo_top == 0 && !es.modified);
done:
    if (moved && test_change_folder(cwd) != 0) failed = 1;
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/*
 * The history holds the latest CAMPAIGN_HISTORY_MAX steps: one edit more
 * drops the oldest, which can then no longer be undone.
 */
static int campaign_history_keeps_the_latest_steps(void)
{
    int failed = 0;
    EditorState es;
    char cwd[4096];
    int moved = 0;
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(enter_ui_campaign(cwd, sizeof(cwd)) == 0);
    moved = 1;
    key_frame(&es, KEY_M, INPUT_CTRL);
    CHECK(es.campaign != NULL);

    /* One move more than fits: b and c change places each time. */
    for (int i = 0; i < CAMPAIGN_HISTORY_MAX + 1; i++)
        CHECK(editor_campaign_move(&es, 1, +1) == 0);
    CHECK(strcmp(campaign_rows(&es), "acb") == 0);   /* an odd number of swaps */
    CHECK(editor_campaign_undo_steps(&es) == CAMPAIGN_HISTORY_MAX);

    /* Every step kept can be undone; the first move is beyond reach. */
    for (int i = 0; i < CAMPAIGN_HISTORY_MAX; i++)
        CHECK(editor_campaign_undo(&es) == 1);
    CHECK(editor_campaign_undo(&es) == 0);
    CHECK(strstr(es.status_message, "nothing to undo") != NULL);
    CHECK(strcmp(campaign_rows(&es), "acb") == 0);
    CHECK(editor_campaign_unsaved(&es) == 1);
    CHECK(editor_campaign_redo_steps(&es) == CAMPAIGN_HISTORY_MAX);
    for (int i = 0; i < CAMPAIGN_HISTORY_MAX; i++)
        CHECK(editor_campaign_redo(&es) == 1);
    CHECK(editor_campaign_redo(&es) == 0);
    CHECK(strcmp(campaign_rows(&es), "acb") == 0);
done:
    if (moved && test_change_folder(cwd) != 0) failed = 1;
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/*
 * A row that left the list comes back by reading its level file again.  A
 * name changed and not yet saved is in no file, so the step keeps it; and
 * while the file cannot be read, the step is refused and the list stays as
 * it is.
 */
static int campaign_undo_reads_the_level_file_again(void)
{
    int failed = 0;
    EditorState es;
    char cwd[4096];
    int moved = 0;
    const CampaignCatalog *view;
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(enter_ui_campaign(cwd, sizeof(cwd)) == 0);
    moved = 1;
    key_frame(&es, KEY_M, INPUT_CTRL);
    view = editor_campaign_entries(&es);
    CHECK(view && view->count == 3);

    /* Rename b without saving, then remove it. */
    click_campaign_name(&es, 1);
    push_text("2");
    key_frame(&es, KEY_ENTER, 0);
    CHECK(strcmp(view->levels[1].level.name, "Bravo2") == 0);
    click_campaign_row(&es, 1);
    click_campaign_button(&es, CAMPAIGN_BUTTON_REMOVE);
    CHECK(strcmp(campaign_rows(&es), "ac") == 0);

    /* Its file disappears: undo says so and changes nothing. */
    CHECK(remove("levels/b.toml") == 0);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(strstr(es.status_message, "Cannot undo: levels/b.toml no longer loads") != NULL);
    CHECK(strcmp(campaign_rows(&es), "ac") == 0);
    CHECK(editor_campaign_undo_steps(&es) == 2 && editor_campaign_redo_steps(&es) == 0);
    /* A file that is there but broken is refused the same way.  A second
     * refusal asks whether to skip the step; "Keep the step" (the
     * dialog's default, and the answer these frames give) changes nothing. */
    CHECK(write_text_file("levels/b.toml", "format_version = \n") == 0);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(strstr(es.status_message, "no longer loads") != NULL);
    CHECK(strcmp(campaign_rows(&es), "ac") == 0);
    CHECK(editor_campaign_undo_steps(&es) == 2 && editor_campaign_redo_steps(&es) == 0);

    /* The file is back: the same undo now works, and the row has the name
     * it showed, which the file never held. */
    CHECK(write_ui_campaign_level("levels/b.toml", "Bravo", "levels/c.toml") == 0);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(strcmp(campaign_rows(&es), "abc") == 0);
    CHECK(view->levels[1].loaded && strcmp(view->levels[1].level.name, "Bravo2") == 0);
    CHECK(editor_campaign_unsaved(&es) == 1);
    key_frame(&es, KEY_Z, INPUT_CTRL);               /* ...and then the rename */
    CHECK(strcmp(view->levels[1].level.name, "Bravo") == 0);
    CHECK(editor_campaign_unsaved(&es) == 0);

    /* Redoing an Add reads the file too. */
    CHECK(editor_campaign_add(&es, "levels/d.toml") == 0);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(remove("levels/d.toml") == 0);
    key_frame(&es, KEY_Y, INPUT_CTRL);
    CHECK(strstr(es.status_message, "Cannot redo: levels/d.toml no longer loads") != NULL);
    CHECK(strcmp(campaign_rows(&es), "abc") == 0);
    CHECK(editor_campaign_redo_steps(&es) == 1);
done:
    if (moved && test_change_folder(cwd) != 0) failed = 1;
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/* A status no command writes, to tell "the click did nothing" from a
 * command that ran and reported. */
#define UNTOUCHED_STATUS "status left alone"

/*
 * The Undo and Redo buttons do what Ctrl+Z and Ctrl+Y do, for every kind of
 * step, and say which step it was.  With nothing to undo or redo a button
 * is drawn disabled and a click on it does nothing at all.
 */
static int campaign_undo_and_redo_buttons_replay_each_edit(void)
{
    int failed = 0;
    EditorState es;
    char cwd[4096];
    int moved = 0;
    const CampaignCatalog *view;
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(enter_ui_campaign(cwd, sizeof(cwd)) == 0);
    moved = 1;
    key_frame(&es, KEY_M, INPUT_CTRL);
    view = editor_campaign_entries(&es);
    CHECK(view && strcmp(campaign_rows(&es), "abc") == 0);

    /* Nothing done yet: both buttons are disabled.  The keys would say
     * "nothing to undo"; a disabled button says nothing. */
    editor_set_status(&es, UNTOUCHED_STATUS);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    click_campaign_button(&es, CAMPAIGN_BUTTON_REDO);
    CHECK(strcmp(es.status_message, UNTOUCHED_STATUS) == 0);
    CHECK(strcmp(campaign_rows(&es), "abc") == 0);
    CHECK(editor_campaign_undo_steps(&es) == 0 && editor_campaign_redo_steps(&es) == 0);
    CHECK(editor_campaign_unsaved(&es) == 0);

    /* Move: c goes up, Undo brings it down, Redo takes it up again. */
    click_campaign_row(&es, 2);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UP);
    CHECK(strcmp(campaign_rows(&es), "acb") == 0);
    editor_set_status(&es, UNTOUCHED_STATUS);
    click_campaign_button(&es, CAMPAIGN_BUTTON_REDO);     /* still disabled */
    CHECK(strcmp(es.status_message, UNTOUCHED_STATUS) == 0);
    CHECK(strcmp(campaign_rows(&es), "acb") == 0);
    CHECK(editor_campaign_undo_steps(&es) == 1 && editor_campaign_redo_steps(&es) == 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(strcmp(campaign_rows(&es), "abc") == 0);
    CHECK(strcmp(es.status_message, "Undid: moved levels/c.toml") == 0);
    CHECK(editor_campaign_undo_steps(&es) == 0 && editor_campaign_redo_steps(&es) == 1);
    editor_set_status(&es, UNTOUCHED_STATUS);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);     /* disabled again */
    CHECK(strcmp(es.status_message, UNTOUCHED_STATUS) == 0);
    CHECK(strcmp(campaign_rows(&es), "abc") == 0);
    CHECK(editor_campaign_undo_steps(&es) == 0 && editor_campaign_redo_steps(&es) == 1);
    click_campaign_button(&es, CAMPAIGN_BUTTON_REDO);
    CHECK(strcmp(campaign_rows(&es), "acb") == 0);
    CHECK(strcmp(es.status_message, "Redid: moved levels/c.toml") == 0);
    /* The buttons and the keys walk the same history. */
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(strcmp(campaign_rows(&es), "abc") == 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_REDO);
    CHECK(strcmp(campaign_rows(&es), "acb") == 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(strcmp(campaign_rows(&es), "abc") == 0);

    /* Remove b. */
    click_campaign_row(&es, 1);
    click_campaign_button(&es, CAMPAIGN_BUTTON_REMOVE);
    CHECK(strcmp(campaign_rows(&es), "ac") == 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(strcmp(campaign_rows(&es), "abc") == 0);
    CHECK(strcmp(es.status_message, "Undid: removed levels/b.toml") == 0);
    CHECK(view->levels[1].loaded && strcmp(view->levels[1].level.name, "Bravo") == 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_REDO);
    CHECK(strcmp(campaign_rows(&es), "ac") == 0);
    CHECK(strcmp(es.status_message, "Redid: removed levels/b.toml") == 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(editor_campaign_unsaved(&es) == 0);

    /* Add d (the picker's answer, given straight to the view). */
    CHECK(editor_campaign_add(&es, "levels/d.toml") == 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(strcmp(campaign_rows(&es), "abc") == 0);
    CHECK(strcmp(es.status_message, "Undid: added levels/d.toml") == 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_REDO);
    CHECK(strcmp(campaign_rows(&es), "abcd") == 0);
    CHECK(strcmp(es.status_message, "Redid: added levels/d.toml") == 0);

    /* Link in order points c at d. */
    click_campaign_button(&es, CAMPAIGN_BUTTON_LINK);
    CHECK(strcmp(view->levels[2].level.next_phase, "levels/d.toml") == 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(view->levels[2].level.next_phase[0] == '\0');
    CHECK(strcmp(es.status_message, "Undid: linked the levels in order") == 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_REDO);
    CHECK(strcmp(view->levels[2].level.next_phase, "levels/d.toml") == 0);
    CHECK(strcmp(es.status_message, "Redid: linked the levels in order") == 0);

    /* Rename a. */
    click_campaign_name(&es, 0);
    push_text("!");
    key_frame(&es, KEY_ENTER, 0);
    CHECK(strcmp(view->levels[0].level.name, "Alpha!") == 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(strcmp(view->levels[0].level.name, "Alpha") == 0);
    CHECK(strcmp(es.status_message, "Undid: renamed levels/a.toml") == 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_REDO);
    CHECK(strcmp(view->levels[0].level.name, "Alpha!") == 0);
    CHECK(strcmp(es.status_message, "Redid: renamed levels/a.toml") == 0);

    /* A click on Undo while a name is being typed applies the name as a
     * step and takes that step back, as Ctrl+Z does; Redo returns it. */
    click_campaign_name(&es, 1);
    push_text("#");
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(es.ui.active_id == CAMPAIGN_NAME_FIELD_ID + 1);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(es.ui.active_id == 0);
    CHECK(strcmp(view->levels[1].level.name, "Bravo") == 0);
    CHECK(editor_campaign_redo_steps(&es) == 1);
    click_campaign_button(&es, CAMPAIGN_BUTTON_REDO);
    CHECK(strcmp(view->levels[1].level.name, "Bravo#") == 0);

    /* The same click with an empty history: a typed name is work for Undo,
     * so the button is not disabled while one is being typed. */
    click_campaign_button(&es, CAMPAIGN_BUTTON_REVERT);
    click_campaign_button(&es, CAMPAIGN_BUTTON_REVERT);
    view = editor_campaign_entries(&es);
    CHECK(view && editor_campaign_undo_steps(&es) == 0);
    click_campaign_name(&es, 0);
    push_text("?");
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(es.ui.active_id == 0 && strcmp(view->levels[0].level.name, "Alpha") == 0);
    CHECK(editor_campaign_undo_steps(&es) == 0 && editor_campaign_redo_steps(&es) == 1);
    click_campaign_button(&es, CAMPAIGN_BUTTON_REDO);
    CHECK(strcmp(view->levels[0].level.name, "Alpha?") == 0);

    /* None of this touched the level's own history. */
    CHECK(es.undo->top == 0 && es.undo->redo_top == 0 && !es.modified);
done:
    if (moved && test_change_folder(cwd) != 0) failed = 1;
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/*
 * An Undo refused because the removed level's file no longer loads can be
 * skipped, but only by answering "Skip this step" to a question the second
 * refusal asks: the dialog's default ("Keep the step", what Enter chooses)
 * and Cancel leave the history alone.  The skipped step is named, the list
 * stays as it is, and
 * the steps before it then undo and redo what they did to the other rows:
 * their row numbers no longer count the level that is gone for good.
 */
static int campaign_refused_undo_can_be_skipped(void)
{
    int failed = 0;
    EditorState es;
    char cwd[4096];
    int moved = 0;
    const CampaignCatalog *view;
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(enter_ui_campaign(cwd, sizeof(cwd)) == 0);
    moved = 1;
    key_frame(&es, KEY_M, INPUT_CTRL);
    view = editor_campaign_entries(&es);
    CHECK(view && strcmp(campaign_rows(&es), "abc") == 0);

    /* Seven steps.  c is the level that will be lost: two of the steps
     * act on it alone, the others on rows above and below it. */
    CHECK(editor_campaign_add(&es, "levels/d.toml") == 0);      /* a b c d */
    click_campaign_row(&es, 2);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UP);             /* a c b d */
    click_campaign_name(&es, 1);
    push_text("2");
    key_frame(&es, KEY_ENTER, 0);                               /* Charlie2 */
    click_campaign_name(&es, 3);
    push_text("2");
    key_frame(&es, KEY_ENTER, 0);                               /* Delta2 */
    click_campaign_row(&es, 3);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UP);             /* a c d b */
    click_campaign_button(&es, CAMPAIGN_BUTTON_LINK);           /* a>c>d>b */
    click_campaign_row(&es, 1);
    click_campaign_button(&es, CAMPAIGN_BUTTON_REMOVE);         /* a d b */
    CHECK(strcmp(campaign_rows(&es), "adb") == 0);
    CHECK(editor_campaign_undo_steps(&es) == 7);
    CHECK(strcmp(view->levels[0].level.next_phase, "levels/c.toml") == 0);
    CHECK(editor_campaign_undo_blocked(&es) == 0);

    /* The file goes.  The first Undo only says so, however it is given. */
    CHECK(remove("levels/c.toml") == 0);
    skip_step_answer = EDITOR_SKIP_STEP_SKIP;   /* not asked yet: must not matter */
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(strstr(es.status_message, "Cannot undo: levels/c.toml no longer loads") != NULL);
    CHECK(strstr(es.status_message, "Undo again to skip this step") != NULL);
    CHECK(editor_campaign_undo_blocked(&es) == 1);
    CHECK(strcmp(campaign_rows(&es), "adb") == 0 && editor_campaign_undo_steps(&es) == 7);

    /* The second one asks.  The default answer, "Keep the step" (Enter
     * in the dialog), changes nothing, by button or by key, however often;
     * neither does Cancel, or closing the dialog. */
    skip_step_answer = EDITOR_SKIP_STEP_KEEP;
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(strstr(es.status_message, "Undo step kept: levels/c.toml") != NULL);
    CHECK(editor_campaign_undo_blocked(&es) == 1);
    CHECK(strcmp(campaign_rows(&es), "adb") == 0 && editor_campaign_undo_steps(&es) == 7);
    editor_set_status(&es, UNTOUCHED_STATUS);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(strstr(es.status_message, "Undo step kept: levels/c.toml") != NULL);
    skip_step_answer = EDITOR_SKIP_STEP_CANCEL;
    editor_set_status(&es, UNTOUCHED_STATUS);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(strstr(es.status_message, "Undo step kept: levels/c.toml") != NULL);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(editor_campaign_undo_blocked(&es) == 1);
    CHECK(strcmp(campaign_rows(&es), "adb") == 0);
    CHECK(editor_campaign_undo_steps(&es) == 7 && editor_campaign_redo_steps(&es) == 0);
    CHECK(strcmp(view->levels[1].level.name, "Delta2") == 0);

    /* "Skip this step" drops the Remove, and with it the two steps that
     * only changed c: its move up and its rename.  The status names what
     * was skipped; the list on screen is as it was. */
    skip_step_answer = EDITOR_SKIP_STEP_SKIP;
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(strcmp(es.status_message, "Skipped: removed levels/c.toml (it stays out of the "
                 "campaign), and 2 older steps that changed nothing else") == 0);
    CHECK(editor_campaign_undo_blocked(&es) == 0);
    CHECK(strcmp(campaign_rows(&es), "adb") == 0);
    CHECK(editor_campaign_undo_steps(&es) == 4 && editor_campaign_redo_steps(&es) == 0);
    CHECK(strcmp(view->levels[0].level.next_phase, "levels/c.toml") == 0);
    CHECK(strcmp(view->levels[1].level.name, "Delta2") == 0);

    /* The relink, now of three rows: each gets back the link it had. */
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(strcmp(es.status_message, "Undid: linked the levels in order") == 0);
    CHECK(strcmp(campaign_rows(&es), "adb") == 0);
    CHECK(strcmp(view->levels[0].level.next_phase, "levels/b.toml") == 0);
    CHECK(view->levels[1].level.next_phase[0] == '\0');
    CHECK(strcmp(view->levels[2].level.next_phase, "levels/c.toml") == 0);
    /* Redo writes the links the relink wrote then: a pointed at c, and
     * that is what the list showed before this Undo, c gone or not. */
    click_campaign_button(&es, CAMPAIGN_BUTTON_REDO);
    CHECK(strcmp(view->levels[0].level.next_phase, "levels/c.toml") == 0);
    CHECK(strcmp(view->levels[1].level.next_phase, "levels/b.toml") == 0);
    CHECK(view->levels[2].level.next_phase[0] == '\0');
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);

    /* d's move up: it was row 4 going to row 3, and is now row 3 to 2. */
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(strcmp(es.status_message, "Undid: moved levels/d.toml") == 0);
    CHECK(strcmp(campaign_rows(&es), "abd") == 0);
    /* d's rename, one row higher than when it was typed. */
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(strcmp(es.status_message, "Undid: renamed levels/d.toml") == 0);
    CHECK(strcmp(view->levels[2].level.name, "Delta") == 0);
    CHECK(strcmp(view->levels[0].level.name, "Alpha") == 0 &&
          strcmp(view->levels[1].level.name, "Bravo") == 0);
    /* d's Add: all the way back is the campaign as it was, without c. */
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(strcmp(es.status_message, "Undid: added levels/d.toml") == 0);
    CHECK(strcmp(campaign_rows(&es), "ab") == 0);
    CHECK(editor_campaign_undo_steps(&es) == 0 && editor_campaign_redo_steps(&es) == 4);
    CHECK(editor_campaign_unsaved(&es) == 1);        /* the file lists c */

    /* And forwards again, to exactly the list that was on screen. */
    for (int i = 0; i < 4; i++) click_campaign_button(&es, CAMPAIGN_BUTTON_REDO);
    CHECK(strcmp(campaign_rows(&es), "adb") == 0);
    CHECK(strcmp(view->levels[1].level.name, "Delta2") == 0);
    CHECK(strcmp(view->levels[0].level.next_phase, "levels/c.toml") == 0);
    CHECK(strcmp(view->levels[1].level.next_phase, "levels/b.toml") == 0);
    CHECK(view->levels[2].level.next_phase[0] == '\0');
    CHECK(editor_campaign_redo_steps(&es) == 0);

    /* A level that was added and removed inside the history leaves it
     * altogether, and an older Remove puts its row back where it belongs.
     * Start again from the files. */
    CHECK(write_ui_campaign_level("levels/c.toml", "Charlie", "") == 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_REVERT);
    click_campaign_button(&es, CAMPAIGN_BUTTON_REVERT);
    view = editor_campaign_entries(&es);
    CHECK(view && strcmp(campaign_rows(&es), "abc") == 0);
    CHECK(editor_campaign_move(&es, 1, +1) == 0);               /* a c b */
    CHECK(editor_campaign_add(&es, "levels/d.toml") == 0);      /* a c b d */
    CHECK(editor_campaign_remove(&es, 0) == 0);                 /* c b d */
    CHECK(editor_campaign_move(&es, 2, -1) == 0);               /* c d b */
    CHECK(editor_campaign_remove(&es, 1) == 0);                 /* c b */
    CHECK(remove("levels/d.toml") == 0);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(editor_campaign_undo_blocked(&es) == 1);
    skip_step_answer = EDITOR_SKIP_STEP_SKIP;
    key_frame(&es, KEY_Z, INPUT_CTRL);               /* the key asks as well */
    CHECK(strstr(es.status_message, "Skipped: removed levels/d.toml") != NULL);
    CHECK(strstr(es.status_message, "2 older steps") != NULL);
    CHECK(strcmp(campaign_rows(&es), "cb") == 0 && editor_campaign_undo_steps(&es) == 2);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(strcmp(es.status_message, "Undid: removed levels/a.toml") == 0);
    CHECK(strcmp(campaign_rows(&es), "acb") == 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(strcmp(campaign_rows(&es), "abc") == 0);
    CHECK(editor_campaign_unsaved(&es) == 0);        /* d was never saved */

    /* Skipping is offered for a step refused twice in a row only: once
     * the file is back, the very same Undo works and nothing is asked. */
    CHECK(editor_campaign_remove(&es, 2) == 0);                 /* a b */
    CHECK(editor_campaign_remove(&es, 0) == 0);                 /* b */
    CHECK(remove("levels/a.toml") == 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(editor_campaign_undo_blocked(&es) == 1);
    CHECK(write_ui_campaign_level("levels/a.toml", "Alpha", "levels/b.toml") == 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);           /* answer: skip */
    CHECK(strcmp(es.status_message, "Undid: removed levels/a.toml") == 0);
    CHECK(strcmp(campaign_rows(&es), "ab") == 0);
    CHECK(editor_campaign_undo_blocked(&es) == 0);
    /* A new edit also ends the blocked state: the newest step is then
     * another one, and that one has not been refused. */
    CHECK(editor_campaign_remove(&es, 0) == 0);                 /* b */
    CHECK(remove("levels/a.toml") == 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(editor_campaign_undo_blocked(&es) == 1);
    CHECK(editor_campaign_add(&es, "levels/d.toml") == -1);     /* d is gone */
    CHECK(editor_campaign_undo_blocked(&es) == 1);
    CHECK(editor_campaign_move(&es, 0, +1) == -1);              /* one row */
    CHECK(editor_campaign_undo_blocked(&es) == 1);
    click_campaign_name(&es, 0);
    push_text("!");
    key_frame(&es, KEY_ENTER, 0);
    CHECK(editor_campaign_undo_blocked(&es) == 0);
    /* The lost row was above the one an older Remove took out, so that
     * Remove now puts c back one row higher. */
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);           /* the rename */
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);           /* refused */
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);           /* skipped */
    CHECK(strcmp(es.status_message,
                 "Skipped: removed levels/a.toml (it stays out of the campaign)") == 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(strcmp(es.status_message, "Undid: removed levels/c.toml") == 0);
    CHECK(strcmp(campaign_rows(&es), "bc") == 0);
    CHECK(editor_campaign_undo_steps(&es) == 0);
done:
    if (moved && test_change_folder(cwd) != 0) failed = 1;
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/* The scratch campaign's level files: what enter_ui_campaign writes. */
static const struct {
    const char *path, *name, *next;
} ui_campaign_files[] = {
    {"levels/a.toml", "Alpha",   "levels/b.toml"},
    {"levels/b.toml", "Bravo",   "levels/c.toml"},
    {"levels/c.toml", "Charlie", ""},
    {"levels/d.toml", "Delta",   ""},
};
#define UI_CAMPAIGN_FILES ((int)(sizeof(ui_campaign_files) / sizeof(ui_campaign_files[0])))

/* The view's list at one moment, to compare with another moment. */
typedef struct {
    int count;
    struct {
        char file;                          /* 'a' for levels/a.toml */
        char name[LEVEL_NAME_CAPACITY];
        char next[sizeof(((LevelDef *)0)->next_phase)];
    } rows[UI_CAMPAIGN_FILES];
} CampaignSnapshot;

static void take_campaign_snapshot(const EditorState *es, CampaignSnapshot *shot)
{
    const CampaignCatalog *view = editor_campaign_entries(es);

    memset(shot, 0, sizeof(*shot));
    for (size_t i = 0; view && i < view->count && i < (size_t)UI_CAMPAIGN_FILES; i++) {
        shot->rows[i].file = view->levels[i].path[sizeof("levels/") - 1];
        memcpy(shot->rows[i].name, view->levels[i].level.name, sizeof(shot->rows[i].name));
        memcpy(shot->rows[i].next, view->levels[i].level.next_phase,
               sizeof(shot->rows[i].next));
        shot->count++;
    }
}

/* The same list with the row of level `file` taken out (if it has one). */
static void snapshot_without(const CampaignSnapshot *shot, char file, CampaignSnapshot *out)
{
    memset(out, 0, sizeof(*out));
    for (int i = 0; i < shot->count; i++)
        if (shot->rows[i].file != file) out->rows[out->count++] = shot->rows[i];
}

static int same_snapshot(const CampaignSnapshot *a, const CampaignSnapshot *b)
{
    if (a->count != b->count) return 0;
    for (int i = 0; i < a->count; i++)
        if (a->rows[i].file != b->rows[i].file ||
            strcmp(a->rows[i].name, b->rows[i].name) != 0 ||
            strcmp(a->rows[i].next, b->rows[i].next) != 0) return 0;
    return 1;
}

/* Does the list have a row for level `file`? */
static int snapshot_lists(const CampaignSnapshot *shot, char file)
{
    for (int i = 0; i < shot->count; i++)
        if (shot->rows[i].file == file) return 1;
    return 0;
}

/*
 * The next number below `limit` from a small generator (the constants are
 * Numerical Recipes'; its high bits are the usable ones).  Unlike rand(), it
 * gives the same numbers on every system, so a failing trial fails everywhere.
 */
static int next_random(uint32_t *seed, int limit)
{
    *seed = *seed * 1664525u + 1013904223u;
    return (int)((*seed >> 16) % (uint32_t)limit);
}

/*
 * Skipping a refused Undo rewrites the older steps, so check the rule they
 * must obey against many histories rather than a few chosen ones.
 *
 * Each trial makes a run of edits picked by a small fixed generator (the
 * same on every machine and every run), then removes a level, loses its
 * file and skips the refused Undo.  From there, every Undo must show the
 * list that was on screen one edit earlier, minus the lost level's row
 * (from the edit that brought the level in, if the history holds it); an
 * edit that only changed that row must be gone from the history; and
 * redoing everything must end on exactly the list the skip left.
 */
static int campaign_skipped_step_leaves_a_consistent_history(void)
{
    enum { TRIALS = 120, MAX_EDITS = 10 };
    int failed = 0;
    EditorState es;
    char cwd[4096];
    int moved = 0;
    int trial = 0;
    uint32_t seed = 20261010u;
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(enter_ui_campaign(cwd, sizeof(cwd)) == 0);
    moved = 1;

    for (trial = 0; trial < TRIALS; trial++) {
        CampaignSnapshot seen[MAX_EDITS + 1];     /* the list after each edit */
        CampaignSnapshot expect[MAX_EDITS + 1];   /* ...as Undo must show it  */
        CampaignSnapshot now, left;
        const CampaignCatalog *view;
        int edits = 0;         /* how many edits made a step */
        int expected = 0;
        int came_in = 0;       /* the edit that added the lost level, or 0 */
        int wanted;
        int lost_row;
        char lost;
        char lost_path[sizeof("levels/a.toml")];

        /* Every trial starts from the same files and a fresh view. */
        for (int f = 0; f < UI_CAMPAIGN_FILES; f++)
            CHECK(write_ui_campaign_level(ui_campaign_files[f].path, ui_campaign_files[f].name,
                                          ui_campaign_files[f].next) == 0);
        (void)editor_campaign_close(&es, 1);
        CHECK(editor_campaign_open(&es, NULL) == 0);
        view = editor_campaign_entries(&es);
        take_campaign_snapshot(&es, &seen[0]);

        /* Some picks are refused (a move off the end, a level already
         * listed), hence the tries.  The list keeps at least two rows, so
         * that one is left after the Remove that ends the run. */
        wanted = 2 + next_random(&seed, MAX_EDITS - 1);
        for (int tries = 0; edits < wanted && tries < 60; tries++) {
            int steps = editor_campaign_undo_steps(&es);
            int count = (int)view->count;
            int row = next_random(&seed, count);
            int file = next_random(&seed, UI_CAMPAIGN_FILES);

            switch (next_random(&seed, 6)) {
            case 0: (void)editor_campaign_move(&es, row, -1); break;
            case 1: (void)editor_campaign_move(&es, row, +1); break;
            case 2: if (count > 2) (void)editor_campaign_remove(&es, row); break;
            case 3: (void)editor_campaign_add(&es, ui_campaign_files[file].path); break;
            case 4: (void)editor_campaign_link_in_order(&es); break;
            case 5:                 /* a rename, typed as a designer types it */
                click_campaign_name(&es, row);
                push_text("x");
                key_frame(&es, KEY_ENTER, 0);
                break;
            }
            /* An edit that was refused, or changed nothing, is no step. */
            if (editor_campaign_undo_steps(&es) == steps) continue;
            CHECK(editor_campaign_undo_steps(&es) == steps + 1);
            take_campaign_snapshot(&es, &seen[++edits]);
        }
        CHECK(edits >= 1 && view->count >= 2);

        /* Remove one level and lose its file. */
        lost_row = next_random(&seed, (int)view->count);
        lost = view->levels[lost_row].path[sizeof("levels/") - 1];
        snprintf(lost_path, sizeof(lost_path), "levels/%c.toml", lost);
        CHECK(editor_campaign_remove(&es, lost_row) == 0);
        take_campaign_snapshot(&es, &left);
        CHECK(remove(lost_path) == 0);
        CHECK(editor_campaign_undo(&es) == -1);
        take_campaign_snapshot(&es, &now);
        CHECK(same_snapshot(&now, &left) && editor_campaign_undo_blocked(&es) == 1);

        /* Skip it: the list does not change. */
        editor_test_set_skip_step_choice(EDITOR_SKIP_STEP_SKIP);
        CHECK(editor_campaign_undo(&es) == 2);
        take_campaign_snapshot(&es, &now);
        CHECK(same_snapshot(&now, &left) && editor_campaign_undo_blocked(&es) == 0);

        /* What each Undo must show.  The lost level's row is taken out of
         * every list since the edit that last brought the level in (edit
         * 0, the list as opened, if no edit did).  Two neighbouring lists
         * that are then the same mean an edit of that row alone, which
         * the skip must have dropped. */
        for (int e = 1; e <= edits; e++)
            if (!snapshot_lists(&seen[e - 1], lost) && snapshot_lists(&seen[e], lost))
                came_in = e;
        for (int e = 0; e <= edits; e++) {
            CampaignSnapshot shown;
            if (e >= came_in) snapshot_without(&seen[e], lost, &shown);
            else shown = seen[e];
            if (expected > 0 && same_snapshot(&shown, &expect[expected - 1])) continue;
            expect[expected++] = shown;
        }
        CHECK(same_snapshot(&expect[expected - 1], &left));
        CHECK(editor_campaign_undo_steps(&es) == expected - 1);

        /* The file is back (an older step may read it), then every step
         * backwards and forwards. */
        for (int f = 0; f < UI_CAMPAIGN_FILES; f++)
            if (ui_campaign_files[f].path[sizeof("levels/") - 1] == lost)
                CHECK(write_ui_campaign_level(ui_campaign_files[f].path, ui_campaign_files[f].name,
                                              ui_campaign_files[f].next) == 0);
        for (int e = expected - 2; e >= 0; e--) {
            CHECK(editor_campaign_undo(&es) == 1);
            take_campaign_snapshot(&es, &now);
            CHECK(same_snapshot(&now, &expect[e]));
        }
        CHECK(editor_campaign_undo(&es) == 0);
        for (int e = 1; e < expected; e++) {
            CHECK(editor_campaign_redo(&es) == 1);
            take_campaign_snapshot(&es, &now);
            CHECK(same_snapshot(&now, &expect[e]));
        }
        CHECK(editor_campaign_redo(&es) == 0);
    }
done:
    /* The generator is the same everywhere, so the trial number is enough
     * to run that very history again. */
    if (failed) fprintf(stderr, "editor_ui_test: (in skipped-step history %d)\n", trial);
    if (moved && test_change_folder(cwd) != 0) failed = 1;
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/*
 * The unsaved mark compares the view with the files, so it follows the
 * history: undoing back to what was saved is "nothing unsaved", Save makes
 * the list as it stands the saved one, and Revert and reopening the view
 * start a new history.
 */
static int campaign_unsaved_mark_follows_the_history(void)
{
    int failed = 0;
    EditorState es;
    char cwd[4096];
    int moved = 0;
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(enter_ui_campaign(cwd, sizeof(cwd)) == 0);
    moved = 1;
    key_frame(&es, KEY_M, INPUT_CTRL);
    CHECK(es.campaign != NULL && editor_campaign_unsaved(&es) == 0);

    /* Edit, undo, redo. */
    click_campaign_row(&es, 2);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UP);    /* a c b */
    CHECK(editor_campaign_unsaved(&es) == 1);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(editor_campaign_unsaved(&es) == 0);
    key_frame(&es, KEY_Y, INPUT_CTRL);
    CHECK(editor_campaign_unsaved(&es) == 1);

    /* Save: the list as it stands, relinked, is now the saved one. */
    click_campaign_button(&es, CAMPAIGN_BUTTON_LINK);
    click_campaign_button(&es, CAMPAIGN_BUTTON_SAVE);
    CHECK(strstr(es.status_message, "Campaign saved") != NULL);
    CHECK(editor_campaign_unsaved(&es) == 0);
    CHECK(editor_campaign_undo_steps(&es) == 2);     /* Save keeps the history */

    /* Undoing past the Save differs from the files again, step by step,
     * and redoing arrives back at the saved campaign. */
    key_frame(&es, KEY_Z, INPUT_CTRL);               /* the links */
    CHECK(strcmp(campaign_rows(&es), "acb") == 0 && editor_campaign_unsaved(&es) == 1);
    key_frame(&es, KEY_Z, INPUT_CTRL);               /* the move */
    CHECK(strcmp(campaign_rows(&es), "abc") == 0 && editor_campaign_unsaved(&es) == 1);
    key_frame(&es, KEY_Y, INPUT_CTRL);
    CHECK(editor_campaign_unsaved(&es) == 1);
    key_frame(&es, KEY_Y, INPUT_CTRL);
    CHECK(strcmp(campaign_rows(&es), "acb") == 0 && editor_campaign_unsaved(&es) == 0);

    /* Revert (asked twice) reads the files again and clears the history. */
    click_campaign_row(&es, 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_DOWN);  /* c a b */
    CHECK(editor_campaign_undo_steps(&es) == 3);
    click_campaign_button(&es, CAMPAIGN_BUTTON_REVERT);
    CHECK(editor_campaign_undo_steps(&es) == 3);     /* only warned so far */
    click_campaign_button(&es, CAMPAIGN_BUTTON_REVERT);
    CHECK(strcmp(campaign_rows(&es), "acb") == 0 && editor_campaign_unsaved(&es) == 0);
    CHECK(editor_campaign_undo_steps(&es) == 0 && editor_campaign_redo_steps(&es) == 0);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(strstr(es.status_message, "nothing to undo") != NULL);
    CHECK(strcmp(campaign_rows(&es), "acb") == 0);

    /* So does closing the view and opening it again. */
    click_campaign_row(&es, 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_DOWN);  /* c a b */
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(editor_campaign_redo_steps(&es) == 1 && editor_campaign_unsaved(&es) == 0);
    key_frame(&es, KEY_ESCAPE, 0);
    CHECK(es.campaign == NULL);
    key_frame(&es, KEY_M, INPUT_CTRL);
    CHECK(es.campaign != NULL);
    CHECK(editor_campaign_undo_steps(&es) == 0 && editor_campaign_redo_steps(&es) == 0);
    key_frame(&es, KEY_Y, INPUT_CTRL);
    CHECK(strcmp(campaign_rows(&es), "acb") == 0);
done:
    if (moved && test_change_folder(cwd) != 0) failed = 1;
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/*
 * While the Campaign view is open the undo keys belong to it: the level's
 * own undo stack and the level are left alone, whether or not the view has
 * a step to replay.  With the view closed the keys undo the level again.
 */
static int campaign_undo_keys_leave_the_level_history_alone(void)
{
    static const int mods[] = { INPUT_CTRL, INPUT_CTRL | INPUT_SHIFT };
    int failed = 0;
    EditorState es;
    char cwd[4096];
    int moved = 0;
    uint64_t level;
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(enter_ui_campaign(cwd, sizeof(cwd)) == 0);
    moved = 1;

    /* One step in the level's history: a coin. */
    es.tool = TOOL_PLACE;
    es.palette_type = ENT_COIN;
    click_frame(&es, 400, 500);
    CHECK(es.level.coin_count == 1 && es.undo->top == 1 && es.undo->redo_top == 0);
    level = doc_hash(&es);

    /* An empty campaign history must not let the keys fall through. */
    key_frame(&es, KEY_M, INPUT_CTRL);
    CHECK(es.campaign != NULL);
    for (size_t i = 0; i < sizeof(mods) / sizeof(mods[0]); i++) {
        key_frame(&es, KEY_Z, mods[i]);
        CHECK(es.undo->top == 1 && es.undo->redo_top == 0 && doc_hash(&es) == level);
    }
    key_frame(&es, KEY_Y, INPUT_CTRL);
    CHECK(es.undo->top == 1 && es.undo->redo_top == 0 && doc_hash(&es) == level);

    /* Nor do campaign steps: a move and a rename, undone and redone. */
    click_campaign_row(&es, 2);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UP);
    click_campaign_name(&es, 0);
    push_text("!");
    key_frame(&es, KEY_ENTER, 0);
    CHECK(editor_campaign_undo_steps(&es) == 2);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(strcmp(campaign_rows(&es), "abc") == 0);
    CHECK(es.undo->top == 1 && es.undo->redo_top == 0 && doc_hash(&es) == level);
    key_frame(&es, KEY_Y, INPUT_CTRL);
    key_frame(&es, KEY_Z, INPUT_CTRL | INPUT_SHIFT);
    CHECK(strcmp(campaign_rows(&es), "acb") == 0);
    CHECK(es.undo->top == 1 && es.undo->redo_top == 0 && doc_hash(&es) == level);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(editor_campaign_unsaved(&es) == 0);

    /* Back in the level, the same keys undo and redo the coin. */
    key_frame(&es, KEY_ESCAPE, 0);
    CHECK(es.campaign == NULL);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coin_count == 0 && es.undo->top == 0 && es.undo->redo_top == 1);
    key_frame(&es, KEY_Y, INPUT_CTRL);
    CHECK(es.level.coin_count == 1 && doc_hash(&es) == level);
done:
    if (moved && test_change_folder(cwd) != 0) failed = 1;
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/*
 * Multi-select: a box on empty canvas selects what it touches, Shift+click
 * adds or removes one entity, and move (drag or arrows), delete, copy /
 * paste and duplicate act on the whole group as ONE undo step each.
 */
static int box_and_shift_select_act_on_the_group(void)
{
    int failed = 0;
    EditorState es;
    float x[3], y[3];
    CHECK(open_editor(&es, NULL) == 0);
    es.tool = TOOL_PLACE;
    es.palette_type = ENT_COIN;
    for (int i = 0; i < 3; i++) click_frame(&es, 200 + 60 * i, 300);
    CHECK(es.level.coin_count == 3);
    es.tool = TOOL_SELECT;
    for (int i = 0; i < 3; i++) { x[i] = es.level.coins[i].x; y[i] = es.level.coins[i].y; }

    /* Box from empty canvas over all three coins. */
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, 0, 150, 250);
    push_event(INPUT_MOUSE_MOVE, 0, 0, 250, 300);
    push_event(INPUT_MOUSE_MOVE, 0, 0, 350, 350);
    ui_frame(&es, 350, 350);
    CHECK(es.box_selecting);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, 0, 350, 350);
    ui_frame(&es, 350, 350);
    CHECK(!es.box_selecting && editor_selection_count(&es) == 3);
    for (int i = 0; i < 3; i++) CHECK(editor_is_selected(&es, ENT_COIN, i));
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);           /* "3 selected" panel */

    /* Dragging one member moves all three: one undo step. */
    int undo_top = es.undo->top;
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, 0, 262, 302);
    push_event(INPUT_MOUSE_MOVE, 0, 0, 282, 302);
    push_event(INPUT_MOUSE_MOVE, 0, 0, 302, 302);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, 0, 302, 302);
    ui_frame(&es, 302, 302);
    for (int i = 0; i < 3; i++) CHECK(es.level.coins[i].x == x[i] + 20.0f);
    CHECK(es.undo->top == undo_top + 1 && editor_selection_count(&es) == 3);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    for (int i = 0; i < 3; i++) CHECK(es.level.coins[i].x == x[i]);
    CHECK(es.undo->top == undo_top);
    key_frame(&es, KEY_Y, INPUT_CTRL);
    for (int i = 0; i < 3; i++) CHECK(es.level.coins[i].x == x[i] + 20.0f);
    key_frame(&es, KEY_Z, INPUT_CTRL);

    /* The arrows nudge the group. */
    key_frame(&es, KEY_DOWN, 0);
    for (int i = 0; i < 3; i++) CHECK(es.level.coins[i].y == y[i] + 1.0f);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    for (int i = 0; i < 3; i++) CHECK(es.level.coins[i].y == y[i]);

    /* Shift+click takes one out, and puts it back. */
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, INPUT_SHIFT, 262, 302);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, INPUT_SHIFT, 262, 302);
    ui_frame(&es, 262, 302);
    CHECK(editor_selection_count(&es) == 2 && !editor_is_selected(&es, ENT_COIN, 1));
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, INPUT_SHIFT, 262, 302);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, INPUT_SHIFT, 262, 302);
    ui_frame(&es, 262, 302);
    CHECK(editor_selection_count(&es) == 3);

    /* Copy and paste the group: three copies, selected, one undo step. */
    undo_top = es.undo->top;
    key_frame(&es, KEY_C, INPUT_CTRL);
    CHECK(es.clipboard_count == 3);
    key_frame(&es, KEY_V, INPUT_CTRL);
    CHECK(es.level.coin_count == 6 && editor_selection_count(&es) == 3);
    for (int i = 3; i < 6; i++) CHECK(editor_is_selected(&es, ENT_COIN, i));
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coin_count == 3 && es.undo->top == undo_top);
    /* Redo brings the copies back selected together, as the paste left them. */
    key_frame(&es, KEY_Y, INPUT_CTRL);
    CHECK(es.level.coin_count == 6 && editor_selection_count(&es) == 3);
    for (int i = 3; i < 6; i++) CHECK(editor_is_selected(&es, ENT_COIN, i));
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coin_count == 3 && es.undo->top == undo_top);

    /* Duplicate the group, then undo it in one step. */
    CHECK(editor_select_items(&es, (Selection[]){{ENT_COIN, 0}, {ENT_COIN, 1}, {ENT_COIN, 2}}, 3) == 3);
    key_frame(&es, KEY_D, INPUT_CTRL);
    CHECK(es.level.coin_count == 6 && editor_selection_count(&es) == 3);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coin_count == 3);

    /* Delete removes the whole group; one Ctrl+Z brings all three back. */
    CHECK(editor_select_items(&es, (Selection[]){{ENT_COIN, 0}, {ENT_COIN, 1}, {ENT_COIN, 2}}, 3) == 3);
    key_frame(&es, KEY_DELETE, 0);
    CHECK(es.level.coin_count == 0 && editor_selection_count(&es) == 0);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coin_count == 3);
    for (int i = 0; i < 3; i++) CHECK(es.level.coins[i].x == x[i] && es.level.coins[i].y == y[i]);

    /* The Delete tool, or a right-click, on a member deletes the whole
     * selection as one step; on anything else just that entity. */
    CHECK(editor_select_items(&es, (Selection[]){{ENT_COIN, 0}, {ENT_COIN, 1}}, 2) == 2);
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_RIGHT, 0, 322, 302);   /* coin 2: not selected */
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_RIGHT, 0, 322, 302);
    ui_frame(&es, 322, 302);
    CHECK(es.level.coin_count == 2 && editor_selection_count(&es) == 2);
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_RIGHT, 0, 262, 302);   /* coin 1: a member */
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_RIGHT, 0, 262, 302);
    ui_frame(&es, 262, 302);
    CHECK(es.level.coin_count == 0 && editor_selection_count(&es) == 0);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coin_count == 2);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coin_count == 3);
    CHECK(editor_select_items(&es, (Selection[]){{ENT_COIN, 1}, {ENT_COIN, 2}}, 2) == 2);
    es.tool = TOOL_DELETE;
    click_frame(&es, 322, 302);
    CHECK(es.level.coin_count == 1 && es.level.coins[0].x == x[0]);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coin_count == 3);
    es.tool = TOOL_SELECT;

    /* A plain click on one member (no drag) selects just it. */
    CHECK(editor_select_items(&es, (Selection[]){{ENT_COIN, 0}, {ENT_COIN, 1}, {ENT_COIN, 2}}, 3) == 3);
    click_frame(&es, 322, 302);
    CHECK(editor_selection_count(&es) == 1 && es.selection.index == 2);
    /* Shift+box adds to the selection; Esc clears it. */
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, INPUT_SHIFT, 150, 250);
    push_event(INPUT_MOUSE_MOVE, 0, INPUT_SHIFT, 230, 350);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, INPUT_SHIFT, 230, 350);
    ui_frame(&es, 230, 350);
    CHECK(editor_selection_count(&es) == 2 && editor_is_selected(&es, ENT_COIN, 0));
    key_frame(&es, KEY_ESCAPE, 0);
    CHECK(editor_selection_count(&es) == 0);
    CHECK(level_is_valid(&es));
done:
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/* ------------------------------------------------------------------ */
/* Text editing: caret keys and Tab                                    */
/* ------------------------------------------------------------------ */


static void text_frame(EditorState *es, const char *text)
{
    push_text(text);
    ui_frame(es, NEUTRAL_X, NEUTRAL_Y);
}

/*
 * Fields used to append and backspace at the end only.  The caret now
 * moves with Left/Right/Home/End, typing inserts at it, Backspace and
 * Delete remove the whole UTF-8 character on either side, and Tab /
 * Shift+Tab commit the field and move to the next / previous one.
 */
static int text_fields_move_the_caret_and_tab_between_fields(void)
{
    int failed = 0;
    EditorState es;
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(strcmp(es.level.name, "Untitled") == 0);

    click_frame(&es, CANVAS_W + 60, CFG_NAME_Y);
    CHECK(es.ui.active_id == 9000);
    key_frame(&es, KEY_HOME, 0);
    text_frame(&es, "\xc3\xa9");                 /* é, two bytes */
    CHECK(strcmp(es.ui.edit_buf, "\xc3\xa9Untitled") == 0 && es.ui.edit_cursor == 2);
    key_frame(&es, KEY_RIGHT, 0);                 /* past the U */
    key_frame(&es, KEY_DELETE, 0);                /* the n goes */
    key_frame(&es, KEY_END, 0);
    text_frame(&es, "!");
    key_frame(&es, KEY_LEFT, 0);
    key_frame(&es, KEY_LEFT, 0);
    key_frame(&es, KEY_BACKSPACE, 0);             /* the e before d */
    CHECK(strcmp(es.ui.edit_buf, "\xc3\xa9Utitld!") == 0);
    /* Left and Backspace step over the whole two-byte character. */
    key_frame(&es, KEY_HOME, 0);
    key_frame(&es, KEY_RIGHT, 0);
    CHECK(es.ui.edit_cursor == 2);
    key_frame(&es, KEY_BACKSPACE, 0);
    CHECK(strcmp(es.ui.edit_buf, "Utitld!") == 0 && es.ui.edit_cursor == 0);
    /* Keys and text in one frame keep their order: x, Right, y. */
    {
        InputEvent event = {0};
        event.type = INPUT_TEXT;
        event.text[0] = 'x';
        input_push(&event);
        push_key(KEY_RIGHT, 0);
        event.text[0] = 'y';
        input_push(&event);
        ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    }
    CHECK(strcmp(es.ui.edit_buf, "xUytitld!") == 0);
    key_frame(&es, KEY_ENTER, 0);
    CHECK(es.ui.active_id == 0 && strcmp(es.level.name, "xUytitld!") == 0);

    /* Tab commits the typed value as one undo step and moves on. */
    int undo_top = es.undo->top;
    click_frame(&es, CANVAS_W + 90, CFG_SCREENS_Y);
    CHECK(es.ui.active_id == 9011);
    key_frame(&es, KEY_END, 0);
    key_frame(&es, KEY_BACKSPACE, 0);
    text_frame(&es, "5");
    key_frame(&es, KEY_TAB, 0);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(es.level.screen_count == 5 && es.undo->top == undo_top + 1);
    CHECK(es.ui.active_id != 0 && es.ui.active_id != 9011);
    /* Shift+Tab comes back, with nothing to commit. */
    key_frame(&es, KEY_TAB, INPUT_SHIFT);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(es.ui.active_id == 9011 && es.undo->top == undo_top + 1);
    /* An invalid value keeps the focus where it is. */
    key_frame(&es, KEY_END, 0);
    key_frame(&es, KEY_BACKSPACE, 0);
    text_frame(&es, "-");
    key_frame(&es, KEY_TAB, 0);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(es.ui.active_id == 9011 && es.level.screen_count == 5);
    key_frame(&es, KEY_ESCAPE, 0);
    CHECK(es.ui.active_id == 0);

    /* Dropdowns are in the Tab order: Tab from the axe's y field opens its
     * mode list, Down + Enter picks Spin as one undo step, and Tab from an
     * open list closes it and moves on. */
    es.level.axe_trap_count = 1;
    es.level.axe_traps[0] = (AxeTrapPlacement){.pillar_x = 500.0f, .mode = AXE_MODE_PENDULUM};
    editor_select_only(&es, ENT_AXE_TRAP, 0);
    es.panel_open = 1;
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    ui_focus_field(&es.ui, (int)ENT_AXE_TRAP * 100 + 4);          /* y */
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(es.ui.active_id == (int)ENT_AXE_TRAP * 100 + 4);
    undo_top = es.undo->top;
    key_frame(&es, KEY_TAB, 0);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(es.ui.active_id == 0 && es.ui.dropdown_open_id == (int)ENT_AXE_TRAP * 100 + 2);
    key_frame(&es, KEY_DOWN, 0);
    key_frame(&es, KEY_DOWN, 0);                                  /* stops at Spin */
    CHECK(es.ui.dropdown_highlight == 1 && es.level.axe_traps[0].mode == AXE_MODE_PENDULUM);
    key_frame(&es, KEY_ENTER, 0);
    CHECK(es.ui.dropdown_open_id == 0 && es.level.axe_traps[0].mode == AXE_MODE_SPIN);
    CHECK(es.undo->top == undo_top + 1);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.axe_traps[0].mode == AXE_MODE_PENDULUM);
    /* Shift+Tab from the reopened list goes back to y; Esc just closes. */
    ui_focus_field(&es.ui, (int)ENT_AXE_TRAP * 100 + 2);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(es.ui.dropdown_open_id == (int)ENT_AXE_TRAP * 100 + 2);
    key_frame(&es, KEY_TAB, INPUT_SHIFT);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(es.ui.dropdown_open_id == 0 && es.ui.active_id == (int)ENT_AXE_TRAP * 100 + 4);
    key_frame(&es, KEY_ESCAPE, 0);
    ui_focus_field(&es.ui, (int)ENT_AXE_TRAP * 100 + 2);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    key_frame(&es, KEY_ESCAPE, 0);
    CHECK(es.ui.dropdown_open_id == 0 && es.level.axe_traps[0].mode == AXE_MODE_PENDULUM);
done:
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/*
 * Text selection: Shift+arrows, Ctrl+A and a mouse drag select; Ctrl+Left /
 * Right jump a word (Shift: selecting it); Ctrl+C / X / V and typing act on
 * the selection; all in whole UTF-8 characters.
 */
/*
 * Up / Down step the active number field (Shift: ten steps) inside its
 * limits, as a staged edit that Return commits in one undo step.  Text
 * fields ignore them.
 */
static int up_and_down_step_number_fields(void)
{
    int failed = 0;
    EditorState es;
    CHECK(open_editor(&es, NULL) == 0);
    int undo_top = es.undo->top;
    CHECK(es.level.screen_count == 4);

    click_frame(&es, CANVAS_W + 90, CFG_SCREENS_Y);
    CHECK(es.ui.active_id == 9011);
    key_frame(&es, KEY_UP, 0);
    CHECK(strcmp(es.ui.edit_buf, "5") == 0);
    key_frame(&es, KEY_UP, INPUT_SHIFT);
    CHECK(strcmp(es.ui.edit_buf, "15") == 0);
    CHECK(es.level.screen_count == 4);            /* staged, not stored */
    for (int i = 0; i < 10; i++) key_frame(&es, KEY_UP, INPUT_SHIFT);
    CHECK(strcmp(es.ui.edit_buf, "99") == 0);     /* the field's maximum */
    key_frame(&es, KEY_DOWN, INPUT_SHIFT);
    CHECK(strcmp(es.ui.edit_buf, "89") == 0);
    key_frame(&es, KEY_ENTER, 0);
    CHECK(es.ui.active_id == 0 && es.level.screen_count == 89);
    CHECK(es.undo->top == undo_top + 1);

    /* Down stops at the minimum; a half-typed value steps from the
     * stored one. */
    click_frame(&es, CANVAS_W + 90, CFG_SCREENS_Y);
    key_frame(&es, KEY_END, 0);
    key_frame(&es, KEY_BACKSPACE, 0);
    key_frame(&es, KEY_BACKSPACE, 0);
    text_frame(&es, "-");
    key_frame(&es, KEY_DOWN, 0);
    CHECK(strcmp(es.ui.edit_buf, "88") == 0);
    for (int i = 0; i < 10; i++) key_frame(&es, KEY_DOWN, INPUT_SHIFT);
    CHECK(strcmp(es.ui.edit_buf, "1") == 0);
    key_frame(&es, KEY_ESCAPE, 0);
    CHECK(es.level.screen_count == 89);

    /* A text field keeps its text and caret. */
    click_frame(&es, CANVAS_W + 60, CFG_NAME_Y);
    CHECK(es.ui.active_id == 9000);
    key_frame(&es, KEY_HOME, 0);
    key_frame(&es, KEY_UP, 0);
    key_frame(&es, KEY_DOWN, 0);
    CHECK(strcmp(es.ui.edit_buf, "Untitled") == 0 && es.ui.edit_cursor == 0);
    key_frame(&es, KEY_ESCAPE, 0);
done:
    close_editor(&es);
    return failed;
}

/* Float fields step by 1 from 10 up and by 0.1 below, without float
 * error showing, step over 0 when 0 is refused, and keep to the limits;
 * an int field with a step uses it. */
static int number_field_steps_suit_the_field(void)
{
    int failed = 0;
    UIState ui;
    float small = 0.2f, big = 12.5f, speed = 0.1f;
    int snapped = 32;
    ui_init(&ui, NULL);

#define ACTIVATE(call) do { ui_begin_frame(&ui); ui.mouse_x = ui.mouse_y = 5; \
        ui.mouse_clicked = 1; (void)(call); } while (0)
    ACTIVATE(ui_float_field_limited(&ui, 1, 0, 0, 100, &small, -1.0f, 1.0f));
    CHECK(ui.active_id == 1);
    ui_edit_key(&ui, UI_KEY_UP, 0);
    CHECK(strcmp(ui.edit_buf, "0.3") == 0);
    ui_edit_key(&ui, UI_KEY_UP, 0);
    ui_edit_key(&ui, UI_KEY_UP, 0);
    CHECK(strcmp(ui.edit_buf, "0.5") == 0);
    ui_edit_key(&ui, UI_KEY_UP, 1);
    CHECK(strcmp(ui.edit_buf, "1") == 0);         /* the limit */
    CHECK(ui_apply_active_edit(&ui) == 2 && small == 1.0f);

    ACTIVATE(ui_float_field(&ui, 2, 0, 0, 100, &big));
    ui_edit_key(&ui, UI_KEY_UP, 0);
    CHECK(strcmp(ui.edit_buf, "13.5") == 0);
    ui_edit_key(&ui, UI_KEY_DOWN, 1);
    CHECK(strcmp(ui.edit_buf, "3.5") == 0);
    ui_edit_key(&ui, UI_KEY_DOWN, 0);
    CHECK(strcmp(ui.edit_buf, "3.4") == 0);
    ui_cancel_active_edit(&ui);

    ACTIVATE(ui_float_field_nonzero(&ui, 3, 0, 0, 100, &speed, 5.0f));
    ui_edit_key(&ui, UI_KEY_DOWN, 0);
    CHECK(strcmp(ui.edit_buf, "-0.1") == 0);      /* 0 is skipped */
    ui_cancel_active_edit(&ui);

    ACTIVATE(ui_int_field_limited(&ui, 4, 0, 0, 100, &snapped, 0, 64, 16));
    ui_edit_key(&ui, UI_KEY_UP, 0);
    CHECK(strcmp(ui.edit_buf, "48") == 0);
    ui_edit_key(&ui, UI_KEY_UP, 1);
    CHECK(strcmp(ui.edit_buf, "64") == 0);
    ui_edit_key(&ui, UI_KEY_DOWN, 1);
    CHECK(strcmp(ui.edit_buf, "0") == 0);
#undef ACTIVATE
done:
    ui_cleanup(&ui);
    return failed;
}

static int text_fields_select_copy_cut_and_paste(void)
{
    int failed = 0;
    EditorState es;
    const int field_left = CANVAS_W + 55, field_right = CANVAS_W + 55 + 310;
    CHECK(open_editor(&es, NULL) == 0);
    editor_test_set_clipboard("");

    click_frame(&es, CANVAS_W + 60, CFG_NAME_Y);
    CHECK(es.ui.active_id == 9000 && strcmp(es.ui.edit_buf, "Untitled") == 0);
    /* Nothing selected: Ctrl+C copies nothing and says how to select. */
    key_frame(&es, KEY_C, INPUT_CTRL);
    CHECK(editor_test_clipboard()[0] == '\0' && strstr(es.status_message, "Nothing selected"));
    /* Ctrl+A, Ctrl+C: the whole field. */
    key_frame(&es, KEY_A, INPUT_CTRL);
    key_frame(&es, KEY_C, INPUT_CTRL);
    CHECK(strcmp(editor_test_clipboard(), "Untitled") == 0);
    /* Shift+Left twice selects "ed"; Ctrl+X cuts it. */
    key_frame(&es, KEY_END, 0);
    key_frame(&es, KEY_LEFT, INPUT_SHIFT);
    key_frame(&es, KEY_LEFT, INPUT_SHIFT);
    key_frame(&es, KEY_X, INPUT_CTRL);
    CHECK(strcmp(editor_test_clipboard(), "ed") == 0 && strcmp(es.ui.edit_buf, "Untitl") == 0);
    /* Ctrl+Left jumps to the start of the word; Ctrl+V pastes there. */
    key_frame(&es, KEY_LEFT, INPUT_CTRL);
    CHECK(es.ui.edit_cursor == 0);
    key_frame(&es, KEY_V, INPUT_CTRL);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(strcmp(es.ui.edit_buf, "edUntitl") == 0);
    /* Typing replaces a selection; word jumps step over whole UTF-8
     * characters, so Ctrl+Shift+Right selects "w\xc3\xb6rld" whole. */
    key_frame(&es, KEY_A, INPUT_CTRL);
    text_frame(&es, "H\xc3\xa9 llo w\xc3\xb6rld");
    CHECK(strcmp(es.ui.edit_buf, "H\xc3\xa9 llo w\xc3\xb6rld") == 0);
    key_frame(&es, KEY_LEFT, INPUT_CTRL);
    CHECK(es.ui.edit_cursor == (int)strlen("H\xc3\xa9 llo "));
    key_frame(&es, KEY_RIGHT, INPUT_CTRL | INPUT_SHIFT);
    key_frame(&es, KEY_C, INPUT_CTRL);
    CHECK(strcmp(editor_test_clipboard(), "w\xc3\xb6rld") == 0);
    /* Shift+Left from inside a two-byte character's neighbour takes it
     * whole: select "\xc3\xa9" and delete it. */
    key_frame(&es, KEY_HOME, 0);
    key_frame(&es, KEY_RIGHT, 0);
    key_frame(&es, KEY_RIGHT, INPUT_SHIFT);
    key_frame(&es, KEY_DELETE, 0);
    CHECK(strcmp(es.ui.edit_buf, "H llo w\xc3\xb6rld") == 0);
    /* Left without Shift ends a selection at its start. */
    key_frame(&es, KEY_END, INPUT_SHIFT);
    key_frame(&es, KEY_LEFT, 0);
    CHECK(es.ui.edit_cursor == 1 && es.ui.edit_anchor < 0);

    /* A press past the end of the text and a drag to the field's left
     * edge select everything; Backspace deletes the selection. */
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, 0, field_right - 10, CFG_NAME_Y);
    ui_frame(&es, field_right - 10, CFG_NAME_Y);
    CHECK(es.ui.active_id == 9000 && es.ui.edit_cursor == (int)strlen(es.ui.edit_buf));
    ui_frame(&es, field_left + 1, CFG_NAME_Y);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, 0, field_left + 1, CFG_NAME_Y);
    ui_frame(&es, field_left + 1, CFG_NAME_Y);
    CHECK(es.ui.edit_cursor == 0 && es.ui.edit_anchor == (int)strlen(es.ui.edit_buf));
    key_frame(&es, KEY_BACKSPACE, 0);
    CHECK(es.ui.edit_buf[0] == '\0');
    key_frame(&es, KEY_ESCAPE, 0);
    CHECK(strcmp(es.level.name, "Untitled") == 0);
done:
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/* ------------------------------------------------------------------ */
/* Playtest process and native pickers (POSIX)                         */
/* ------------------------------------------------------------------ */

#ifndef _WIN32
/*
 * wait_until_exited — Block until the child has exited, but leave it
 * unreaped (WNOWAIT) so the editor's own status check still collects it and
 * reports the exit code or signal. No time limit: a slow host (sanitizers,
 * a busy CI runner) only makes the test slower, never wrong.
 */
static int wait_until_exited(pid_t child)
{
    siginfo_t info;
    int result;
    do {
        result = waitid(P_PID, (id_t)child, &info, WEXITED | WNOWAIT);
    } while (result != 0 && errno == EINTR);
    return result;
}

static int playtest_status_follows_the_game_process(void)
{
    int failed = 0;
    EditorState es;
    CHECK(open_editor(&es, NULL) == 0);

    /* While a playtest runs, edits are refused with a hint. The child
     * waits on a pipe so it is certainly still running during that frame. */
    int gate[2];
    CHECK(pipe(gate) == 0);
    pid_t child = fork();
    CHECK(child >= 0);
    if (child == 0) {
        char byte;
        (void)close(gate[1]);
        /* read() returns 0 once the parent closes its end. glibc marks
         * read() warn_unused_result, and a (void) cast does not silence
         * that under release flags, so the result is consumed by the if. */
        if (read(gate[0], &byte, 1) < 0) _exit(4);
        _exit(3);
    }
    (void)close(gate[0]);
    es.playing = 1;
    es.play_pid = (int)child;
    key_frame(&es, KEY_TWO, 0);
    /* The game exits with code 3: the status line says so. */
    (void)close(gate[1]);
    CHECK(wait_until_exited(child) == 0);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(!es.playing && es.play_pid == 0);
    CHECK(strstr(es.status_message, "code 3") != NULL);
    CHECK(es.tool == TOOL_SELECT);

    /* A game killed by a signal reports the signal. */
    child = fork();
    CHECK(child >= 0);
    if (child == 0) { raise(SIGKILL); _exit(0); }
    es.playing = 1;
    es.play_pid = (int)child;
    CHECK(wait_until_exited(child) == 0);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(!es.playing && strstr(es.status_message, "signal") != NULL);

    /* A child that something else already reaped ends the playtest too. */
    child = fork();
    CHECK(child >= 0);
    if (child == 0) _exit(0);
    CHECK(waitpid(child, NULL, 0) == child);
    es.playing = 1;
    es.play_pid = (int)child;
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(!es.playing && strstr(es.status_message, "already reaped") != NULL);

    /* The overlay's Stop button ends a running game. */
    child = fork();
    CHECK(child >= 0);
    if (child == 0) { for (;;) pause(); }
    es.playing = 1;
    es.play_pid = (int)child;
    for (int y = EDITOR_H / 2; y < EDITOR_H / 2 + 80 && es.playing; y += 4)
        click_frame(&es, EDITOR_W / 2, y);
    CHECK(!es.playing && es.play_pid == 0);
    CHECK(strstr(es.status_message, "stopped") != NULL);
    /* The child is gone: there is nothing left to reap. */
    CHECK(waitpid(child, NULL, WNOHANG) == -1 && errno == ECHILD);

    /* Starting a playtest of an invalid level is refused up front. */
    es.level.screen_count = -1;
    key_frame(&es, KEY_F5, 0);
    CHECK(!es.playing && strstr(es.status_message, "Playtest blocked") != NULL);
done:
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/* The command line the last test launch would have run, one string per
 * argument, and how many launches there were. */
static char launched_args[8][EDITOR_PATH_MAX];
static int launched_argc;
static int launch_count;

/* The write end of the pipe the current stand-in game is waiting on, or -1
 * when none is running. Closing it is what lets that child exit. */
static int launch_gate = -1;

/* Stand-in for running the game: record the arguments and start a child
 * that waits on a pipe until the test lets it go, so the editor follows and
 * reaps a real process. A child that exited at once could be gone before
 * the frame that launched it had finished, and "is the playtest running"
 * would then depend on how busy the machine was. */
static int record_launch(const char *const *argv)
{
    int gate[2];
    pid_t child;
    launched_argc = 0;
    for (int i = 0; argv[i] && i < 8; i++) {
        snprintf(launched_args[i], sizeof(launched_args[i]), "%s", argv[i]);
        launched_argc++;
    }
    launch_count++;
    if (pipe(gate) != 0) return -1;
    child = fork();
    if (child == 0) {
        char byte;
        /* The child holds a copy of the write end too; read() only sees
         * end-of-file once every copy is closed. */
        (void)close(gate[1]);
        if (read(gate[0], &byte, 1) < 0) _exit(4);
        _exit(0);
    }
    (void)close(gate[0]);
    if (child < 0) {
        (void)close(gate[1]);
        return -1;
    }
    launch_gate = gate[1];
    return (int)child;
}

/* Let the stand-in game exit, if one is still waiting. */
static void release_launched_game(void)
{
    if (launch_gate >= 0) (void)close(launch_gate);
    launch_gate = -1;
}

/* Is `flag` followed by `value` in the recorded command line? */
static int launched_with(const char *flag, const char *value)
{
    for (int i = 0; i + 1 < launched_argc; i++)
        if (strcmp(launched_args[i], flag) == 0 && strcmp(launched_args[i + 1], value) == 0)
            return 1;
    return 0;
}

/* Let the recorded child exit and the editor notice. */
static void finish_launched_game(EditorState *es)
{
    release_launched_game();
    if (es->play_pid > 0) (void)wait_until_exited((pid_t)es->play_pid);
    ui_frame(es, NEUTRAL_X, NEUTRAL_Y);
}

/*
 * Shift+F5 is "Playtest from here": the game gets --start-x for the spot
 * under the mouse, or --start-checkpoint for a selected checkpoint.  A
 * spot the game would refuse (over a floor gap) is refused before
 * anything is launched.  F5 alone still starts at the level's start.
 */
static int playtest_from_here_passes_the_start_point(void)
{
    int failed = 0;
    EditorState es;
    const char *prefs = TEST_OUT "ui-playtest-prefs";
    const int canvas_y = TOOLBAR_H + 100;
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(mkdir(prefs, 0700) == 0 || errno == EEXIST);
    CHECK(editor_set_preference_root(&es, prefs) == 0);
    CHECK(editor_init_persistence_paths(&es) == 0);
    editor_test_set_play_launcher(record_launch);
    es.level.floor_gap_count = 1;
    es.level.floor_gaps[0] = 400;
    es.level.checkpoint_count = 1;
    es.level.checkpoints[0] = (CheckpointPlacement){800.0f, 252.0f};
    CHECK(level_is_valid(&es));
    es.camera.x = 0.0f;
    es.camera.y = 0.0f;
    es.camera.zoom = 1.0f;

    /* The mouse over world x 200, on the grass. */
    ui_frame(&es, 200, canvas_y);
    push_key(KEY_F5, INPUT_SHIFT);
    ui_frame(&es, 200, canvas_y);
    CHECK(launch_count == 1 && es.playing);
    CHECK(launched_with("--start-x", "200"));
    CHECK(launched_with("--level", es.playtest_path));
    CHECK(strstr(es.status_message, "from x 200") != NULL);
    finish_launched_game(&es);
    CHECK(!es.playing);

    /* Over the gap there is nothing to stand on: nothing is launched. */
    ui_frame(&es, 416, canvas_y);
    push_key(KEY_F5, INPUT_SHIFT);
    ui_frame(&es, 416, canvas_y);
    CHECK(launch_count == 1 && !es.playing);
    CHECK(strstr(es.status_message, "nothing to stand on") != NULL);

    /* A selected checkpoint wins over the mouse. */
    editor_select_only(&es, ENT_CHECKPOINT, 0);
    ui_frame(&es, 200, canvas_y);
    push_key(KEY_F5, INPUT_SHIFT);
    ui_frame(&es, 200, canvas_y);
    CHECK(launch_count == 2 && launched_with("--start-checkpoint", "0"));
    finish_launched_game(&es);

    /* Nothing selected and the mouse off the canvas: say what to do. */
    editor_select_none(&es);
    key_frame(&es, KEY_F5, INPUT_SHIFT);
    CHECK(launch_count == 2 && strstr(es.status_message, "point at the canvas") != NULL);

    /* Plain F5 starts where the level starts. */
    key_frame(&es, KEY_F5, 0);
    CHECK(launch_count == 3 && es.playing);
    for (int i = 0; i < launched_argc; i++)
        CHECK(strncmp(launched_args[i], "--start", 7) != 0);
    finish_launched_game(&es);
done:
    /* A check that failed part-way may have left a child waiting. */
    release_launched_game();
    editor_test_set_play_launcher(NULL);
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

#define FAKE_BIN TEST_OUT "fake-picker-bin"

/* Write a stand-in for osascript/zenity: it prints MANGO_FAKE_PICK (when
 * set) and exits with MANGO_FAKE_STATUS, like a real picker would. */
static int install_fake_pickers(void)
{
    static const char script[] =
        "#!/bin/sh\n"
        "if [ -n \"$MANGO_FAKE_PICK\" ]; then printf '%s\\n' \"$MANGO_FAKE_PICK\"; fi\n"
        "exit \"${MANGO_FAKE_STATUS:-1}\"\n";
    static const char *const names[] = {"osascript", "zenity"};
    char path[512], cwd[2048], new_path[8192];
    if (mkdir(FAKE_BIN, 0755) != 0 && errno != EEXIST) return -1;
    for (size_t i = 0; i < 2; i++) {
        snprintf(path, sizeof(path), FAKE_BIN "/%s", names[i]);
        FILE *file = fopen(path, "w");
        if (!file) return -1;
        fputs(script, file);
        if (fclose(file) != 0 || chmod(path, 0755) != 0) return -1;
    }
    const char *old_path = getenv("PATH");
    if (!old_path) old_path = "";
    /* PATH needs a full path. An absolute OUTDIR (make test OUTDIR=/tmp/x)
     * already is one; a relative OUTDIR gets the working directory in front. */
    if (FAKE_BIN[0] == '/') {
        snprintf(new_path, sizeof(new_path), FAKE_BIN ":%s", old_path);
    } else {
        if (!getcwd(cwd, sizeof(cwd))) return -1;
        snprintf(new_path, sizeof(new_path), "%s/" FAKE_BIN ":%s", cwd, old_path);
    }
    return setenv("PATH", new_path, 1);
}

static int pick(int save, const char *output, const char *status, char *buf, int size)
{
    if (output) setenv("MANGO_FAKE_PICK", output, 1);
    else unsetenv("MANGO_FAKE_PICK");
    setenv("MANGO_FAKE_STATUS", status, 1);
    int result = save ? file_dialog_save(buf, size) : file_dialog_open(buf, size);
    unsetenv("MANGO_FAKE_PICK");
    setenv("MANGO_FAKE_STATUS", "1", 1);
    return result;
}

static int native_pickers_report_choice_cancel_and_failure(void)
{
    int failed = 0;
    char buf[256];
    clear_dialog_seams();

    /* A picked file comes back without its line ending. */
    CHECK(pick(0, "/levels/one.toml", "0", buf, sizeof(buf)) == FILE_DIALOG_SELECTED);
    CHECK(strcmp(buf, "/levels/one.toml") == 0);
    /* Cancel: no output and exit status 1 (osascript/zenity convention). */
    CHECK(pick(0, NULL, "1", buf, sizeof(buf)) == FILE_DIALOG_CANCELLED);
    /* No output and a successful exit is also a cancel (PowerShell's). */
    CHECK(pick(0, NULL, "0", buf, sizeof(buf)) == FILE_DIALOG_CANCELLED);
    /* Any other exit status is a failure, even with a path printed. */
    CHECK(pick(0, NULL, "2", buf, sizeof(buf)) == FILE_DIALOG_ERROR);
    CHECK(pick(0, "/levels/one.toml", "4", buf, sizeof(buf)) == FILE_DIALOG_ERROR);

    /* Save adds ".toml" when the name has no extension, keeps any case of
     * an existing one, and refuses a name with no room for it. */
    CHECK(pick(1, "/levels/new", "0", buf, sizeof(buf)) == FILE_DIALOG_SELECTED);
    CHECK(strcmp(buf, "/levels/new.toml") == 0);
    CHECK(pick(1, "/levels/Loud.TOML", "0", buf, sizeof(buf)) == FILE_DIALOG_SELECTED);
    CHECK(strcmp(buf, "/levels/Loud.TOML") == 0);
    CHECK(pick(1, "/levels.d/plain", "0", buf, sizeof(buf)) == FILE_DIALOG_SELECTED);
    CHECK(strcmp(buf, "/levels.d/plain.toml") == 0);
    CHECK(pick(1, "/levels/abcdefgh", "0", buf, 20) == FILE_DIALOG_ERROR);
    CHECK(pick(1, NULL, "1", buf, sizeof(buf)) == FILE_DIALOG_CANCELLED);
    CHECK(pick(1, "/levels/x", "3", buf, sizeof(buf)) == FILE_DIALOG_ERROR);

    /* The editor's Open command with a cancelled picker keeps the document. */
    {
        EditorState es;
        CHECK(open_editor(&es, "levels/labs/04_climbing.toml") == 0);
        uint64_t hash = doc_hash(&es);
        editor_open_level_file(&es);
        CHECK(doc_hash(&es) == hash && es.modified == 0);
        close_editor(&es);
    }
done:
    return failed;
}

#endif

/* ------------------------------------------------------------------ */
/* In-window prompts                                                    */
/* ------------------------------------------------------------------ */

/* Queue a left-button press at one point and its release at another. */
static void push_press_release(int x0, int y0, int x1, int y1)
{
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, 0, x0, y0);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, 0, x1, y1);
}

/*
 * The "Skip this undo step?" prompt, with no canned answer in the way and
 * its input queued as a designer would give it: Enter presses the
 * highlighted button, which starts on "Keep the step"; Esc and closing
 * the window cancel; Tab moves the highlight.  A button is chosen by a
 * press and release on it.  Only Skip drops the step, and a prompt nobody
 * can answer keeps it.
 */
static int campaign_skip_prompt_keeps_the_step_by_default(void)
{
    int failed = 0;
    EditorState es;
    char cwd[4096];
    int moved = 0;
    DialogLayout layout;
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(enter_ui_campaign(cwd, sizeof(cwd)) == 0);
    moved = 1;
    CHECK(editor_campaign_open(&es, NULL) == 0);
    CHECK(editor_campaign_move(&es, 2, -1) == 0);               /* a c b */
    CHECK(editor_campaign_remove(&es, 0) == 0);                 /* c b */
    CHECK(remove("levels/a.toml") == 0);
    clear_dialog_seams();
    CHECK(editor_campaign_undo(&es) == -1);          /* refused; nothing asked */
    CHECK(editor_campaign_undo_blocked(&es) == 1);

    /* Nothing queued: the prompt is drawn, nobody answers, the step stays. */
    int frames = dialog_choice_test_frames();
    CHECK(editor_campaign_undo(&es) == -1);
    CHECK(strcmp(es.status_message, "Undo step kept: confirmation failed") == 0);
    CHECK(dialog_choice_test_frames() > frames);
    CHECK(strcmp(campaign_rows(&es), "cb") == 0 && editor_campaign_undo_steps(&es) == 2);
    layout = *dialog_choice_test_layout();
    CHECK(layout.button_count == 3);
    const int skip_x = layout.button_x[EDITOR_SKIP_STEP_SKIP] + layout.button_w[EDITOR_SKIP_STEP_SKIP] / 2;
    const int button_y = layout.button_y + layout.button_h / 2;

    /* Enter: the highlighted button, "Keep the step" to begin with. */
    push_key(KEY_ENTER, 0);
    CHECK(editor_campaign_undo(&es) == -1);
    CHECK(strstr(es.status_message, "Undo step kept: levels/a.toml") != NULL);
    CHECK(strcmp(campaign_rows(&es), "cb") == 0 && editor_campaign_undo_steps(&es) == 2);
    CHECK(editor_campaign_undo_blocked(&es) == 1);

    /* Esc, and closing the window, cancel. */
    input_clear();
    push_key(KEY_ESCAPE, 0);
    CHECK(editor_campaign_undo(&es) == -1);
    CHECK(strstr(es.status_message, "Undo step kept: levels/a.toml") != NULL);
    input_clear();
    push_event(INPUT_QUIT, 0, 0, 0, 0);
    CHECK(editor_campaign_undo(&es) == -1);
    CHECK(strcmp(campaign_rows(&es), "cb") == 0 && editor_campaign_undo_steps(&es) == 2);

    /* A press on Skip released elsewhere is no click; Tab twice from
     * Keep wraps round to Cancel, which Enter then presses. */
    input_clear();
    push_press_release(skip_x, button_y, skip_x, button_y - 60);
    push_key(KEY_TAB, 0);
    push_key(KEY_TAB, 0);
    push_key(KEY_ENTER, 0);
    CHECK(editor_campaign_undo(&es) == -1);
    CHECK(editor_campaign_undo_steps(&es) == 2);

    /* The Skip button, and nothing else, drops the step. */
    input_clear();
    push_press_release(skip_x, button_y, skip_x + 2, button_y + 1);
    CHECK(editor_campaign_undo(&es) == 2);
    CHECK(strcmp(es.status_message,
                 "Skipped: removed levels/a.toml (it stays out of the campaign)") == 0);
    CHECK(strcmp(campaign_rows(&es), "cb") == 0 && editor_campaign_undo_steps(&es) == 1);
    CHECK(editor_campaign_undo(&es) == 1);           /* c's move, one row higher */
    CHECK(strcmp(campaign_rows(&es), "bc") == 0);
done:
    input_clear();
    if (moved && test_change_folder(cwd) != 0) failed = 1;
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/*
 * A prompt that opens in the middle of drawing (a button acts when it is
 * drawn) leaves the frame's target and clip and puts both back, so the
 * frame goes on as before.  A mouse release it read for a press made
 * before it opened goes back to the editor.  The default and cancel
 * buttons may be the same one.
 */
static int prompt_returns_to_the_interrupted_frame(void)
{
    int failed = 0;
    EditorState es;
    InputEvent event;
    const char *labels[] = {"No", "Yes"};
    int choice = -1, released = 0;
    CHECK(open_editor(&es, NULL) == 0);

    BeginDrawing();
    BeginTextureMode(tiny_target);
    es.drawing_target = &tiny_target;
    ui_clip_begin(&es.ui, 10, 20, 30, 40);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, 0, 5, 5);  /* pressed before */
    push_key(KEY_TAB, 0);                                    /* Yes -> No */
    push_key(KEY_ENTER, 0);
    static const char message[] =
        "A message long enough to wrap onto a second line inside the box, with a "
        "path wider than the box itself: /home/someone/projects/super-mango/levels/"
        "campaigns/a-very-long-folder-name/another-folder/main-campaign-level.toml";
    CHECK(rlGetActiveFramebuffer() == tiny_target.id);
    int result = dialog_choice("Question", message, labels, 2, 1, 0, &choice);
    /* The frame's own target takes the draws again, not the screen the
     * prompt presented to. */
    CHECK(rlGetActiveFramebuffer() == tiny_target.id);
    CHECK(es.ui.clip_active && es.ui.clip_x == 10 && es.ui.clip_y == 20 &&
          es.ui.clip_w == 30 && es.ui.clip_h == 40);
    DrawRectangle(0, 0, 4, 4, ui_color(&es.ui, UI_COLOR_ACCENT));  /* still drawing */
    ui_clip_end(&es.ui);
    EndTextureMode();
    es.drawing_target = NULL;
    EndDrawing();
    CHECK(result == 0 && choice == 0);
    /* Wrapped at spaces, and the path, wider than the box, cut inside. */
    const DialogLayout *layout = dialog_choice_test_layout();
    CHECK(layout->line_count >= 3);
    for (int i = 0; i < layout->line_count; i++) {
        char line[256];
        CHECK(layout->line_length[i] > 0 && layout->line_length[i] < (int)sizeof(line));
        memcpy(line, message + layout->line_start[i], (size_t)layout->line_length[i]);
        line[layout->line_length[i]] = '\0';
        CHECK(ui_text_width(&es.ui, line) <= layout->box_w - 2 * (layout->text_x - layout->box_x));
    }
    CHECK(layout->line_start[layout->line_count - 1] + layout->line_length[layout->line_count - 1] ==
          (int)strlen(message));
    while (input_poll(&event))
        released |= event.type == INPUT_MOUSE_UP && event.button == MOUSE_BUTTON_LEFT;
    CHECK(released);

    /* Default and cancel on one button: Enter and Esc both choose it. */
    push_key(KEY_ESCAPE, 0);
    CHECK(dialog_choice("Question", "Message", labels, 2, 0, 0, &choice) == 0 && choice == 0);
    input_clear();

    /* A toolbar button's prompt opens mid-frame: New with unsaved changes.
     * Nothing answers it here, so nothing is lost and the frame finishes. */
    es.level.screen_count = 6;
    editor_refresh_dirty(&es);
    int frames = dialog_choice_test_frames();
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    editor_test_set_discard_choice(-1);
    push_click(EDITOR_W - 4 - 52 - 4 * 68 + 20, 16);         /* New */
    {
        /* ui_frame without arm_dialog_seams, which would answer for us. */
        InputEvent queued;
        ui_begin_frame(&es.ui);
        while (input_poll(&queued)) editor_handle_event(&es, &queued);
        es.ui.mouse_x = EDITOR_W - 4 - 52 - 4 * 68 + 20;
        es.ui.mouse_y = 16;
        BeginDrawing();
        BeginTextureMode(tiny_target);
        es.drawing_target = &tiny_target;
        editor_render_toolbar(&es);
        EndTextureMode();
        es.drawing_target = NULL;
        EndDrawing();
    }
    CHECK(dialog_choice_test_frames() > frames);
    CHECK(es.level.screen_count == 6 && es.modified);
    CHECK(strstr(es.status_message, "confirmation failed") != NULL);
done:
    input_clear();
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/* ------------------------------------------------------------------ */

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
#ifndef _WIN32
    /* Any native dialog the seams miss answers "Cancel" at once. */
    if (install_fake_pickers() != 0) {
        fprintf(stderr, "editor_ui_test: could not install fake pickers\n");
        return 1;
    }
#endif
    const struct { const char *name; int (*run)(void); } cases[] = {
#define CASE(fn) {#fn, fn}
        CASE(ui_scroll_keeps_fractions_and_stays_in_range),
        CASE(scrolled_out_field_wants_no_ibeam),
        CASE(ui_scrollbar_pages_drags_and_uses_its_clicks),
        CASE(scrollbar_drag_ends_with_its_bar),
        CASE(ui_styles_keep_the_default_look_and_high_contrast),
        CASE(toolbar_contrast_toggles_the_high_contrast_style),
        CASE(toolbar_toggles_show_what_is_on),
        CASE(toolbar_tooltips_wait_then_name_the_shortcut),
        CASE(floor_reaches_both_canvas_edges_at_any_scroll),
        CASE(palette_clicks_choose_what_the_place_tool_adds),
        CASE(canvas_place_select_drag_delete_and_undo),
        CASE(history_reports_and_reveals_the_step),
        CASE(mixed_step_reports_change_and_shows_survivor),

        CASE(minimap_jumps_and_drags_the_view),
        CASE(f1_sheet_lists_the_shortcuts),
        CASE(properties_panel_handles_every_entity_type),
        CASE(level_config_sections_resize_the_panel),
        CASE(text_fields_move_the_caret_and_tab_between_fields),
        CASE(up_and_down_step_number_fields),
        CASE(number_field_steps_suit_the_field),
        CASE(arrow_keys_nudge_and_backspace_deletes),
        CASE(mouse_cursor_follows_fields_and_drags),
        CASE(ctrl_d_duplicates_with_a_stepping_offset),
        CASE(snap_toggle_applies_to_placing_and_dragging),
        CASE(alt_click_cycles_through_overlapping_entities),
        CASE(validation_messages_take_you_to_the_problem),
        CASE(files_that_will_not_open_list_why),
        CASE(box_and_shift_select_act_on_the_group),
        CASE(validation_rows_scroll_and_copy),
        CASE(text_fields_select_copy_cut_and_paste),
        CASE(dropped_level_opens_like_a_recent_file),
        CASE(drop_during_the_prompt_waits_its_turn),
        CASE(shipped_wav_loop_opens_unmodified),
        CASE(campaign_view_reorders_renames_and_saves),
        CASE(campaign_undo_and_redo_replay_each_edit),
        CASE(campaign_rename_is_one_undo_step),
        CASE(campaign_history_keeps_the_latest_steps),
        CASE(campaign_undo_reads_the_level_file_again),
        CASE(campaign_unsaved_mark_follows_the_history),
        CASE(campaign_undo_keys_leave_the_level_history_alone),
        CASE(campaign_undo_and_redo_buttons_replay_each_edit),
        CASE(campaign_refused_undo_can_be_skipped),
        CASE(campaign_skipped_step_leaves_a_consistent_history),
#ifndef _WIN32
        CASE(playtest_status_follows_the_game_process),
        CASE(playtest_from_here_passes_the_start_point),
        CASE(native_pickers_report_choice_cancel_and_failure),
#endif
        CASE(campaign_skip_prompt_keeps_the_step_by_default),
        CASE(prompt_returns_to_the_interrupted_frame),
        CASE(unfocused_editor_slows_its_frames),
#undef CASE
    };
    int failures = 0;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        int result = cases[i].run();
        printf("editor_ui: %s %s\n", cases[i].name, result ? "FAIL" : "PASS");
        failures += result != 0;
    }
    printf("editor_ui_test: %d failing cases\n", failures);
    return failures ? 1 : 0;
}
