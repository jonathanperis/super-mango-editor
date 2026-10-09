/*
 * editor_panels.h — Right-side editor panel layout helpers.
 */

#ifndef EDITOR_PANELS_H
#define EDITOR_PANELS_H

#include "editor.h" /* EditorState */

void editor_render_side_panels(EditorState *es);
int editor_handle_side_panel_scroll(EditorState *es, int mx, int my, float wheel_y);

/* Go to what validation message `message` is about: select the entity and
 * pan the canvas to it, or focus the Level Config field.  Returns 1 when
 * the message pointed somewhere. */
int editor_focus_validation_issue(EditorState *es, int message);

#endif /* EDITOR_PANELS_H */
