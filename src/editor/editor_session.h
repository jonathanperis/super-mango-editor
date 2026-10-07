/*
 * editor_session.h — Editor session state helpers.
 */
#pragma once

#include "editor.h"  /* EditorState */
#include "../shared/printf_format.h" /* PRINTF_FORMAT */

/* Format the editor status bar message (printf-style; the compiler checks
 * every call's arguments against fmt). */
void editor_set_status(EditorState *es, const char *fmt, ...) PRINTF_FORMAT(2, 3);

/* Fill a LevelDef with the editor's blank-level defaults. */
void editor_level_init_defaults(LevelDef *level);

/* Refresh the window title from file path and modified flag. */
void editor_update_window_title(EditorState *es);

/* Recompute dirty marker from current document vs explicit save point. */
uint64_t editor_document_hash(const LevelDef *level);
void editor_set_document_save_point(EditorState *es);
void editor_set_recovered_dirty(EditorState *es);
void editor_refresh_dirty(EditorState *es);

/* Resolve retained field text before any command can change editor state. */
int editor_finish_field_edit(EditorState *es);
int editor_before_command(void *context);

/* Reset the editor to a blank default level. */
void editor_reset_new_level(EditorState *es);

/* Return non-zero when validation allows save or playtest. */
int editor_can_persist(EditorState *es, const char *action);

/* Ask user whether unsaved changes may be discarded. */
int editor_confirm_discard_changes(EditorState *es, const char *action);

/* Ask user whether an existing Save As target may be replaced. */
int editor_confirm_overwrite(EditorState *es, const char *path);

typedef enum {
    EDITOR_EXTERNAL_CANCEL = 0,
    EDITOR_EXTERNAL_REPLACE = 1,
    EDITOR_EXTERNAL_SAVE_AS = 2
} EditorExternalChoice;

/* Ask how to handle a source changed on disk. */
EditorExternalChoice editor_confirm_external_change(EditorState *es);

#ifdef MANGO_TESTING
/* Native-dialog seams used by focused editor state tests.  Only test builds
 * define MANGO_TESTING (see TEST_CFLAGS in the Makefile), so the shipped
 * editor has no way to skip a confirmation dialog. */
void editor_test_set_finish_field_choice(int button_id);
void editor_test_set_discard_choice(int button_id);
void editor_test_set_overwrite_choice(int button_id);
void editor_test_set_external_choice(EditorExternalChoice choice);
#endif
