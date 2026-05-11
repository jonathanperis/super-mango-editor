/*
 * editor_clipboard.h — Editor clipboard copy/paste helpers.
 */
#pragma once

#include "editor.h"  /* EditorState */

/* Snapshot the current selection into the editor clipboard. */
void editor_copy_selected(EditorState *es);

/* Paste the clipboard entity as a new placement. */
void editor_paste_clipboard(EditorState *es);
