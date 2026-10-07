/*
 * editor_recovery.h — Autosave snapshots and crash recovery.
 *
 * Every open document has a recovery id.  While it has unsaved changes the
 * editor writes editor_recovery_<id>.toml (the level) and
 * editor_recovery_<id>.meta (where it came from) into its preference
 * folder; a clean save or an explicit discard deletes them.  After a crash
 * the next start lists the leftover pairs and offers to load one.
 */
#pragma once

#include <stdint.h>  /* uint64_t */

#include "editor.h"  /* EditorState, EditorRecoveryEntry */

/* Every 30 s while dirty, write a recovery snapshot.  An invalid level is
 * not loadable, so its last valid version is snapshotted instead.  Failed
 * attempts also wait a full interval before retrying. */
void editor_maybe_autosave(EditorState *es);

/* Record the current level as this document's newest valid version. */
void editor_remember_valid_level(EditorState *es);

/* Give the document a fresh recovery id and snapshot path after a
 * new/open/save-as transition.  document_path may be NULL (untitled). */
int editor_set_recovery_document(EditorState *es, const char *document_path);

/* Re-read the recovery entries on disk into es->recovery_entries. */
int editor_discover_recoveries(EditorState *es);

/* Show the native Recover / Next / Cancel picker.  Returns the chosen
 * entry's index (its id is left in es->pending_recovery_id) or -1. */
int editor_choose_recovery(EditorState *es);

/* Load one recovery snapshot, found by its id, as an unsaved document. */
int editor_recover_entry_by_id(EditorState *es, uint64_t recovery_id);

/* Load this document's own recovery copy as dirty without adding it to
 * recents. */
int editor_recover_autosave(EditorState *es);

/* Retire recovery only when metadata identifies the supplied document. */
void editor_retire_matching_recovery(EditorState *es, const char *destination);

/* Retire the current document recovery after a save or explicit discard. */
void editor_retire_current_recovery(EditorState *es);

/* Non-zero when the file name in path is a recovery snapshot
 * (editor_recovery_<16 hex digits>.toml); such files are never saved over. */
int editor_path_is_recovery(const char *path);

#ifdef MANGO_TESTING
/* Native picker seam used by focused recovery tests (test builds only). */
void editor_test_set_recovery_choice(int button_id);
#endif
