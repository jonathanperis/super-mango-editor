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
