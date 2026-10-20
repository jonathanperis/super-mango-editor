/*
 * editor_chrome.c — Editor toolbar and status bar rendering.
 */

#include "editor_chrome.h"
#include "editor_panels.h" /* editor_focus_validation_issue */

#include <stdio.h> /* snprintf */

#include "canvas.h"            /* canvas_set_zoom, canvas_screen_to_world */
#include "editor_campaign.h"   /* the Campaign view's button */
#include "editor_files.h"      /* editor file/save helpers */
#include "editor_playtest.h"   /* editor_play_test/editor_stop_play */
#include "editor_session.h"    /* editor reset/confirm helpers */
#include "editor_validation.h" /* editor_validation_summary */
#include "entity_meta.h"       /* editor_placed_entity_total */
#include "editor_minimap.h"    /* editor_minimap_render */
#include "editor_shortcuts.h"  /* editor_shortcut_tooltip */
#include "../shared/ui.h"      /* ui_panel, ui_button_icon, ui_label */

/*
 * editor_render_toolbar — Draw the top toolbar (32 px tall, full width).
 *
 * Layout from left to right:
 *   [Select] [Place] [Delete] | [Grid] | [Debug] | Zoom | [Campaign] |
 *   [Contrast] | file buttons
 *
 * The tools and the on/off buttons are ui_toggle: the current tool, a
 * shown grid, an open Campaign view and so on draw in the selected colour.
 * Every button but the zoom list has a raygui icon before its label
 * (vendor/raygui/raygui_icons.h).
 *
 * Each control has a tooltip naming its shortcut, when it has one, built
 * from the shortcut table the F1 list shows too (editor_shortcuts.c).
 */
void editor_render_toolbar(EditorState *es)
{
    ui_panel(&es->ui, 0, 0, EDITOR_W, TOOLBAR_H);

    int bx = 4;
    int by = 4;
    int bw = 64;
    int bh = 24;

    if (ui_toggle_icon(&es->ui, bx, by, bw, bh, UI_ICON_SELECT, "Select", es->tool == TOOL_SELECT)) {
        es->tool = TOOL_SELECT;
    }
    ui_tooltip(&es->ui, editor_shortcut_tooltip(EDITOR_SHORTCUT_SELECT_TOOL));

    bx += bw + 4;
    if (ui_toggle_icon(&es->ui, bx, by, bw, bh, UI_ICON_PLACE, "Place", es->tool == TOOL_PLACE)) {
        es->tool = TOOL_PLACE;
    }
    ui_tooltip(&es->ui, editor_shortcut_tooltip(EDITOR_SHORTCUT_PLACE_TOOL));

    bx += bw + 4;
    if (ui_toggle_icon(&es->ui, bx, by, bw, bh, UI_ICON_DELETE, "Delete", es->tool == TOOL_DELETE)) {
        es->tool = TOOL_DELETE;
    }
    ui_tooltip(&es->ui, editor_shortcut_tooltip(EDITOR_SHORTCUT_DELETE_TOOL));

    bx += bw + 20;
    if (ui_toggle_icon(&es->ui, bx, by, 56, bh, UI_ICON_GRID, "Grid", es->show_grid)) {
        es->show_grid ^= 1;
    }
    ui_tooltip(&es->ui, editor_shortcut_tooltip(EDITOR_SHORTCUT_GRID));

    bx += 60;
    if (ui_toggle_icon(&es->ui, bx, by, 56, bh, UI_ICON_DEBUG, "Debug", es->debug_play)) {
        es->debug_play ^= 1;
    }
    ui_tooltip(&es->ui, "Playtest with the game's --debug overlay");

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
    ui_tooltip(&es->ui, editor_shortcut_tooltip(EDITOR_SHORTCUT_ZOOM));

    /* The Campaign view (Ctrl+M) covers the canvas; the button closes it
     * again (asking first about unsaved campaign edits). */
    bx += 88;
    if (ui_toggle_icon(&es->ui, bx, by, 84, bh, UI_ICON_CAMPAIGN, "Campaign", es->campaign != NULL)) {
        if (es->campaign) (void)editor_campaign_close(es, 0);
        else (void)editor_campaign_open(es, NULL);
    }
    ui_tooltip(&es->ui, editor_shortcut_tooltip(EDITOR_SHORTCUT_CAMPAIGN));

    /* High contrast re-themes every widget and panel (UI_STYLE_HIGH_CONTRAST:
     * black faces, white outlines, text at 7:1 or more). The level canvas
     * keeps its own colours. */
    bx += 88;
    if (ui_toggle_icon(&es->ui, bx, by, 84, bh, UI_ICON_CONTRAST, "Contrast", es->high_contrast)) {
        editor_set_high_contrast(es, !es->high_contrast);
        /* Remembered for the next session; without a preference folder
         * (render smoke) it simply stays for this one. */
        if (es->settings_path[0] != '\0' && editor_save_settings(es) != 0)
            editor_set_status(es, "Could not save the Contrast setting");
    }
    ui_tooltip(&es->ui, "High-contrast colours");

    int rx = EDITOR_W - 4 - 52;
    if (es->playing) {
        if (ui_button_icon(&es->ui, rx, by, 52, bh, UI_ICON_STOP, "Stop")) {
            editor_stop_play(es);
        }
        ui_tooltip(&es->ui, "Stop the playtest");
    } else {
        if (ui_button_icon(&es->ui, rx, by, 52, bh, UI_ICON_PLAY, "Play")) {
            editor_play_test(es);
        }
        ui_tooltip(&es->ui, editor_shortcut_tooltip(EDITOR_SHORTCUT_PLAYTEST));
    }

    rx -= 64 + 4;
    if (ui_button_icon(&es->ui, rx, by, 64, bh, UI_ICON_SAVE, "Save")) {
        (void)editor_save_current_level(es);
    }
    ui_tooltip(&es->ui, editor_shortcut_tooltip(EDITOR_SHORTCUT_SAVE));

    rx -= 64 + 4;
    if (ui_button_icon(&es->ui, rx, by, 64, bh, UI_ICON_SAVE_AS, "Save As")) {
        (void)editor_save_current_level_as(es);
    }
    ui_tooltip(&es->ui, editor_shortcut_tooltip(EDITOR_SHORTCUT_SAVE_AS));

    rx -= 64 + 4;
    if (ui_button_icon(&es->ui, rx, by, 64, bh, UI_ICON_OPEN, "Open")) {
        if (editor_confirm_discard_changes(es, "open another level")) {
            editor_open_level_file(es);
        }
    }
    ui_tooltip(&es->ui, editor_shortcut_tooltip(EDITOR_SHORTCUT_OPEN));

    rx -= 64 + 4;
    if (ui_button_icon(&es->ui, rx, by, 64, bh, UI_ICON_NEW, "New")) {
        if (editor_confirm_discard_changes(es, "create a new level")) {
            editor_reset_new_level(es);
        }
    }
    ui_tooltip(&es->ui, editor_shortcut_tooltip(EDITOR_SHORTCUT_NEW));
}

void editor_status_tool_text(const EditorState *es, char *out, size_t out_size)
{
    static const char *tool_names[] = { "Select", "Place", "Delete" };
    /* Placing, the bar names what a click adds. */
    if (es->tool == TOOL_PLACE)
        snprintf(out, out_size, "Place: %s", editor_entity_palette_name(es->palette_type));
    else
        snprintf(out, out_size, "Tool: %s",
                 (es->tool >= 0 && es->tool < 3) ? tool_names[es->tool] : "Unknown");
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
                   es->snap_to_grid ? ui_color(&es->ui, UI_COLOR_ACCENT) : ui_color(&es->ui, UI_COLOR_TEXT_DIM));

    char tool_text[64];
    editor_status_tool_text(es, tool_text, sizeof(tool_text));
    ui_label(&es->ui, 228, bar_y + 8, tool_text);

    /* The validation summary is a link: clicking it goes to the first
     * message's entity or field, like clicking that message in the panel. */
    {
        const char *summary = editor_validation_summary(&es->validation_report);
        int width = ui_text_width(&es->ui, summary);
        int clickable = es->validation_report.message_count > 0;
        /* Above the overview strip, whose rows take their own clicks. */
        int hovered = clickable &&
                      es->ui.mouse_x >= STATUS_SUMMARY_X && es->ui.mouse_x < STATUS_SUMMARY_X + width &&
                      es->ui.mouse_y >= bar_y && es->ui.mouse_y < EDITOR_MINIMAP_Y;
        ui_label_color(&es->ui, STATUS_SUMMARY_X, bar_y + 8, summary,
                       es->validation_report.error_count > 0 ?
                       ui_color(&es->ui, UI_COLOR_ERROR) : ui_color(&es->ui, UI_COLOR_TEXT_DIM));
        if (hovered) DrawRectangle(STATUS_SUMMARY_X, bar_y + 22, width, 1, ui_color(&es->ui, UI_COLOR_TEXT_DIM));
        if (hovered && es->ui.mouse_clicked) {
            es->ui.mouse_clicked = 0;
            (void)editor_focus_validation_issue(es, 0);
        }
    }

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
    ui_label(&es->ui, STATUS_ENTITIES_X, bar_y + 8, info_text);
    if (es->status_message[0] != '\0') {
        ui_label_color(&es->ui, 920, bar_y + 8,
                       es->status_message, ui_color(&es->ui, UI_COLOR_TEXT_DIM));
    }
    /* Last, in the rows under the text: the level overview strip. */
    editor_minimap_render(es);
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
                   ui_color(&es->ui, UI_COLOR_TEXT_DIM));

    if (ui_button(&es->ui, EDITOR_W / 2 - 40, EDITOR_H / 2 + 30,
                  80, 28, "Stop")) {
        editor_stop_play(es);
    }
}
