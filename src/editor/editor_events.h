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

#ifdef MANGO_TESTING
/* Test builds keep the system clipboard out of it (the headless raylib
 * has none): Ctrl+C / Ctrl+X store text here, and Ctrl+V pastes it. */
const char *editor_test_clipboard(void);
void editor_test_set_clipboard(const char *text);
#endif
