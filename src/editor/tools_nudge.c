/*
 * tools_nudge.c — Move the selection with the arrow keys.
 *
 * A run of nudges of the same selection is one undo group. The
 * fingerprint is an in-memory FNV-1a of the selected entities; it is
 * not written into the level.
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

/* ------------------------------------------------------------------ */
/* tools_nudge_selection                                               */
/* ------------------------------------------------------------------ */

/* A fingerprint of which entities a list names, so a run of nudges is
 * only merged while it keeps moving the same ones (FNV-1a over the list). */
/* Low byte first, the same order a little-endian machine reads the two ints.
 * The hash is only compared in this process; it is not written to a file. */
static uint64_t fnv_int(uint64_t hash, unsigned value)
{
    for (int shift = 0; shift < 32; shift += 8) {
        hash ^= (value >> shift) & 0xffu;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t selection_key(const Selection *items, int count)
{
    uint64_t hash = UINT64_C(1469598103934665603);
    for (int i = 0; i < count; i++) {
        hash = fnv_int(hash, (unsigned)items[i].type);
        hash = fnv_int(hash, (unsigned)items[i].index);
    }
    return hash;
}

void tools_nudge_selection(EditorState *es, float dx, float dy)
{
    static Selection items[EDITOR_MAX_SELECTION];
    static PlacementData before[EDITOR_MAX_SELECTION];
    static PlacementData after[EDITOR_MAX_SELECTION];
    char error[128];
    int count;
    int changed = 0;
    uint32_t now;
    uint64_t key;
    int merge;

    if (!es || !tools_can_hit_test(es)) return;
    editor_selection_reconcile(es);
    count = editor_selection_items(es, items, EDITOR_MAX_SELECTION);
    if (count == 0) return;
    EditorIssueSnapshot issues_before;
    editor_level_issue_snapshot(&es->level, &issues_before);

    /* Move every selected entity, exactly as a drag by (dx, dy) would. */
    for (int i = 0; i < count; i++) {
        before[i] = editor_snapshot_entity(&es->level, items[i].type, items[i].index);
        after[i] = editor_move_placement(items[i].type, &before[i], dx, dy);
        editor_clamp_placement(&es->level, items[i].type, &after[i]);
        (void)editor_entity_write(&es->level, items[i].type, items[i].index, &after[i]);
        if (!placement_same(items[i].type, &before[i], &after[i])) changed++;
    }
    if (editor_edit_kept_level(&es->level, &issues_before, error, sizeof(error)) != 0) {
        for (int i = 0; i < count; i++)
            (void)editor_entity_write(&es->level, items[i].type, items[i].index, &before[i]);
        editor_set_status(es, "Cannot move here: %s", error);
        return;
    }
    if (changed == 0) {
        editor_set_status(es, "Nothing moved: the selection cannot go further that way");
        return;
    }

    /*
     * Record the move.  Holding an arrow key sends a stream of nudges; one
     * undo step per pixel would bury everything else in the history.  So
     * a nudge of the same entities soon after the last one, with nothing
     * else recorded in between, extends that step's "after" instead.
     */
    now = (uint32_t)clock_millis();
    key = selection_key(items, count);
    merge = es->nudge_group != 0 &&
            undo_top_group(es->undo) == es->nudge_group &&
            now - es->nudge_ms < NUDGE_COALESCE_MS &&
            key == es->nudge_selection_key;
    if (!merge) {
        /* Room for every selected entity, so a later nudge of the same
         * selection merged into this step fits too.  Without it, put the
         * selection back. */
        int group = undo_group_begin(es->undo, count);
        if (group == 0) {
            for (int i = 0; i < count; i++)
                (void)editor_entity_write(&es->level, items[i].type, items[i].index, &before[i]);
            editor_set_status(es, "Nothing moved: cannot allocate undo history");
            return;
        }
        es->nudge_group = group;
    }
    for (int i = 0; i < count; i++) {
        Command cmd;
        if (placement_same(items[i].type, &before[i], &after[i])) continue;
        if (merge && undo_amend_after(es->undo, es->nudge_group, (int)items[i].type,
                                      items[i].index, &after[i])) continue;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = CMD_MOVE;
        cmd.entity_type = (int)items[i].type;
        cmd.entity_index = items[i].index;
        cmd.group = es->nudge_group;
        cmd.before = before[i];
        cmd.after = after[i];
        (void)undo_push(es->undo, &cmd);  /* fits the room begin reserved */
    }
    if (!merge) undo_group_end(es->undo);
    es->nudge_ms = now;
    es->nudge_selection_key = key;
    editor_refresh_dirty(es);
}
