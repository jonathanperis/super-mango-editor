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

/*
 * Rail riders (spike blocks, rail-mode float platforms) move at more than 0
 * and at most MAX_RAIL_SPEED tiles/s (rail.h).  RAIL_SPEED_MIN stands in
 * for "more than 0" in the speed fields; RAIL_SPEED_DEFAULT is the speed a
 * new rider gets, the same as a newly placed spike block.
 */
#define RAIL_SPEED_MIN     0.1f
#define RAIL_SPEED_DEFAULT 3.0f

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
 * If a drag is in progress (es->dragging) and the cursor has moved past a
 * small threshold, moves the dragged entity so the point grabbed at
 * mouse-down follows the cursor.  Holding Shift snaps to TILE_SIZE grid.
 * Patrolling enemies take their patrol range along; positions are clamped
 * so the level stays valid.
 */
void tools_mouse_drag(EditorState *es, float world_x, float world_y);

/*
 * tools_cancel_drag --- Abort a drag (Esc) and put the entity back where
 * it was at mouse-down.  No undo entry is recorded.
 */
void tools_cancel_drag(EditorState *es);

/*
 * editor_clamp_placement --- Pull a placement back inside the level.
 *
 * Applies the world-bounds rules level validation enforces: positions
 * within [0, world width] x [0, GAME_H], wide entities fully inside,
 * patrol ranges inside the world (the entity slides with its range),
 * rail riders' t_offset on their rail (loops wrap, open rails stop), and
 * flames onto the nearest floor gap (the gap they erupt from).
 * Used by place, paste and drag so none of them creates a rejected level.
 */
void editor_clamp_placement(const LevelDef *level, EntityType type,
                            PlacementData *pd);

/*
 * editor_insert_checked --- The part of editor_add_placement that touches
 * only `level`: check there is room (and a rail or floor gap when the type
 * needs one), append pd (or move a singleton, whose old data goes in
 * before), and validate the result.  Returns the new entity's index, or -1
 * with the level unchanged and why, a whole status-bar sentence, in why.
 * Paste and Duplicate run it on a scratch copy of the level first, so a
 * group that cannot be added whole touches neither level nor history.
 */
int editor_insert_checked(LevelDef *level, EntityType type,
                          const PlacementData *pd, const char *action,
                          PlacementData *before, char *why, size_t why_size);

/*
 * editor_add_placement --- Add one placement as a single undoable step.
 *
 * Shared by Place and Paste.  Appends pd (or moves a singleton), refuses
 * with a status-bar message when the array is full, when a rail rider's
 * rail does not exist, or when the result would fail level validation
 * (the level is left unchanged).  On success it records the undo command,
 * selects the new entity and returns 0; otherwise returns -1.
 *
 * action : verb for messages, e.g. "place" or "paste".
 */
int editor_add_placement(EditorState *es, EntityType type,
                         const PlacementData *pd, const char *action);

/*
 * Snap to grid.  editor_snap_active says whether the current click or drag
 * snaps: es->snap_to_grid (toggled with S), inverted while Shift is held.
 * editor_snap_point then rounds a world point down to the TILE_SIZE grid
 * (and leaves it alone when snapping is off).  Placing, dragging and the
 * placement ghost all go through these two.
 */
int editor_snap_active(const EditorState *es);
void editor_snap_point(const EditorState *es, float *x, float *y);

/*
 * editor_nearest_rail --- Index of the rail closest to world point (x, y),
 * or -1 when the level has no rails.  Spike-block placement and the float
 * platform's switch to Rail mode both attach to this rail.
 */
int editor_nearest_rail(const LevelDef *level, float x, float y);

/*
 * editor_set_float_platform_mode --- Change float platform `index` to mode.
 *
 * Switching to Rail needs a rail in the level (otherwise the status bar
 * explains and -1 is returned with nothing changed).  A stored rail_index
 * that names no rail any more is replaced by the nearest rail, the speed
 * is given a valid rail value, and the status bar names the rail.
 * Returns 0 when the mode was applied.  The caller records the undo step.
 */
int editor_set_float_platform_mode(EditorState *es, int index,
                                   FloatPlatformMode mode);

/*
 * tools_nudge_selection --- Move the selection by (dx, dy) world pixels
 * (the arrow keys: 1 px, or NUDGE_LARGE_PX with Shift).
 *
 * Uses the same per-type rules as dragging (floor-bound things ignore dy,
 * rail riders stay on their rail) and the same world clamping.  A nudge
 * that would make the level invalid is refused with a status message.
 * Nudges of the same selection less than NUDGE_COALESCE_MS apart extend
 * one undo step instead of filling the history with 1-px moves.
 */
#define NUDGE_LARGE_PX    16.0f
#define NUDGE_COALESCE_MS 1000u
void tools_nudge_selection(EditorState *es, float dx, float dy);

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
