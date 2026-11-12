/*
 * properties.c — Entity property inspector panel for the level editor.
 *
 * One draw_<type>_properties function per entity lives in
 * properties_entities.c; Level Config lives in properties_config.c.
 * This file keeps the shared field helpers and s_property_panels,
 * which properties_render indexes by EntityType.
 *
 * Renders per-entity-type editable fields (position, velocity, patrol range,
 * mode, tile counts, etc.) in the bottom-right panel when an entity is
 * selected on the canvas.  Uses the immediate-mode UI widgets from ui.h.
 *
 * The panel occupies the bottom half of the right column:
 *   x = CANVAS_W (896),  y = EDITOR_H / 2 (360)
 *   w = PANEL_W  (384),  h = EDITOR_H / 2 - STATUS_H (328)
 *
 * Each field has a unique widget ID built from the entity type enum value
 * multiplied by 100 plus a per-field offset (e.g. ENT_COIN * 100 + 1 = 301).
 * This guarantees no two fields share an ID, which the IMGUI system requires
 * to track which widget is actively being edited.
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
#include "snapshot_equal.h"   /* placement_same: did a shared edit change an entity? */
#include "undo.h"             /* undo_group_begin: several entities, one undo step  */
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


/* Collapsible subsection open states — accessed by editor.c for config_h computation */
int g_plx_open  = 0;
int g_fg_open   = 0;
int g_fog_open  = 0;
int g_phys_open = 0;




/*
 * show_field_note — When the last commit limited or refused what was
 * typed, the UI left a note saying so; put it in the status bar.
 */
void show_field_note(EditorState *es)
{
    if (es->ui.edit_note[0] == '\0') return;
    editor_set_status(es, "%s", es->ui.edit_note);
    es->ui.edit_note[0] = '\0';
}

/*
 * option_index — Position of value in a dropdown's paths, or -1.
 *
 * -1 tells ui_dropdown the stored path is none of its options (a path
 * typed into the TOML by hand).  Mapping it to option 0 instead would make
 * picking option 0 look like "no change", so it could never be chosen.
 */
int option_index(const char *value, const char *const *paths,
                        int count)
{
    for (int i = 0; i < count; i++) {
        if (strcmp(value, paths[i]) == 0) return i;
    }
    return -1;
}

/*
 * A field a validation message asked to focus ("spiders[2].vx" -> the vx
 * field of spider 2), until the properties panel draws that entity.
 * focus_key is the TOML key (field_label matches on it, not on the label
 * shown); "" = none.
 */
static char       s_focus_key[32];
static EntityType s_focus_type;
static int        s_focus_index = -1;

/*
 * field_label — Draw a property's label at row y ("Patrol left:", with a
 * tooltip giving the meaning, the unit and the TOML key; see
 * s_field_labels), and when a validation message asked for this key, give
 * the field the focus: the widget `id` drawn right after the label then
 * activates (a text field takes the caret, a dropdown opens).  Messages
 * name fields by TOML key, which is why the key, not the label, is what
 * is matched.
 */
/*
 * What each property key reads as in the panel, and what its tooltip says:
 * the meaning and the unit, then the TOML key in brackets.  Keys missing
 * here show as they are.  Labels stay within LABEL_W.
 */
static const struct { const char *key, *label, *tip; } s_field_labels[] = {
    {"x",           "X",            "Horizontal position, in pixels from the level's left edge"},
    {"y",           "Y",            "Vertical position, in pixels from the top"},
    {"patrol_x0",   "Patrol left",  "Left end of the patrol, in pixels"},
    {"patrol_x1",   "Patrol right", "Right end of the patrol, in pixels"},
    {"vx",          "Speed",        "Speed in pixels per second; negative starts it moving left"},
    {"tile_count",  "Length",       "Width in 16 px pieces"},
    {"count",       "Length",       "Length in 16 px tiles"},
    {"brick_count", "Bricks",       "Length in 16 px bricks"},
    {"launch_vy",   "Launch",       "Upward speed the pad gives, in pixels per second"},
    {"frame_index", "Start frame",  "Animation frame it starts on (0-2), so neighbours do not move in step"},
    {"t_offset",    "Rail start",   "Where it starts along its rail, in tiles"},
    {"speed",       "Speed",        "Speed along the rail, in tiles per second"},
    {"rail_index",  "Rail",         "Which rail it rides (the number in the rail's title)"},
    {"mode",        "Mode",         "How it moves"},
    {"base_y",      "Flight y",     "Height of the middle of its wavy flight, in pixels"},
    {"w",           "Width",        "Width, in rail tiles"},
    {"h",           "Height",       "Height, in rail tiles (Rect rails only)"},
    {"vine_type",   "Colour",       "0 = green, 1 = brown"},
    {"tile_width",  "Width",        "Width in 48 px tiles (0 or 1 = one tile)"},
    {"tile_height", "Height",       "Height in 48 px tiles"},
    {"tile_path",   "Tile image",   "The platform's tile image; (default) is grass"},
    {"pillar_x",    "Pillar x",     "Left edge of the pillar the axe hangs from, in pixels"},
    {"layout",      "Shape",        "Rect: a closed loop; Horiz: a straight line"},
    {"end_cap",     "End cap",      "Horiz rails: 1 = a cap at the right end (riders bounce back), 0 = riders fall off"},
    {"direction",   "Direction",    "+1 starts moving right, -1 left"},
};

void field_label(EditorState *es, int y, const char *key, int id)
{
    static char tips[sizeof(s_field_labels) / sizeof(s_field_labels[0])][160];
    char text[40];
    size_t n = sizeof(s_field_labels) / sizeof(s_field_labels[0]);
    size_t i = 0;

    while (i < n && strcmp(s_field_labels[i].key, key) != 0) i++;
    if (i < n) {
        if (!tips[i][0])
            snprintf(tips[i], sizeof(tips[i]), "%s (%s)", s_field_labels[i].tip, key);
        snprintf(text, sizeof(text), "%s:", s_field_labels[i].label);
        ui_label_tip(&es->ui, CONTENT_X, y, text, tips[i]);
    } else {
        snprintf(text, sizeof(text), "%s:", key);
        ui_label(&es->ui, CONTENT_X, y, text);
    }
    if (s_focus_key[0] != '\0' && strcmp(s_focus_key, key) == 0 &&
        es->selection.type == s_focus_type && es->selection.index == s_focus_index) {
        ui_focus_field(&es->ui, id);
        s_focus_key[0] = '\0';
    }
}

/*
 * draw_section_title — The title bar of a collapsible right-panel section
 * (the entity properties and Level Config).
 *
 * Fills the bar, toggles *open when it is clicked, and draws the ">"/"v"
 * symbol in UI_COLOR_ACCENT followed by title, which turns from the accent to
 * UI_COLOR_TEXT while hovered.  The bar is ROW_H + 4 px tall.
 */
void draw_section_title(EditorState *es, int x, int y,
                               const char *title, int *open)
{
    DrawRectangle(x, y, PROP_W, ROW_H+4, ui_color(&es->ui, UI_COLOR_TITLE));

    int hovered = (es->ui.mouse_x >= x &&
                   es->ui.mouse_x < x + PROP_W &&
                   es->ui.mouse_y >= y &&
                   es->ui.mouse_y < y + ROW_H + 4);
    if (hovered && es->ui.mouse_clicked)
        *open = !*open;

    const char *sym = *open ? "v" : ">";
    int sym_w = ui_text_width(&es->ui, sym);
    ui_label_color(&es->ui, x + 8, y + 4, sym, ui_color(&es->ui, UI_COLOR_ACCENT));
    ui_label_color(&es->ui, x + 8 + sym_w, y + 4, title,
                   hovered ? ui_color(&es->ui, UI_COLOR_TEXT) : ui_color(&es->ui, UI_COLOR_ACCENT));
}


/* ------------------------------------------------------------------ */
/* Which function draws each type's fields                             */
/* ------------------------------------------------------------------ */

/*
 * s_property_panels — the function that draws each EntityType's fields,
 * in the enum order of editor.h.  Most types are a table of field rows and
 * share draw_table_properties (properties_entities.c); a type whose panel
 * needs more than rows has a draw_<type>_properties function of its own.
 * The table is sized by its rows, so the _Static_assert fails the build
 * when a type added at the end of the enum has no panel yet.  Every
 * function takes the panel's first row y.
 */
typedef void (*PropertyPanelFn)(EditorState *es, int y);

static const PropertyPanelFn s_property_panels[] = {
    [ENT_FLOOR_GAP]        = draw_floor_gap_properties,
    [ENT_CHECKPOINT]       = draw_checkpoint_properties,
    [ENT_RAIL]             = draw_table_properties,
    [ENT_PLATFORM]         = draw_platform_properties,
    [ENT_COIN]             = draw_table_properties,
    [ENT_STAR_YELLOW]      = draw_table_properties,
    [ENT_STAR_GREEN]       = draw_table_properties,
    [ENT_STAR_RED]         = draw_table_properties,
    [ENT_LAST_STAR]        = draw_last_star_properties,
    [ENT_SPIDER]           = draw_table_properties,
    [ENT_JUMPING_SPIDER]   = draw_table_properties,
    [ENT_BIRD]             = draw_table_properties,
    [ENT_FASTER_BIRD]      = draw_table_properties,
    [ENT_FISH]             = draw_table_properties,
    [ENT_FASTER_FISH]      = draw_table_properties,
    [ENT_AXE_TRAP]         = draw_table_properties,
    [ENT_CIRCULAR_SAW]     = draw_table_properties,
    [ENT_SPIKE_ROW]        = draw_table_properties,
    [ENT_SPIKE_PLATFORM]   = draw_table_properties,
    [ENT_SPIKE_BLOCK]      = draw_table_properties,
    [ENT_BLUE_FLAME]       = draw_blue_flame_properties,
    [ENT_FIRE_FLAME]       = draw_fire_flame_properties,
    [ENT_FLOAT_PLATFORM]   = draw_float_platform_properties,
    [ENT_BRIDGE]           = draw_table_properties,
    [ENT_BOUNCEPAD_SMALL]  = draw_bouncepad_small_properties,
    [ENT_BOUNCEPAD_MEDIUM] = draw_bouncepad_medium_properties,
    [ENT_BOUNCEPAD_HIGH]   = draw_bouncepad_high_properties,
    [ENT_VINE]             = draw_table_properties,
    [ENT_LADDER]           = draw_table_properties,
    [ENT_ROPE]             = draw_table_properties,
    [ENT_PLAYER_SPAWN]     = draw_table_properties,
};

_Static_assert(sizeof(s_property_panels) / sizeof(s_property_panels[0]) == ENT_COUNT,
               "s_property_panels needs one draw function per EntityType");

/* The function that draws `type`'s fields, or NULL for an invalid type. */
static PropertyPanelFn property_panel(EntityType type)
{
    if (type < 0 || type >= ENT_COUNT) return NULL;
    return s_property_panels[type];
}

/* ------------------------------------------------------------------ */
/* One field, several entities                                         */
/* ------------------------------------------------------------------ */

void properties_apply_to_selection(EditorState *es, EntityType type, const char *key,
                                   size_t offset, size_t size)
{
    static Selection items[EDITOR_MAX_SELECTION];
    static PlacementData before[EDITOR_MAX_SELECTION];
    PlacementData primary_before, primary_after, after;
    EditorIssueSnapshot issues;
    char error[128];
    int count, changed = 0, group;

    if (!es || !es->pending_change_valid || es->pending_entity_type != (int)type ||
        size == 0 || offset + size > sizeof(PlacementData)) {
        editor_commit_change(es);
        return;
    }
    /* The widget has already written the new value into the primary
     * entity; pending_entity_before is that entity as it was. */
    primary_before = es->pending_entity_before;
    primary_after = editor_snapshot_entity(&es->level, type, es->selection.index);
    count = editor_selection_items(es, items, EDITOR_MAX_SELECTION);

    /*
     * What errors did the level have before this edit?  Put the primary
     * back for a moment to ask, then restore the new value.  Each entity's
     * own limits (a patrol's left end below its right end) are enforced by
     * the widget for the primary only, so the others are checked the way
     * every multi-entity edit is: the level must not gain an error.
     */
    (void)editor_entity_write(&es->level, type, es->selection.index, &primary_before);
    editor_level_issue_snapshot(&es->level, &issues);
    (void)editor_entity_write(&es->level, type, es->selection.index, &primary_after);

    /* Room for every entry before anything else is touched (undo.h). */
    group = undo_group_begin(es->undo, count);
    if (group == 0) {
        (void)editor_entity_write(&es->level, type, es->selection.index, &primary_before);
        es->pending_change_valid = 0;
        editor_set_status(es, "Edit cancelled: cannot allocate undo history");
        return;
    }

    /*
     * Copy the value into the others.  A PlacementData is a union whose
     * members all start at its first byte, so the bytes at `offset` in the
     * snapshot are the bytes at `offset` in the entity's own struct.
     */
    for (int i = 0; i < count; i++) {
        if (items[i].type != type || items[i].index == es->selection.index) continue;
        before[i] = editor_snapshot_entity(&es->level, type, items[i].index);
        after = before[i];
        memcpy((char *)&after + offset, (const char *)&primary_after + offset, size);
        (void)editor_entity_write(&es->level, type, items[i].index, &after);
    }

    if (editor_edit_kept_level(&es->level, &issues, error, sizeof(error)) != 0) {
        /* It would break one of them: take everything back, the primary's
         * own edit included, so the selection stays consistent. */
        for (int i = 0; i < count; i++) {
            if (items[i].type != type || items[i].index == es->selection.index) continue;
            (void)editor_entity_write(&es->level, type, items[i].index, &before[i]);
        }
        (void)editor_entity_write(&es->level, type, es->selection.index, &primary_before);
        es->pending_change_valid = 0;
        undo_group_end(es->undo);
        editor_set_status(es, "Not applied to all %d: %s", count, error);
        return;
    }

    /* Record it: the primary through the usual commit (it joins the open
     * group), then one entry per other entity that really changed. */
    editor_commit_change(es);
    for (int i = 0; i < count; i++) {
        Command cmd;
        if (items[i].type != type || items[i].index == es->selection.index) continue;
        after = editor_snapshot_entity(&es->level, type, items[i].index);
        if (placement_same(type, &before[i], &after)) continue;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = CMD_PROPERTY;
        cmd.entity_type = (int)type;
        cmd.entity_index = items[i].index;
        cmd.before = before[i];
        cmd.after = after;
        (void)undo_push(es->undo, &cmd);   /* fits the room begin reserved */
        changed++;
    }
    undo_group_end(es->undo);
    editor_refresh_dirty(es);
    editor_set_status(es, "%s set on %d %s", key, changed + 1, editor_entity_type_name(type));
}

/* ------------------------------------------------------------------ */
/* properties_render                                                   */
/* ------------------------------------------------------------------ */

void properties_render(EditorState *es, int start_y, int available_h)
{
    if (!es) return;
    editor_selection_reconcile(es);
    if (!editor_selection_is_valid(es))
        return;

    /*
     * Use caller-supplied position instead of the old fixed PROP_Y / PROP_H
     * constants.  The layout orchestrator in editor.c computes where this
     * section starts and how tall it is based on the sections above it.
     */
    int prop_x = PROP_X;
    int prop_y = start_y;
    int prop_h = available_h;

    /*
     * Several entities selected.  When they are all of one type whose
     * fields are a table of rows, the panel shows those fields with the
     * primary entity's values, and a value typed there is given to every
     * selected entity at once (properties_apply_to_selection).  Otherwise
     * there is no single set of fields to edit, so the panel shows how
     * many of each type are selected and what acts on them.
     */
    int selected = editor_selection_count(es);
    if (selected > 1) {
        static Selection items[EDITOR_MAX_SELECTION];
        int counts[ENT_COUNT] = {0};
        char header[64];
        int y;
        int n = editor_selection_items(es, items, EDITOR_MAX_SELECTION);
        int one_type = 1;

        for (int i = 0; i < n; i++) {
            counts[items[i].type]++;
            if (items[i].type != es->selection.type) one_type = 0;
        }
        one_type = one_type && properties_type_has_rows(es->selection.type);

        if (one_type)
            snprintf(header, sizeof(header), " %d x %s", selected,
                     editor_entity_type_name(es->selection.type));
        else
            snprintf(header, sizeof(header), " %d selected", selected);
        ui_panel(&es->ui, prop_x, prop_y, PROP_W, prop_h);
        draw_section_title(es, prop_x, prop_y, header, &es->panel_open);
        if (!es->panel_open) return;
        ui_clip_begin(&es->ui, prop_x, prop_y + ROW_H + 4, PROP_W, prop_h - ROW_H - 4);
        y = prop_y + ROW_H + 8;
        if (one_type) {
            /* The same change tracking as a single entity's panel: the
             * widget snapshots the primary entity before writing to it. */
            editor_begin_change_tracking(es, EDITOR_CHANGE_ENTITY);
            es->ui.before_change = editor_before_change;
            es->ui.before_change_context = es;
            ui_label_color(&es->ui, CONTENT_X, y, "A value typed here is given to all of them.",
                           ui_color(&es->ui, UI_COLOR_TEXT_DIM));
            y += 20;
            draw_table_properties(es, y);
            show_field_note(es);
            editor_end_change_tracking(es);
            ui_clip_end(&es->ui);
            return;
        }
        for (int type = 0; type < ENT_COUNT; type++) {
            char line[64];
            if (counts[type] == 0) continue;
            snprintf(line, sizeof(line), "%d x %s", counts[type],
                     editor_entity_type_name((EntityType)type));
            ui_label(&es->ui, CONTENT_X, y, line);
            y += 18;
        }
        y += 6;
        ui_label_color(&es->ui, CONTENT_X, y,
                       "Drag, arrows, Delete, Ctrl+C / Ctrl+D act on all.",
                       ui_color(&es->ui, UI_COLOR_TEXT_DIM));
        ui_label_color(&es->ui, CONTENT_X, y + 18,
                       "Shift+click adds or removes one.", ui_color(&es->ui, UI_COLOR_TEXT_DIM));
        ui_clip_end(&es->ui);
        return;
    }

    /* Panel background, then the "Coin #3" title bar that collapses it.
     * Singletons (Last Star, Player Spawn) have no index to show. */
    char header[64];
    if (editor_entity_type_is_singleton(es->selection.type)) {
        snprintf(header, sizeof(header), " %s",
                 editor_entity_type_name(es->selection.type));
    } else {
        snprintf(header, sizeof(header), " %s #%d",
                 editor_entity_type_name(es->selection.type),
                 es->selection.index);
    }
    ui_panel(&es->ui, prop_x, prop_y, PROP_W, prop_h);
    draw_section_title(es, prop_x, prop_y, header, &es->panel_open);

    if (!es->panel_open) return;

    editor_begin_change_tracking(es, EDITOR_CHANGE_ENTITY);
    es->ui.before_change = editor_before_change;
    es->ui.before_change_context = es;

    /* The fields scroll below the title bar when they need more room than
     * the panel has.  Their height is measured as they are drawn (the
     * lowest widget), so the scroll uses last frame's measure; another
     * selection starts again at the top. */
    long long key = (long long)es->selection.type * 100000 + es->selection.index;
    if (key != es->props_scroll_key) {
        es->props_scroll_key = key;
        es->props_scroll.offset = 0;
        es->props_content_h = 0;
    }
    int content_top = prop_y + ROW_H + 4;
    int view_h = prop_h - ROW_H - 4;
    (void)ui_scrollbar_input(&es->ui, &es->props_scroll, prop_x + PROP_W - SCROLLBAR_W,
                             content_top, SCROLLBAR_W, view_h, view_h, es->props_content_h);
    int y = ui_scroll_begin(&es->ui, &es->props_scroll, prop_x, content_top, PROP_W,
                            view_h, es->props_content_h) + 4;
    int top = y;
    es->ui.widgets_bottom = 0;

    /* ---- Per-type field rendering ----------------------------------- */

    /*
     * The selected type's draw_<type>_properties function (table above)
     * draws its fields by pointer into the placement, so the ui_*_field
     * widgets read and write the value in place, and commits each edit
     * as one undoable property command.
     */
    PropertyPanelFn draw_fields = property_panel(es->selection.type);
    if (draw_fields) draw_fields(es, y);
    show_field_note(es);

    /* A request is for the first drawing of fields after it: a key this
     * entity has no field for (or another entity) must not linger. */
    s_focus_key[0] = '\0';

    if (es->ui.widgets_bottom > top) es->props_content_h = es->ui.widgets_bottom - top + 12;
    ui_scroll_end(&es->ui);
    ui_scrollbar_draw(&es->ui, &es->props_scroll, prop_x + PROP_W - SCROLLBAR_W,
                      content_top, SCROLLBAR_W, view_h, view_h, es->props_content_h);
    editor_end_change_tracking(es);
}


void properties_focus_entity_field(EditorState *es, EntityType type, int index,
                                   const char *field)
{
    /* A few messages name a pair of fields together; edit the one that
     * usually fixes it ("patrol is 20 px wide" -> move its right end). */
    static const struct { const char *alias, *key; } aliases[] = {
        { "patrol", "patrol_x1" },
    };

    if (!es || !field || field[0] == '\0') return;
    snprintf(s_focus_key, sizeof(s_focus_key), "%s", field);
    for (size_t i = 0; i < sizeof(aliases) / sizeof(aliases[0]); i++)
        if (strcmp(field, aliases[i].alias) == 0)
            snprintf(s_focus_key, sizeof(s_focus_key), "%s", aliases[i].key);
    s_focus_type = type;
    s_focus_index = index;
    es->panel_open = 1;
}

