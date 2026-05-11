/*
 * editor_files.h — Editor file, export, autosave, and recent-file helpers.
 */
#pragma once

#include "editor.h"  /* EditorState */

/* Show a native file picker and load the selected level. */
void editor_open_level_file(EditorState *es);

/* Save the current level to TOML, choosing a default path if needed. */
int editor_save_current_level(EditorState *es);

/* Export the current level to generated C source. */
int editor_export_current_level(EditorState *es);

/* Periodically write a valid dirty level to the autosave path. */
void editor_maybe_autosave(EditorState *es);

/* Load recent file paths from persistent editor state. */
void editor_load_recent_files(EditorState *es);

/* Return non-zero when a path exists and can be opened for reading. */
int editor_file_exists(const char *path);
