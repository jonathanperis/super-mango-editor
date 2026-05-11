/*
 * editor_frame.h — One-frame update/render step for the editor.
 *
 * The core editor loop calls this once while running.  It resets per-frame
 * UI input, dispatches SDL events, updates editor state, renders the active
 * view, presents the frame, and applies the fallback frame cap.
 */

#pragma once

#include "editor.h"  /* EditorState */

/* Run one full editor frame. */
void editor_run_frame(EditorState *es);
