/*
 * hit_test.h — Where each entity appears on the canvas, and which one is
 *              under the mouse.
 *
 * Clicking, the selection outline and dragging all need the same answer to
 * "which rectangle does this entity cover?".  Keeping that answer in one
 * function means a click target always matches what the canvas draws.
 *
 * All coordinates are world-space logical pixels (the same units as the
 * TOML file), never screen pixels; canvas.c converts between the two.
 */
#pragma once

#include "editor.h"  /* EntityType, Selection, LevelDef */

/*
 * EditorRect — an axis-aligned box in world pixels.
 *
 * x, y is the top-left corner; w, h are the width and height.  Floats are
 * used because positions are floats in LevelDef, and so that a corrupt
 * tile count cannot overflow an int while computing a width.
 */
typedef struct {
    float x;
    float y;
    float w;
    float h;
} EditorRect;

/*
 * editor_entity_bounds — Compute the on-canvas rectangle of one entity.
 *
 * Mirrors canvas.c: derived positions (spiders on the floor, flames centred
 * in their gap, rail riders on their rail) are computed here too.
 *
 * Returns 1 and fills *out when the entity exists and has a position;
 * returns 0 for an invalid index or a rail rider whose rail is missing.
 */
int editor_entity_bounds(const LevelDef *level, EntityType type, int index,
                         EditorRect *out);

/*
 * editor_hit_test — Return the frontmost entity whose rectangle contains
 *                   the world point (wx, wy).
 *
 * Types are tested in reverse draw order (enemies first, world geometry
 * last) so that when entities overlap, the one drawn on top wins.  Within a
 * type, later array entries are drawn later and therefore tested first.
 *
 * Returns {type, index}; index is -1 when nothing is under the point.
 */
Selection editor_hit_test(const LevelDef *level, float wx, float wy);
