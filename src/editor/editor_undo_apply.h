/*
 * editor_undo_apply.h — Level mutation helpers for undo/redo commands.
 */

#ifndef EDITOR_UNDO_APPLY_H
#define EDITOR_UNDO_APPLY_H

#include "editor.h" /* EditorState */
#include "undo.h"   /* Command */

void editor_apply_undo_command(EditorState *es, const Command *cmd, int reverse);

#endif /* EDITOR_UNDO_APPLY_H */
