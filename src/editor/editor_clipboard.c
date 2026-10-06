/*
 * editor_clipboard.c — Editor clipboard copy/paste helpers.
 */

#include "editor_clipboard.h"

#include "../levels/level_loader.h" /* level_validate_runtime */
#include "../surfaces/rail.h" /* RAIL_TILE_W */
#include "editor_session.h" /* editor_set_status */
#include "entity_meta.h" /* central selection validity and entity storage */
#include "tools.h"       /* editor_clamp_placement, editor_add_placement */

/* Paste lands 24 px right and down so the copy does not hide the original. */
#define PASTE_OFFSET 24.0f

/*
 * editor_copy_selected — Snapshot the currently selected entity into the clipboard.
 *
 * Stores the entity type and a PlacementData union so paste can recreate it.
 */
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

void editor_copy_selected(EditorState *es)
{
    if (!es) return;
    editor_selection_reconcile(es);
    if (!editor_selection_is_valid(es)) return;

    es->clipboard_type = es->selection.type;
    es->clipboard_data = editor_snapshot_entity(&es->level, es->selection.type,
                                                es->selection.index);
    es->has_clipboard = 1;

    /* Remember the rail itself, not just its position in the rails array. */
    int *rail_index = rider_rail_index(es->clipboard_type, &es->clipboard_data);
    es->clipboard_has_rail = rail_index && *rail_index >= 0 &&
                             *rail_index < es->level.rail_count;
    if (es->clipboard_has_rail)
        es->clipboard_rail = es->level.rails[*rail_index];
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
 * editor_paste_clipboard — Create a new entity from the clipboard data.
 *
 * Inserts a copy of the last Ctrl+C'd entity into the level, offset by
 * 24px right and 24px down so it doesn't overlap the original. The new
 * entity is auto-selected for immediate repositioning.  The two singletons
 * (Last Star, Player Spawn) move instead and record a CMD_MOVE.
 *
 * The clipboard survives opening another level, so the copy may come from
 * a document with a different width or different rails.  It is clamped
 * into this level, and editor_add_placement refuses (with a status-bar
 * message) a full array, a missing rail, or a result that fails validation.
 */
void editor_paste_clipboard(EditorState *es)
{
    char error[128];
    EntityType type;
    PlacementData d;

    if (!es) return;
    if (!es->has_clipboard) {
        editor_set_status(es, "Nothing to paste: copy an entity with Ctrl+C first");
        return;
    }
    if (level_validate_runtime(&es->level, error, sizeof(error)) != 0) {
        editor_set_status(es, "Paste blocked: fix level errors first (%s)", error);
        return;
    }
    editor_selection_reconcile(es);

    type = es->clipboard_type;
    d = es->clipboard_data;

    /* Re-attach a rail rider to the rail it was copied from, found by its
     * shape and position.  Without that rail in this level, refuse instead
     * of silently riding whichever rail now has the old index. */
    int *rail_index = rider_rail_index(type, &d);
    if (rail_index) {
        int found = -1;
        for (int i = 0; es->clipboard_has_rail && i < es->level.rail_count; i++) {
            if (same_rail(&es->level.rails[i], &es->clipboard_rail)) { found = i; break; }
        }
        if (found < 0) {
            editor_set_status(es, "Paste blocked: the copied %s's rail is not in this level",
                              editor_entity_type_name(type));
            return;
        }
        *rail_index = found;
    }
    offset_pasted_copy(type, &d);
    editor_clamp_placement(&es->level, type, &d);
    (void)editor_add_placement(es, type, &d, "paste");
}
