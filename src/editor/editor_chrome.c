/*
 * editor_chrome.c — Editor toolbar and status bar rendering.
 */

#include "editor_chrome.h"

#include <stdio.h> /* snprintf */

#include "canvas.h"            /* canvas_set_zoom, canvas_screen_to_world */
#include "editor_files.h"      /* editor file/save helpers */
#include "editor_playtest.h"   /* editor_play_test/editor_stop_play */
#include "editor_session.h"    /* editor reset/confirm helpers */
#include "editor_validation.h" /* editor_validation_summary */
#include "entity_meta.h"       /* editor_placed_entity_total */
#include "../shared/ui.h"      /* ui_panel, ui_button, ui_label */

/*
 * editor_render_toolbar — Draw the top toolbar (32 px tall, full width).
 *
 * Layout from left to right:
 *   [Select] [Place] [Delete] | [Grid] | [Debug] | Zoom | file buttons
 */
void editor_render_toolbar(EditorState *es)
{
    ui_panel(&es->ui, 0, 0, EDITOR_W, TOOLBAR_H);

    int bx = 4;
    int by = 4;
    int bw = 64;
    int bh = 24;

    if (ui_button(&es->ui, bx, by, bw, bh, "Select")) {
        es->tool = TOOL_SELECT;
    }
    if (es->tool == TOOL_SELECT) {
        DrawRectangle(bx, by+bh, bw, 2, UI_BTN_ACTIVE);
    }

    bx += bw + 4;
    if (ui_button(&es->ui, bx, by, bw, bh, "Place")) {
        es->tool = TOOL_PLACE;
    }
    if (es->tool == TOOL_PLACE) {
        DrawRectangle(bx, by+bh, bw, 2, UI_BTN_ACTIVE);
    }

    bx += bw + 4;
    if (ui_button(&es->ui, bx, by, bw, bh, "Delete")) {
        es->tool = TOOL_DELETE;
    }
    if (es->tool == TOOL_DELETE) {
        DrawRectangle(bx, by+bh, bw, 2, UI_BTN_ACTIVE);
    }

    bx += bw + 20;
    const char *grid_label = es->show_grid ? "[Grid]" : " Grid ";
    if (ui_button(&es->ui, bx, by, 56, bh, grid_label)) {
        es->show_grid ^= 1;
    }

    bx += 60;
    const char *dbg_label = es->debug_play ? "[Debug]" : " Debug ";
    if (ui_button(&es->ui, bx, by, 56, bh, dbg_label)) {
        es->debug_play ^= 1;
    }

    bx += 60;
    static const char *zoom_opts[] = {
        "Zoom: 1x", "Zoom: 2x", "Zoom: 3x", "Zoom: 5x"
    };
    /* Zoom is always one of these whole numbers (startup, this dropdown and
     * Ctrl+wheel assign them; nothing accumulates), so compare integers. */
    static const int zoom_vals[] = { 1, 2, 3, 5 };
    static const int zoom_count = 4;

    int sel = 1;
    for (int zi = 0; zi < zoom_count; zi++) {
        if ((int)es->camera.zoom == zoom_vals[zi]) { sel = zi; break; }
    }
    if (ui_dropdown(&es->ui, 8888, bx, by + 2, 80,
                    zoom_opts, zoom_count, &sel)) {
        /* Zoom around the canvas centre so the view stays on the same area. */
        canvas_set_zoom(es, (float)zoom_vals[sel], CANVAS_W / 2, TOOLBAR_H + CANVAS_H / 2);
    }

    int rx = EDITOR_W - 4 - 52;
    if (es->playing) {
        if (ui_button(&es->ui, rx, by, 52, bh, "Stop")) {
            editor_stop_play(es);
        }
    } else {
        if (ui_button(&es->ui, rx, by, 52, bh, "Play")) {
            editor_play_test(es);
        }
    }

    rx -= 64 + 4;
    if (ui_button(&es->ui, rx, by, 64, bh, "Save")) {
        (void)editor_save_current_level(es);
    }

    rx -= 64 + 4;
    if (ui_button(&es->ui, rx, by, 64, bh, "Save As")) {
        (void)editor_save_current_level_as(es);
    }

    rx -= 64 + 4;
    if (ui_button(&es->ui, rx, by, 64, bh, "Open")) {
        if (editor_confirm_discard_changes(es, "open another level")) {
            editor_open_level_file(es);
        }
    }

    rx -= 64 + 4;
    if (ui_button(&es->ui, rx, by, 64, bh, "New")) {
        if (editor_confirm_discard_changes(es, "create a new level")) {
            editor_reset_new_level(es);
        }
    }
}

/*
 * editor_render_status_bar — Draw mouse, tool, validation, and file status.
 */
void editor_render_status_bar(EditorState *es)
{
    int bar_y = EDITOR_H - STATUS_H;
    ui_panel(&es->ui, 0, bar_y, EDITOR_W, STATUS_H);

    float wx, wy;
    canvas_screen_to_world(es, es->mouse_x, es->mouse_y, &wx, &wy);

    char mouse_text[64];
    snprintf(mouse_text, sizeof(mouse_text), "Mouse: (%.0f, %.0f)", wx, wy);
    ui_label(&es->ui, 8, bar_y + 8, mouse_text);

    /* Snap indicator: bright while on, dim while off (key S toggles). */
    ui_label_color(&es->ui, 150, bar_y + 8,
                   es->snap_to_grid ? "Snap: on" : "Snap: off",
                   es->snap_to_grid ? UI_ACCENT : UI_TEXT_DIM);

    const char *tool_names[] = { "Select", "Place", "Delete" };
    const char *tool_name = (es->tool >= 0 && es->tool < 3)
                            ? tool_names[es->tool]
                            : "Unknown";

    char tool_text[64];
    snprintf(tool_text, sizeof(tool_text), "Tool: %s", tool_name);
    ui_label(&es->ui, 228, bar_y + 8, tool_text);

    ui_label_color(&es->ui, 330, bar_y + 8,
                   editor_validation_summary(&es->validation_report),
                   es->validation_report.error_count > 0 ?
                   (Color){0xFF,0x70,0x70,0xFF} : UI_TEXT_DIM);

    int total = editor_placed_entity_total(&es->level);

    char info_text[512];
    if (es->file_path[0] != '\0') {
        /* About 40 characters fit between x=600 and the status message at
         * x=920; a longer path keeps its file name and starts with "...". */
        char shown_path[40];
        editor_path_for_display(es->file_path, shown_path, sizeof(shown_path));
        snprintf(info_text, sizeof(info_text), "Entities: %d  |  %s%s",
                 total, shown_path, es->modified ? " *" : "");
    } else {
        snprintf(info_text, sizeof(info_text), "Entities: %d  |  (untitled)%s",
                 total, es->modified ? " *" : "");
    }
    ui_label(&es->ui, 600, bar_y + 8, info_text);
    if (es->status_message[0] != '\0') {
        ui_label_color(&es->ui, 920, bar_y + 8,
                       es->status_message, UI_TEXT_DIM);
    }
}

/*
 * editor_render_play_overlay — Draw play-test mode message and stop button.
 */
void editor_render_play_overlay(EditorState *es)
{
    ui_label(&es->ui, EDITOR_W / 2 - 60, EDITOR_H / 2 - 40,
             "Playing level...");
    ui_label_color(&es->ui, EDITOR_W / 2 - 80, EDITOR_H / 2 - 10,
                   "Close the game window or click Stop",
                   UI_TEXT_DIM);

    if (ui_button(&es->ui, EDITOR_W / 2 - 40, EDITOR_H / 2 + 30,
                  80, 28, "Stop")) {
        editor_stop_play(es);
    }
}
