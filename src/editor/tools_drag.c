/*
 * tools_drag.c — Select, box-select and drag with the left mouse button.
 *
 * A press arms a drag; the cursor has to travel DRAG_THRESHOLD_PX
 * before it becomes a move, so a click never nudges the selection.
 */

#include <math.h>    /* floorf, fmodf, isfinite, roundf */
#include <string.h>  /* memcmp, memset */

#include "tools.h"
#include "editor.h"  /* EditorState, EntityType, Selection, EditorTool    */
#include "editor_session.h" /* save-point dirty tracking */
#include "entity_meta.h" /* Editor display dimensions and rail helpers     */
#include "hit_test.h"    /* editor_hit_test                                */
#include "undo.h"    /* Command, PlacementData, undo_push                 */
#include "snapshot_equal.h" /* field comparison, not memcmp of the union */
#include "../levels/level_loader.h" /* level_validate_runtime            */
#include "../levels/level_validate.h" /* level_validate_preview, _each   */
#include "../game_constants.h" /* GAME_W, GAME_H, FLOOR_Y, TILE_SIZE, WORLD_W,
                                   FLOOR_GAP_W; MAX_* via level.h           */
#include "../surfaces/rail.h" /* MAX_RAIL_SPEED                            */
#include "tools_internal.h"

/* The errors the level had when the current drag began (one drag at a
 * time): every step and the drop are measured against them. */
static EditorIssueSnapshot drag_issues;

/* ================================================================== */
/* Public API                                                          */
/* ================================================================== */

/*
 * DRAG_THRESHOLD_PX --- how far (in canvas pixels) the cursor must travel
 * before a press on an entity becomes a move.  Smaller wobbles during a
 * click are ignored, so selecting never nudges the entity or adds an undo
 * step.
 */
#define DRAG_THRESHOLD_PX 3.0f

/* ------------------------------------------------------------------ */
/* tools_mouse_down                                                    */
/* ------------------------------------------------------------------ */

/*
 * select_and_arm_drag — TOOL_SELECT press: select what is under the cursor
 * and remember everything a drag needs.
 *
 * Clicking empty space clears the selection.  Clicking an entity selects it
 * and records its original placement (for undo), its anchor point, and where
 * inside it the cursor grabbed it, so motion events can move it without a
 * jump.  The drag only counts as a move once the cursor travels a few pixels
 * (see tools_mouse_drag).
 */
/*
 * pick_under_cursor — Which entity a Select click at (world_x, world_y)
 * picks.  Normally the topmost one.  Alt+click, or a second click on the
 * very spot of the previous click, picks the one below the current
 * selection instead (wrapping back to the top), so entities hidden under
 * others can be reached without moving anything out of the way.
 */
static Selection pick_under_cursor(EditorState *es, float world_x, float world_y)
{
    static Selection hits[EDITOR_MAX_SELECTION];
    int count = editor_hit_test_all(&es->level, world_x, world_y, hits,
                                    EDITOR_MAX_SELECTION);
    float zoom = es->camera.zoom > 0.0f ? es->camera.zoom : 1.0f;
    /* "The same spot": within one canvas pixel, whatever the zoom. */
    float same = 1.0f / zoom;
    int alt = (es->input_mods & INPUT_ALT) != 0;
    int again = es->last_click_valid &&
                fabsf(world_x - es->last_click_x) <= same &&
                fabsf(world_y - es->last_click_y) <= same;
    Selection none = { 0, -1 };

    if (count == 0) return none;
    if ((alt || again) && count > 1) {
        for (int i = 0; i < count; i++) {
            if (hits[i].type == es->selection.type &&
                hits[i].index == es->selection.index &&
                es->selection.index >= 0) {
                Selection next = hits[(i + 1) % count];
                editor_set_status(es, "Selected %s (%d of %d here)",
                                  editor_entity_type_name(next.type),
                                  (i + 1) % count + 1, count);
                return next;
            }
        }
    }
    if (count > 1)
        editor_set_status(es, "Selected %s (1 of %d here; Alt+click or click "
                          "again for the next)",
                          editor_entity_type_name(hits[0].type), count);
    return hits[0];
}

/*
 * arm_drag — Remember everything a drag of `items` needs, grabbed at
 * `grabbed` (whose corner is the one that snaps to the grid).
 *
 * Each entity's placement at mouse-down is kept, so every motion event
 * recomputes positions from the originals (no drift) and mouse-up can
 * record exact undo "befores".  The drag only counts as a move once the
 * cursor travels a few pixels (see tools_mouse_drag).
 */
static void arm_drag(EditorState *es, Selection grabbed, const Selection *items,
                     int count, float world_x, float world_y)
{
    float anchor_x, anchor_y;

    if (!get_entity_anchor(&es->level, grabbed.type, grabbed.index,
                           &anchor_x, &anchor_y)) return;
    if (count > EDITOR_MAX_SELECTION) count = EDITOR_MAX_SELECTION;
    es->dragging     = 1;
    es->drag_moved   = 0;
    editor_level_issue_snapshot(&es->level, &drag_issues);
    es->drag_type    = grabbed.type;
    es->drag_index   = grabbed.index;
    es->drag_count   = count;
    for (int i = 0; i < count; i++) {
        es->drag_items[i] = items[i];
        es->drag_befores[i] = editor_snapshot_entity(&es->level, items[i].type,
                                                     items[i].index);
    }
    es->drag_start_x = anchor_x;
    es->drag_start_y = anchor_y;
    es->drag_grab_x  = world_x - anchor_x;
    es->drag_grab_y  = world_y - anchor_y;
    es->drag_mouse_x = world_x;
    es->drag_mouse_y = world_y;
}

/* Start a rubber-band box at the press point. */
static void start_box(EditorState *es, float world_x, float world_y, int additive)
{
    es->box_selecting = 1;
    es->box_additive = additive;
    es->box_x0 = es->box_x1 = world_x;
    es->box_y0 = es->box_y1 = world_y;
}

/*
 * select_and_arm_drag — TOOL_SELECT press.
 *
 *   Shift+click on an entity   add it to the selection, or take it out
 *   Shift+press on empty space start a box that adds to the selection
 *   press on a member of a     drag the whole selection (a click without
 *   multi-selection            moving selects just that member)
 *   press on another entity    select it alone and arm a drag of it;
 *                              Alt or a repeat click picks the one below
 *   press on empty space       clear the selection and start a box
 */
static void select_and_arm_drag(EditorState *es, float world_x, float world_y)
{
    static Selection items[EDITOR_MAX_SELECTION];
    int shift = (es->input_mods & INPUT_SHIFT) != 0;
    int alt = (es->input_mods & INPUT_ALT) != 0;
    Selection top = editor_hit_test(&es->level, world_x, world_y);
    Selection hit;

    es->dragging = 0;
    es->box_selecting = 0;

    if (shift && !alt) {
        es->last_click_valid = 0;
        if (top.index >= 0) editor_selection_toggle(es, top.type, top.index);
        else start_box(es, world_x, world_y, 1);
        return;
    }

    if (!alt && top.index >= 0 && editor_selection_count(es) > 1 &&
        editor_is_selected(es, top.type, top.index)) {
        int count = editor_selection_items(es, items, EDITOR_MAX_SELECTION);
        es->last_click_valid = 0;
        arm_drag(es, top, items, count, world_x, world_y);
        return;
    }

    hit = pick_under_cursor(es, world_x, world_y);
    es->last_click_valid = 0;
    if (hit.index < 0) {
        editor_select_none(es);   /* clicked empty space */
        start_box(es, world_x, world_y, 0);
        return;
    }
    es->last_click_valid = 1;
    es->last_click_x = world_x;
    es->last_click_y = world_y;
    editor_select_only(es, hit.type, hit.index);
    arm_drag(es, hit, &hit, 1, world_x, world_y);
}

/*
 * tools_mouse_down --- Dispatch a left-click to the active tool handler.
 *
 * TOOL_SELECT : hit-test and select/deselect, optionally start a drag or a box.
 * TOOL_PLACE  : stamp a new entity at the click position.
 * TOOL_DELETE : hit-test and delete the clicked entity.
 */
/*
 * delete_at — The Delete tool's click and the right-click shortcut.
 *
 * Clicking a member of a multi-selection deletes the whole selection, as
 * the Delete key would: the designer pointed at the group.  Anything else
 * under the cursor is deleted on its own and the selection is left alone.
 */
void delete_at(EditorState *es, float world_x, float world_y)
{
    Selection hit = editor_hit_test(&es->level, world_x, world_y);

    if (hit.index < 0) return;
    if (editor_selection_count(es) > 1 && editor_is_selected(es, hit.type, hit.index)) {
        tools_delete_selected(es);
        return;
    }
    (void)delete_entity(es, hit.type, hit.index);
}

void tools_mouse_down(EditorState *es, float world_x, float world_y)
{
    if (!es) return;
    /* A row still open (its release never came) ends before a new press. */
    tools_end_paint(es);
    if (!tools_can_hit_test(es)) return;
    editor_selection_reconcile(es);

    switch (es->tool) {

    case TOOL_SELECT:
        select_and_arm_drag(es, world_x, world_y);
        break;

    case TOOL_PLACE:
        begin_paint(es, world_x, world_y);
        break;

    case TOOL_DELETE:
        delete_at(es, world_x, world_y);
        break;
    }
}

/* Every dragged entity still exists where mouse-down found it.  (Commands
 * that add or remove entities wait while the button is held, so this only
 * fails if something unexpected changed the arrays.) */
static int drag_targets_exist(const EditorState *es)
{
    for (int i = 0; i < es->drag_count; i++) {
        const Selection *item = &es->drag_items[i];
        if (item->index < 0 ||
            item->index >= editor_entity_count(&es->level, item->type)) return 0;
    }
    return es->drag_count > 0;
}

/* Put every dragged entity back where it was at mouse-down. */
static void restore_drag(EditorState *es)
{
    for (int i = 0; i < es->drag_count; i++)
        (void)editor_entity_write(&es->level, es->drag_items[i].type,
                                  es->drag_items[i].index, &es->drag_befores[i]);
}

/*
 * finish_box — Select what the rubber band touches.  A box smaller than
 * the drag threshold was just a click on empty space, which already
 * cleared the selection (unless Shift was held).
 */
static void finish_box(EditorState *es)
{
    static Selection found[EDITOR_MAX_SELECTION];
    float zoom = es->camera.zoom > 0.0f ? es->camera.zoom : 1.0f;
    float x0 = fminf(es->box_x0, es->box_x1), x1 = fmaxf(es->box_x0, es->box_x1);
    float y0 = fminf(es->box_y0, es->box_y1), y1 = fmaxf(es->box_y0, es->box_y1);
    int count = 0;
    int skipped = 0;

    es->box_selecting = 0;
    if ((x1 - x0) * zoom < DRAG_THRESHOLD_PX && (y1 - y0) * zoom < DRAG_THRESHOLD_PX)
        return;

    if (es->box_additive) count = editor_selection_items(es, found, EDITOR_MAX_SELECTION);
    for (int type = 0; type < ENT_COUNT; type++) {
        for (int i = 0; i < editor_entity_count(&es->level, (EntityType)type); i++) {
            EditorRect r;
            int already = 0;
            if (!editor_entity_bounds(&es->level, (EntityType)type, i, &r)) continue;
            /* Touching the box is enough; it need not be inside it. */
            if (r.x >= x1 || r.x + r.w <= x0 || r.y >= y1 || r.y + r.h <= y0) continue;
            for (int k = 0; k < count && !already; k++)
                already = found[k].type == (EntityType)type && found[k].index == i;
            if (already) continue;
            if (count == EDITOR_MAX_SELECTION) {
                skipped++;
                continue;
            }
            found[count].type = (EntityType)type;
            found[count].index = i;
            count++;
        }
    }
    (void)editor_select_items(es, found, count);
    if (skipped > 0)
        editor_set_status(es, "Selected %d entities (a selection holds at most %d)",
                          count, EDITOR_MAX_SELECTION);
    else if (count > 1)
        editor_set_status(es, "Selected %d entities", count);
}

/* ------------------------------------------------------------------ */
/* tools_mouse_up                                                      */
/* ------------------------------------------------------------------ */

/*
 * tools_mouse_up --- End a box or a drag; record a move for undo.
 *
 * Compares each dragged entity's final placement with the copy taken at
 * mouse-down.  Those that changed are recorded as CMD_MOVE commands in one
 * undo group, so moving a multi-selection is a single undo step.
 */
void tools_mouse_up(EditorState *es, float world_x, float world_y)
{
    int moved = 0;

    (void)world_x;
    (void)world_y;

    if (!es) return;
    if (es->painting) {
        tools_end_paint(es);
        return;
    }
    if (es->box_selecting) {
        finish_box(es);
        return;
    }
    if (!es->dragging) return;
    es->dragging = 0;
    if (!drag_targets_exist(es)) return;
    if (!es->drag_moved) {
        /* A click on a member of a multi-selection selects just it. */
        if (es->drag_count > 1) editor_select_only(es, es->drag_type, es->drag_index);
        return;
    }

    /*
     * Every motion event kept the level valid, but a field edit could have
     * broken it while the button was held.  Never leave an unrecorded
     * change behind: put the entities back where the drag started.
     */
    if (editor_edit_kept_level(&es->level, &drag_issues, NULL, 0) != 0) {
        restore_drag(es);
        editor_set_status(es, "Move cancelled: level has errors");
        return;
    }

    /* Set aside the history room first: a move that cannot be recorded is
     * put back rather than left in the level with nothing to undo it. */
    if (undo_group_begin(es->undo, es->drag_count) == 0) {
        restore_drag(es);
        editor_set_status(es, "Move cancelled: cannot allocate undo history");
        return;
    }
    for (int i = 0; i < es->drag_count; i++) {
        Command cmd;
        PlacementData after = editor_snapshot_entity(&es->level, es->drag_items[i].type,
                                                     es->drag_items[i].index);
        if (placement_same(es->drag_items[i].type, &after, &es->drag_befores[i])) continue;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type         = CMD_MOVE;
        cmd.entity_type  = (int)es->drag_items[i].type;
        cmd.entity_index = es->drag_items[i].index;
        cmd.before       = es->drag_befores[i];
        cmd.after        = after;
        (void)undo_push(es->undo, &cmd);  /* fits the room begin reserved */
        moved++;
    }
    undo_group_end(es->undo);
    if (moved == 0) return;

    editor_refresh_dirty(es);
    if (es->drag_count > 1)
        editor_set_status(es, "Moved %d entities", moved);
    else if (es->drag_type == ENT_CHECKPOINT)
        editor_set_status(es, "Checkpoint moved");
}

/* ------------------------------------------------------------------ */
/* tools_mouse_drag                                                    */
/* ------------------------------------------------------------------ */

/*
 * tools_mouse_drag --- Grow the box, or move what is being dragged.
 *
 * Called on every mouse-motion event while the left button is held.  The
 * grabbed entity's new corner is always computed from its mouse-down
 * placement:
 *   new top-left = cursor - grab offset   (optionally snapped)
 * and every dragged entity moves by that same amount from its own
 * mouse-down placement, then is clamped into the world.  If the result
 * would fail level validation (for example a checkpoint dragged behind the
 * player start), everything stays at its last valid position.
 */
void tools_mouse_drag(EditorState *es, float world_x, float world_y)
{
    static PlacementData previous[EDITOR_MAX_SELECTION];
    float zoom;
    float target_x, target_y;
    float dx, dy;

    if (!es) return;
    if (es->painting) {
        paint_step(es, world_x, world_y);
        return;
    }
    if (es->box_selecting) {
        es->box_x1 = world_x;
        es->box_y1 = world_y;
        return;
    }
    if (!es->dragging || !drag_targets_exist(es)) return;

    /* Ignore small wobbles until the cursor leaves the click threshold.
     * The threshold is in canvas pixels, so divide by zoom for world px. */
    if (!es->drag_moved) {
        float mx = world_x - es->drag_mouse_x;
        float my = world_y - es->drag_mouse_y;
        zoom = es->camera.zoom > 0.0f ? es->camera.zoom : 1.0f;
        float limit = DRAG_THRESHOLD_PX / zoom;
        if (mx * mx + my * my < limit * limit) return;
        es->drag_moved = 1;
        es->last_click_valid = 0;   /* a move, not a click on one spot */
    }

    target_x = world_x - es->drag_grab_x;
    target_y = world_y - es->drag_grab_y;

    /*
     * Snapping: with snap-to-grid on (key S), or while Shift is held with it
     * off, round the grabbed entity's top-left corner down to the TILE_SIZE
     * (48 px) grid.  Shift always means "the other way" for the move in
     * progress.  The rest of a group keeps its distance to it.
     */
    editor_snap_point(es, &target_x, &target_y);
    dx = target_x - es->drag_start_x;
    dy = target_y - es->drag_start_y;

    for (int i = 0; i < es->drag_count; i++) {
        const Selection *item = &es->drag_items[i];
        PlacementData moved = editor_move_placement(item->type, &es->drag_befores[i], dx, dy);
        editor_clamp_placement(&es->level, item->type, &moved);
        previous[i] = editor_snapshot_entity(&es->level, item->type, item->index);
        (void)editor_entity_write(&es->level, item->type, item->index, &moved);
    }
    /* A step that would add an error is reverted to the last position
     * that did not. */
    if (editor_edit_kept_level(&es->level, &drag_issues, NULL, 0) != 0) {
        for (int i = 0; i < es->drag_count; i++)
            (void)editor_entity_write(&es->level, es->drag_items[i].type,
                                      es->drag_items[i].index, &previous[i]);
    }
}

void tools_cancel_drag(EditorState *es)
{
    if (!es) return;
    if (es->box_selecting) {
        es->box_selecting = 0;
        return;
    }
    if (!es->dragging) return;
    es->dragging = 0;
    if (es->drag_moved && drag_targets_exist(es)) restore_drag(es);
    editor_set_status(es, "Move cancelled");
}
