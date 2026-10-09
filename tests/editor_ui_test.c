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
#include "editor/editor_campaign.h"
#include "editor/editor.h"
#include "editor/editor_chrome.h"
#include "editor/editor_events.h"
#include "editor/editor_files.h"
#include "editor/editor_frame.h"
#include "editor/editor_layout.h"
#include "editor/editor_panels.h"
#include "editor/editor_playtest.h"
#include "editor/editor_session.h"
#include "editor/entity_meta.h"
#include "editor/file_dialog.h"
#include "editor/properties.h"
#include "editor/tools.h"
#include "levels/level_loader.h"
#include "shared/serializer.h"
#include "test_paths.h"

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

/* A dialog that would block the run instead returns its safe answer. */
static void arm_dialog_seams(void)
{
    editor_test_set_finish_field_choice(2);   /* Discard the field edit */
    file_dialog_test_set_open_result(FILE_DIALOG_CANCELLED, NULL);
    file_dialog_test_set_save_result(FILE_DIALOG_CANCELLED, NULL);
}

static void clear_dialog_seams(void)
{
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
    es->mouse_x = es->ui.mouse_x = mx;
    es->mouse_y = es->ui.mouse_y = my;
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
    if (es->playing) {
        editor_render_play_overlay(es);
    } else {
        canvas_render(es);
        editor_render_toolbar(es);
        editor_render_side_panels(es);
        editor_render_status_bar(es);
        ui_draw_overlays(&es->ui);
    }
    EndTextureMode();
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

static int palette_clicks_choose_what_the_place_tool_adds(void)
{
    int failed = 0;
    EditorState es;
    int picked[ENT_COUNT] = {0};
    CHECK(open_editor(&es, NULL) == 0);

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
    cfg_scroll(-100000);   /* an earlier case may have scrolled the panel */
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

    /* The panel shows them; clicking the heading hides them. */
    CHECK(es.config_open == 1);
    int with_list = editor_config_total_height(&es);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
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

#ifndef _WIN32
/* A scratch game folder: levels/a, b, c chained and listed in order. */
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

/* Click button `index` of the Campaign view's bottom row. */
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
    (void)mkdir(UI_CAMPAIGN_ROOT, 0755);
    (void)mkdir(UI_CAMPAIGN_ROOT "/levels", 0755);
    (void)mkdir(UI_CAMPAIGN_ROOT "/levels/campaigns", 0755);
    CHECK(write_ui_campaign_level(UI_CAMPAIGN_ROOT "/levels/a.toml", "Alpha", "levels/b.toml") == 0);
    CHECK(write_ui_campaign_level(UI_CAMPAIGN_ROOT "/levels/b.toml", "Bravo", "levels/c.toml") == 0);
    CHECK(write_ui_campaign_level(UI_CAMPAIGN_ROOT "/levels/c.toml", "Charlie", "") == 0);
    CHECK(write_text_file(UI_CAMPAIGN_ROOT "/levels/campaigns/main.toml",
                          "format_version = 1\nlevels = [\"levels/a.toml\", "
                          "\"levels/b.toml\", \"levels/c.toml\"]\n") == 0);
    CHECK(getcwd(cwd, sizeof(cwd)) != NULL);
    CHECK(chdir(UI_CAMPAIGN_ROOT) == 0);
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
    click_campaign_button(&es, 0);
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
    click_campaign_button(&es, 4);
    CHECK(editor_campaign_problem(&es)[0] == '\0');
    click_campaign_button(&es, 5);
    CHECK(strstr(es.status_message, "Campaign saved") != NULL);
    CHECK(campaign_catalog_load(CAMPAIGN_MANIFEST_PATH, &reloaded) == 0);
    CHECK(reloaded.count == 3 && strcmp(reloaded.levels[1].path, "levels/c.toml") == 0);
    CHECK(strcmp(reloaded.levels[0].display_name, "Alpha!") == 0);
    CHECK(reloaded.levels[0].available && reloaded.levels[1].available &&
          reloaded.levels[2].available);

    /* Esc goes back to the level. */
    key_frame(&es, KEY_ESCAPE, 0);
    CHECK(es.campaign == NULL);
    click_frame(&es, 400, 500);
    CHECK(es.level.coin_count == 1);
done:
    campaign_catalog_cleanup(&reloaded);
    if (moved && chdir(cwd) != 0) failed = 1;
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}
#endif

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
    CHECK(es.undo->top == undo_top + 3 && editor_selection_count(&es) == 3);
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

/* Level Config rows (the panel is open and unscrolled at start). */
#define CFG_NAME_Y    (TOOLBAR_H + 64 + 4)
#define CFG_SCREENS_Y (TOOLBAR_H + 136 + 4)

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

/* Stand-in for running the game: record the arguments and start a child
 * that exits at once, so the editor follows and reaps a real process. */
static int record_launch(const char *const *argv)
{
    pid_t child;
    launched_argc = 0;
    for (int i = 0; argv[i] && i < 8; i++) {
        snprintf(launched_args[i], sizeof(launched_args[i]), "%s", argv[i]);
        launched_argc++;
    }
    launch_count++;
    child = fork();
    if (child == 0) _exit(0);
    return child > 0 ? (int)child : -1;
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
        CASE(palette_clicks_choose_what_the_place_tool_adds),
        CASE(canvas_place_select_drag_delete_and_undo),
        CASE(properties_panel_handles_every_entity_type),
        CASE(level_config_sections_resize_the_panel),
        CASE(text_fields_move_the_caret_and_tab_between_fields),
        CASE(arrow_keys_nudge_and_backspace_deletes),
        CASE(ctrl_d_duplicates_with_a_stepping_offset),
        CASE(snap_toggle_applies_to_placing_and_dragging),
        CASE(alt_click_cycles_through_overlapping_entities),
        CASE(validation_messages_take_you_to_the_problem),
        CASE(files_that_will_not_open_list_why),
        CASE(box_and_shift_select_act_on_the_group),
#ifndef _WIN32
        CASE(playtest_status_follows_the_game_process),
        CASE(playtest_from_here_passes_the_start_point),
        CASE(campaign_view_reorders_renames_and_saves),
        CASE(native_pickers_report_choice_cancel_and_failure),
#endif
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
