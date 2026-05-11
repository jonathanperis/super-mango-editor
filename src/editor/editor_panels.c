/*
 * editor_panels.c — Right-side editor panel layout and rendering.
 */

#include "editor_panels.h"

#include "editor_layout.h" /* editor_config_total_height */
#include "palette.h"       /* palette_render */
#include "properties.h"    /* level_config_render, properties_render */

/*
 * editor_render_side_panels — Draw level config, palette, and properties.
 *
 * The right column stacks three collapsible sections. Level config is capped
 * to half the right column; palette fills the remaining space above optional
 * properties.
 */
void editor_render_side_panels(EditorState *es)
{
    int right_top    = TOOLBAR_H;
    int right_bottom = EDITOR_H - STATUS_H;
    int section_hdr  = 28;  /* matches palette TITLE_H */

    int config_y = right_top;
    int config_h_total;
    if (es->config_open) {
        config_h_total = editor_config_total_height(es);
    } else {
        config_h_total = section_hdr;
    }

    int config_h_max = (right_bottom - right_top) / 2;
    int config_h = config_h_total < config_h_max
                 ? config_h_total : config_h_max;

    int props_h = 0;
    if (es->selection.index >= 0) {
        props_h = es->panel_open ? 200 : section_hdr;
    }

    int palette_y = config_y + config_h;
    int palette_h;
    if (es->palette_open) {
        palette_h = right_bottom - palette_y - props_h;
        if (palette_h < section_hdr + 50) palette_h = section_hdr + 50;
    } else {
        palette_h = section_hdr;
    }

    int props_y = palette_y + palette_h;

    level_config_render(es, config_y, config_h, config_h_total);
    palette_render(es, palette_y, palette_h);
    if (es->selection.index >= 0) {
        properties_render(es, props_y, props_h);
    }
}
