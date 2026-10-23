/*
 * tools_place.c — TOOL_PLACE: stamp a new entity and paint a run of them.
 *
 * default_placement fills in the numbers a new entity shows until the
 * designer edits them. Painting reuses that same insert path.
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
/* TOOL_PLACE: create a new entity with sensible defaults              */
/* ------------------------------------------------------------------ */

/*
 * default_placement --- Fill *out with a new entity of `type` at (x, y).
 *
 * Each entity type needs different fields; the switch below fills in
 * starting position, velocity, patrol bounds, and mode flags so the
 * designer can immediately see and test the new entity.
 *
 * Returns 1 when `type` is placeable, 0 otherwise.
 */
static int default_placement(EntityType type, float world_x, float world_y,
                             PlacementData *out)
{
    memset(out, 0, sizeof(*out));

    switch (type) {
    case ENT_SPIDER:
        out->spider.x           = world_x;
        out->spider.vx          = 50.0f;
        out->spider.patrol_x0   = world_x - 50.0f;
        out->spider.patrol_x1   = world_x + 50.0f;
        out->spider.frame_index = 0;
        return 1;
    case ENT_JUMPING_SPIDER:
        out->jumping_spider.x         = world_x;
        out->jumping_spider.vx        = 55.0f;
        out->jumping_spider.patrol_x0 = world_x - 50.0f;
        out->jumping_spider.patrol_x1 = world_x + 50.0f;
        return 1;
    case ENT_BIRD:
    case ENT_FASTER_BIRD:
        /* Both bird variants share BirdPlacement; only the speed differs. */
        out->bird.x           = world_x;
        out->bird.base_y      = world_y;
        out->bird.vx          = (type == ENT_BIRD) ? 45.0f : 80.0f;
        out->bird.patrol_x0   = world_x - 80.0f;
        out->bird.patrol_x1   = world_x + 80.0f;
        out->bird.frame_index = 0;
        return 1;
    case ENT_FISH:
    case ENT_FASTER_FISH:
        out->fish.x         = world_x;
        out->fish.vx        = (type == ENT_FISH) ? 70.0f : 120.0f;
        out->fish.patrol_x0 = world_x - 60.0f;
        out->fish.patrol_x1 = world_x + 60.0f;
        return 1;
    case ENT_AXE_TRAP:
        out->axe_trap.pillar_x = world_x;
        out->axe_trap.mode     = AXE_MODE_PENDULUM;
        return 1;
    case ENT_CIRCULAR_SAW:
        out->circular_saw.x         = world_x;
        out->circular_saw.patrol_x0 = world_x - 48.0f;
        out->circular_saw.patrol_x1 = world_x + 48.0f;
        out->circular_saw.direction = 1;
        return 1;
    case ENT_SPIKE_ROW:
        out->spike_row.x     = world_x;
        out->spike_row.count = 3;
        return 1;
    case ENT_SPIKE_PLATFORM:
        out->spike_platform.x          = world_x;
        out->spike_platform.y          = world_y;
        out->spike_platform.tile_count = 3;
        return 1;
    case ENT_SPIKE_BLOCK:
        out->spike_block.rail_index = 0;
        out->spike_block.t_offset   = 0.0f;
        out->spike_block.speed      = 3.0f;
        return 1;
    /* x is the gap's left edge; centring it on the click (like the ghost)
     * lets editor_clamp_placement pick the gap under the cursor. */
    case ENT_BLUE_FLAME:
        out->blue_flame.x = world_x - FLOOR_GAP_W / 2.0f;
        return 1;
    case ENT_FIRE_FLAME:
        out->fire_flame.x = world_x - FLOOR_GAP_W / 2.0f;
        return 1;
    case ENT_FLOAT_PLATFORM:
        out->float_platform.mode       = FLOAT_PLATFORM_STATIC;
        out->float_platform.x          = world_x;
        out->float_platform.y          = world_y;
        out->float_platform.tile_count = 3;
        out->float_platform.rail_index = 0;
        out->float_platform.t_offset   = 0.0f;
        out->float_platform.speed      = 0.0f;
        return 1;
    case ENT_BRIDGE:
        out->bridge.x           = world_x;
        out->bridge.y           = world_y;
        out->bridge.brick_count = 8;
        return 1;
    case ENT_BOUNCEPAD_SMALL:
        out->bouncepad.x         = world_x;
        out->bouncepad.launch_vy = -380.0f;
        out->bouncepad.pad_type  = BOUNCEPAD_GREEN;
        return 1;
    case ENT_BOUNCEPAD_MEDIUM:
        out->bouncepad.x         = world_x;
        out->bouncepad.launch_vy = -536.25f;
        out->bouncepad.pad_type  = BOUNCEPAD_WOOD;
        return 1;
    case ENT_BOUNCEPAD_HIGH:
        out->bouncepad.x         = world_x;
        out->bouncepad.launch_vy = -700.0f;
        out->bouncepad.pad_type  = BOUNCEPAD_RED;
        return 1;
    case ENT_PLATFORM:
        out->platform.x           = world_x;
        out->platform.tile_height = 2;
        out->platform.tile_width  = 1;
        return 1;
    case ENT_VINE:
        out->vine.x          = world_x;
        out->vine.y          = world_y;
        out->vine.tile_count = 3;
        return 1;
    case ENT_LADDER:
        out->ladder.x          = world_x;
        out->ladder.y          = world_y;
        out->ladder.tile_count = 3;
        return 1;
    case ENT_ROPE:
        out->rope.x          = world_x;
        out->rope.y          = world_y;
        out->rope.tile_count = 3;
        return 1;
    case ENT_COIN:
        out->coin.x = world_x;
        out->coin.y = world_y;
        return 1;
    case ENT_STAR_YELLOW:
        out->star_yellow.x = world_x;
        out->star_yellow.y = world_y;
        return 1;
    case ENT_STAR_GREEN:
        out->star_green.x = world_x;
        out->star_green.y = world_y;
        return 1;
    case ENT_STAR_RED:
        out->star_red.x = world_x;
        out->star_red.y = world_y;
        return 1;
    case ENT_LAST_STAR:
    case ENT_PLAYER_SPAWN:
        /* Singletons: "placing" moves the one existing position. */
        out->last_star.x = world_x;
        out->last_star.y = world_y;
        return 1;
    case ENT_FLOOR_GAP:
        /*
         * Floor gaps snap to a 32-px grid (FLOOR_GAP_W) so they align
         * with the floor tile boundaries.
         */
        out->floor_gap = ((int)world_x / FLOOR_GAP_W) * FLOOR_GAP_W;
        return 1;
    case ENT_CHECKPOINT:
        out->checkpoint.x = world_x;
        out->checkpoint.y = world_y;
        return 1;
    case ENT_RAIL:
        out->rail.layout  = RAIL_LAYOUT_RECT;
        out->rail.x       = (int)world_x;
        out->rail.y       = (int)world_y;
        out->rail.w       = 4;
        out->rail.h       = 4;
        out->rail.end_cap = 0;
        return 1;
    case ENT_COUNT:
        break;
    }
    return 0;
}

/*
 * rail_rider_index --- The rail a placement rides, or -1 if it rides none.
 * Spike blocks always ride a rail; float platforms only in RAIL mode.
 */
static int rail_rider_index(EntityType type, const PlacementData *pd)
{
    if (type == ENT_SPIKE_BLOCK) return pd->spike_block.rail_index;
    if (type == ENT_FLOAT_PLATFORM && pd->float_platform.mode == FLOAT_PLATFORM_RAIL)
        return pd->float_platform.rail_index;
    return -1;
}

int editor_insert_checked(LevelDef *level, EntityType type,
                          const PlacementData *pd, const char *action,
                          PlacementData *before, char *why, size_t why_size)
{
    int singleton;
    int count;
    int index;
    int rail_index;
    char error[128];

    if (!level || !pd || !before || !why || type < 0 || type >= ENT_COUNT) return -1;
    if (!action) action = "add";
    singleton = editor_entity_type_is_singleton(type);

    /* Check capacity — every entity type has a fixed-size array */
    count = editor_entity_count(level, type);
    if (!singleton && count >= editor_entity_capacity(type)) {
        snprintf(why, why_size, "Cannot %s %s: limit of %d reached", action,
                 editor_entity_type_name(type), editor_entity_capacity(type));
        return -1;
    }

    /* A rail rider needs an existing rail; say so before validation would
     * report it in schema terms. */
    rail_index = rail_rider_index(type, pd);
    if (rail_index >= 0 || type == ENT_SPIKE_BLOCK) {
        if (level->rail_count == 0) {
            snprintf(why, why_size, "Cannot %s %s: place a rail first", action,
                     editor_entity_type_name(type));
            return -1;
        }
        if (rail_index < 0 || rail_index >= level->rail_count) {
            snprintf(why, why_size, "Cannot %s %s: rail %d does not exist in this level",
                     action, editor_entity_type_name(type), rail_index);
            return -1;
        }
    }

    /* A flame needs a floor gap to erupt from; say so before validation
     * reports it as a field value. */
    if ((type == ENT_BLUE_FLAME || type == ENT_FIRE_FLAME) &&
        level->floor_gap_count == 0) {
        snprintf(why, why_size, "Cannot %s %s: place a floor gap first", action,
                 editor_entity_type_name(type));
        return -1;
    }

    EditorIssueSnapshot issues_before;
    editor_level_issue_snapshot(level, &issues_before);
    memset(before, 0, sizeof(*before));
    if (singleton) {
        index = 0;
        *before = editor_snapshot_entity(level, type, 0);
        (void)editor_entity_write(level, type, 0, pd);
    } else {
        index = count;  /* append: new entities draw on top of older ones */
        if (editor_entity_insert(level, type, index, pd) != 0) {
            snprintf(why, why_size, "Cannot %s %s", action, editor_entity_type_name(type));
            return -1;
        }
    }

    /*
     * Callers clamp positions into the world, but some rules involve other
     * entities (a checkpoint must be after the player start and must not
     * share an x with another checkpoint).  Undo the change rather than
     * add an error to the level.
     */
    if (editor_edit_kept_level(level, &issues_before, error, sizeof(error)) != 0) {
        if (singleton) (void)editor_entity_write(level, type, 0, before);
        else (void)editor_entity_remove(level, type, index);
        snprintf(why, why_size, "Cannot %s %s here: %s", action,
                 editor_entity_type_name(type), error);
        return -1;
    }
    return index;
}

int editor_add_placement(EditorState *es, EntityType type,
                         const PlacementData *pd, const char *action)
{
    PlacementData before;
    Command cmd;
    char why[192];
    int singleton;
    int index;

    if (!es || !pd || type < 0 || type >= ENT_COUNT) return -1;
    singleton = editor_entity_type_is_singleton(type);
    editor_selection_reconcile(es);
    index = editor_insert_checked(&es->level, type, pd, action, &before, why, sizeof(why));
    if (index < 0) {
        editor_set_status(es, "%s", why);
        return -1;
    }

    /*
     * Push the undo command.  "after" holds the new entity data so redo can
     * re-insert it; singletons also keep "before" so undo can move them back.
     */
    memset(&cmd, 0, sizeof(cmd));
    cmd.type         = singleton ? CMD_MOVE : CMD_PLACE;
    cmd.entity_type  = (int)type;
    cmd.entity_index = index;
    cmd.before       = before;
    cmd.after        = *pd;
    undo_push(es->undo, &cmd);

    /* Select the new entity for immediate inspection */
    editor_select_only(es, type, index);
    editor_refresh_dirty(es);
    if (type == ENT_CHECKPOINT) editor_set_status(es, "Checkpoint placed");
    return 0;
}

/*
 * editor_nearest_rail --- Index of the rail closest to (x, y), or -1 when the
 * level has none.  Distance is measured to the rail's rectangle, so a click
 * anywhere on or inside a rail loop picks that rail.
 */
int editor_nearest_rail(const LevelDef *level, float x, float y)
{
    int best = -1;
    float best_distance = 0.0f;

    for (int i = 0; i < editor_entity_count(level, ENT_RAIL); i++) {
        EditorRect r;
        if (!editor_entity_bounds(level, ENT_RAIL, i, &r)) continue;
        float dx = clampf(x, r.x, r.x + r.w) - x;
        float dy = clampf(y, r.y, r.y + r.h) - y;
        float distance = dx * dx + dy * dy;
        if (best < 0 || distance < best_distance) {
            best = i;
            best_distance = distance;
        }
    }
    return best;
}

/*
 * editor_set_float_platform_mode --- Switch a float platform between
 * Static, Crumble and Rail without leaving it on a rail it never meant.
 *
 * Rails are referred to by their position in the rails array, and the
 * editor only renumbers the references that are in use: RAIL-mode float
 * platforms and spike blocks.  A Static or Crumble platform keeps whatever
 * rail_index it last had, and after rails were added or deleted that number
 * may name another rail or no rail at all.  So switching to Rail checks it:
 * a number that names no rail is replaced by the rail nearest the platform,
 * and either way the status bar says which rail it now rides, so a stale
 * (but existing) rail is visible at once and easy to change.
 */
int editor_set_float_platform_mode(EditorState *es, int index,
                                   FloatPlatformMode mode)
{
    FloatPlatformPlacement *p;

    if (!es || index < 0 || index >= editor_entity_count(&es->level, ENT_FLOAT_PLATFORM))
        return -1;
    p = &es->level.float_platforms[index];
    if (mode != FLOAT_PLATFORM_RAIL) {
        p->mode = mode;
        return 0;
    }
    if (es->level.rail_count <= 0) {
        editor_set_status(es, "Cannot switch Float Platform to Rail: place a rail first");
        return -1;
    }
    p->mode = FLOAT_PLATFORM_RAIL;
    /* Static and crumbling platforms are placed with speed 0, which a rail
     * rider may not have; give it a speed it can save with. */
    if (!(p->speed >= RAIL_SPEED_MIN && p->speed <= MAX_RAIL_SPEED))
        p->speed = RAIL_SPEED_DEFAULT;
    if (p->rail_index < 0 || p->rail_index >= es->level.rail_count) {
        int old = p->rail_index;
        p->rail_index = editor_nearest_rail(&es->level, p->x, p->y);
        p->t_offset = clamp_rail_t(&es->level, p->rail_index, p->t_offset);
        editor_set_status(es, "Float Platform rides rail %d (its rail %d no longer exists)",
                          p->rail_index, old);
    } else {
        p->t_offset = clamp_rail_t(&es->level, p->rail_index, p->t_offset);
        editor_set_status(es, "Float Platform rides rail %d; edit rail_index to pick another",
                          p->rail_index);
    }
    return 0;
}

int editor_snap_active(const EditorState *es)
{
    int shift = es && (es->input_mods & INPUT_SHIFT) != 0;
    return es && (es->snap_to_grid != 0) != shift;
}

void editor_snap_point(const EditorState *es, float *x, float *y)
{
    if (!editor_snap_active(es)) return;
    /* floorf, not a cast: a cast rounds -10 up to 0, floorf down to -48. */
    if (x) *x = floorf(*x / TILE_SIZE) * TILE_SIZE;
    if (y) *y = floorf(*y / TILE_SIZE) * TILE_SIZE;
}

/*
 * place_entity --- Add one entity of the palette type at (world_x, world_y).
 *
 * Builds the default placement, attaches spike blocks to the nearest rail,
 * clamps the result into the world, and hands it to editor_add_placement,
 * which checks capacity and validation and records the undo command.
 */
int place_entity(EditorState *es, float world_x, float world_y)
{
    EntityType type = es->palette_type;
    PlacementData pd;

    /* With snapping on, the click lands on the grid cell's corner, the same
     * point the placement ghost was drawn at. */
    editor_snap_point(es, &world_x, &world_y);
    es->paint_x = world_x;
    es->paint_y = world_y;
    if (!default_placement(type, world_x, world_y, &pd)) return -1;
    if (type == ENT_SPIKE_BLOCK) {
        int rail = editor_nearest_rail(&es->level, world_x, world_y);
        /* With no rail at all, editor_add_placement explains the refusal. */
        pd.spike_block.rail_index = rail >= 0 ? rail : 0;
    }
    editor_clamp_placement(&es->level, type, &pd);
    /* A row never stacks copies: past the world's edge the clamp, and for
     * a spike block or a flame its rail or gap, can land the next one
     * exactly where the last one went. */
    if (es->painting && es->paint_count > 0 &&
        placement_same(type, &pd, &es->paint_last)) return -1;
    if (editor_add_placement(es, type, &pd, "place") != 0) return -1;
    es->paint_last = pd;
    return 0;
}

/*
 * Painting a row: a press with the Place tool places one entity and opens
 * an undo group; each time the pointer then moves at least the entity's
 * own width or height from the last one placed, another goes there (on
 * the grid when snapping), up to UNDO_GROUP_MAX.  Release (or Esc) closes
 * the group, so the whole row is one undo step.  Singletons (the player
 * spawn, the Last Star) move instead and do not paint.
 */
void begin_paint(EditorState *es, float world_x, float world_y)
{
    es->painting = 0;
    es->paint_count = 0;
    es->paint_grouped = 0;
    if (!editor_entity_type_is_singleton(es->palette_type))
        es->paint_grouped = undo_group_begin(es->undo, UNDO_GROUP_MAX) != 0;
    if (place_entity(es, world_x, world_y) == 0) es->paint_count = 1;
    /* Without the room to keep the row as one step, the press places one. */
    es->painting = es->paint_grouped;
}

void paint_step(EditorState *es, float world_x, float world_y)
{
    const EditorEntityPreview *preview = editor_entity_preview(es->palette_type);
    float step_w = preview && preview->w > 0 ? (float)preview->w : (float)TILE_SIZE;
    float step_h = preview && preview->h > 0 ? (float)preview->h : (float)TILE_SIZE;
    float x = world_x, y = world_y;
    float last_x = es->paint_x, last_y = es->paint_y;

    if (es->paint_count >= UNDO_GROUP_MAX) return;
    editor_snap_point(es, &x, &y);
    if (fabsf(x - last_x) < step_w && fabsf(y - last_y) < step_h) return;
    if (place_entity(es, world_x, world_y) == 0) es->paint_count++;
    else { es->paint_x = last_x; es->paint_y = last_y; }
}

void tools_end_paint(EditorState *es)
{
    if (!es || !es->painting) return;
    if (es->paint_grouped) undo_group_end(es->undo);
    es->painting = 0;
    es->paint_grouped = 0;
    if (es->paint_count > 1)
        editor_set_status(es, "Placed %d %s", es->paint_count,
                          editor_entity_type_name(es->palette_type));
}
