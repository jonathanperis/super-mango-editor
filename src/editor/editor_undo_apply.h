/*
 * editor_undo_apply.h — Level mutation helpers for undo/redo commands.
 */

#ifndef EDITOR_UNDO_APPLY_H
#define EDITOR_UNDO_APPLY_H

#include "editor.h" /* EditorState */
#include "undo.h"   /* Command */

#define EDITOR_CHANGE_ENTITY 1
#define EDITOR_CHANGE_CONFIG 2
#define EDITOR_LAST_STAR_NEXT_PHASE_WIDGET ((int)ENT_LAST_STAR * 100 + 3)

/* Snapshot helpers shared by tools, inspector commits, and tests. */
PlacementData editor_snapshot_entity(const LevelDef *level,
                                     EntityType type, int index);
LevelConfigSnapshot editor_snapshot_config(const LevelDef *level);

/* UI mutation tracking.  Capture happens before widget writes its value. */
void editor_begin_change_tracking(EditorState *es, int kind);
void editor_capture_change_before(EditorState *es, int widget_id);
void editor_before_change(void *context, int widget_id);
void editor_commit_change(EditorState *es);
void editor_end_change_tracking(EditorState *es);

void editor_apply_undo_command(EditorState *es, const Command *cmd, int reverse);

#endif /* EDITOR_UNDO_APPLY_H */
