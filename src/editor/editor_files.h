/*
 * editor_files.h — Editor file, autosave, and recent-file helpers.
 */
#pragma once

#include <stddef.h>    /* size_t */

#include "editor.h"  /* EditorState */

/* Show a native file picker and load the selected level. */
void editor_open_level_file(EditorState *es);

/* Save the current level; untitled documents route to Save As. */
int editor_save_current_level(EditorState *es);

/* Show Save As and save without changing the active path until success. */
int editor_save_current_level_as(EditorState *es);

/* Periodically write a valid dirty level to the recovery path. */
void editor_maybe_autosave(EditorState *es);

/* Load recovery content as dirty without adding it to recents. */
int editor_recover_autosave(EditorState *es);

/* Resolve SDL preference-storage paths used by editor persistence. */
int editor_init_persistence_paths(EditorState *es);

/* Inject a preference root before persistence initialization (tests/tools). */
int editor_set_preference_root(EditorState *es, const char *root);

/* Select per-document recovery storage after a new/open/save-as transition. */
int editor_set_recovery_document(EditorState *es, const char *document_path);

/* Load recent file paths from persistent editor state. */
void editor_load_recent_files(EditorState *es);

/* Return non-zero when a path exists and can be opened for reading. */
int editor_file_exists(const char *path);

/* Reject paths that cannot fit without truncation in editor state. */
int editor_path_fits(const char *path);

/* Discover persistent recovery entries and update startup status. */
int editor_discover_recoveries(EditorState *es);

/* Recover one manifest entry after its snapshot has loaded successfully. */
int editor_recover_entry(EditorState *es, int entry_index);

/* Recover by stable session ID after re-discovering current entries. */
int editor_recover_entry_by_id(EditorState *es, uint64_t recovery_id);

/* Show the native Recover / Next / Cancel picker. */
int editor_choose_recovery(EditorState *es);

/* Native picker seam used by focused recovery tests. */
void editor_test_set_recovery_choice(int button_id);

/* Retire recovery only when metadata identifies the supplied document. */
void editor_retire_matching_recovery(EditorState *es, const char *destination);

/* Retire the current document recovery after an explicit discard. */
void editor_retire_current_recovery(EditorState *es);

/* Serialize playtest content to preference storage without changing save state. */
int editor_prepare_playtest_level(EditorState *es, char *path, size_t path_size);

/* Remove the private playtest snapshot after child completion/stop. */
void editor_retire_playtest_level(EditorState *es);

/* Shared config-dependent preview resource synchronization. */
void editor_sync_config_resources(EditorState *es);
