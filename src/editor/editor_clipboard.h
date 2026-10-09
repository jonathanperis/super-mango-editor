/*
 * editor_clipboard.h — Editor clipboard copy/paste helpers.
 */
#pragma once

#include "editor.h"  /* EditorState */

/* Snapshot the current selection into the editor clipboard. */
void editor_copy_selected(EditorState *es);

/* Paste the clipboard entity as a new placement. */
void editor_paste_clipboard(EditorState *es);

/* Ctrl+D: copy the selection one paste offset along and select the copy
 * (repeat to keep stepping).  The clipboard is left alone. */
void editor_duplicate_selection(EditorState *es);

/* Keep a copied rail rider pointing at its rail when the rails array
 * shifts: call after a rail at index is removed or inserted. */
void editor_clipboard_after_rail_remove(EditorState *es, int index);
void editor_clipboard_after_rail_insert(EditorState *es, int index);
