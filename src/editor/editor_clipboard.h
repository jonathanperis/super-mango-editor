/*
 * editor_clipboard.h — Editor clipboard copy/paste helpers.
 */
#pragma once

#include "editor.h"  /* EditorState */

/* Snapshot every selected entity into the editor clipboard. */
void editor_copy_selected(EditorState *es);

/* Another document was opened: the clipboard's rail riders can no longer
 * name a rail by position here (their rail shapes still match). */
void editor_clipboard_forget_rails(EditorState *es);

/* Paste the clipboard entity as a new placement. */
void editor_paste_clipboard(EditorState *es);

/* Ctrl+D: copy the selection one paste offset along and select the copy
 * (repeat to keep stepping).  The clipboard is left alone. */
void editor_duplicate_selection(EditorState *es);

/* Keep copied rail riders (and copied rails) pointing at their rails when
 * the rails array shifts: call after a rail at index is removed or inserted. */
void editor_clipboard_after_rail_remove(EditorState *es, int index);
void editor_clipboard_after_rail_insert(EditorState *es, int index);
