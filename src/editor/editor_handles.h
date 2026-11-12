/*
 * editor_handles.h — Patrol ranges and resize handles on the canvas.
 *
 * Two things the Properties panel only shows as numbers are also drawn on
 * the level itself:
 *
 *   - the range a patrolling enemy or saw travels (patrol_x0 .. patrol_x1),
 *     as a line with a tick at each end;
 *   - small square handles on the one selected entity, which the Select
 *     tool can drag: the two ends of a patrol range, the growing edge of
 *     anything built from tiles, and the corner of a rail.
 *
 * This file answers "where are they?" and "what does dragging one do?".
 * canvas.c draws them and tools_drag.c routes the mouse here, so a handle
 * is always drawn exactly where a press on it is tested.
 *
 * All positions are world pixels (the units of the TOML file).  Handles
 * keep the same size on screen at every zoom: the caller divides screen
 * pixels by the zoom, as editor_handle_at does.
 */
#pragma once

#include "editor.h"   /* EditorState, EntityType, LevelDef */

/*
 * EditorPatrolRange — the stretch of the level a patroller's sprite covers
 * while it patrols, as a horizontal line.
 *
 * x0 is where the sprite's left edge turns round and x1 where its right
 * edge does.  That is the meaning level validation gives the stored fields
 * (validate_patrol in src/levels/level_validate.c): an enemy turns when its
 * left edge reaches patrol_x0 and its right edge reaches patrol_x1, so for
 * the six enemies x0 and x1 are the stored values.  A circular saw bounces
 * its x itself between the two, so its sprite reaches one sprite width
 * past patrol_x1, and x1 here includes that.
 *
 * y is the middle of the entity as the canvas draws it.
 */
typedef struct {
    float x0;
    float x1;
    float y;
} EditorPatrolRange;

/*
 * editor_patrol_range — The range of entity (type, index).  Returns 1 and
 * fills *out for a spider, jumping spider, bird, faster bird, fish, faster
 * fish or circular saw that exists; 0 for any other type or index.
 */
int editor_patrol_range(const LevelDef *level, EntityType type, int index,
                        EditorPatrolRange *out);

/* What dragging a handle changes. */
typedef enum {
    EDITOR_HANDLE_NONE = 0,
    EDITOR_HANDLE_PATROL_START,  /* patrol_x0: the left end of the range     */
    EDITOR_HANDLE_PATROL_END,    /* patrol_x1: the right end of the range    */
    EDITOR_HANDLE_WIDTH,         /* tiles across: count, tile_count,
                                    brick_count, a platform's tile_width     */
    EDITOR_HANDLE_HEIGHT,        /* tiles down a vine, ladder or rope; a
                                    platform's tile_height (its top edge)    */
    EDITOR_HANDLE_CORNER         /* a rail's w and h together                */
} EditorHandleKind;

/* One handle: what it changes, and its centre in world pixels. */
typedef struct {
    EditorHandleKind kind;
    float            x;
    float            y;
} EditorHandle;

/* No entity has more than two handles (a patrol's two ends; a platform's
 * width and height). */
#define EDITOR_HANDLE_MAX 2

/*
 * A handle is drawn as a square EDITOR_HANDLE_SIZE_PX canvas pixels wide,
 * centred on its point.  A press counts as "on the handle" up to
 * EDITOR_HANDLE_GRAB_PX canvas pixels from that centre along each axis:
 * the square itself and two pixels of forgiveness around it.
 */
#define EDITOR_HANDLE_SIZE_PX 8
#define EDITOR_HANDLE_GRAB_PX 6.0f

/*
 * editor_entity_handles — The handles entity (type, index) has, written to
 * out (room for EDITOR_HANDLE_MAX).  Returns how many: 0 for a type with
 * nothing to resize (a coin), or an entity that does not exist.
 */
int editor_entity_handles(const LevelDef *level, EntityType type, int index,
                          EditorHandle *out);

/*
 * editor_handles_shown — 1 while the canvas shows handles: the Select tool
 * is active, exactly one entity is selected, and no move is under way.
 */
int editor_handles_shown(const EditorState *es);

/*
 * editor_handle_at — The handle of the selected entity under world point
 * (world_x, world_y), or EDITOR_HANDLE_NONE.  The tolerance is in canvas
 * pixels, so it is divided by the zoom here.
 */
EditorHandleKind editor_handle_at(const EditorState *es, float world_x, float world_y);

/*
 * The resize gesture.  tools_drag.c calls these from the Select tool's
 * press, motion, release and Esc.
 *
 * editor_resize_begin  : a press at (world_x, world_y).  Returns 1 when it
 *                        landed on a handle and a resize has started (the
 *                        press then selects nothing and moves nothing).
 * editor_resize_drag   : pointer motion.  Sizes change by whole tiles;
 *                        patrol ends follow the pointer in whole pixels, or
 *                        to the nearest 48 px grid line while snapping
 *                        (key S, Shift for the other way).  A value level
 *                        validation would reject is never written: the
 *                        size stops at the last one it accepts.  The
 *                        status bar shows the value.
 * editor_resize_finish : the release.  A changed entity becomes one undo
 *                        step.
 * editor_resize_cancel : Esc.  The entity is put back as it was.
 */
int editor_resize_begin(EditorState *es, float world_x, float world_y);
void editor_resize_drag(EditorState *es, float world_x, float world_y);
void editor_resize_finish(EditorState *es);
void editor_resize_cancel(EditorState *es);
