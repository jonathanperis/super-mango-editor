/*
 * editor_clipboard.c — Editor clipboard copy/paste helpers.
 */

#include "editor_clipboard.h"

#include <string.h>  /* memset */

#include "../surfaces/rail.h" /* RAIL_TILE_W */
#include "undo.h"   /* Command, undo_push */
#include "editor_session.h" /* save-point dirty tracking */
#include "entity_meta.h" /* central selection validity and entity storage */

/* Paste lands 24 px right and down so the copy does not hide the original. */
#define PASTE_OFFSET 24.0f

/*
 * editor_copy_selected — Snapshot the currently selected entity into the clipboard.
 *
 * Stores the entity type and a PlacementData union so paste can recreate it.
 */
void editor_copy_selected(EditorState *es)
{
    if (!es) return;
    editor_selection_reconcile(es);
    if (!editor_selection_is_valid(es)) return;

    es->clipboard_type = es->selection.type;
    es->clipboard_data = editor_snapshot_entity(&es->level, es->selection.type,
                                                es->selection.index);
    es->has_clipboard = 1;
}

/*
 * offset_pasted_copy — Move a copied placement by PASTE_OFFSET.
 *
 * Every type stores its position in different fields (patrolling enemies
 * also carry a patrol range that must move with them), so this is the one
 * per-type switch in the clipboard.
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
        d->float_platform.x += PASTE_OFFSET;
        d->float_platform.y += PASTE_OFFSET;
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
 * editor_paste_clipboard — Create a new entity from the clipboard data.
 *
 * Inserts a copy of the last Ctrl+C'd entity into the level, offset by
 * 24px right and 24px down so it doesn't overlap the original. The new
 * entity is auto-selected for immediate repositioning.  The two singletons
 * (Last Star, Player Spawn) move instead and record a CMD_MOVE.
 */
void editor_paste_clipboard(EditorState *es)
{
    EntityType type;
    PlacementData d;
    Command cmd;
    int index;

    if (!es || !es->has_clipboard) return;
    editor_selection_reconcile(es);

    type = es->clipboard_type;
    d = es->clipboard_data;
    offset_pasted_copy(type, &d);

    memset(&cmd, 0, sizeof(cmd));
    if (editor_entity_type_is_singleton(type)) {
        index = 0;
        cmd.type = CMD_MOVE;
        cmd.before = editor_snapshot_entity(&es->level, type, 0);
        (void)editor_entity_write(&es->level, type, 0, &d);
    } else {
        index = editor_entity_count(&es->level, type);
        if (editor_entity_insert(&es->level, type, index, &d) != 0) return;
        cmd.type = CMD_PLACE;
    }

    cmd.entity_type = (int)type;
    cmd.entity_index = index;
    cmd.after = d;
    undo_push(es->undo, cmd);

    es->selection.type = type;
    es->selection.index = index;
    editor_refresh_dirty(es);
    editor_selection_reconcile(es);
}
