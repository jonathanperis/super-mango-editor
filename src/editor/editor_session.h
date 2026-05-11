/*
 * editor_session.h — Editor session state helpers.
 */
#pragma once

#include "editor.h"  /* EditorState */

/* Format the editor status bar message. */
void editor_set_status(EditorState *es, const char *fmt, ...);

/* Refresh the window title from file path and modified flag. */
void editor_update_window_title(EditorState *es);

/* Reset the editor to a blank default level. */
void editor_reset_new_level(EditorState *es);

/* Return non-zero when validation allows save/export/playtest. */
int editor_can_persist(EditorState *es, const char *action);

/* Ask user whether unsaved changes may be discarded. */
int editor_confirm_discard_changes(EditorState *es, const char *action);
