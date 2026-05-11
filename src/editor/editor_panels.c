/*
 * editor_panels.c — Right-side editor panel layout and rendering.
 */

#include "editor_panels.h"

#include "editor_layout.h" /* editor_config_total_height */
#include "palette.h"       /* palette_render */
#include "properties.h"    /* level_config_render, properties_render */

typedef struct {
    int y;
    int total_h;
    int visible_h;
    int bottom_y;
} ConfigPanelGeometry;

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
int editor_handle_side_panel_scroll(EditorState *es, int mx, int my, int wheel_y)
{
    if (mx < CANVAS_W || my <= TOOLBAR_H || my >= EDITOR_H - STATUS_H) {
        return 0;
    }

    ConfigPanelGeometry config = config_panel_geometry(es);

    if (my < config.bottom_y) {
        cfg_scroll(-wheel_y * 20);
    } else {
        palette_scroll(-wheel_y * 20);
    }

    return 1;
}
