/*
 * editor_playtest.h — Editor playtest process helpers.
 */
#pragma once

#include <stddef.h>

/* Resolve the game executable beside the editor, independent of OUTDIR/cwd. */
int editor_playtest_binary_path(char *path, size_t size);

#include "editor.h"  /* EditorState */
#include "../levels/level_start.h"  /* LevelStart */

/* Save and launch the current level in the game executable. */
void editor_play_test(EditorState *es);

/*
 * editor_play_test_from — editor_play_test, starting the player at
 * `start` (the game's --start-x / --start-checkpoint).  NULL, or a
 * LEVEL_START_DEFAULT start, is the level's own start.
 */
void editor_play_test_from(EditorState *es, const LevelStart *start);

/*
 * editor_playtest_start_here — Where "Playtest from here" (Shift+F5)
 * starts: on the selected checkpoint when exactly one checkpoint is
 * selected, otherwise under the mouse when it is over the canvas.  The
 * point is checked with the game's own rules (level_start_resolve), so a
 * spot over a floor gap is refused here, with the reason in the status
 * bar, instead of by the game.  Returns 0 and fills *start, or -1.
 */
int editor_playtest_start_here(EditorState *es, LevelStart *start);

/* Shift+F5: editor_playtest_start_here, then editor_play_test_from. */
void editor_play_test_here(EditorState *es);

/*
 * editor_playtest_arguments — The game's command line for a playtest:
 * binary, --level level, --no-save, --debug when debug, and the start
 * flag.  The start's number is written into number (at least 16 bytes),
 * which argv then points into.  argv ends with NULL; returns how many
 * arguments it holds, or -1 when max is too small.
 */
int editor_playtest_arguments(const char *binary, const char *level, int debug,
                              const LevelStart *start, char *number,
                              const char **argv, int max);

#if defined(MANGO_TESTING) && !defined(_WIN32)
/* Test seam: when set, a playtest calls launch(argv) instead of looking
 * for the game executable and running it; launch returns the pid of a
 * child the editor then follows and reaps (or -1 for a failed launch).
 * NULL (the default) launches the real game. */
void editor_test_set_play_launcher(int (*launch)(const char *const *argv));
#endif

/* Stop the active playtest process and restore editor mode. */
void editor_stop_play(EditorState *es);

/* Poll the active playtest process without blocking. */
void editor_check_play_status(EditorState *es);
