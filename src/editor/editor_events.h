/*
 * editor_events.h — SDL event dispatch for the Super Mango editor.
 *
 * The editor loop polls SDL events once per frame and forwards each event
 * here.  This module owns keyboard shortcuts, text input forwarding, mouse
 * tool routing, panel scroll routing, and camera zoom/pan controls.
 */

#pragma once

#include <SDL.h>      /* SDL_Event */

#include "editor.h"  /* EditorState */

/*
 * editor_handle_event — Dispatch one SDL event to editor subsystems.
 *
 * Routes window close, keyboard shortcuts, text input, mouse buttons,
 * wheel scrolling, and drag motion.  Mutates EditorState directly.
 */
void editor_handle_event(EditorState *es, SDL_Event *event);
