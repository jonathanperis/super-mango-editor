/*
 * tools.c --- Implementation of the editor's interactive tools.
 *
 * Keeping a placement inside the world, and moving one, is
 * tools_bounds.c. Stamping a new entity is tools_place.c. The mouse
 * drag is tools_drag.c and the arrow-key nudge is tools_nudge.c.
 * This file keeps the level-issue gate, deletion and select-all.
 *
 * Implements SELECT, PLACE, MOVE, and DELETE tool interactions for all
 * entity types.  Every function operates on EditorState, modifying the
 * LevelDef in place and pushing undo commands for reversibility.
 *
 * Key design decisions:
 *   - Which entity is under the mouse is answered by hit_test.c, the same
 *     rectangles the selection outline uses.
 *   - Adding and removing entities goes through entity_meta.c's shared
 *     insert/remove helpers, the same path undo/redo uses.
 *   - PLACE provides sensible defaults for every entity type so the
 *     designer can immediately see and test the new entity.
 */

#include <math.h>    /* floorf, fmodf, isfinite, roundf */
#include <string.h>  /* memcmp, memset */

#include "tools.h"
#include "editor.h"  /* EditorState, EntityType, Selection, EditorTool    */
#include "editor_session.h" /* save-point dirty tracking */
#include "entity_meta.h" /* Editor display dimensions and rail helpers     */
#include "hit_test.h"    /* editor_hit_test                                */
#include "undo.h"    /* Command, PlacementData, undo_push                 */
#include "../levels/level_loader.h" /* level_validate_runtime            */
#include "../levels/level_validate.h" /* level_validate_preview, _each   */
#include "../game_constants.h" /* GAME_W, GAME_H, FLOOR_Y, TILE_SIZE, WORLD_W,
                                   FLOOR_GAP_W; MAX_* via level.h           */
#include "../surfaces/rail.h" /* MAX_RAIL_SPEED                            */
#include "tools_internal.h"

static uint64_t message_hash(const char *text)
{
    uint64_t h = UINT64_C(1469598103934665603);   /* FNV-1a */
    for (; *text; text++) h = (h ^ (unsigned char)*text) * UINT64_C(1099511628211);
    return h;
}

static void record_issue(void *context, const char *message, const LevelIssueLocation *where)
{
    EditorIssueSnapshot *snap = context;
    (void)where;
    if (snap->count < EDITOR_ISSUE_SNAPSHOT) snap->hash[snap->count] = message_hash(message);
    snap->count++;
}

void editor_level_issue_snapshot(const LevelDef *level, EditorIssueSnapshot *out)
{
    memset(out, 0, sizeof(*out));
    (void)level_validate_runtime_each(level, record_issue, out);
}

typedef struct {
    const EditorIssueSnapshot *before;
    char  *err;
    size_t err_size;
    int    found;
} NewIssueSearch;

static void find_new_issue(void *context, const char *message, const LevelIssueLocation *where)
{
    NewIssueSearch *search = context;
    uint64_t h = message_hash(message);
    (void)where;
    if (search->found) return;
    for (int i = 0; i < search->before->count && i < EDITOR_ISSUE_SNAPSHOT; i++)
        if (search->before->hash[i] == h) return;
    search->found = 1;
    if (search->err && search->err_size > 0) {
        size_t n = strlen(message);
        if (n >= search->err_size) n = search->err_size - 1;
        memcpy(search->err, message, n);
        search->err[n] = '\0';
    }
}

int editor_edit_kept_level(const LevelDef *level, const EditorIssueSnapshot *before,
                           char *err, size_t err_size)
{
    NewIssueSearch search = { before, err, err_size, 0 };

    if (level_validate_preview(level, err, err_size) != 0) return -1;
    if (level_validate_runtime_each(level, find_new_issue, &search) <= before->count) return 0;
    /* More errors than before, but each message matches an old one (the
     * same mistake twice): name the first. */
    if (!search.found) (void)level_validate_runtime(level, err, err_size);
    return -1;
}

/*
 * tools_can_hit_test --- Canvas clicks need a level the canvas can draw.
 *
 * The canvas stops drawing a level whose counts or tile sizes it cannot
 * trust (a hand-edited tile count could make its loops enormous), so
 * clicking would act on invisible entities.  The editor's own actions
 * never produce such a level; it only arrives via a hand-edited file or a
 * property field.  Tell the designer how to recover.  Other mistakes (a
 * bad next_phase) leave the level drawn and editable.
 */
int tools_can_hit_test(EditorState *es)
{
    char error[128];
    if (level_validate_preview(&es->level, error, sizeof(error)) == 0) return 1;
    editor_set_status(es, "Canvas paused: %s. Fix it in the side panel or press Ctrl+Z",
                      error);
    return 0;
}


/* ------------------------------------------------------------------ */
/* Internal: delete an entity by type and index                        */
/* ------------------------------------------------------------------ */

/*
 * delete_entity --- Remove one entity from the level and push undo.
 * Returns 0 when it was removed, -1 when it was refused (with a status
 * message) or does not exist.
 *
 * Snapshots the entity data before removal, removes it through the shared
 * editor_entity_remove helper (the same path undo/redo uses), pushes a
 * CMD_DELETE command to the undo stack, and refreshes the dirty flag.
 */
int delete_entity(EditorState *es, EntityType type, int index)
{
    LevelDef *level;
    PlacementData before;
    Command cmd;

    if (!es) return -1;
    level = &es->level;
    if (index < 0 || index >= editor_entity_count(level, type))
        return -1;

    /*
     * Spike blocks and rail-mode float platforms store the *position* of
     * their rail in the rails array.  Deleting a rail they ride would leave
     * them pointing at a different rail (or past the end), so refuse and
     * tell the designer what to fix first.  Deleting an unused rail is fine:
     * editor_entity_remove renumbers references to the rails after it.
     */
    if (type == ENT_RAIL) {
        int blocks = 0;
        int platforms = 0;
        if (editor_rail_reference_count(level, index, &blocks, &platforms) > 0) {
            editor_set_status(es,
                              "Rail %d is used by %d spike block%s and %d float "
                              "platform%s; move or delete them first",
                              index, blocks, blocks == 1 ? "" : "s",
                              platforms, platforms == 1 ? "" : "s");
            return -1;
        }
    }

    /* Snapshot entity data before deletion for undo */
    before = editor_snapshot_entity(level, type, index);
    EditorIssueSnapshot issues_before;
    editor_level_issue_snapshot(level, &issues_before);

    if (editor_entity_type_is_singleton(type)) {
        /*
         * Last Star and Player Spawn are single positions, not arrays.
         * "Deleting" one resets it to (0, 0), the "not placed" sentinel
         * that validation and the runtime already understand.
         */
        PlacementData cleared;
        memset(&cleared, 0, sizeof(cleared));
        (void)editor_entity_write(level, type, 0, &cleared);
    } else if (editor_entity_remove(level, type, index) != 0) {
        return -1;
    }

    /*
     * A delete must not add an error (e.g. removing the player spawn moves
     * the effective start past a checkpoint).  Put the entity back and
     * explain instead.
     */
    {
        char error[128];
        if (editor_edit_kept_level(level, &issues_before, error, sizeof(error)) != 0) {
            if (editor_entity_type_is_singleton(type))
                (void)editor_entity_write(level, type, 0, &before);
            else
                (void)editor_entity_insert(level, type, index, &before);
            editor_set_status(es, "Cannot delete %s: %s",
                              editor_entity_type_name(type), error);
            return -1;
        }
    }

    /* Push undo command — CMD_DELETE stores "before" so undo re-inserts */
    memset(&cmd, 0, sizeof(cmd));
    cmd.type         = CMD_DELETE;
    cmd.entity_type  = (int)type;
    cmd.entity_index = index;
    cmd.before       = before;
    undo_push(es->undo, &cmd);

    editor_selection_after_remove(es, type, index);
    editor_refresh_dirty(es);
    if (type == ENT_CHECKPOINT) editor_set_status(es, "Checkpoint deleted");
    return 0;
}


/* ------------------------------------------------------------------ */
/* tools_right_click                                                   */
/* ------------------------------------------------------------------ */

/*
 * tools_right_click --- Right-click deletes whatever entity is under the cursor.
 *
 * This is a convenience shortcut: regardless of the current tool mode,
 * right-clicking an entity removes it immediately (with the rest of the
 * selection, when it is part of a multi-selection; see delete_at).
 * Useful for quick corrections without switching to the delete tool.
 */
void tools_right_click(EditorState *es, float world_x, float world_y)
{
    if (!es) return;
    if (!tools_can_hit_test(es)) return;
    editor_selection_reconcile(es);
    delete_at(es, world_x, world_y);
}

/* ------------------------------------------------------------------ */
/* tools_delete_selected                                               */
/* ------------------------------------------------------------------ */

/*
 * delete_order — Sort key for deleting several entities: rails last (a
 * rail can only go once the riders deleted with it are gone), and within
 * one type the highest index first, so removing one never shifts the
 * index of another still waiting in the list.
 */
static int delete_before(Selection a, Selection b)
{
    int a_rail = a.type == ENT_RAIL, b_rail = b.type == ENT_RAIL;
    if (a_rail != b_rail) return b_rail;
    if (a.type != b.type) return a.type < b.type;
    return a.index > b.index;
}

/*
 * tools_delete_selected --- Delete every selected entity.
 *
 * Called from the keyboard handler when Delete or Backspace is pressed.
 * Several entities are deleted as one undo group (one Ctrl+Z restores
 * them all).  An entity that cannot go (a rail still ridden by something
 * outside the selection, say) stays, and the status bar says why.
 */
void tools_delete_selected(EditorState *es)
{
    static Selection items[EDITOR_MAX_SELECTION];
    int count;
    int deleted = 0;

    if (!es) return;
    editor_selection_reconcile(es);
    count = editor_selection_items(es, items, EDITOR_MAX_SELECTION);
    if (count == 0) return;
    if (count == 1) {
        (void)delete_entity(es, items[0].type, items[0].index);
        return;
    }

    /* Insertion sort into delete order: the list is short. */
    for (int i = 1; i < count; i++) {
        Selection item = items[i];
        int j = i;
        while (j > 0 && delete_before(item, items[j - 1])) {
            items[j] = items[j - 1];
            j--;
        }
        items[j] = item;
    }

    if (undo_group_begin(es->undo, count) == 0) {
        editor_set_status(es, "Delete cancelled: cannot allocate undo history");
        return;
    }
    for (int i = 0; i < count; i++)
        if (delete_entity(es, items[i].type, items[i].index) == 0) deleted++;
    undo_group_end(es->undo);
    editor_select_none(es);
    if (deleted == count)
        editor_set_status(es, "Deleted %d entities", deleted);
    else if (deleted > 0)
        editor_set_status(es, "Deleted %d of %d entities; the rest could not go",
                          deleted, count);
}

/*
 * editor_select_all --- Ctrl+A on the canvas: select every entity in the
 * level, as many as a selection holds (EDITOR_MAX_SELECTION), and say how
 * many in the status bar.  Returns the number selected.
 */
int editor_select_all(EditorState *es)
{
    static Selection items[EDITOR_MAX_SELECTION];
    int count = 0, total = 0;

    if (!es) return 0;
    for (int type = 0; type < ENT_COUNT; type++) {
        int n = editor_entity_count(&es->level, (EntityType)type);
        for (int i = 0; i < n; i++, total++)
            if (count < EDITOR_MAX_SELECTION) items[count++] = (Selection){(EntityType)type, i};
    }
    count = editor_select_items(es, items, count);
    if (total > count)
        editor_set_status(es, "Selected %d of %d entities: a selection holds %d at most",
                          count, total, EDITOR_MAX_SELECTION);
    else
        editor_set_status(es, count == 1 ? "Selected %d entity" : "Selected %d entities", count);
    return count;
}
