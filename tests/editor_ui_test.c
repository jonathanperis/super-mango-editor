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

#include "editor/asset_choices.h"
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
#include "editor/palette.h"
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

/* Where the selection's properties panel starts this frame. */
static int props_top(const EditorState *es)
{
    EditorSideLayout layout;
    editor_side_panel_layout(es, &layout);
    return layout.props_y;
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


/* The cases live in tests/editor_ui/ and are included here so they stay
 * one program. Do not compile those files on their own.
 */
#include "editor_ui/palette.c"
#include "editor_ui/canvas.c"
#include "editor_ui/properties.c"
#include "editor_ui/selection.c"
#include "editor_ui/campaign.c"
#include "editor_ui/group.c"
#include "editor_ui/text.c"
#include "editor_ui/playtest.c"
#include "editor_ui/prompts.c"


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
        CASE(raygui_icons_draw_their_bits),
        CASE(place_drag_paints_a_row),
        CASE(canvas_draws_the_level_layers_and_its_edges),
        CASE(canvas_navigates_like_a_level_editor),
        CASE(property_labels_explain_themselves),
        CASE(asset_dropdowns_offer_every_asset_file),
        CASE(side_column_follows_the_tool),
        CASE(palette_shows_what_place_will_add),
        CASE(a_drawable_mistake_keeps_the_canvas_editable),
        CASE(long_status_messages_show_whole_in_a_tooltip),
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
        CASE(float_fields_show_the_shortest_text),
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
        CASE(text_fields_keep_line_breaks_on_one_row),
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
