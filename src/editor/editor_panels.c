/*
 * editor_panels.c — Right-side editor panel layout and rendering.
 */

#include "editor_panels.h"

#include "canvas.h"        /* canvas_clamp_camera */
#include "editor_layout.h" /* editor_config_total_height */
#include "editor_session.h" /* editor_finish_field_edit, editor_set_status */
#include "entity_meta.h"   /* editor_selection_reconcile */
#include "hit_test.h"      /* editor_entity_bounds */
#include "palette.h"       /* palette_render */
#include "properties.h"    /* level_config_render, properties_render */

typedef struct {
    int y;
    int total_h;
    int visible_h;
    int bottom_y;
} ConfigPanelGeometry;

/*
 * config_panel_geometry — Compute config panel full and visible bounds.
 */
static ConfigPanelGeometry config_panel_geometry(EditorState *es)
{
    const int section_hdr = 28;

    ConfigPanelGeometry g;
    g.y = TOOLBAR_H;
    g.total_h = es->config_open
              ? editor_config_total_height(es)
              : section_hdr;

    int max_h = (EDITOR_H - STATUS_H - TOOLBAR_H) / 2;
    g.visible_h = g.total_h < max_h ? g.total_h : max_h;
    g.bottom_y = g.y + g.visible_h;

    return g;
}

/*
 * editor_render_side_panels — Draw level config, palette, and properties.
 *
 * The right column stacks three collapsible sections. Level config is capped
 * to half the right column; palette fills the remaining space above optional
 * properties.
 */
void editor_render_side_panels(EditorState *es)
{
    editor_selection_reconcile(es);
    int right_bottom = EDITOR_H - STATUS_H;
    int section_hdr  = 28;  /* matches palette TITLE_H */

    ConfigPanelGeometry config = config_panel_geometry(es);

    int props_h = 0;
    if (es->selection.index >= 0) {
        props_h = es->panel_open ? 200 : section_hdr;
    }

    int palette_y = config.bottom_y;
    int palette_h;
    if (es->palette_open) {
        palette_h = right_bottom - palette_y - props_h;
        if (palette_h < section_hdr + 50) palette_h = section_hdr + 50;
    } else {
        palette_h = section_hdr;
    }

    int props_y = palette_y + palette_h;

    level_config_render(es, config.y, config.visible_h, config.total_h);
    palette_render(es, palette_y, palette_h);
    if (es->selection.index >= 0) {
        properties_render(es, props_y, props_h);
    }
}

/*
 * editor_handle_side_panel_scroll — Route scroll wheel to config or palette.
 *
 * Returns 1 when the mouse is over the right panel and the event was handled.
 * The config height calculation mirrors editor_render_side_panels so hit tests
 * match what the user sees.
 */
int editor_handle_side_panel_scroll(EditorState *es, int mx, int my, float wheel_y)
{
    int pixels;

    if (mx < CANVAS_W || my <= TOOLBAR_H || my >= EDITOR_H - STATUS_H) {
        return 0;
    }

    /*
     * One wheel notch scrolls 20 px.  A trackpad sends fractions of a notch
     * (0.1, say), and the panels scroll in whole pixels, so keep the part
     * that is not yet a whole pixel for the next event.  Truncating each
     * event to an int on its own would turn every small swipe into 0.
     */
    es->panel_wheel_accum += -wheel_y * 20.0f;
    pixels = (int)es->panel_wheel_accum;     /* toward 0, sign kept */
    es->panel_wheel_accum -= (float)pixels;
    if (pixels == 0) return 1;

    ConfigPanelGeometry config = config_panel_geometry(es);

    if (my < config.bottom_y) {
        cfg_scroll(pixels);
    } else {
        palette_scroll(pixels);
    }

    return 1;
}

/*
 * editor_focus_validation_issue — Take the designer to what validation
 * message `message` is about (spec N-001: clickable diagnostics).
 *
 * The validator reports a location in TOML terms (see LevelIssueLocation).
 * An entity array such as "coins" or "checkpoints" selects that entity,
 * switches to the Select tool and pans the canvas so the entity sits in
 * the middle; a Level Config key opens the panel at that field.  The
 * message itself is repeated in the status bar.
 */
int editor_focus_validation_issue(EditorState *es, int message)
{
    const LevelIssueLocation *where;
    const char *text;
    EntityType type;

    if (!es || message < 0 || message >= es->validation_report.message_count)
        return 0;
    where = &es->validation_report.locations[message];
    text = es->validation_report.messages[message];
    if (!editor_finish_field_edit(es)) return 0;

    type = editor_entity_type_for_toml(where->path);
    if (type != ENT_COUNT) {
        int index = editor_entity_type_is_singleton(type) ? 0 : where->index;
        EditorRect r;
        if (index < 0 || index >= editor_entity_count(&es->level, type)) {
            editor_set_status(es, "Cannot show it: %s", text);
            return 0;
        }
        es->tool = TOOL_SELECT;
        editor_select_only(es, type, index);
        es->panel_open = 1;
        if (editor_entity_bounds(&es->level, type, index, &r)) {
            float zoom = es->camera.zoom > 0.0f ? es->camera.zoom : 1.0f;
            es->camera.x = r.x + r.w / 2.0f - (float)CANVAS_W / (2.0f * zoom);
            es->camera.y = r.y + r.h / 2.0f - (float)CANVAS_H / (2.0f * zoom);
            canvas_clamp_camera(es);
        }
        editor_set_status(es, "%s %d: %s", editor_entity_type_name(type), index, text);
        return 1;
    }
    if (properties_focus_config(es, where)) {
        editor_set_status(es, "Level Config: %s", text);
        return 1;
    }
    editor_set_status(es, "%s", text);
    return 0;
}
