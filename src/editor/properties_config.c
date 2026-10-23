/*
 * properties_config.c — The Level Config panel.
 *
 * Drawn top to bottom by one config_* helper per settings group.
 * level_config_render is that list. Widget ids here are the 9000+ literals
 * properties_focus_config jumps to; they stay next to their widgets.
 */

#include <float.h>      /* FLT_MAX for one-sided field limits    */
#include <limits.h>     /* INT_MIN, INT_MAX for unlimited int fields */
#include <stdio.h>      /* snprintf for header label formatting */
#include <string.h>     /* strrchr for filename extraction       */

#include "properties.h"
#include "asset_choices.h" /* the files each asset dropdown offers */
#include "../screens/hud.h"  /* MAX_HEARTS */
#include "editor.h"     /* EditorState, EntityType, CANVAS_W, PANEL_W, etc. */
#include "editor_undo_apply.h" /* committed property/config command tracking */
#include "editor_files.h"  /* editor_path_for_display for recent entries   */
#include "entity_meta.h" /* editor_entity_type_name/is_singleton             */
#include "tools.h"       /* editor_set_float_platform_mode                    */
#include "editor_panels.h" /* editor_focus_validation_issue                     */
#include "editor_session.h" /* editor_set_status (show_field_note)              */
#include "editor_playtest.h" /* editor_play_test_from (checkpoint "Playtest here") */
#include "../shared/ui.h" /* ui_panel, ui_label, ui_separator, ui_float_field,
                           ui_int_field, ui_dropdown                         */
#include "../levels/level.h" /* LevelDef, all *Placement structs            */
#include "../player/player.h"  /* JUMP_VY (bouncepad launch limit)           */
#include "../surfaces/rail.h"  /* MAX_RAIL_SPEED (rail rider speed limit)    */
#include "../game_constants.h" /* MAX_PATROL_SPEED (enemy vx limit)          */
#include "properties_internal.h"

/* The part of the screen the scrolled config content shows this frame
 * (set by level_config_render); rows outside it cannot be clicked. */
static int cfg_clip_top = 0;
static int cfg_clip_bottom = 0;

/* ================================================================== */
/* level_config_render — Level-wide configuration panel                 */
/* ================================================================== */

/*
 * The Level Config panel is drawn top to bottom by the config_* helpers
 * below, one per group of settings.  Each takes the running row position y
 * and returns where the next group starts, so level_config_render reads as
 * the list of groups in panel order.
 *
 * Widget IDs use the 9000+ range to avoid collisions with entity fields
 * (FIELD_ID stays below 9000).  Every ID is a literal next to its widget,
 * so moving a field between helpers cannot change which ID it has.
 */

/* Is the mouse over the 18-px header row of a subsection at y, on the part
 * of Level Config that shows?  A header scrolled out of the panel's view
 * must not take a click meant for the panel drawn there. */
static int config_row_hovered(const EditorState *es, int x, int y)
{
    return es->ui.mouse_x >= x &&
           es->ui.mouse_x < x + PROP_W &&
           es->ui.mouse_y >= y &&
           es->ui.mouse_y < y + 18 &&
           es->ui.mouse_y >= cfg_clip_top && es->ui.mouse_y < cfg_clip_bottom;
}

/*
 * config_subsection_header — Draw a ">"/"v" subsection header such as
 * "Physics" or "Background Layers (2)" and toggle *open when it is clicked.
 * The caller decides the hover area (the fog header uses a narrower one).
 */
static void config_subsection_header(EditorState *es, int x, int y,
                                     const char *label, int *open, int hovered)
{
    if (hovered && es->ui.mouse_clicked) *open = !*open;

    const char *sym = *open ? "v" : ">";
    int sym_w = ui_text_width(&es->ui, sym);
    ui_label_color(&es->ui, x + 8, y, sym, ui_color(&es->ui, UI_COLOR_ACCENT));
    ui_label_color(&es->ui, x + 8 + sym_w, y, label,
                   hovered ? ui_color(&es->ui, UI_COLOR_TEXT) : ui_color(&es->ui, UI_COLOR_TEXT_DIM));
}

/* Where the validation rows were drawn this frame, for the wheel; empty
 * when they all fit and there is nothing to scroll. */
static int msg_list_top = 0;
static int msg_list_bottom = 0;

int properties_message_list_contains(int x, int y)
{
    return x >= PROP_X && y >= msg_list_top && y < msg_list_bottom &&
           y >= cfg_clip_top && y < cfg_clip_bottom;
}

/*
 * message_row — One message row, error or warning or load problem.  The
 * row clicked last is highlighted while the list has the focus, and Ctrl+C
 * copies it (editor_events.c).  A click on the row is used up here, so no
 * widget below sees it; returns 1 on that click.
 */
static int message_row(EditorState *es, int x, int y, const char *text, Color color)
{
    int hovered = es->ui.mouse_x >= x && es->ui.mouse_x < x + PROP_W &&
                  es->ui.mouse_y >= y && es->ui.mouse_y < y + 18 &&
                  es->ui.mouse_y >= cfg_clip_top && es->ui.mouse_y < cfg_clip_bottom;
    int selected = es->focus_area == EDITOR_FOCUS_MESSAGES &&
                   strcmp(es->selected_message, text) == 0;

    if (selected) DrawRectangle(x + 4, y - 1, PROP_W - 8, 18, ui_color(&es->ui, UI_COLOR_SELECTED));
    else if (hovered) DrawRectangle(x + 4, y - 1, PROP_W - 8, 18, ui_color(&es->ui, UI_COLOR_HOT));
    /* A long message ends in "..." at the panel's edge; resting the
     * pointer on it shows all of it. */
    (void)ui_label_fit(&es->ui, x + 16, y, PROP_W - 24, text, color);
    if (!hovered || !es->ui.mouse_clicked) return 0;
    es->ui.mouse_clicked = 0;
    snprintf(es->selected_message, sizeof(es->selected_message), "%s", text);
    es->focus_area = EDITOR_FOCUS_MESSAGES;
    return 1;
}

/*
 * Validation result and every error/warning message.  Each message is a
 * row the designer can click: it selects the row (Ctrl+C then copies it)
 * and goes to what the message is about: the entity (panning the canvas
 * to it) or the Level Config field.  When there are more messages than
 * EDITOR_VALIDATION_VISIBLE_ROWS, the wheel over the rows scrolls them.
 */
static int config_load_problems(EditorState *es, int x, int y);

static int config_validation(EditorState *es, int x, int y)
{
    const EditorValidationReport *report = &es->validation_report;
    int count = report->message_count;
    int shown = count < EDITOR_VALIDATION_VISIBLE_ROWS ? count : EDITOR_VALIDATION_VISIBLE_ROWS;
    int first;

    y = config_load_problems(es, x, y);
    ui_label_color(&es->ui, x + 8, y, editor_validation_summary(report),
                   report->error_count > 0 ? ui_color(&es->ui, UI_COLOR_ERROR) : ui_color(&es->ui, UI_COLOR_TEXT_DIM));
    y += 20;

    /* Keep the window on the list as it shrinks or grows. */
    ui_scroll_clamp(&es->message_scroll, count, shown);
    first = es->message_scroll.offset;
    msg_list_top = msg_list_bottom = 0;
    if (count > shown) {
        msg_list_top = y;
        msg_list_bottom = y + shown * 18;
    }
    for (int i = first; i < first + shown; i++) {
        Color msg_color = i < report->error_count ? ui_color(&es->ui, UI_COLOR_ERROR)
                                                  : (Color){0xFF,0xC0,0x60,0xFF};
        if (message_row(es, x, y, report->messages[i], msg_color) &&
            report->locations[i].path[0] != '\0')
            (void)editor_focus_validation_issue(es, i);
        y += 18;
    }
    if (count > shown) {
        char where[96];   /* the words below with three row numbers */
        snprintf(where, sizeof(where), "Rows %d-%d of %d (the wheel scrolls them)",
                 first + 1, first + shown, count);
        ui_label_color(&es->ui, x + 16, y, where, ui_color(&es->ui, UI_COLOR_TEXT_DIM));
        y += 18;
    }
    /* The report keeps EDITOR_VALIDATION_MAX_MESSAGES; say how many more
     * the counts include, so a long report is not mistaken for a short one. */
    {
        int hidden = editor_validation_hidden_count(report);
        if (hidden > 0) {
            char more[48];
            snprintf(more, sizeof(more), "... and %d more", hidden);
            ui_label_color(&es->ui, x + 16, y, more, ui_color(&es->ui, UI_COLOR_TEXT_DIM));
            y += 18;
        }
    }
    ui_separator(&es->ui, x + 4, y, PROP_W - 8);
    return y + 8;
}

/*
 * config_load_problems — Why the last Open failed (nothing when it did
 * not).  The file never became the document, so its rows cannot take the
 * designer anywhere, but a click selects one for Ctrl+C (to search for the
 * line it names, say).  Clicking the heading hides the list.
 */
static int config_load_problems(EditorState *es, int x, int y)
{
    const EditorLoadReport *report = &es->load_report;
    char heading[96];
    int hovered;

    if (report->count <= 0) return y;
    snprintf(heading, sizeof(heading), "Could not open %s (click to hide):",
             report->file);
    hovered = es->ui.mouse_x >= x && es->ui.mouse_x < x + PROP_W &&
              es->ui.mouse_y >= y && es->ui.mouse_y < y + 18 &&
              es->ui.mouse_y >= cfg_clip_top && es->ui.mouse_y < cfg_clip_bottom;
    if (hovered) DrawRectangle(x + 4, y - 1, PROP_W - 8, 18, ui_color(&es->ui, UI_COLOR_HOT));
    ui_label_color(&es->ui, x + 8, y, heading, ui_color(&es->ui, UI_COLOR_ERROR));
    if (hovered && es->ui.mouse_clicked) {
        es->ui.mouse_clicked = 0;
        es->load_report.count = 0;
        es->load_report.total = 0;
        return y + 18;
    }
    y += 18;
    for (int i = 0; i < report->count; i++) {
        (void)message_row(es, x, y, report->messages[i], ui_color(&es->ui, UI_COLOR_ERROR));
        y += 18;
    }
    if (report->total > report->count) {
        char more[48];
        snprintf(more, sizeof(more), "... and %d more", report->total - report->count);
        ui_label_color(&es->ui, x + 16, y, more, ui_color(&es->ui, UI_COLOR_TEXT_DIM));
        y += 18;
    }
    ui_separator(&es->ui, x + 4, y, PROP_W - 8);
    return y + 8;
}

/* The recent-file list with its Ctrl+1..5 shortcuts (nothing when empty). */
static int config_recent_files(EditorState *es, int x, int y)
{
    if (es->recent_file_count <= 0) return y;

    ui_label_color(&es->ui, x + 8, y, "Recent files (Ctrl+1..5):", ui_color(&es->ui, UI_COLOR_TEXT_DIM));
    y += 18;
    for (int i = 0; i < es->recent_file_count; i++) {
        /* Keep each entry to one panel-width line: the file name at the
         * end of the path is the part that tells entries apart. */
        char shown_path[48];
        char recent[64];
        editor_path_for_display(es->recent_files[i], shown_path, sizeof(shown_path));
        snprintf(recent, sizeof(recent), "%d: %s", i + 1, shown_path);
        ui_label_color(&es->ui, x + 16, y, recent, ui_color(&es->ui, UI_COLOR_TEXT_DIM));
        y += 18;
    }
    ui_separator(&es->ui, x + 4, y, PROP_W - 8);
    return y + 8;
}

/* Name, description, author, world width and the next-phase path. */
static int config_level_fields(EditorState *es, int x, int y)
{
    ui_label(&es->ui, x + 8, y, "Name:");
    if (ui_text_field_hint(&es->ui, 9000, x + 55, y, 310, es->level.name,
                           (int)sizeof(es->level.name), "Untitled", 0))
        editor_commit_change(es);
    y += 24;

    ui_label(&es->ui, x + 8, y, "Desc:");
    /* The description may run over several lines (Shift+Enter). */
    if (ui_text_field_hint(&es->ui, 9001, x + 55, y, 310, es->level.description,
                           (int)sizeof(es->level.description), "(no description)", 1))
        editor_commit_change(es);
    y += 24;

    ui_label(&es->ui, x + 8, y, "By:");
    if (ui_text_field_hint(&es->ui, 9002, x + 55, y, 310, es->level.generated_by,
                           (int)sizeof(es->level.generated_by), "(no author)", 0))
        editor_commit_change(es);
    y += 24;

    /* ---- World Width (screen count) ---- */
    ui_label(&es->ui, x + 8, y, "Screens:");
    /* The limits live in the field, so a value applied by clicking away
     * (the Apply choice) is clamped exactly like one confirmed with Return. */
    if (ui_int_field_limited(&es->ui, 9011, x + 80, y, 50,
                             &es->level.screen_count, 1, 99, 1))
        editor_commit_change(es);
    {
        char width_text[32];
        snprintf(width_text, sizeof(width_text), "= %lldpx",
                 (long long)es->level.screen_count * GAME_W);
        ui_label_color(&es->ui, x + 140, y, width_text, ui_color(&es->ui, UI_COLOR_TEXT_DIM));
    }
    y += 24;

    /* ---- Next phase path ---- */
    ui_label(&es->ui, x + 8, y, "Next:");
    if (ui_text_field_hint(&es->ui, 9031, x + 55, y, 310, es->level.next_phase,
                           (int)sizeof(es->level.next_phase), NEXT_PHASE_HINT, 0))
        editor_commit_change(es);
    return y + 24;
}

/* Background sound: which loop plays, and how loud. */
static int config_music(EditorState *es, int x, int y)
{
    const AssetChoices *music = editor_asset_choices(ASSET_CHOICES_MUSIC);

    ui_separator(&es->ui, x + 4, y, PROP_W - 8);
    y += 6;
    ui_label(&es->ui, x + 8, y, "Background Sound:");
    {
        int sel = option_index(es->level.music_path, music->paths, music->count);
        if (ui_dropdown_unlisted(&es->ui, 9009, x + 160, y, 210, music->names, music->count,
                                 &sel, editor_asset_unlisted_name(es->level.music_path))) {
            strncpy(es->level.music_path, music->paths[sel],
                    sizeof(es->level.music_path) - 1);
            es->level.music_path[sizeof(es->level.music_path) - 1] = '\0';
            editor_commit_change(es);
        }
    }
    y += 22;
    ui_label_tip(&es->ui, x + 8, y, "Vol:", "Music volume, 0-128");
    /* The same 0..128 range the validator and the TOML format accept. */
    if (ui_int_field_limited(&es->ui, 9003, x + 50, y, 80,
                             &es->level.music_volume, 0,
                             LEVEL_MUSIC_VOLUME_MAX, 1))
        editor_commit_change(es);
    return y + 24;
}

/* The floor's tileset. */
static int config_floor_tile(EditorState *es, int x, int y)
{
    const AssetChoices *tiles = editor_asset_choices(ASSET_CHOICES_FLOOR_TILES);

    ui_separator(&es->ui, x + 4, y, PROP_W - 8);
    y += 6;
    ui_label(&es->ui, x + 8, y, "Floor Tile:");
    {
        int sel = option_index(es->level.floor_tile_path, tiles->paths, tiles->count);
        if (ui_dropdown_unlisted(&es->ui, 9010, x + 100, y, 270, tiles->names, tiles->count,
                                 &sel, editor_asset_unlisted_name(es->level.floor_tile_path))) {
            strncpy(es->level.floor_tile_path, tiles->paths[sel],
                    sizeof(es->level.floor_tile_path) - 1);
            es->level.floor_tile_path[sizeof(es->level.floor_tile_path) - 1] = '\0';
            editor_commit_change(es);
        }
    }
    return y + 24;
}

/* Starting hearts and lives, and the score rules. */
static int config_lives_and_score(EditorState *es, int x, int y)
{
    ui_separator(&es->ui, x + 4, y, PROP_W - 8);
    y += 6;
    /* 0 in each of these means the game's default; the tooltips say so. */
    ui_label_tip(&es->ui, x + 8, y, "Hearts:", "Hearts the player starts with, 1-3; 0 = the default (3)");
    if (ui_int_field_limited(&es->ui, 9006, x + 70, y, 60,
                             &es->level.initial_hearts, 0, MAX_HEARTS, 1))
        editor_commit_change(es);
    ui_label_tip(&es->ui, x + 150, y, "Lives:", "Extra lives the player starts with; 0 = the default (3)");
    if (ui_int_field_limited(&es->ui, 9007, x + 205, y, 60,
                             &es->level.initial_lives, 0, 99, 1))
        editor_commit_change(es);
    y += 22;
    ui_label_tip(&es->ui, x + 8, y, "Extra life:", "Score that earns an extra life; 0 = the default (1000)");
    if (ui_int_field(&es->ui, 9008, x + 80, y, 80, &es->level.score_per_life))
        editor_commit_change(es);
    ui_label_tip(&es->ui, x + 180, y, "Per coin:", "Points a coin is worth; 0 = the default (100)");
    if (ui_int_field(&es->ui, 9012, x + 240, y, 60, &es->level.coin_score))
        editor_commit_change(es);
    return y + 24;
}

/* Movement physics overrides — a collapsible two-column grid. */
static int config_physics(EditorState *es, int x, int y)
{
    /*
     * Two-column layout constants for the physics grid.
     * Each column holds a label and an input field side by side.
     *
     * Column 1: label at COL1_L, field at COL1_F (width PHYS_FW = 82)
     *   → field ends at x+178, leaving 18 px gap before COL2_L.
     * Column 2: label at COL2_L, field at COL2_F (width PHYS_FW = 82)
     *   → field ends at x+350, right margin = 26 px.
     *
     * The 18 px gap between left field and right label prevents the two
     * columns from visually merging into a single unreadable block.
     */
#define COL1_L  (x +  8)
#define COL1_F  (x + 96)
#define COL2_L  (x + 196)
#define COL2_F  (x + 268)
#define PHYS_FW  82

    ui_separator(&es->ui, x + 4, y, PROP_W - 8);
    y += 6;
    config_subsection_header(es, x, y, " Physics (-1 = engine default)",
                             &g_phys_open, config_row_hovered(es, x, y));
    y += 20;

    if (g_phys_open) {
        /* -- Walk / Run speeds -- */
        ui_label(&es->ui, COL1_L, y, "walk spd:");
        if (ui_float_field(&es->ui, 9020, COL1_F, y, PHYS_FW, &es->level.physics.walk_max_speed))
            editor_commit_change(es);
        ui_label(&es->ui, COL2_L, y, "run spd:");
        if (ui_float_field(&es->ui, 9021, COL2_F, y, PHYS_FW, &es->level.physics.run_max_speed))
            editor_commit_change(es);
        y += 22;

        /* -- Ground acceleration -- */
        ui_label(&es->ui, COL1_L, y, "walk accel:");
        if (ui_float_field(&es->ui, 9022, COL1_F, y, PHYS_FW, &es->level.physics.walk_ground_accel))
            editor_commit_change(es);
        ui_label(&es->ui, COL2_L, y, "run accel:");
        if (ui_float_field(&es->ui, 9023, COL2_F, y, PHYS_FW, &es->level.physics.run_ground_accel))
            editor_commit_change(es);
        y += 22;

        /* -- Ground friction / counter -- */
        ui_label(&es->ui, COL1_L, y, "friction:");
        if (ui_float_field(&es->ui, 9024, COL1_F, y, PHYS_FW, &es->level.physics.ground_friction))
            editor_commit_change(es);
        ui_label(&es->ui, COL2_L, y, "counter:");
        if (ui_float_field(&es->ui, 9025, COL2_F, y, PHYS_FW, &es->level.physics.ground_counter_accel))
            editor_commit_change(es);
        y += 22;

        /* -- Air acceleration -- */
        ui_label(&es->ui, COL1_L, y, "air walk:");
        if (ui_float_field(&es->ui, 9026, COL1_F, y, PHYS_FW, &es->level.physics.air_accel_walk))
            editor_commit_change(es);
        ui_label(&es->ui, COL2_L, y, "air run:");
        if (ui_float_field(&es->ui, 9027, COL2_F, y, PHYS_FW, &es->level.physics.air_accel_run))
            editor_commit_change(es);
        y += 22;

        /* -- Air friction -- */
        ui_label(&es->ui, COL1_L, y, "air fric:");
        if (ui_float_field(&es->ui, 9028, COL1_F, y, PHYS_FW, &es->level.physics.air_friction))
            editor_commit_change(es);
        y += 22;

        /* -- Camera lookahead -- */
        ui_label(&es->ui, COL1_L, y, "cam vx:");
        if (ui_float_field(&es->ui, 9029, COL1_F, y, PHYS_FW, &es->level.physics.cam_lookahead_vx_factor))
            editor_commit_change(es);
        ui_label(&es->ui, COL2_L, y, "cam max:");
        if (ui_float_field(&es->ui, 9030, COL2_F, y, PHYS_FW, &es->level.physics.cam_lookahead_max))
            editor_commit_change(es);
        y += 22;
    }
#undef COL1_L
#undef COL1_F
#undef COL2_L
#undef COL2_F
#undef PHYS_FW
    return y;
}

/*
 * LayerChoices — the assets offered for one kind of layer (background,
 * foreground or fog) and the widget IDs its rows use: row i's dropdown is
 * dropdown_id + i and its speed field speed_id + i.
 */
typedef struct {
    AssetChoicesKind assets;   /* the files offered (asset_choices.c) */
    int              dropdown_id;
    int              speed_id;
} LayerChoices;

/* One "i: [asset] spd: [speed]" row of a layer list. */
static int config_layer_row(EditorState *es, int x, int y, int i,
                            const LayerChoices *choices,
                            char *path, size_t path_size, float *speed)
{
    char label[16];
    snprintf(label, sizeof(label), "%d:", i);
    ui_label(&es->ui, x + 8, y, label);

    const AssetChoices *assets = editor_asset_choices(choices->assets);
    int sel = option_index(path, assets->paths, assets->count);
    if (ui_dropdown_unlisted(&es->ui, choices->dropdown_id + i, x + 28, y, 200,
                             assets->names, assets->count, &sel,
                             editor_asset_unlisted_name(path))) {
        strncpy(path, assets->paths[sel], path_size - 1);
        editor_commit_change(es);
    }
    ui_label_tip(&es->ui, x + 236, y, "Spd:", "How fast the layer scrolls with the view: 0 = still, 1 = with the level");
    if (ui_float_field(&es->ui, choices->speed_id + i, x + 265, y, 60, speed))
        editor_commit_change(es);
    return y + 20;
}

/*
 * config_layer_buttons — "+ Add" and "- Remove Last" under a layer list.
 *
 * *count is the list's layer count and max its capacity.  next_path and
 * next_speed are the fields of the first unused slot (NULL when the list is
 * full); Add fills them with default_path and default_speed.
 */
static int config_layer_buttons(EditorState *es, int x, int y,
                                int *count, int max,
                                char *next_path, size_t path_size,
                                float *next_speed,
                                const char *default_path, float default_speed)
{
    if (*count < max) {
        if (ui_button(&es->ui, x + 8, y, 80, 20, "+ Add")) {
            editor_capture_change_before(es, -1);
            strncpy(next_path, default_path, path_size - 1);
            *next_speed = default_speed;
            (*count)++;
            editor_commit_change(es);
        }
    }
    if (*count > 0) {
        if (ui_button(&es->ui, x + 96, y, 100, 20, "- Remove Last")) {
            editor_capture_change_before(es, -1);
            (*count)--;
            editor_commit_change(es);
        }
    }
    return y + 24;
}

/* Parallax background layers — a collapsible list. */
static int config_background_layers(EditorState *es, int x, int y)
{
    static const LayerChoices choices = { ASSET_CHOICES_BACKGROUNDS, 9200, 9100 };
    LevelDef *level = &es->level;
    char label[48];

    ui_separator(&es->ui, x + 4, y, PROP_W - 8);
    y += 6;
    snprintf(label, sizeof(label), " Background Layers (%d)",
             level->background_layer_count);
    config_subsection_header(es, x, y, label, &g_plx_open,
                             config_row_hovered(es, x, y));
    y += 18;
    if (!g_plx_open) return y;

    for (int i = 0; i < level->background_layer_count && i < MAX_BACKGROUND_LAYERS; i++)
        y = config_layer_row(es, x, y, i, &choices,
                             level->background_layers[i].path,
                             sizeof(level->background_layers[i].path),
                             &level->background_layers[i].speed);

    int next = level->background_layer_count;
    int has_room = next < MAX_BACKGROUND_LAYERS;
    return config_layer_buttons(es, x, y, &level->background_layer_count,
                                MAX_BACKGROUND_LAYERS,
                                has_room ? level->background_layers[next].path : NULL,
                                sizeof(level->background_layers[0].path),
                                has_room ? &level->background_layers[next].speed : NULL,
                                "assets/sprites/backgrounds/sky_blue.png", 0.1f);
}

/* Foreground strips (water, lava, clouds) drawn over the floor. */
static int config_foreground_layers(EditorState *es, int x, int y)
{
    static const LayerChoices choices = { ASSET_CHOICES_FOREGROUNDS, 9300, 9400 };
    LevelDef *level = &es->level;
    char label[48];

    ui_separator(&es->ui, x + 4, y, PROP_W - 8);
    y += 6;
    snprintf(label, sizeof(label), " Foreground Layers (%d)",
             level->foreground_layer_count);
    config_subsection_header(es, x, y, label, &g_fg_open,
                             config_row_hovered(es, x, y));
    y += 18;
    if (!g_fg_open) return y;

    for (int i = 0; i < level->foreground_layer_count && i < MAX_BACKGROUND_LAYERS; i++)
        y = config_layer_row(es, x, y, i, &choices,
                             level->foreground_layers[i].path,
                             sizeof(level->foreground_layers[i].path),
                             &level->foreground_layers[i].speed);

    int next = level->foreground_layer_count;
    int has_room = next < MAX_BACKGROUND_LAYERS;
    return config_layer_buttons(es, x, y, &level->foreground_layer_count,
                                MAX_BACKGROUND_LAYERS,
                                has_room ? level->foreground_layers[next].path : NULL,
                                sizeof(level->foreground_layers[0].path),
                                has_room ? &level->foreground_layers[next].speed : NULL,
                                "assets/sprites/foregrounds/fog_1.png", 0.5f);
}

/* Fog overlays that drift across the screen. */
static int config_fog_layers(EditorState *es, int x, int y)
{
    static const LayerChoices choices = { ASSET_CHOICES_FOG, 9600, 9700 };
    LevelDef *level = &es->level;
    char label[48];

    /* Unlike the other headers, this one has no separator above it and
     * reacts only over its left 200 px (edges included). */
    int hovered = (es->ui.mouse_x >= x &&
                   es->ui.mouse_x <= x + 200 &&
                   es->ui.mouse_y >= y &&
                   es->ui.mouse_y <= y + 18 &&
                   es->ui.mouse_y >= cfg_clip_top && es->ui.mouse_y < cfg_clip_bottom);
    snprintf(label, sizeof(label), " Fog Layers (%d)", level->fog_layer_count);
    config_subsection_header(es, x, y, label, &g_fog_open, hovered);
    y += 18;
    if (!g_fog_open) return y;

    for (int i = 0; i < level->fog_layer_count && i < MAX_FOG_TEXTURES; i++)
        y = config_layer_row(es, x, y, i, &choices,
                             level->fog_layers[i].path,
                             sizeof(level->fog_layers[i].path),
                             &level->fog_layers[i].speed);

    int next = level->fog_layer_count;
    int has_room = next < MAX_FOG_TEXTURES;
    return config_layer_buttons(es, x, y, &level->fog_layer_count,
                                MAX_FOG_TEXTURES,
                                has_room ? level->fog_layers[next].path : NULL,
                                sizeof(level->fog_layers[0].path),
                                has_room ? &level->fog_layers[next].speed : NULL,
                                "assets/sprites/foregrounds/fog_1.png", 0.5f);
}

/*
 * level_config_render — Show editable level-wide settings.
 *
 * Draws the collapsible "Level Config" section at the top of the right
 * column: validation messages, recent files, the level's name and size,
 * sound, floor, lives and score, physics, and the background, foreground
 * and fog layer lists.  total_content_h is the height of everything when
 * fully shown (editor_layout.c); content past available_h scrolls.
 */
void level_config_render(EditorState *es, int start_y, int available_h,
                         int total_content_h) {
    editor_begin_change_tracking(es, EDITOR_CHANGE_CONFIG);
    es->ui.before_change = editor_before_change;
    es->ui.before_change_context = es;

    int x     = PROP_X;
    int cfg_h = available_h;

    ui_panel(&es->ui, x, start_y, PROP_W, cfg_h);
    draw_section_title(es, x, start_y, " Level Config", &es->config_open);

    if (!es->config_open) {
        /* Nothing is drawn, so the wheel must not reach a hidden message
         * list: the palette now fills the place where it was. */
        msg_list_top = msg_list_bottom = 0;
        editor_end_change_tracking(es);
        return;
    }

    /*
     * Only the content below the fixed title scrolls (es->config_scroll,
     * moved by the wheel in editor_panels.c). ui_scroll_begin keeps the
     * offset within the content, clips drawing to the area below the title
     * and returns where the content's top edge goes; ui_scroll_end below
     * clears the clip.
     */
    int title_h           = ROW_H + 4;
    int content_visible_h = cfg_h - title_h;
    int content_top       = start_y + title_h;
    int content_scroll_h  = total_content_h - title_h;
    /* The scrollbar takes its clicks before the fields below can see them. */
    (void)ui_scrollbar_input(&es->ui, &es->config_scroll, x + PROP_W - SCROLLBAR_W, content_top,
                             SCROLLBAR_W, content_visible_h, content_visible_h, content_scroll_h);
    int y = ui_scroll_begin(&es->ui, &es->config_scroll, x, content_top, PROP_W, content_visible_h,
                            content_scroll_h) + 8;
    cfg_clip_top = content_top;
    cfg_clip_bottom = content_top + content_visible_h;
    y = config_validation(es, x, y);
    y = config_recent_files(es, x, y);
    y = config_level_fields(es, x, y);
    y = config_music(es, x, y);
    y = config_floor_tile(es, x, y);
    y = config_lives_and_score(es, x, y);
    y = config_physics(es, x, y);
    y = config_background_layers(es, x, y);
    y = config_foreground_layers(es, x, y);
    (void)config_fog_layers(es, x, y);

    ui_scroll_end(&es->ui);
    ui_scrollbar_draw(&es->ui, &es->config_scroll, x + PROP_W - SCROLLBAR_W, content_top,
                      SCROLLBAR_W, content_visible_h, content_visible_h, content_scroll_h);
    editor_end_change_tracking(es);
    show_field_note(es);

    /* A field activated by Tab or by a clicked validation message may be
     * scrolled out of sight: scroll just enough to show it (the clamp at
     * the top of this function keeps the result in range next frame). */
    if (es->ui.focus_landed_id >= 9000) {
        int field_top = es->ui.focus_landed_y;
        ui_scroll_reveal(&es->config_scroll, field_top, field_top + 20,
                         cfg_clip_top, cfg_clip_bottom, 8);
        es->ui.focus_landed_id = 0;
    }
}

/*
 * Where each Level Config key lives in the panel, for jumping to it from a
 * validation message.  widget is the field's ID: a text field takes the
 * caret and a dropdown opens (ui_focus_field).  A layer list has one row
 * per layer, so its targets also name the key inside the row (field) and
 * give row 0's ID: row i's widget is widget + i, as config_layer_row
 * numbers them.  A target without a field matches any message about that
 * key and only unfolds its group (widget 0).  open is the foldout that
 * must be open for the widget to be drawn (NULL when it always is).
 */
typedef struct {
    const char *key;
    const char *field;
    int widget;
    int *open;
} ConfigTarget;

static const ConfigTarget s_config_targets[] = {
    {"name", NULL, 9000, NULL},             {"description", NULL, 9001, NULL},
    {"generated_by", NULL, 9002, NULL},     {"screen_count", NULL, 9011, NULL},
    {"next_phase", NULL, 9031, NULL},       {"music_path", NULL, 9009, NULL},
    {"music_volume", NULL, 9003, NULL},     {"floor_tile_path", NULL, 9010, NULL},
    {"initial_hearts", NULL, 9006, NULL},   {"initial_lives", NULL, 9007, NULL},
    {"score_per_life", NULL, 9008, NULL},   {"coin_score", NULL, 9012, NULL},
    {"physics.walk_max_speed", NULL, 9020, &g_phys_open},
    {"physics.run_max_speed", NULL, 9021, &g_phys_open},
    {"physics.walk_ground_accel", NULL, 9022, &g_phys_open},
    {"physics.run_ground_accel", NULL, 9023, &g_phys_open},
    {"physics.ground_friction", NULL, 9024, &g_phys_open},
    {"physics.ground_counter_accel", NULL, 9025, &g_phys_open},
    {"physics.air_accel_walk", NULL, 9026, &g_phys_open},
    {"physics.air_accel_run", NULL, 9027, &g_phys_open},
    {"physics.air_friction", NULL, 9028, &g_phys_open},
    {"physics.cam_lookahead_vx_factor", NULL, 9029, &g_phys_open},
    {"physics.cam_lookahead_max", NULL, 9030, &g_phys_open},
    /* Layer rows: the asset dropdown and the speed field (the IDs in the
     * LayerChoices of config_background_layers and friends). */
    {"background_layers", "path", 9200, &g_plx_open},
    {"background_layers", "speed", 9100, &g_plx_open},
    {"background_layers", NULL, 0, &g_plx_open},
    {"foreground_layers", "path", 9300, &g_fg_open},
    {"foreground_layers", "speed", 9400, &g_fg_open},
    {"foreground_layers", NULL, 0, &g_fg_open},
    {"fog_layers", "path", 9600, &g_fog_open},
    {"fog_layers", "speed", 9700, &g_fog_open},
    {"fog_layers", NULL, 0, &g_fog_open},
};

int properties_focus_config(EditorState *es, const LevelIssueLocation *where)
{
    char key[80];

    if (!es || !where || where->path[0] == '\0') return 0;
    /* Physics keys are "physics.<name>"; everything else is the path. */
    if (strcmp(where->path, "physics") == 0)
        snprintf(key, sizeof(key), "physics.%s", where->field);
    else
        snprintf(key, sizeof(key), "%s", where->path);

    for (size_t i = 0; i < sizeof(s_config_targets) / sizeof(s_config_targets[0]); i++) {
        const ConfigTarget *target = &s_config_targets[i];
        int widget = target->widget;
        if (strcmp(target->key, key) != 0) continue;
        if (target->field) {
            /* A row target: the message must name this key of a row. */
            if (strcmp(target->field, where->field) != 0 || where->index < 0) continue;
            widget += where->index;
        }
        es->config_open = 1;
        if (target->open) *target->open = 1;
        if (widget) ui_focus_field(&es->ui, widget);
        return 1;
    }
    return 0;
}
