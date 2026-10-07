/*
 * editor_files.h — Opening and saving levels, playtest copies and the
 *                  recent-file list.
 *
 * Autosave and crash recovery are declared in editor_recovery.h.
 */
#pragma once

#include <stddef.h>    /* size_t */
#include <stdio.h>     /* FILE */

#include "editor.h"  /* EditorState */

/* Show a native file picker and load the selected level. */
void editor_open_level_file(EditorState *es);

/* Save the current level; untitled documents route to Save As. */
int editor_save_current_level(EditorState *es);

/* Show Save As and save without changing the active path until success. */
int editor_save_current_level_as(EditorState *es);

/* Resolve OS preference-storage paths used by editor persistence. */
int editor_init_persistence_paths(EditorState *es);

/* Inject a preference root before persistence initialization (tests/tools). */
int editor_set_preference_root(EditorState *es, const char *root);

/* Load recent file paths from persistent editor state. */
void editor_load_recent_files(EditorState *es);

/* Return non-zero when a path exists and can be opened for reading. */
int editor_file_exists(const char *path);

/* Reject paths that cannot fit without truncation in editor state. */
int editor_path_fits(const char *path);

/* Copy path into out for display. A path too long for out keeps its end
 * (the file name) and starts with "..." instead of being cut mid-name. */
void editor_path_for_display(const char *path, char *out, size_t out_size);

#ifdef MANGO_TESTING
/* Test seam: called after each parse inside editor_load_level, so a test
 * can rewrite the file mid-load.  NULL (the default) disables it. */
void editor_test_set_load_hook(void (*hook)(const char *path));
#endif

/* Serialize playtest content to preference storage without changing save state. */
int editor_prepare_playtest_level(EditorState *es, char *path, size_t path_size);

/* Remove the private playtest snapshot after child completion/stop. */
void editor_retire_playtest_level(EditorState *es);

/* Shared config-dependent preview resource synchronization. */
void editor_sync_config_resources(EditorState *es);

/* ------------------------------------------------------------------ */
/* Helpers shared with editor_recovery.c                               */
/* ------------------------------------------------------------------ */

/*
 * Make `level` the open document.  path becomes file_path (NULL means
 * untitled); modified = 1 marks it unsaved (a recovered snapshot), 0 makes
 * it the save point; add_recent = 1 also lists path under recent files.
 */
void editor_apply_loaded_level(EditorState *es, const LevelDef *level,
                               const char *path, int modified,
                               int add_recent);

/*
 * Finish an atomic replace begun with serializer_open_temp: check, flush
 * and close fp, then rename temp_path over target_path.  On failure the
 * temp file is deleted and the target is untouched.  Takes ownership of fp.
 */
int editor_commit_temp_file(FILE *fp, const char *temp_path,
                            const char *target_path);

/* The editor's preference folder (or the root injected by
 * editor_set_preference_root), and a file name inside it. */
int editor_preference_root_path(const EditorState *es, char *buf,
                                size_t buf_size);
int editor_preference_file_path(const EditorState *es, const char *name,
                                char *buf, size_t buf_size);
