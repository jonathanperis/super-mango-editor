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

/* Resolve retained field text before any command can change editor state:
 * a valid value is applied as its own undo step; only an invalid one asks
 * (Keep Editing / Discard).  Returns 1 when the command may proceed. */
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

/* The buttons of the skip-this-undo-step prompt, in the order shown. */
typedef enum {
    EDITOR_SKIP_STEP_CANCEL = 0,   /* also: the dialog was closed           */
    EDITOR_SKIP_STEP_KEEP = 1,     /* the default: Enter changes nothing    */
    EDITOR_SKIP_STEP_SKIP = 2      /* the only answer that drops the step   */
} EditorSkipStepChoice;

/* Ask whether the Campaign view may drop an Undo step it cannot replay,
 * because the level file `path` it must read no longer loads.  Returns 1
 * only when the designer chose "Skip this step"; "Keep the step" is the
 * dialog's default, and it and Cancel both return 0. */
int editor_confirm_skip_undo_step(EditorState *es, const char *path);

typedef enum {
    EDITOR_EXTERNAL_CANCEL = 0,
    EDITOR_EXTERNAL_REPLACE = 1,
    EDITOR_EXTERNAL_SAVE_AS = 2
} EditorExternalChoice;

/* Ask how to handle a source changed on disk. */
EditorExternalChoice editor_confirm_external_change(EditorState *es);

/* Frame rates for the editor's window with and without focus. */
#define EDITOR_FPS_FOCUSED   60
#define EDITOR_FPS_UNFOCUSED 10

/* The frame cap a shown window should have: EDITOR_FPS_FOCUSED while it
 * has focus, EDITOR_FPS_UNFOCUSED while another window does. 0 for a
 * hidden window, whose own pacing (none, for smoke runs) stays as it is. */
int editor_frame_rate_for(int hidden, int focused);

/* Apply editor_frame_rate_for to the real window, calling SetTargetFPS only
 * when the rate changes. Both frame loops call it: the editor's and a
 * prompt's. A Memory-backend build has no real focus and skips it. */
void editor_pace_frames(EditorState *es);

#ifdef MANGO_TESTING
/* Native-dialog seams used by focused editor state tests.  Only test builds
 * define MANGO_TESTING (see TEST_CFLAGS in the Makefile), so the shipped
 * editor has no way to skip a confirmation dialog. */
/* Answer for the invalid-field prompt: 2 = Discard, other values keep
 * editing, -1 = no canned answer.  Used up by the next finish call. */
void editor_test_set_finish_field_choice(int button_id);
void editor_test_set_discard_choice(int button_id);
void editor_test_set_overwrite_choice(int button_id);
void editor_test_set_external_choice(EditorExternalChoice choice);
/* Answer for the skip-this-undo-step prompt: an EditorSkipStepChoice, or
 * -1 for no canned answer.  Used up by the next prompt. */
void editor_test_set_skip_step_choice(int button_id);
#endif
