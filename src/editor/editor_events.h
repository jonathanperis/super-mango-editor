/*
 * editor_events.h — Input command dispatch for the Super Mango editor.
 *
 * The editor loop samples input once per frame and forwards each command
 * here.  This module owns keyboard shortcuts, text input forwarding, mouse
 * tool routing, panel scroll routing, and camera zoom/pan controls.
 */

#pragma once

#include "../input/input_backend.h"

#include "editor.h"  /* EditorState */

/*
 * editor_handle_event — Dispatch one input command to editor subsystems.
 *
 * Routes window close, keyboard shortcuts, text input, mouse buttons,
 * wheel scrolling, and drag motion.  Mutates EditorState directly.
 */
void editor_handle_event(EditorState *es, const InputEvent *event);

/*
 * editor_handle_dropped_files — Open a level dropped on the window.
 *
 * Call once per frame after the queued events.  One dropped .toml file
 * opens like a recent file (asking about unsaved changes first, and
 * closing the Campaign view); anything else, or a drop during a
 * playtest, leaves the document alone and says why in the status bar.
 */
void editor_handle_dropped_files(EditorState *es);

#ifdef MANGO_TESTING
/* Test builds keep the system clipboard out of it (the headless raylib
 * has none): Ctrl+C / Ctrl+X store text here, and Ctrl+V pastes it. */
const char *editor_test_clipboard(void);
void editor_test_set_clipboard(const char *text);
/* The next editor_handle_dropped_files sees these paths as one drop.  The
 * paths are copied into a list that behaves like raylib's: a drop made
 * while an earlier one is still held replaces it in place. */
void editor_test_drop_files(const char *const *paths, unsigned int count);
/* Times the editor released a drop that a newer one had already replaced
 * (raylib would have freed that list twice). */
int editor_test_drop_misuses(void);
#endif
