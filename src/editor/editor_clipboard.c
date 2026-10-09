/*
 * editor_clipboard.c — Editor clipboard copy/paste helpers.
 */

#include "editor_clipboard.h"

#include "../levels/level_loader.h" /* level_validate_runtime */
#include "../surfaces/rail.h" /* RAIL_TILE_W */
#include "editor_session.h" /* editor_set_status */
#include "entity_meta.h" /* central selection validity and entity storage */
#include "tools.h"       /* editor_clamp_placement, editor_add_placement */
#include "editor_undo_apply.h" /* editor_apply_undo_command (group rollback) */

/* How far a pasted copy moves so it does not hide the original (see
 * offset_pasted_copy for which way each type moves). */
#define PASTE_OFFSET 24.0f

/*
 * rider_rail_index — Point at the rail_index of an entity that rides a rail
 * (every spike block; float platforms in RAIL mode), or return NULL.
 */
static int *rider_rail_index(EntityType type, PlacementData *d)
{
    if (type == ENT_SPIKE_BLOCK) return &d->spike_block.rail_index;
    if (type == ENT_FLOAT_PLATFORM && d->float_platform.mode == FLOAT_PLATFORM_RAIL)
        return &d->float_platform.rail_index;
    return NULL;
}

static int same_rail(const RailPlacement *a, const RailPlacement *b)
{
    return a->layout == b->layout && a->x == b->x && a->y == b->y &&
           a->w == b->w && a->h == b->h && a->end_cap == b->end_cap;
}

/*
 * fill_item — Snapshot one entity of this level into a clipboard item,
 * remembering the rail it rides (or, for a rail, which rail it is) both by
 * position and by shape (see EditorClipboardItem in editor.h).
 */
static void fill_item(const LevelDef *level, Selection which,
                      EditorClipboardItem *item)
{
    int *rail_index;

    item->type = which.type;
    item->data = editor_snapshot_entity(level, which.type, which.index);
    item->rail_index = -1;
    item->has_rail = 0;
    item->rail_item = -1;
    if (which.type == ENT_RAIL) {
        item->rail_index = which.index;
        return;
    }
    rail_index = rider_rail_index(which.type, &item->data);
    if (rail_index && *rail_index >= 0 && *rail_index < level->rail_count) {
        item->rail_index = *rail_index;
        item->has_rail = 1;
        item->rail = level->rails[*rail_index];
    }
}

/*
 * fill_items — Snapshot the selection into items[] and link each rider to
 * the slot of its rail when that rail is part of the selection too, so a
 * pasted rider rides the pasted rail rather than the original.
 */
static int fill_items(EditorState *es, EditorClipboardItem *items)
{
    static Selection selected[EDITOR_MAX_SELECTION];
    int count;

    editor_selection_reconcile(es);
    count = editor_selection_items(es, selected, EDITOR_MAX_SELECTION);
    for (int i = 0; i < count; i++) fill_item(&es->level, selected[i], &items[i]);
    for (int i = 0; i < count; i++) {
        if (items[i].type == ENT_RAIL || !items[i].has_rail) continue;
        for (int j = 0; j < count; j++)
            if (items[j].type == ENT_RAIL && items[j].rail_index == items[i].rail_index)
                items[i].rail_item = j;
    }
    return count;
}

/*
 * editor_copy_selected — Snapshot every selected entity into the clipboard.
 *
 * Stores each entity's type and a PlacementData union so paste can
 * recreate it.  A rail rider also records which rail it rides (see
 * EditorClipboardItem in editor.h).
 */
void editor_copy_selected(EditorState *es)
{
    int count;

    if (!es) return;
    editor_selection_reconcile(es);
    if (!editor_selection_is_valid(es)) return;
    count = fill_items(es, es->clipboard);
    es->clipboard_count = count;
    if (count > 1) editor_set_status(es, "Copied %d entities", count);
}

/* Forget which rails the clipboard's riders rode in this document (another
 * document is open now); their rail shapes still match a paste there. */
void editor_clipboard_forget_rails(EditorState *es)
{
    if (!es) return;
    for (int i = 0; i < es->clipboard_count; i++) es->clipboard[i].rail_index = -1;
}

void editor_clipboard_after_rail_remove(EditorState *es, int index)
{
    if (!es) return;
    for (int i = 0; i < es->clipboard_count; i++) {
        EditorClipboardItem *item = &es->clipboard[i];
        if (item->rail_index < 0) continue;
        if (item->rail_index == index) {
            /* The rail is gone; its old shape must not match a look-alike. */
            item->rail_index = -1;
            item->has_rail = 0;
        } else if (item->rail_index > index) {
            item->rail_index--;
        }
    }
}

void editor_clipboard_after_rail_insert(EditorState *es, int index)
{
    if (!es) return;
    for (int i = 0; i < es->clipboard_count; i++)
        if (es->clipboard[i].rail_index >= index) es->clipboard[i].rail_index++;
}

/*
 * offset_pasted_copy — Move a copied placement by PASTE_OFFSET.
 *
 * Every type stores its position in different fields (patrolling enemies
 * also carry a patrol range that must move with them), so this is the one
 * per-type switch in the clipboard.  Free-floating things move right and
 * down; things tied to the floor move only right; rail riders move along
 * their rail; floor gaps move one whole gap width.
 */
static void offset_pasted_copy(EntityType type, PlacementData *d)
{
    switch (type) {
    case ENT_COIN:
        d->coin.x += PASTE_OFFSET;
        d->coin.y += PASTE_OFFSET;
        break;
    case ENT_STAR_YELLOW:
        d->star_yellow.x += PASTE_OFFSET;
        d->star_yellow.y += PASTE_OFFSET;
        break;
    case ENT_STAR_GREEN:
        d->star_green.x += PASTE_OFFSET;
        d->star_green.y += PASTE_OFFSET;
        break;
    case ENT_STAR_RED:
        d->star_red.x += PASTE_OFFSET;
        d->star_red.y += PASTE_OFFSET;
        break;
    case ENT_LAST_STAR:
    case ENT_PLAYER_SPAWN:
        d->last_star.x += PASTE_OFFSET;
        d->last_star.y += PASTE_OFFSET;
        break;
    case ENT_SPIDER:
        d->spider.x += PASTE_OFFSET;
        d->spider.patrol_x0 += PASTE_OFFSET;
        d->spider.patrol_x1 += PASTE_OFFSET;
        break;
    case ENT_JUMPING_SPIDER:
        d->jumping_spider.x += PASTE_OFFSET;
        d->jumping_spider.patrol_x0 += PASTE_OFFSET;
        d->jumping_spider.patrol_x1 += PASTE_OFFSET;
        break;
    case ENT_BIRD:
    case ENT_FASTER_BIRD:
        d->bird.x += PASTE_OFFSET;
        d->bird.patrol_x0 += PASTE_OFFSET;
        d->bird.patrol_x1 += PASTE_OFFSET;
        break;
    case ENT_FISH:
    case ENT_FASTER_FISH:
        d->fish.x += PASTE_OFFSET;
        d->fish.patrol_x0 += PASTE_OFFSET;
        d->fish.patrol_x1 += PASTE_OFFSET;
        break;
    case ENT_AXE_TRAP:
        d->axe_trap.pillar_x += PASTE_OFFSET;
        break;
    case ENT_CIRCULAR_SAW:
        d->circular_saw.x += PASTE_OFFSET;
        d->circular_saw.patrol_x0 += PASTE_OFFSET;
        d->circular_saw.patrol_x1 += PASTE_OFFSET;
        break;
    case ENT_SPIKE_ROW:
        d->spike_row.x += PASTE_OFFSET;
        break;
    case ENT_SPIKE_PLATFORM:
        d->spike_platform.x += PASTE_OFFSET;
        d->spike_platform.y += PASTE_OFFSET;
        break;
    case ENT_SPIKE_BLOCK:
        /* Spike blocks ride a rail: move along it instead of in x/y. */
        d->spike_block.t_offset += PASTE_OFFSET / (float)RAIL_TILE_W;
        break;
    case ENT_BLUE_FLAME:
        d->blue_flame.x += PASTE_OFFSET;
        break;
    case ENT_FIRE_FLAME:
        d->fire_flame.x += PASTE_OFFSET;
        break;
    case ENT_FLOAT_PLATFORM:
        /* RAIL mode ignores x/y (the rail positions it): move along it. */
        if (d->float_platform.mode == FLOAT_PLATFORM_RAIL) {
            d->float_platform.t_offset += PASTE_OFFSET / (float)RAIL_TILE_W;
        } else {
            d->float_platform.x += PASTE_OFFSET;
            d->float_platform.y += PASTE_OFFSET;
        }
        break;
    case ENT_BRIDGE:
        d->bridge.x += PASTE_OFFSET;
        break;
    case ENT_BOUNCEPAD_SMALL:
    case ENT_BOUNCEPAD_MEDIUM:
    case ENT_BOUNCEPAD_HIGH:
        d->bouncepad.x += PASTE_OFFSET;
        break;
    case ENT_PLATFORM:
        d->platform.x += PASTE_OFFSET;
        break;
    case ENT_VINE:
        d->vine.x += PASTE_OFFSET;
        break;
    case ENT_LADDER:
        d->ladder.x += PASTE_OFFSET;
        break;
    case ENT_ROPE:
        d->rope.x += PASTE_OFFSET;
        break;
    case ENT_FLOOR_GAP:
        /* Gaps live on the 32-px floor grid; step one whole gap instead. */
        d->floor_gap += FLOOR_GAP_W;
        break;
    case ENT_CHECKPOINT:
        d->checkpoint.x += PASTE_OFFSET;
        d->checkpoint.y += PASTE_OFFSET;
        break;
    case ENT_RAIL:
        d->rail.x += (int)PASTE_OFFSET;
        break;
    case ENT_COUNT:
        break;
    }
}

/*
 * rollback_group — Take back the entries of `group` already recorded, newest
 * first, and forget them (they must not come back with Redo).  A group
 * paste or duplicate is all or nothing.
 */
static void rollback_group(EditorState *es, int group)
{
    Command cmd;
    while (group != 0 && undo_top_group(es->undo) == group &&
           undo_take(es->undo, &cmd))
        editor_apply_undo_command(es, &cmd, 1);
    editor_refresh_dirty(es);
}

/*
 * add_items — Add one offset copy of each item as a single undo step and
 * select the copies.  Shared by Paste and Duplicate; verb ("paste") goes
 * into editor_add_placement's messages, title ("Paste") starts our own.
 *
 * Rails go first, so a rider whose rail was copied with it (rail_item)
 * can ride the new copy of that rail.  Any other rider re-attaches to the
 * rail it was copied from: in this document that rail is tracked by index,
 * so it is found even after it moved and never confused with an identical
 * rail; in another document a rail with the same shape and position is
 * used.  With neither, or when a copy cannot be added (a full array, a
 * failed validation...), everything added so far is taken back and the
 * status bar explains.  On success out_data[i] holds the copy of item i.
 * Returns how many copies were added (0 on failure).
 */
static int add_items(EditorState *es, const EditorClipboardItem *items, int count,
                     const char *verb, const char *title, PlacementData *out_data)
{
    static int new_index[EDITOR_MAX_SELECTION];
    static Selection added[EDITOR_MAX_SELECTION];
    int added_count = 0;
    int group;

    /* One entry per copy; without room for them, add nothing. */
    group = undo_group_begin(es->undo, count);
    if (group == 0) {
        editor_set_status(es, "%s cancelled: cannot allocate undo history", title);
        return 0;
    }
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < count; i++) {
            const EditorClipboardItem *item = &items[i];
            PlacementData d = item->data;
            int *rail_index;

            if ((item->type == ENT_RAIL) != (pass == 0)) continue;
            rail_index = rider_rail_index(item->type, &d);
            if (rail_index) {
                int found = -1;
                if (item->rail_item >= 0) {
                    found = new_index[item->rail_item];
                } else if (item->rail_index >= 0 && item->rail_index < es->level.rail_count) {
                    found = item->rail_index;
                }
                for (int r = 0; found < 0 && item->has_rail && r < es->level.rail_count; r++)
                    if (same_rail(&es->level.rails[r], &item->rail)) found = r;
                if (found < 0) {
                    undo_group_end(es->undo);
                    rollback_group(es, group);
                    editor_set_status(es, "%s blocked: the copied %s's rail is not in this level",
                                      title, editor_entity_type_name(item->type));
                    return 0;
                }
                *rail_index = found;
            }
            offset_pasted_copy(item->type, &d);
            editor_clamp_placement(&es->level, item->type, &d);
            if (editor_add_placement(es, item->type, &d, verb) != 0) {
                /* editor_add_placement already said why. */
                undo_group_end(es->undo);
                rollback_group(es, group);
                return 0;
            }
            new_index[i] = es->selection.index;
            added[added_count++] = es->selection;
            if (out_data) out_data[i] = d;
        }
    }
    undo_group_end(es->undo);
    (void)editor_select_items(es, added, added_count);
    return added_count;
}

/*
 * editor_duplicate_selection — Ctrl+D: copy the selection in place, one
 * paste offset along, without touching the clipboard.
 *
 * Each copy is moved by offset_pasted_copy (the same step Paste uses) and
 * the copies become the selection, so pressing Ctrl+D again duplicates the
 * copies and the row keeps stepping.  A rail rider stays on its own rail
 * (or rides the copy of its rail when that was selected too).  The player
 * spawn and the Last Star exist once per level and are refused.  Several
 * selected entities are copied as one undo step.
 */
void editor_duplicate_selection(EditorState *es)
{
    static EditorClipboardItem items[EDITOR_MAX_SELECTION];
    char error[128];
    int count;
    int added;

    if (!es) return;
    if (level_validate_runtime(&es->level, error, sizeof(error)) != 0) {
        editor_set_status(es, "Duplicate blocked: fix level errors first (%s)", error);
        return;
    }
    count = fill_items(es, items);
    if (count == 0) {
        editor_set_status(es, "Nothing to duplicate: select an entity first");
        return;
    }
    for (int i = 0; i < count; i++) {
        if (editor_entity_type_is_singleton(items[i].type)) {
            editor_set_status(es, "%s is unique; it cannot be duplicated",
                              editor_entity_type_name(items[i].type));
            return;
        }
    }
    added = add_items(es, items, count, "duplicate", "Duplicate", NULL);
    if (added > 0)
        editor_set_status(es, added == 1 ? "Duplicated %d entity" : "Duplicated %d entities",
                          added);
}

/*
 * editor_paste_clipboard — Create new entities from the clipboard.
 *
 * Inserts a copy of each entity the last Ctrl+C copied, moved a little
 * (offset_pasted_copy) so it doesn't hide the original; each further paste
 * moves one more step, so repeated Ctrl+V lays out a row.  The copies are
 * selected for immediate repositioning.  The two singletons (Last Star,
 * Player Spawn) move instead and record a CMD_MOVE.
 *
 * The clipboard survives opening another level, so the copy may come from
 * a document with a different width or different rails.  It is clamped
 * into this level, and the paste is refused as a whole (with a status-bar
 * message) on a full array, a missing rail, or a result that fails
 * validation.
 */
void editor_paste_clipboard(EditorState *es)
{
    static PlacementData pasted[EDITOR_MAX_SELECTION];
    char error[128];
    int added;

    if (!es) return;
    if (es->clipboard_count <= 0) {
        editor_set_status(es, "Nothing to paste: copy an entity with Ctrl+C first");
        return;
    }
    if (level_validate_runtime(&es->level, error, sizeof(error)) != 0) {
        editor_set_status(es, "Paste blocked: fix level errors first (%s)", error);
        return;
    }
    editor_selection_reconcile(es);
    added = add_items(es, es->clipboard, es->clipboard_count, "paste", "Paste", pasted);
    if (added == 0) return;
    /*
     * The next paste starts from these copies, so pressing Ctrl+V again
     * steps one more offset along instead of stacking a second copy on
     * exactly the same spot (which, for a floor gap, validation would even
     * refuse as a duplicate).
     */
    for (int i = 0; i < es->clipboard_count; i++) es->clipboard[i].data = pasted[i];
    if (added > 1) editor_set_status(es, "Pasted %d entities", added);
}
