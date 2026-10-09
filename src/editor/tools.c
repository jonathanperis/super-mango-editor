/*
 * tools.c --- Implementation of the editor's interactive tools.
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
#include "../game.h" /* GAME_W, GAME_H, FLOOR_Y, TILE_SIZE, WORLD_W,
                        FLOOR_GAP_W, MAX_* constants                      */
#include "../surfaces/rail.h" /* MAX_RAIL_SPEED                            */

/*
 * tools_can_hit_test --- Canvas clicks need a level that passes validation.
 *
 * The canvas stops drawing an invalid level (a hand-edited tile count could
 * make its loops enormous), so clicking would act on invisible entities.
 * The editor's own actions never produce such a level; it only arrives via
 * a hand-edited file or a property field.  Tell the designer how to recover.
 */
static int tools_can_hit_test(EditorState *es)
{
    char error[128];
    if (level_validate_runtime(&es->level, error, sizeof(error)) == 0) return 1;
    editor_set_status(es, "Canvas paused: %s. Fix it in the side panel or press Ctrl+Z",
                      error);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Keeping placements inside the world                                 */
/* ------------------------------------------------------------------ */

/* Clamp v into [lo, hi]; an empty range (hi < lo) yields lo. */
static float clampf(float v, float lo, float hi)
{
    if (v > hi) v = hi;
    if (v < lo) v = lo;
    return v;
}

/* Keep a box of width w that starts at *x inside [0, world_w]. */
static void clamp_span(float *x, float w, float world_w)
{
    *x = clampf(*x, 0.0f, world_w - w);
}

/*
 * clamp_patrol --- Slide a patrolling entity so its whole patrol range
 * [x0, x1] lies inside the world.  x moves by the same amount, so the
 * entity keeps its place inside its range.  Validation requires both the
 * range and x to be in-world, and x to stay between x0 and x1.
 */
static void clamp_patrol(float *x, float *x0, float *x1, float world_w)
{
    float shift = 0.0f;

    if (*x0 < 0.0f) shift = -*x0;
    else if (*x1 > world_w) shift = world_w - *x1;
    *x += shift;
    *x0 += shift;
    *x1 += shift;

    /* A range wider than the whole world cannot slide in; trim it. */
    *x0 = clampf(*x0, 0.0f, world_w);
    *x1 = clampf(*x1, 0.0f, world_w);
    *x = clampf(*x, *x0, *x1);
}

/*
 * Axe traps and saws store y = 0 for "default height", so a custom y is
 * kept at 1 px or lower down; clamping it to exactly 0 would make the
 * entity jump back to its default height.
 */
static void clamp_custom_y(float *y)
{
    if (*y != 0.0f) *y = clampf(*y, 1.0f, (float)GAME_H);
}

/*
 * clamp_rail_t --- Keep a rail rider's t_offset on its rail.
 *
 * t counts rail tiles from the start.  A closed rectangle loop wraps
 * around (t = count is the start again); an open horizontal rail stops at
 * its last tile.  Tile counts use the same formula as level validation.
 */
static float clamp_rail_t(const LevelDef *level, int rail_index, float t)
{
    const RailPlacement *rail;
    int count;

    if (rail_index < 0 || rail_index >= level->rail_count) return t;
    rail = &level->rails[rail_index];
    count = (rail->layout == RAIL_LAYOUT_RECT) ? 2 * rail->w + 2 * (rail->h - 2)
                                               : rail->w;
    if (count <= 0) return t;
    if (!isfinite(t) || t < 0.0f) return 0.0f;
    if (rail->layout == RAIL_LAYOUT_RECT) return fmodf(t, (float)count);
    return clampf(t, 0.0f, (float)(count - 1));
}

/*
 * editor_clamp_placement — Pull a new or moved placement back inside the world.
 *
 * Place, paste and drag all call this before a placement is written, so the
 * editor never produces a level that the validator would reject for being
 * out of bounds.  The world is world_w x GAME_H logical pixels.  The rules
 * fall into a few families, each with its own helper:
 *
 *   points        : clamp (x, y) into [0, world_w] x [0, world_h]
 *   patrols       : clamp_patrol moves x *and* its patrol range together
 *   spans         : clamp_span keeps a whole width (spikes, platforms...) inside
 *   custom y      : clamp_custom_y keeps "0 = default height" meaningful
 *   rail riders   : clamp_rail_t keeps t_offset on the rail they ride
 *   flames        : editor_nearest_floor_gap puts x on a floor gap
 *   stacked tiles : vines/ladders/ropes keep every tile above the bottom edge
 */
void editor_clamp_placement(const LevelDef *level, EntityType type,
                            PlacementData *pd)
{
    float world_w;
    const float world_h = (float)GAME_H;

    if (!level || !pd) return;
    world_w = editor_world_width(level);

    switch (type) {
    /* ---- Points: pickups, singletons and checkpoints ---------------- */
    case ENT_COIN:
        pd->coin.x = clampf(pd->coin.x, 0.0f, world_w);
        pd->coin.y = clampf(pd->coin.y, 0.0f, world_h);
        break;
    case ENT_STAR_YELLOW:
        pd->star_yellow.x = clampf(pd->star_yellow.x, 0.0f, world_w);
        pd->star_yellow.y = clampf(pd->star_yellow.y, 0.0f, world_h);
        break;
    case ENT_STAR_GREEN:
        pd->star_green.x = clampf(pd->star_green.x, 0.0f, world_w);
        pd->star_green.y = clampf(pd->star_green.y, 0.0f, world_h);
        break;
    case ENT_STAR_RED:
        pd->star_red.x = clampf(pd->star_red.x, 0.0f, world_w);
        pd->star_red.y = clampf(pd->star_red.y, 0.0f, world_h);
        break;
    case ENT_LAST_STAR:
    case ENT_PLAYER_SPAWN:
        pd->last_star.x = clampf(pd->last_star.x, 0.0f, world_w);
        pd->last_star.y = clampf(pd->last_star.y, 0.0f, world_h);
        break;
    case ENT_CHECKPOINT:
        /* The checkpoint flag needs one tile of room before the world end. */
        pd->checkpoint.x = clampf(pd->checkpoint.x, 0.0f, world_w - TILE_SIZE);
        pd->checkpoint.y = clampf(pd->checkpoint.y, 0.0f, world_h);
        break;
    /* ---- Patrolling enemies: the range moves with the entity -------- */
    case ENT_SPIDER:
        clamp_patrol(&pd->spider.x, &pd->spider.patrol_x0,
                     &pd->spider.patrol_x1, world_w);
        break;
    case ENT_JUMPING_SPIDER:
        clamp_patrol(&pd->jumping_spider.x, &pd->jumping_spider.patrol_x0,
                     &pd->jumping_spider.patrol_x1, world_w);
        break;
    case ENT_BIRD:
    case ENT_FASTER_BIRD:
        clamp_patrol(&pd->bird.x, &pd->bird.patrol_x0, &pd->bird.patrol_x1,
                     world_w);
        pd->bird.base_y = clampf(pd->bird.base_y, 0.0f, world_h);
        break;
    case ENT_FISH:
    case ENT_FASTER_FISH:
        clamp_patrol(&pd->fish.x, &pd->fish.patrol_x0, &pd->fish.patrol_x1,
                     world_w);
        break;
    /* ---- Hazards: y = 0 means "default height" for axes and saws ---- */
    case ENT_AXE_TRAP:
        pd->axe_trap.pillar_x = clampf(pd->axe_trap.pillar_x, 0.0f, world_w);
        clamp_custom_y(&pd->axe_trap.y);
        break;
    case ENT_CIRCULAR_SAW:
        clamp_patrol(&pd->circular_saw.x, &pd->circular_saw.patrol_x0,
                     &pd->circular_saw.patrol_x1, world_w);
        clamp_custom_y(&pd->circular_saw.y);
        break;
    case ENT_SPIKE_ROW:
        clamp_span(&pd->spike_row.x, (float)pd->spike_row.count * SPIKE_TILE_W,
                   world_w);
        break;
    case ENT_SPIKE_PLATFORM:
        clamp_span(&pd->spike_platform.x,
                   (float)pd->spike_platform.tile_count * SPIKE_PLAT_PIECE_W,
                   world_w);
        pd->spike_platform.y = clampf(pd->spike_platform.y, 0.0f,
                                      world_h - SPIKE_PLAT_SRC_H);
        break;
    /* A spike block's position comes from its rail: keep t on that rail. */
    case ENT_SPIKE_BLOCK:
        pd->spike_block.t_offset = clamp_rail_t(level, pd->spike_block.rail_index,
                                                pd->spike_block.t_offset);
        break;
    /* A flame's x is the gap it erupts from: land on the nearest one. */
    case ENT_BLUE_FLAME:
        clamp_span(&pd->blue_flame.x, FLOOR_GAP_W, world_w);
        pd->blue_flame.x = editor_nearest_floor_gap(level, pd->blue_flame.x);
        break;
    case ENT_FIRE_FLAME:
        clamp_span(&pd->fire_flame.x, FLOOR_GAP_W, world_w);
        pd->fire_flame.x = editor_nearest_floor_gap(level, pd->fire_flame.x);
        break;
    /* ---- Surfaces: keep the full width (and height) inside -------- */
    case ENT_FLOAT_PLATFORM:
        /* RAIL mode ignores x/y (the rail positions it), like spike blocks. */
        if (pd->float_platform.mode == FLOAT_PLATFORM_RAIL) {
            pd->float_platform.t_offset =
                clamp_rail_t(level, pd->float_platform.rail_index,
                             pd->float_platform.t_offset);
        } else {
            clamp_span(&pd->float_platform.x,
                       (float)pd->float_platform.tile_count * FLOAT_PLATFORM_PIECE_W,
                       world_w);
            pd->float_platform.y = clampf(pd->float_platform.y, 0.0f,
                                          world_h - FLOAT_PLATFORM_H);
        }
        break;
    case ENT_BRIDGE:
        clamp_span(&pd->bridge.x, (float)pd->bridge.brick_count * BRIDGE_TILE_W,
                   world_w);
        pd->bridge.y = clampf(pd->bridge.y, 0.0f, world_h - BRIDGE_TILE_H);
        break;
    case ENT_BOUNCEPAD_SMALL:
    case ENT_BOUNCEPAD_MEDIUM:
    case ENT_BOUNCEPAD_HIGH:
        pd->bouncepad.x = clampf(pd->bouncepad.x, 0.0f, world_w);
        break;
    case ENT_PLATFORM: {
        int tile_w = pd->platform.tile_width > 0 ? pd->platform.tile_width : 1;
        clamp_span(&pd->platform.x, (float)tile_w * TILE_SIZE, world_w);
        break;
    }
    /* ---- Climbables: the last stacked tile must end above GAME_H --- */
    case ENT_VINE:
        clamp_span(&pd->vine.x, VINE_W, world_w);
        pd->vine.y = clampf(pd->vine.y, 0.0f, world_h -
                            ((float)(pd->vine.tile_count - 1) * VINE_STEP + VINE_H));
        break;
    case ENT_LADDER:
        clamp_span(&pd->ladder.x, LADDER_W, world_w);
        pd->ladder.y = clampf(pd->ladder.y, 0.0f, world_h -
                              ((float)(pd->ladder.tile_count - 1) * LADDER_STEP + LADDER_H));
        break;
    case ENT_ROPE:
        clamp_span(&pd->rope.x, ROPE_W, world_w);
        pd->rope.y = clampf(pd->rope.y, 0.0f, world_h -
                            ((float)(pd->rope.tile_count - 1) * ROPE_STEP + ROPE_H));
        break;
    /* ---- World geometry stored as integers -------------------------- */
    case ENT_FLOOR_GAP: {
        float gap_x = clampf((float)pd->floor_gap, 0.0f, world_w - FLOOR_GAP_W);
        pd->floor_gap = (int)gap_x;
        break;
    }
    case ENT_RAIL: {
        float w = (float)pd->rail.w * RAIL_TILE_W;
        float h = (pd->rail.layout == RAIL_LAYOUT_RECT)
                ? (float)pd->rail.h * RAIL_TILE_H : (float)RAIL_TILE_H;
        pd->rail.x = (int)clampf((float)pd->rail.x, 0.0f, world_w - w);
        pd->rail.y = (int)clampf((float)pd->rail.y, 0.0f, world_h - h);
        break;
    }
    case ENT_COUNT:
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Dragging: the anchor point and moving a placement                   */
/* ------------------------------------------------------------------ */

/*
 * get_entity_anchor --- The point a drag moves: the top-left corner of the
 * entity's on-canvas rectangle (see hit_test.c).  Using what is drawn,
 * rather than the stored fields, keeps derived positions consistent: a
 * flame's stored x is its gap, but the flame is drawn 8 px left of it.
 */
static int get_entity_anchor(const LevelDef *level, EntityType type, int index,
                             float *x, float *y)
{
    EditorRect r;
    if (!editor_entity_bounds(level, type, index, &r)) return 0;
    *x = r.x;
    *y = r.y;
    return 1;
}

/*
 * move_placement --- Return a copy of `from` shifted by (dx, dy) world px.
 *
 * Working with a delta from the original placement makes this the exact
 * inverse of get_entity_anchor: a zero delta returns identical bytes, so a
 * click without movement can never change the document.  Types whose y is
 * derived from the floor or water ignore dy; rail riders are positioned by
 * their rail and do not move at all.
 */
static PlacementData move_placement(EntityType type, const PlacementData *from,
                                    float dx, float dy)
{
    PlacementData pd = *from;

    switch (type) {
    case ENT_COIN:        pd.coin.x += dx;        pd.coin.y += dy;        break;
    case ENT_STAR_YELLOW: pd.star_yellow.x += dx; pd.star_yellow.y += dy; break;
    case ENT_STAR_GREEN:  pd.star_green.x += dx;  pd.star_green.y += dy;  break;
    case ENT_STAR_RED:    pd.star_red.x += dx;    pd.star_red.y += dy;    break;
    case ENT_LAST_STAR:
    case ENT_PLAYER_SPAWN:
        pd.last_star.x += dx;
        pd.last_star.y += dy;
        break;
    case ENT_CHECKPOINT:  pd.checkpoint.x += dx;  pd.checkpoint.y += dy;  break;
    /* Patrolling enemies carry their patrol range along with them. */
    case ENT_SPIDER:
        pd.spider.x += dx;
        pd.spider.patrol_x0 += dx;
        pd.spider.patrol_x1 += dx;
        break;
    case ENT_JUMPING_SPIDER:
        pd.jumping_spider.x += dx;
        pd.jumping_spider.patrol_x0 += dx;
        pd.jumping_spider.patrol_x1 += dx;
        break;
    case ENT_BIRD:
    case ENT_FASTER_BIRD:
        pd.bird.x += dx;
        pd.bird.patrol_x0 += dx;
        pd.bird.patrol_x1 += dx;
        pd.bird.base_y += dy;
        break;
    case ENT_FISH:
    case ENT_FASTER_FISH:
        pd.fish.x += dx;
        pd.fish.patrol_x0 += dx;
        pd.fish.patrol_x1 += dx;
        break;
    case ENT_AXE_TRAP:
        pd.axe_trap.pillar_x += dx;
        /* y = 0 means "default height".  Keep that 0 for a purely
         * horizontal move so undo returns to the exact saved bytes;
         * a vertical move stores the real height plus dy.  A move that
         * lands exactly on y = 0 (e.g. Shift-snap to the top row) is stored
         * as 1 px so it is not read back as "default height". */
        if (dy != 0.0f) {
            pd.axe_trap.y = level_axe_trap_y(&from->axe_trap) + dy;
            if (pd.axe_trap.y <= 0.0f) pd.axe_trap.y = 1.0f;
        }
        break;
    case ENT_CIRCULAR_SAW:
        pd.circular_saw.x += dx;
        pd.circular_saw.patrol_x0 += dx;
        pd.circular_saw.patrol_x1 += dx;
        if (dy != 0.0f) {    /* same "0 = default height" rule as axes */
            pd.circular_saw.y = level_circular_saw_y(&from->circular_saw) + dy;
            if (pd.circular_saw.y <= 0.0f) pd.circular_saw.y = 1.0f;
        }
        break;
    case ENT_SPIKE_ROW:      pd.spike_row.x += dx; break;
    case ENT_SPIKE_PLATFORM:
        pd.spike_platform.x += dx;
        pd.spike_platform.y += dy;
        break;
    case ENT_SPIKE_BLOCK:    break;  /* positioned by its rail */
    case ENT_BLUE_FLAME:     pd.blue_flame.x += dx; break;
    case ENT_FIRE_FLAME:     pd.fire_flame.x += dx; break;
    case ENT_FLOAT_PLATFORM:
        if (pd.float_platform.mode != FLOAT_PLATFORM_RAIL) {
            pd.float_platform.x += dx;
            pd.float_platform.y += dy;
        }
        break;
    case ENT_BRIDGE:         pd.bridge.x += dx; pd.bridge.y += dy; break;
    case ENT_BOUNCEPAD_SMALL:
    case ENT_BOUNCEPAD_MEDIUM:
    case ENT_BOUNCEPAD_HIGH: pd.bouncepad.x += dx; break;
    case ENT_PLATFORM:       pd.platform.x += dx; break;
    case ENT_VINE:           pd.vine.x += dx;   pd.vine.y += dy;   break;
    case ENT_LADDER:         pd.ladder.x += dx; pd.ladder.y += dy; break;
    case ENT_ROPE:           pd.rope.x += dx;   pd.rope.y += dy;   break;
    case ENT_FLOOR_GAP:
        /* Gaps move in whole FLOOR_GAP_W steps to stay on the floor grid. */
        pd.floor_gap += (int)roundf(dx / FLOOR_GAP_W) * FLOOR_GAP_W;
        break;
    case ENT_RAIL:
        /* Rails store whole pixels. */
        pd.rail.x += (int)roundf(dx);
        pd.rail.y += (int)roundf(dy);
        break;
    case ENT_COUNT:
        break;
    }
    return pd;
}

/* ------------------------------------------------------------------ */
/* Internal: delete an entity by type and index                        */
/* ------------------------------------------------------------------ */

/*
 * delete_entity --- Remove one entity from the level and push undo.
 *
 * Snapshots the entity data before removal, removes it through the shared
 * editor_entity_remove helper (the same path undo/redo uses), pushes a
 * CMD_DELETE command to the undo stack, and refreshes the dirty flag.
 */
static void delete_entity(EditorState *es, EntityType type, int index)
{
    LevelDef *level;
    PlacementData before;
    Command cmd;

    if (!es) return;
    level = &es->level;
    if (index < 0 || index >= editor_entity_count(level, type))
        return;

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
            return;
        }
    }

    /* Snapshot entity data before deletion for undo */
    before = editor_snapshot_entity(level, type, index);
    int was_valid = level_validate_runtime(level, NULL, 0) == 0;

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
        return;
    }

    /*
     * A delete must not turn a valid level into one the canvas refuses to
     * draw (e.g. removing the player spawn moves the effective start past a
     * checkpoint).  Put the entity back and explain instead.
     */
    {
        char error[128];
        if (was_valid && level_validate_runtime(level, error, sizeof(error)) != 0) {
            if (editor_entity_type_is_singleton(type))
                (void)editor_entity_write(level, type, 0, &before);
            else
                (void)editor_entity_insert(level, type, index, &before);
            editor_set_status(es, "Cannot delete %s: %s",
                              editor_entity_type_name(type), error);
            return;
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
}

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

int editor_add_placement(EditorState *es, EntityType type,
                         const PlacementData *pd, const char *action)
{
    LevelDef *level;
    int singleton;
    int count;
    int index;
    int rail_index;
    PlacementData before;
    Command cmd;
    char error[128];

    if (!es || !pd || type < 0 || type >= ENT_COUNT) return -1;
    if (!action) action = "add";
    level = &es->level;
    singleton = editor_entity_type_is_singleton(type);
    editor_selection_reconcile(es);

    /* Check capacity — every entity type has a fixed-size array */
    count = editor_entity_count(level, type);
    if (!singleton && count >= editor_entity_capacity(type)) {
        editor_set_status(es, "Cannot %s %s: limit of %d reached", action,
                          editor_entity_type_name(type),
                          editor_entity_capacity(type));
        return -1;
    }

    /* A rail rider needs an existing rail; say so before validation would
     * report it in schema terms. */
    rail_index = rail_rider_index(type, pd);
    if (rail_index >= 0 || type == ENT_SPIKE_BLOCK) {
        if (level->rail_count == 0) {
            editor_set_status(es, "Cannot %s %s: place a rail first", action,
                              editor_entity_type_name(type));
            return -1;
        }
        if (rail_index < 0 || rail_index >= level->rail_count) {
            editor_set_status(es, "Cannot %s %s: rail %d does not exist in this level",
                              action, editor_entity_type_name(type), rail_index);
            return -1;
        }
    }

    /* A flame needs a floor gap to erupt from; say so before validation
     * reports it as a field value. */
    if ((type == ENT_BLUE_FLAME || type == ENT_FIRE_FLAME) &&
        level->floor_gap_count == 0) {
        editor_set_status(es, "Cannot %s %s: place a floor gap first", action,
                          editor_entity_type_name(type));
        return -1;
    }

    memset(&before, 0, sizeof(before));
    if (singleton) {
        index = 0;
        before = editor_snapshot_entity(level, type, 0);
        (void)editor_entity_write(level, type, 0, pd);
    } else {
        index = count;  /* append: new entities draw on top of older ones */
        if (editor_entity_insert(level, type, index, pd) != 0) return -1;
    }

    /*
     * Callers clamp positions into the world, but some rules involve other
     * entities (a checkpoint must be after the player start and must not
     * share an x with another checkpoint).  Undo the change rather than
     * leave a level the canvas refuses to draw.
     */
    if (level_validate_runtime(level, error, sizeof(error)) != 0) {
        if (singleton) (void)editor_entity_write(level, type, 0, &before);
        else (void)editor_entity_remove(level, type, index);
        editor_set_status(es, "Cannot %s %s here: %s", action,
                          editor_entity_type_name(type), error);
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
    es->selection.type  = type;
    es->selection.index = index;
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

/*
 * place_entity --- Add one entity of the palette type at (world_x, world_y).
 *
 * Builds the default placement, attaches spike blocks to the nearest rail,
 * clamps the result into the world, and hands it to editor_add_placement,
 * which checks capacity and validation and records the undo command.
 */
static void place_entity(EditorState *es, float world_x, float world_y)
{
    EntityType type = es->palette_type;
    PlacementData pd;

    if (!default_placement(type, world_x, world_y, &pd)) return;
    if (type == ENT_SPIKE_BLOCK) {
        int rail = editor_nearest_rail(&es->level, world_x, world_y);
        /* With no rail at all, editor_add_placement explains the refusal. */
        pd.spike_block.rail_index = rail >= 0 ? rail : 0;
    }
    editor_clamp_placement(&es->level, type, &pd);
    (void)editor_add_placement(es, type, &pd, "place");
}

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
static void select_and_arm_drag(EditorState *es, float world_x, float world_y)
{
    Selection hit = editor_hit_test(&es->level, world_x, world_y);
    float anchor_x, anchor_y;

    es->dragging = 0;
    if (hit.index < 0) {
        es->selection.index = -1;   /* clicked empty space */
        return;
    }

    es->selection = hit;
    if (!get_entity_anchor(&es->level, hit.type, hit.index,
                           &anchor_x, &anchor_y)) return;
    es->dragging         = 1;
    es->drag_moved       = 0;
    es->drag_type        = hit.type;
    es->drag_index       = hit.index;
    es->drag_before      = editor_snapshot_entity(&es->level, hit.type,
                                                  hit.index);
    es->drag_start_x     = anchor_x;
    es->drag_start_y     = anchor_y;
    es->drag_grab_x      = world_x - anchor_x;
    es->drag_grab_y      = world_y - anchor_y;
    es->drag_mouse_x     = world_x;
    es->drag_mouse_y     = world_y;
}

/*
 * tools_mouse_down --- Dispatch a left-click to the active tool handler.
 *
 * TOOL_SELECT : hit-test and select/deselect, optionally start a drag.
 * TOOL_PLACE  : stamp a new entity at the click position.
 * TOOL_DELETE : hit-test and delete the clicked entity.
 */
void tools_mouse_down(EditorState *es, float world_x, float world_y)
{
    if (!es) return;
    if (!tools_can_hit_test(es)) return;
    editor_selection_reconcile(es);

    switch (es->tool) {

    case TOOL_SELECT:
        select_and_arm_drag(es, world_x, world_y);
        break;

    case TOOL_PLACE:
        place_entity(es, world_x, world_y);
        break;

    case TOOL_DELETE: {
        Selection hit = editor_hit_test(&es->level, world_x, world_y);
        if (hit.index >= 0) {
            delete_entity(es, hit.type, hit.index);
        }
        break;
    }
    }
}

/* The dragged entity still exists where mouse-down found it. */
static int drag_target_exists(const EditorState *es)
{
    return es->drag_index >= 0 &&
           es->drag_index < editor_entity_count(&es->level, es->drag_type);
}

/* ------------------------------------------------------------------ */
/* tools_mouse_up                                                      */
/* ------------------------------------------------------------------ */

/*
 * tools_mouse_up --- End a drag operation and record the move for undo.
 *
 * Compares the entity's final placement with the copy taken at mouse-down.
 * If they differ (the user actually moved it), a CMD_MOVE command is pushed
 * with "before" = mouse-down placement and "after" = final placement.
 */
void tools_mouse_up(EditorState *es, float world_x, float world_y)
{
    PlacementData after;
    Command cmd;

    (void)world_x;
    (void)world_y;

    if (!es || !es->dragging) return;
    es->dragging = 0;
    if (!es->drag_moved || !drag_target_exists(es)) return;

    /*
     * Every motion event kept the level valid, but a field edit could have
     * broken it while the button was held.  Never leave an unrecorded
     * change behind: put the entity back where the drag started.
     */
    if (level_validate_runtime(&es->level, NULL, 0) != 0) {
        (void)editor_entity_write(&es->level, es->drag_type, es->drag_index,
                                  &es->drag_before);
        editor_set_status(es, "Move cancelled: level has errors");
        return;
    }

    after = editor_snapshot_entity(&es->level, es->drag_type, es->drag_index);
    if (memcmp(&after, &es->drag_before, sizeof(after)) == 0) return;

    memset(&cmd, 0, sizeof(cmd));
    cmd.type         = CMD_MOVE;
    cmd.entity_type  = (int)es->drag_type;
    cmd.entity_index = es->drag_index;
    cmd.before       = es->drag_before;
    cmd.after        = after;
    undo_push(es->undo, &cmd);

    editor_refresh_dirty(es);
    if (es->drag_type == ENT_CHECKPOINT)
        editor_set_status(es, "Checkpoint moved");
}

/* ------------------------------------------------------------------ */
/* tools_mouse_drag                                                    */
/* ------------------------------------------------------------------ */

/*
 * tools_mouse_drag --- Update entity position while dragging.
 *
 * Called on every mouse-motion event while the left button is held.
 * The new position is always computed from the mouse-down placement:
 *   new top-left = cursor - grab offset   (optionally Shift-snapped)
 * then clamped into the world.  If the result would still fail level
 * validation (for example a checkpoint dragged behind the player start),
 * the entity stays at its last valid position.
 */
void tools_mouse_drag(EditorState *es, float world_x, float world_y)
{
    PlacementData previous;
    PlacementData moved;
    float zoom;
    float target_x, target_y;

    if (!es || !es->dragging || !drag_target_exists(es)) return;

    /* Ignore small wobbles until the cursor leaves the click threshold.
     * The threshold is in canvas pixels, so divide by zoom for world px. */
    if (!es->drag_moved) {
        float dx = world_x - es->drag_mouse_x;
        float dy = world_y - es->drag_mouse_y;
        zoom = es->camera.zoom > 0.0f ? es->camera.zoom : 1.0f;
        float limit = DRAG_THRESHOLD_PX / zoom;
        if (dx * dx + dy * dy < limit * limit) return;
        es->drag_moved = 1;
    }

    target_x = world_x - es->drag_grab_x;
    target_y = world_y - es->drag_grab_y;

    /*
     * Shift-snap: when the Shift key is held, round the entity's top-left
     * corner down to the TILE_SIZE (48 px) grid.  This makes alignment easy
     * without needing to toggle a separate grid-snap mode.
     */
    int mods = IsWindowReady() ? input_modifiers() : 0;
    if (mods & INPUT_SHIFT) {
        target_x = floorf(target_x / TILE_SIZE) * TILE_SIZE;
        target_y = floorf(target_y / TILE_SIZE) * TILE_SIZE;
    }

    moved = move_placement(es->drag_type, &es->drag_before,
                           target_x - es->drag_start_x,
                           target_y - es->drag_start_y);
    editor_clamp_placement(&es->level, es->drag_type, &moved);

    previous = editor_snapshot_entity(&es->level, es->drag_type, es->drag_index);
    (void)editor_entity_write(&es->level, es->drag_type, es->drag_index, &moved);
    /* Drags only start on a valid level (tools_can_hit_test), so a step
     * that would make it invalid is reverted to the last valid position. */
    if (level_validate_runtime(&es->level, NULL, 0) != 0) {
        (void)editor_entity_write(&es->level, es->drag_type, es->drag_index,
                                  &previous);
    }
}

void tools_cancel_drag(EditorState *es)
{
    if (!es || !es->dragging) return;
    es->dragging = 0;
    if (es->drag_moved && drag_target_exists(es))
        (void)editor_entity_write(&es->level, es->drag_type, es->drag_index,
                                  &es->drag_before);
    editor_set_status(es, "Move cancelled");
}

/* ------------------------------------------------------------------ */
/* tools_nudge_selection                                               */
/* ------------------------------------------------------------------ */

/* The most entities one group action handles at once. */
#define TOOLS_MAX_GROUP 256

/* A fingerprint of which entities a list names, so a run of nudges is
 * only merged while it keeps moving the same ones (FNV-1a over the list). */
static uint64_t selection_key(const Selection *items, int count)
{
    uint64_t hash = UINT64_C(1469598103934665603);
    for (int i = 0; i < count; i++) {
        int pair[2] = {(int)items[i].type, items[i].index};
        const unsigned char *bytes = (const unsigned char *)pair;
        for (size_t b = 0; b < sizeof(pair); b++) {
            hash ^= bytes[b];
            hash *= UINT64_C(1099511628211);
        }
    }
    return hash;
}

void tools_nudge_selection(EditorState *es, float dx, float dy)
{
    static Selection items[TOOLS_MAX_GROUP];
    static PlacementData before[TOOLS_MAX_GROUP];
    static PlacementData after[TOOLS_MAX_GROUP];
    char error[128];
    int count;
    int changed = 0;
    uint32_t now;
    uint64_t key;
    int merge;

    if (!es || !tools_can_hit_test(es)) return;
    editor_selection_reconcile(es);
    count = editor_selection_items(es, items, TOOLS_MAX_GROUP);
    if (count == 0) return;

    /* Move every selected entity, exactly as a drag by (dx, dy) would. */
    for (int i = 0; i < count; i++) {
        before[i] = editor_snapshot_entity(&es->level, items[i].type, items[i].index);
        after[i] = move_placement(items[i].type, &before[i], dx, dy);
        editor_clamp_placement(&es->level, items[i].type, &after[i]);
        (void)editor_entity_write(&es->level, items[i].type, items[i].index, &after[i]);
        if (memcmp(&before[i], &after[i], sizeof(after[i])) != 0) changed++;
    }
    if (level_validate_runtime(&es->level, error, sizeof(error)) != 0) {
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
        es->nudge_group = undo_group_begin(es->undo);
        undo_group_end(es->undo);
    }
    for (int i = 0; i < count; i++) {
        Command cmd;
        if (memcmp(&before[i], &after[i], sizeof(after[i])) == 0) continue;
        if (merge && undo_amend_after(es->undo, es->nudge_group, (int)items[i].type,
                                      items[i].index, &after[i])) continue;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = CMD_MOVE;
        cmd.entity_type = (int)items[i].type;
        cmd.entity_index = items[i].index;
        cmd.group = es->nudge_group;
        cmd.before = before[i];
        cmd.after = after[i];
        undo_push(es->undo, &cmd);
    }
    es->nudge_ms = now;
    es->nudge_selection_key = key;
    editor_refresh_dirty(es);
}

/* ------------------------------------------------------------------ */
/* tools_right_click                                                   */
/* ------------------------------------------------------------------ */

/*
 * tools_right_click --- Right-click deletes whatever entity is under the cursor.
 *
 * This is a convenience shortcut: regardless of the current tool mode,
 * right-clicking an entity removes it immediately.  Useful for quick
 * corrections without switching to the delete tool.
 */
void tools_right_click(EditorState *es, float world_x, float world_y)
{
    if (!es) return;
    if (!tools_can_hit_test(es)) return;
    editor_selection_reconcile(es);
    Selection hit = editor_hit_test(&es->level, world_x, world_y);
    if (hit.index < 0) return;

    delete_entity(es, hit.type, hit.index);
}

/* ------------------------------------------------------------------ */
/* tools_delete_selected                                               */
/* ------------------------------------------------------------------ */

/*
 * tools_delete_selected --- Delete the currently selected entity.
 *
 * Called from the keyboard handler when Delete or Backspace is pressed.
 * Does nothing if no entity is selected (index == -1).
 */
void tools_delete_selected(EditorState *es)
{
    if (!es) return;
    editor_selection_reconcile(es);
    if (!editor_selection_is_valid(es)) return;

    delete_entity(es, es->selection.type, es->selection.index);
}
