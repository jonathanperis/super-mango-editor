/*
 * editor_frame.h — One-frame update/render step for the editor.
 *
 * The core editor loop calls this once while running.  It resets per-frame
 * UI input, dispatches commands, updates editor state, renders the active
 * view, presents the frame, and applies the fallback frame cap.
 */

#pragma once

#include "editor.h"  /* EditorState */

/* Run one full editor frame. */
void editor_run_frame(EditorState *es);

/* End of a frame's drawing: show the mouse cursor the frame asked for
 * (the four-way arrows while entities are dragged, the I-beam over a
 * field), touching the system cursor only when it changes. */
void editor_update_cursor(EditorState *es);
