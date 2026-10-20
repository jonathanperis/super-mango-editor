/*
 * editor_panels.h — Right-side editor panel layout helpers.
 */

#ifndef EDITOR_PANELS_H
#define EDITOR_PANELS_H

#include "editor.h" /* EditorState */

/*
 * Where the right column's three sections go this frame.  The column
 * follows the tool: with an entity selected and a tool other than Place,
 * the palette folds to its title bar (palette_folded) and the selection's
 * properties take the rest of the column; otherwise the palette fills it
 * above a 200 px properties panel.  Level Config keeps its own open/closed
 * state, at most half the column.  props_h is 0 with nothing selected.
 */
typedef struct {
    int config_y, config_h, config_total_h;
    int palette_y, palette_h, palette_folded;
    int props_y, props_h;
} EditorSideLayout;

void editor_side_panel_layout(const EditorState *es, EditorSideLayout *out);
void editor_render_side_panels(EditorState *es);
int editor_handle_side_panel_scroll(EditorState *es, int mx, int my, float wheel_y);

/* Go to what validation message `message` is about: select the entity and
 * pan the canvas to it, or focus the Level Config field.  Returns 1 when
 * the message pointed somewhere. */
int editor_focus_validation_issue(EditorState *es, int message);

#endif /* EDITOR_PANELS_H */
