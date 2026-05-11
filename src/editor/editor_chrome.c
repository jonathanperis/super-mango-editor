/*
 * editor_chrome.c — Editor toolbar and status bar rendering.
 */

#include "editor_chrome.h"

#include <SDL.h>   /* SDL_Rect, SDL_RenderFillRect */
#include <stdio.h> /* snprintf */

#include "editor_files.h"      /* editor file/save/export helpers */
#include "editor_playtest.h"   /* editor_play_test/editor_stop_play */
#include "editor_session.h"    /* editor reset/confirm helpers */
#include "editor_validation.h" /* editor_validation_summary */
#include "ui.h"                /* ui_panel, ui_button, ui_label */

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
        SDL_SetRenderDrawColor(es->renderer,
                               UI_BTN_ACTIVE.r, UI_BTN_ACTIVE.g,
                               UI_BTN_ACTIVE.b, UI_BTN_ACTIVE.a);
        SDL_Rect underline = { bx, by + bh, bw, 2 };
        SDL_RenderFillRect(es->renderer, &underline);
    }

    bx += bw + 4;
    if (ui_button(&es->ui, bx, by, bw, bh, "Place")) {
        es->tool = TOOL_PLACE;
    }
    if (es->tool == TOOL_PLACE) {
        SDL_SetRenderDrawColor(es->renderer,
                               UI_BTN_ACTIVE.r, UI_BTN_ACTIVE.g,
                               UI_BTN_ACTIVE.b, UI_BTN_ACTIVE.a);
        SDL_Rect underline = { bx, by + bh, bw, 2 };
        SDL_RenderFillRect(es->renderer, &underline);
    }

    bx += bw + 4;
    if (ui_button(&es->ui, bx, by, bw, bh, "Delete")) {
        es->tool = TOOL_DELETE;
    }
    if (es->tool == TOOL_DELETE) {
        SDL_SetRenderDrawColor(es->renderer,
                               UI_BTN_ACTIVE.r, UI_BTN_ACTIVE.g,
                               UI_BTN_ACTIVE.b, UI_BTN_ACTIVE.a);
        SDL_Rect underline = { bx, by + bh, bw, 2 };
        SDL_RenderFillRect(es->renderer, &underline);
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
    static const float zoom_vals[] = { 1.0f, 2.0f, 3.0f, 5.0f };
    static const int zoom_count = 4;

    int sel = 1;
    for (int zi = 0; zi < zoom_count; zi++) {
        if (es->camera.zoom == zoom_vals[zi]) { sel = zi; break; }
    }
    if (ui_dropdown(&es->ui, 8888, bx, by + 2, 80,
                    zoom_opts, zoom_count, &sel)) {
        es->camera.zoom = zoom_vals[sel];
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
    if (ui_button(&es->ui, rx, by, 64, bh, "Export")) {
        (void)editor_export_current_level(es);
    }

    rx -= 64 + 4;
    if (ui_button(&es->ui, rx, by, 64, bh, "Save")) {
        (void)editor_save_current_level(es);
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

    float wx = (float)es->mouse_x / es->camera.zoom + es->camera.x;
    float wy = (float)(es->mouse_y - TOOLBAR_H) / es->camera.zoom;

    char mouse_text[64];
    snprintf(mouse_text, sizeof(mouse_text), "Mouse: (%.0f, %.0f)", wx, wy);
    ui_label(&es->ui, 8, bar_y + 8, mouse_text);

    const char *tool_names[] = { "Select", "Place", "Delete" };
    const char *tool_name = (es->tool >= 0 && es->tool < 3)
                            ? tool_names[es->tool]
                            : "Unknown";

    char tool_text[64];
    snprintf(tool_text, sizeof(tool_text), "Tool: %s", tool_name);
    ui_label(&es->ui, 210, bar_y + 8, tool_text);

    ui_label_color(&es->ui, 330, bar_y + 8,
                   editor_validation_summary(&es->validation_report),
                   es->validation_report.error_count > 0 ?
                   (SDL_Color){0xFF,0x70,0x70,0xFF} : UI_TEXT_DIM);

    int total = es->level.floor_gap_count
              + es->level.rail_count
              + es->level.platform_count
              + es->level.coin_count
              + es->level.star_yellow_count
              + es->level.star_green_count
              + es->level.star_red_count
              + (es->level.last_star.x != 0.0f || es->level.last_star.y != 0.0f
                 ? 1 : 0)
              + es->level.spider_count
              + es->level.jumping_spider_count
              + es->level.bird_count
              + es->level.faster_bird_count
              + es->level.fish_count
              + es->level.faster_fish_count
              + es->level.blue_flame_count
              + es->level.fire_flame_count
              + es->level.axe_trap_count
              + es->level.circular_saw_count
              + es->level.spike_row_count
              + es->level.spike_platform_count
              + es->level.spike_block_count
              + es->level.float_platform_count
              + es->level.bridge_count
              + es->level.bouncepad_small_count
              + es->level.bouncepad_medium_count
              + es->level.bouncepad_high_count
              + es->level.vine_count
              + es->level.ladder_count
              + es->level.rope_count;

    char info_text[512];
    if (es->file_path[0] != '\0') {
        snprintf(info_text, sizeof(info_text), "Entities: %d  |  %s%s",
                 total, es->file_path, es->modified ? " *" : "");
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
