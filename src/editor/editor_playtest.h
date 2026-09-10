/*
 * editor_playtest.h — Editor playtest process helpers.
 */
#pragma once

#include <stddef.h>

/* Resolve the game executable beside the editor, independent of OUTDIR/cwd. */
int editor_playtest_binary_path(char *path, size_t size);

#include "editor.h"  /* EditorState */

/* Save and launch the current level in the game executable. */
void editor_play_test(EditorState *es);

/* Stop the active playtest process and restore editor mode. */
void editor_stop_play(EditorState *es);

/* Poll the active playtest process without blocking. */
void editor_check_play_status(EditorState *es);
