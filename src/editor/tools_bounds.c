/*
 * tools_bounds.c — Keep a placement inside the world, and move one.
 *
 * editor_clamp_placement and editor_move_placement are what drag, nudge
 * and paste all go through, so a type's edges are decided in one place.
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

/* ------------------------------------------------------------------ */
/* Keeping placements inside the world                                 */
/* ------------------------------------------------------------------ */

/* Clamp v into [lo, hi]; an empty range (hi < lo) yields lo. */
float clampf(float v, float lo, float hi)
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
float clamp_rail_t(const LevelDef *level, int rail_index, float t)
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
int get_entity_anchor(const LevelDef *level, EntityType type, int index,
                             float *x, float *y)
{
    EditorRect r;
    if (!editor_entity_bounds(level, type, index, &r)) return 0;
    *x = r.x;
    *y = r.y;
    return 1;
}

/*
 * editor_move_placement --- see tools.h.  This is the one place that knows
 * which fields hold each type's position.
 *
 * Working with a delta from the original placement makes this the exact
 * inverse of get_entity_anchor: a zero delta returns identical bytes, so a
 * click without movement can never change the document.
 */
PlacementData editor_move_placement(EntityType type, const PlacementData *from,
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
