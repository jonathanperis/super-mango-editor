/*
 * tools.h --- Public interface for the editor's interactive tools.
 *
 * Declares the functions that implement the four editing interactions:
 *   SELECT — click an entity to highlight and inspect it.
 *   PLACE  — click empty space to stamp a new entity from the palette.
 *   MOVE   — drag a selected entity to reposition it.
 *   DELETE — click or right-click an entity to remove it.
 *
 * All coordinates are in world-space logical pixels (0..WORLD_W, 0..GAME_H).
 * The caller (editor.c) converts screen mouse coordinates to world coordinates
 * before passing them here.
 *
 * Every function takes a pointer to EditorState so it can read the active
 * tool, modify the LevelDef, update the selection, and push undo commands.
 */
#pragma once

#include "editor.h"   /* EditorState, EditorTool, EntityType, Selection */

/* ------------------------------------------------------------------ */
/* Tool interaction entry points                                       */
/* ------------------------------------------------------------------ */

/*
 * tools_mouse_down --- Handle a left-click at world position (world_x, world_y).
 *
 * Behaviour depends on es->tool:
 *   TOOL_SELECT : hit-test entities; select the topmost one under the cursor.
 *   TOOL_PLACE  : create a new entity of es->palette_type at the position.
 *   TOOL_DELETE : hit-test entities; remove the topmost one under the cursor.
 */
void tools_mouse_down(EditorState *es, float world_x, float world_y);

/*
 * tools_mouse_up --- Handle left-button release at world position.
 *
 * Ends any in-progress drag.  If the entity actually moved from its drag
 * start position, a CMD_MOVE undo command is pushed.
 */
void tools_mouse_up(EditorState *es, float world_x, float world_y);

/*
 * tools_mouse_drag --- Handle mouse motion while left button is held.
 *
 * If a drag is in progress (es->dragging), updates the selected entity's
 * position to (world_x, world_y).  Holding Shift snaps to TILE_SIZE grid.
 */
void tools_mouse_drag(EditorState *es, float world_x, float world_y);

/*
 * tools_right_click --- Handle a right-click at world position.
 *
 * Convenience shortcut: right-clicking any entity deletes it regardless
 * of the current tool mode.  Hit-tests, snapshots for undo, removes.
 */
void tools_right_click(EditorState *es, float world_x, float world_y);

/*
 * tools_delete_selected --- Delete the currently selected entity (if any).
 *
 * Called from keyboard (Delete / Backspace key handler).  Operates on
 * es->selection; does nothing if nothing is selected (index == -1).
 */
void tools_delete_selected(EditorState *es);
