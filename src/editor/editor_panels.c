/*
 * editor_panels.c — Right-side editor panel layout and rendering.
 */

#include "editor_panels.h"

#include "canvas.h"        /* canvas_center_on */
#include "editor_layout.h" /* editor_config_total_height */
#include "editor_session.h" /* editor_finish_field_edit, editor_set_status */
#include "entity_meta.h"   /* editor_selection_reconcile */
#include "hit_test.h"      /* editor_entity_bounds */
#include "palette.h"       /* palette_render */
#include "properties.h"    /* level_config_render, properties_render */

#define SECTION_HDR 28   /* matches palette TITLE_H */
#define PROPS_SHORT_H 200 /* the properties panel under an open palette */

void editor_side_panel_layout(const EditorState *es, EditorSideLayout *out)
{
    const int right_bottom = EDITOR_H - STATUS_H;
    int max_config = (EDITOR_H - STATUS_H - TOOLBAR_H) / 2;
    int selected = es->selection.index >= 0;

    out->config_y = TOOLBAR_H;
    out->config_total_h = es->config_open ? editor_config_total_height(es) : SECTION_HDR;
    out->config_h = out->config_total_h < max_config ? out->config_total_h : max_config;

    out->palette_y = out->config_y + out->config_h;
    /* Selecting is for editing what is selected: the palette, which only
     * the Place tool uses, folds out of the way. */
    out->palette_folded = selected && es->tool != TOOL_PLACE && es->palette_open;

    if (selected && (out->palette_folded || !es->palette_open)) {
        out->palette_h = SECTION_HDR;
        out->props_h = es->panel_open ? right_bottom - out->palette_y - SECTION_HDR : SECTION_HDR;
    } else {
        out->props_h = selected ? (es->panel_open ? PROPS_SHORT_H : SECTION_HDR) : 0;
        if (es->palette_open) {
            out->palette_h = right_bottom - out->palette_y - out->props_h;
            if (out->palette_h < SECTION_HDR + 50) out->palette_h = SECTION_HDR + 50;
        } else {
            out->palette_h = SECTION_HDR;
        }
    }
    out->props_y = out->palette_y + out->palette_h;
}

/*
 * editor_render_side_panels — Draw level config, palette, and properties
 * where editor_side_panel_layout puts them.
 */
void editor_render_side_panels(EditorState *es)
{
    EditorSideLayout layout;

    editor_selection_reconcile(es);
    editor_side_panel_layout(es, &layout);
    level_config_render(es, layout.config_y, layout.config_h, layout.config_total_h);
    palette_render(es, layout.palette_y, layout.palette_h);
    if (es->selection.index >= 0) properties_render(es, layout.props_y, layout.props_h);
}

/*
 * editor_handle_side_panel_scroll — Route scroll wheel to config or palette.
 *
 * Returns 1 when the mouse is over the right panel and the event was handled.
 * It reads the same editor_side_panel_layout as the drawing, so the wheel
 * scrolls the section under the pointer.
 */
int editor_handle_side_panel_scroll(EditorState *es, int mx, int my, float wheel_y)
{
    if (mx < CANVAS_W || my <= TOOLBAR_H || my >= EDITOR_H - STATUS_H) {
        return 0;
    }

    /* Over a validation list longer than its window, the wheel scrolls the
     * list one row per notch (config_validation clamps it). Elsewhere one
     * notch scrolls a panel 20 px. Each UIScroll keeps a trackpad's
     * fractions of a notch until they add up to a whole row or pixel. */
    if (properties_message_list_contains(mx, my)) {
        (void)ui_scroll_wheel(&es->message_scroll, wheel_y, 1.0f);
        return 1;
    }

    EditorSideLayout layout;
    editor_side_panel_layout(es, &layout);
    UIScroll *scroll = &es->palette_scroll;
    if (my < layout.palette_y) scroll = &es->config_scroll;
    else if (layout.props_h > 0 && my >= layout.props_y) scroll = &es->props_scroll;
    (void)ui_scroll_wheel(scroll, wheel_y, 20.0f);
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
        /* "spiders[2].vx" also puts the caret in spider 2's vx field. */
        properties_focus_entity_field(es, type, index, where->field);
        if (editor_entity_bounds(&es->level, type, index, &r)) canvas_center_on(es, &r);
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
