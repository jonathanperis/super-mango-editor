/*
 * tools.c --- Implementation of the editor's interactive tools.
 *
 * Implements SELECT, PLACE, MOVE, and DELETE tool interactions for all 25
 * entity types.  Every function operates on EditorState, modifying the
 * LevelDef in place and pushing undo commands for reversibility.
 *
 * Key design decisions:
 *   - Hit-testing uses display-size bounding boxes (not sprite frame sizes)
 *     so click targets match what the designer sees on screen.
 *   - Entities are tested in reverse render order (topmost first) so
 *     overlapping entities resolve intuitively.
 *   - Array compaction on delete uses memmove to shift trailing elements
 *     left by one slot, maintaining contiguous storage.
 *   - PLACE provides sensible defaults for every entity type so the
 *     designer can immediately see and test the new entity.
 */

#include <stdio.h>   /* fprintf for capacity warnings */
#include <string.h>  /* memmove for array compaction  */

#include "tools.h"
#include "editor.h"  /* EditorState, EntityType, Selection, EditorTool    */
#include "undo.h"    /* Command, PlacementData, undo_push                 */
#include "../game.h" /* GAME_W, GAME_H, FLOOR_Y, TILE_SIZE, WORLD_W,
                        SEA_GAP_W, MAX_* constants                        */

/* ------------------------------------------------------------------ */
/* Entity display dimensions — duplicated from canvas.c                */
/* ------------------------------------------------------------------ */
/*
 * These constants define the on-screen bounding box of each entity type
 * in logical pixels.  They must match the values used by canvas.c for
 * rendering so that hit-testing aligns with what the designer sees.
 */

/* Spider / Jumping Spider — 64-px frame slot, only bottom 10 rows visible */
#define SPIDER_FRAME_W     64
#define SPIDER_ART_H       10

/* Bird / Faster Bird — 48-px frame slot, 14 rows of visible art */
#define BIRD_FRAME_W       48
#define BIRD_ART_H         14

/* Fish / Faster Fish — full 48x48 frame */
#define FISH_FRAME_W       48
#define FISH_FRAME_H       48

/* Collectibles */
#define COIN_DISPLAY_W     16
#define COIN_DISPLAY_H     16
#define YSTAR_DISPLAY_W    16
#define YSTAR_DISPLAY_H    16
#define LSTAR_DISPLAY_W    24
#define LSTAR_DISPLAY_H    24

/* Axe trap — 48x64 frame rendered at computed pivot point */
#define AXE_FRAME_W        48
#define AXE_FRAME_H        64

/* Circular saw — 32x32 display */
#define SAW_DISPLAY_W      32
#define SAW_DISPLAY_H      32

/* Spike row — 16x16 tiles */
#define SPIKE_TILE_W       16
#define SPIKE_TILE_H       16

/* Spike platform — 16-px pieces, 11 px visible height */
#define SPIKE_PLAT_PIECE_W 16
#define SPIKE_PLAT_SRC_H   11

/* Spike block — 24x24 display, rides rails */
#define SPIKE_DISPLAY_W    24
#define SPIKE_DISPLAY_H    24

/* Blue flame — 48x48 display, erupts from sea gaps */
#define BLUE_FLAME_W       48
#define BLUE_FLAME_H       48

/* Fire flame — 48x48 display, erupts from sea gaps (fire variant) */
#define FIRE_FLAME_W       48
#define FIRE_FLAME_H       48

/* Floating platform — 16x16 pieces */
#define FPLAT_PIECE_W      16
#define FPLAT_PIECE_H      16

/* Bridge — 16x16 tiles */
#define BRIDGE_TILE_W      16
#define BRIDGE_TILE_H      16

/* Bouncepad — all three variants share crop dimensions */
#define BP_SRC_H           18
#define BP_FRAME_W         48

/* Vine — stacked tiles with overlap */
#define VINE_W             16
#define VINE_H             32
#define VINE_STEP          19

/* Ladder — tightly stacked tiles */
#define LADDER_W           16
#define LADDER_H           22
#define LADDER_STEP         8

/* Rope — stacked tiles with step */
#define ROPE_W             12
#define ROPE_H             36
#define ROPE_STEP          34

/* Rail — 16x16 tiles */
#define RAIL_TILE_W        16
#define RAIL_TILE_H        16

/* Player spawn — full 48x48 idle frame used as preview */
#define PLAYER_SPAWN_W     48
#define PLAYER_SPAWN_H     48

/* Water art height — needed for fish Y derivation */
#define WATER_ART_H        31

/* ------------------------------------------------------------------ */
/* Utility: generic array element removal                              */
/* ------------------------------------------------------------------ */

/*
 * array_remove --- Remove element at `index` from a contiguous array.
 *
 * Uses memmove to shift all elements after the removed one left by one
 * slot, preserving order.  Decrements *count to reflect the new size.
 *
 * arr       : pointer to the first element of the array.
 * count     : pointer to the element count (decremented on return).
 * index     : zero-based index of the element to remove.
 * elem_size : sizeof one element (passed explicitly because arr is void*).
 */
static void array_remove(void *arr, int *count, int index, size_t elem_size)
{
    char *base = (char *)arr;
    memmove(base + index * elem_size,
            base + (index + 1) * elem_size,
            (*count - index - 1) * elem_size);
    (*count)--;
}

/* ------------------------------------------------------------------ */
/* Utility: get / set entity position by type and index                */
/* ------------------------------------------------------------------ */

/*
 * get_entity_pos --- Read the display position of an entity.
 *
 * Many entity types derive their Y from game constants (spiders sit on
 * the floor, fish float in the water lane, bouncepads align to floor).
 * This function computes the correct display position for any type so
 * the hit-test and drag system don't need per-type switch blocks.
 */
static void get_entity_pos(const LevelDef *level, EntityType type, int index,
                           float *x, float *y)
{
    switch (type) {
    case ENT_PLATFORM: {
        const PlatformPlacement *p = &level->platforms[index];
        *x = p->x;
        *y = (float)(FLOOR_Y - p->tile_height * TILE_SIZE + 16);
        break;
    }
    case ENT_SEA_GAP:
        *x = (float)level->sea_gaps[index];
        *y = (float)FLOOR_Y;
        break;
    case ENT_RAIL:
        *x = (float)level->rails[index].x;
        *y = (float)level->rails[index].y;
        break;
    case ENT_COIN:
        *x = level->coins[index].x;
        *y = level->coins[index].y;
        break;
    case ENT_STAR_YELLOW:
        *x = level->star_yellows[index].x;
        *y = level->star_yellows[index].y;
        break;
    case ENT_STAR_GREEN:
        *x = level->star_greens[index].x;
        *y = level->star_greens[index].y;
        break;
    case ENT_STAR_RED:
        *x = level->star_reds[index].x;
        *y = level->star_reds[index].y;
        break;
    case ENT_LAST_STAR:
        *x = level->last_star.x;
        *y = level->last_star.y;
        break;
    case ENT_PLAYER_SPAWN:
        *x = level->player_start_x;
        *y = level->player_start_y;
        break;
    case ENT_SPIDER:
        *x = level->spiders[index].x;
        *y = (float)(FLOOR_Y - SPIDER_ART_H);
        break;
    case ENT_JUMPING_SPIDER:
        *x = level->jumping_spiders[index].x;
        *y = (float)(FLOOR_Y - SPIDER_ART_H);
        break;
    case ENT_BIRD:
        *x = level->birds[index].x;
        *y = level->birds[index].base_y;
        break;
    case ENT_FASTER_BIRD:
        *x = level->faster_birds[index].x;
        *y = level->faster_birds[index].base_y;
        break;
    case ENT_FISH:
        *x = level->fish[index].x;
        *y = (float)(GAME_H - WATER_ART_H) - FISH_FRAME_H / 2.0f;
        break;
    case ENT_FASTER_FISH:
        *x = level->faster_fish[index].x;
        *y = (float)(GAME_H - WATER_ART_H) - FISH_FRAME_H / 2.0f;
        break;
    case ENT_AXE_TRAP: {
        const AxeTrapPlacement *at = &level->axe_traps[index];
        *x = at->pillar_x;
        *y = (at->y != 0.0f) ? at->y : (float)(FLOOR_Y - 3 * TILE_SIZE + 16);
        break;
    }
    case ENT_CIRCULAR_SAW: {
        const CircularSawPlacement *cs = &level->circular_saws[index];
        *x = cs->x;
        *y = (cs->y != 0.0f) ? cs->y : (float)(FLOOR_Y - 2 * TILE_SIZE + 16 - SAW_DISPLAY_H);
        break;
    }
    case ENT_SPIKE_ROW:
        *x = level->spike_rows[index].x;
        *y = (float)(FLOOR_Y - SPIKE_TILE_H);
        break;
    case ENT_SPIKE_PLATFORM:
        *x = level->spike_platforms[index].x;
        *y = level->spike_platforms[index].y;
        break;
    case ENT_SPIKE_BLOCK: {
        const SpikeBlockPlacement *sb = &level->spike_blocks[index];
        int ri = sb->rail_index;
        if (ri >= 0 && ri < level->rail_count) {
            const RailPlacement *rp = &level->rails[ri];
            if (rp->layout == RAIL_LAYOUT_RECT) {
                *x = (float)rp->x;
                *y = (float)rp->y;
            } else {
                *x = (float)rp->x + sb->t_offset * (float)RAIL_TILE_W;
                *y = (float)rp->y;
            }
        } else {
            *x = 0.0f;
            *y = 0.0f;
        }
        break;
    }
    case ENT_BLUE_FLAME: {
        float gap_x = level->blue_flames[index].x;
        *x = gap_x + (float)(SEA_GAP_W - BLUE_FLAME_W) / 2.0f;
        *y = (float)(FLOOR_Y - BLUE_FLAME_H);
        break;
    }
    case ENT_FIRE_FLAME: {
        float gap_x = level->fire_flames[index].x;
        *x = gap_x + (float)(SEA_GAP_W - FIRE_FLAME_W) / 2.0f;
        *y = (float)(FLOOR_Y - FIRE_FLAME_H);
        break;
    }
    case ENT_FLOAT_PLATFORM:
        *x = level->float_platforms[index].x;
        *y = level->float_platforms[index].y;
        break;
    case ENT_BRIDGE:
        *x = level->bridges[index].x;
        *y = level->bridges[index].y;
        break;
    case ENT_BOUNCEPAD_SMALL:
        *x = level->bouncepads_small[index].x;
        *y = (float)(FLOOR_Y - BP_SRC_H);
        break;
    case ENT_BOUNCEPAD_MEDIUM:
        *x = level->bouncepads_medium[index].x;
        *y = (float)(FLOOR_Y - BP_SRC_H);
        break;
    case ENT_BOUNCEPAD_HIGH:
        *x = level->bouncepads_high[index].x;
        *y = (float)(FLOOR_Y - BP_SRC_H);
        break;
    case ENT_VINE:
        *x = level->vines[index].x;
        *y = level->vines[index].y;
        break;
    case ENT_LADDER:
        *x = level->ladders[index].x;
        *y = level->ladders[index].y;
        break;
    case ENT_ROPE:
        *x = level->ropes[index].x;
        *y = level->ropes[index].y;
        break;
    default:
        *x = 0.0f;
        *y = 0.0f;
        break;
    }
}

/*
 * set_entity_pos --- Update the placement position of an entity.
 *
 * For entities with a derived Y (spiders, fish, bouncepads), only x is
 * written because y is computed from game constants at render time.
 * For entities that store both x and y (coins, stars, bridges), both
 * fields are updated.
 */
static void set_entity_pos(LevelDef *level, EntityType type, int index,
                           float x, float y)
{
    switch (type) {
    case ENT_PLATFORM:
        level->platforms[index].x = x;
        /* tile_height stays — y is derived from it */
        break;
    case ENT_SEA_GAP:
        level->sea_gaps[index] = (int)x;
        break;
    case ENT_RAIL:
        level->rails[index].x = (int)x;
        level->rails[index].y = (int)y;
        break;
    case ENT_COIN:
        level->coins[index].x = x;
        level->coins[index].y = y;
        break;
    case ENT_STAR_YELLOW:
        level->star_yellows[index].x = x;
        level->star_yellows[index].y = y;
        break;
    case ENT_STAR_GREEN:
        level->star_greens[index].x = x;
        level->star_greens[index].y = y;
        break;
    case ENT_STAR_RED:
        level->star_reds[index].x = x;
        level->star_reds[index].y = y;
        break;
    case ENT_LAST_STAR:
        level->last_star.x = x;
        level->last_star.y = y;
        break;
    case ENT_PLAYER_SPAWN:
        level->player_start_x = x;
        level->player_start_y = y;
        break;
    case ENT_SPIDER:
        level->spiders[index].x = x;
        break;
    case ENT_JUMPING_SPIDER:
        level->jumping_spiders[index].x = x;
        break;
    case ENT_BIRD:
        level->birds[index].x = x;
        level->birds[index].base_y = y;
        break;
    case ENT_FASTER_BIRD:
        level->faster_birds[index].x = x;
        level->faster_birds[index].base_y = y;
        break;
    case ENT_FISH:
        level->fish[index].x = x;
        break;
    case ENT_FASTER_FISH:
        level->faster_fish[index].x = x;
        break;
    case ENT_AXE_TRAP:
        level->axe_traps[index].pillar_x = x;
        level->axe_traps[index].y = y;
        break;
    case ENT_CIRCULAR_SAW:
        level->circular_saws[index].x = x;
        level->circular_saws[index].y = y;
        break;
    case ENT_SPIKE_ROW:
        level->spike_rows[index].x = x;
        break;
    case ENT_SPIKE_PLATFORM:
        level->spike_platforms[index].x = x;
        level->spike_platforms[index].y = y;
        break;
    case ENT_SPIKE_BLOCK:
        /* spike blocks ride rails; moving them changes t_offset */
        break;
    case ENT_BLUE_FLAME:
        level->blue_flames[index].x = x;
        break;
    case ENT_FIRE_FLAME:
        level->fire_flames[index].x = x;
        break;
    case ENT_FLOAT_PLATFORM:
        level->float_platforms[index].x = x;
        level->float_platforms[index].y = y;
        break;
    case ENT_BRIDGE:
        level->bridges[index].x = x;
        level->bridges[index].y = y;
        break;
    case ENT_BOUNCEPAD_SMALL:
        level->bouncepads_small[index].x = x;
        break;
    case ENT_BOUNCEPAD_MEDIUM:
        level->bouncepads_medium[index].x = x;
        break;
    case ENT_BOUNCEPAD_HIGH:
        level->bouncepads_high[index].x = x;
        break;
    case ENT_VINE:
        level->vines[index].x = x;
        level->vines[index].y = y;
        break;
    case ENT_LADDER:
        level->ladders[index].x = x;
        level->ladders[index].y = y;
        break;
    case ENT_ROPE:
        level->ropes[index].x = x;
        level->ropes[index].y = y;
        break;
    default:
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Utility: get entity count and capacity by type                      */
/* ------------------------------------------------------------------ */

/*
 * get_count --- Return the current number of entities of the given type.
 */
static int get_count(const LevelDef *level, EntityType type)
{
    switch (type) {
    case ENT_PLATFORM:         return level->platform_count;
    case ENT_SEA_GAP:          return level->sea_gap_count;
    case ENT_RAIL:             return level->rail_count;
    case ENT_COIN:             return level->coin_count;
    case ENT_STAR_YELLOW:      return level->star_yellow_count;
    case ENT_STAR_GREEN:       return level->star_green_count;
    case ENT_STAR_RED:         return level->star_red_count;
    case ENT_LAST_STAR:        return 1; /* always exactly one */
    case ENT_PLAYER_SPAWN:     return 1; /* always exactly one */
    case ENT_SPIDER:           return level->spider_count;
    case ENT_JUMPING_SPIDER:   return level->jumping_spider_count;
    case ENT_BIRD:             return level->bird_count;
    case ENT_FASTER_BIRD:      return level->faster_bird_count;
    case ENT_FISH:             return level->fish_count;
    case ENT_FASTER_FISH:      return level->faster_fish_count;
    case ENT_AXE_TRAP:         return level->axe_trap_count;
    case ENT_CIRCULAR_SAW:     return level->circular_saw_count;
    case ENT_SPIKE_ROW:        return level->spike_row_count;
    case ENT_SPIKE_PLATFORM:   return level->spike_platform_count;
    case ENT_SPIKE_BLOCK:      return level->spike_block_count;
    case ENT_BLUE_FLAME:       return level->blue_flame_count;
    case ENT_FIRE_FLAME:       return level->fire_flame_count;
    case ENT_FLOAT_PLATFORM:   return level->float_platform_count;
    case ENT_BRIDGE:           return level->bridge_count;
    case ENT_BOUNCEPAD_SMALL:  return level->bouncepad_small_count;
    case ENT_BOUNCEPAD_MEDIUM: return level->bouncepad_medium_count;
    case ENT_BOUNCEPAD_HIGH:   return level->bouncepad_high_count;
    case ENT_VINE:             return level->vine_count;
    case ENT_LADDER:           return level->ladder_count;
    case ENT_ROPE:             return level->rope_count;
    default:                   return 0;
    }
}

/*
 * get_max_count --- Return the MAX_* capacity for the given entity type.
 *
 * Used by TOOL_PLACE to check whether there is room for a new entity
 * before inserting into the fixed-size array.
 */
static int get_max_count(EntityType type)
{
    switch (type) {
    case ENT_PLATFORM:         return MAX_PLATFORMS;
    case ENT_SEA_GAP:          return MAX_SEA_GAPS;
    case ENT_RAIL:             return MAX_RAILS;
    case ENT_COIN:             return MAX_COINS;
    case ENT_STAR_YELLOW:      return MAX_STAR_YELLOWS;
    case ENT_STAR_GREEN:       return MAX_STAR_YELLOWS;
    case ENT_STAR_RED:         return MAX_STAR_YELLOWS;
    case ENT_LAST_STAR:        return 1;
    case ENT_PLAYER_SPAWN:     return 1;
    case ENT_SPIDER:           return MAX_SPIDERS;
    case ENT_JUMPING_SPIDER:   return MAX_JUMPING_SPIDERS;
    case ENT_BIRD:             return MAX_BIRDS;
    case ENT_FASTER_BIRD:      return MAX_FASTER_BIRDS;
    case ENT_FISH:             return MAX_FISH;
    case ENT_FASTER_FISH:      return MAX_FASTER_FISH;
    case ENT_AXE_TRAP:         return MAX_AXE_TRAPS;
    case ENT_CIRCULAR_SAW:     return MAX_CIRCULAR_SAWS;
    case ENT_SPIKE_ROW:        return MAX_SPIKE_ROWS;
    case ENT_SPIKE_PLATFORM:   return MAX_SPIKE_PLATFORMS;
    case ENT_SPIKE_BLOCK:      return MAX_SPIKE_BLOCKS;
    case ENT_BLUE_FLAME:       return MAX_BLUE_FLAMES;
    case ENT_FIRE_FLAME:       return MAX_BLUE_FLAMES;
    case ENT_FLOAT_PLATFORM:   return MAX_FLOAT_PLATFORMS;
    case ENT_BRIDGE:           return MAX_BRIDGES;
    case ENT_BOUNCEPAD_SMALL:  return MAX_BOUNCEPADS_SMALL;
    case ENT_BOUNCEPAD_MEDIUM: return MAX_BOUNCEPADS_MEDIUM;
    case ENT_BOUNCEPAD_HIGH:   return MAX_BOUNCEPADS_HIGH;
    case ENT_VINE:             return MAX_VINES;
    case ENT_LADDER:           return MAX_LADDERS;
    case ENT_ROPE:             return MAX_ROPES;
    default:                   return 0;
    }
}

/* ------------------------------------------------------------------ */
/* Utility: snapshot entity data into PlacementData for undo           */
/* ------------------------------------------------------------------ */

/*
 * snapshot_entity --- Copy one entity's placement data into a PlacementData union.
 *
 * The entity_type field in the Command selects which union member is active.
 * The caller must ensure index is valid before calling.
 */
static PlacementData snapshot_entity(const LevelDef *level, EntityType type,
                                     int index)
{
    PlacementData pd;
    memset(&pd, 0, sizeof(pd));

    switch (type) {
    case ENT_PLATFORM:
        pd.platform = level->platforms[index];
        break;
    case ENT_SEA_GAP:
        pd.sea_gap = level->sea_gaps[index];
        break;
    case ENT_RAIL:
        pd.rail = level->rails[index];
        break;
    case ENT_COIN:
        pd.coin = level->coins[index];
        break;
    case ENT_STAR_YELLOW:
        pd.star_yellow = level->star_yellows[index];
        break;
    case ENT_STAR_GREEN:
        pd.star_green = level->star_greens[index];
        break;
    case ENT_STAR_RED:
        pd.star_red = level->star_reds[index];
        break;
    case ENT_LAST_STAR:
        pd.last_star = level->last_star;
        break;
    case ENT_PLAYER_SPAWN: {
        /*
         * Reuse the last_star union member for player spawn data.
         * Both are simple {float x, float y} structs.
         */
        LastStarPlacement psp = { level->player_start_x,
                                  level->player_start_y };
        pd.last_star = psp;
        break;
    }
    case ENT_SPIDER:
        pd.spider = level->spiders[index];
        break;
    case ENT_JUMPING_SPIDER:
        pd.jumping_spider = level->jumping_spiders[index];
        break;
    case ENT_BIRD:
        pd.bird = level->birds[index];
        break;
    case ENT_FASTER_BIRD:
        pd.bird = level->faster_birds[index];
        break;
    case ENT_FISH:
        pd.fish = level->fish[index];
        break;
    case ENT_FASTER_FISH:
        pd.fish = level->faster_fish[index];
        break;
    case ENT_AXE_TRAP:
        pd.axe_trap = level->axe_traps[index];
        break;
    case ENT_CIRCULAR_SAW:
        pd.circular_saw = level->circular_saws[index];
        break;
    case ENT_SPIKE_ROW:
        pd.spike_row = level->spike_rows[index];
        break;
    case ENT_SPIKE_PLATFORM:
        pd.spike_platform = level->spike_platforms[index];
        break;
    case ENT_SPIKE_BLOCK:
        pd.spike_block = level->spike_blocks[index];
        break;
    case ENT_BLUE_FLAME:
        pd.blue_flame = level->blue_flames[index];
        break;
    case ENT_FIRE_FLAME:
        pd.fire_flame = level->fire_flames[index];
        break;
    case ENT_FLOAT_PLATFORM:
        pd.float_platform = level->float_platforms[index];
        break;
    case ENT_BRIDGE:
        pd.bridge = level->bridges[index];
        break;
    case ENT_BOUNCEPAD_SMALL:
        pd.bouncepad = level->bouncepads_small[index];
        break;
    case ENT_BOUNCEPAD_MEDIUM:
        pd.bouncepad = level->bouncepads_medium[index];
        break;
    case ENT_BOUNCEPAD_HIGH:
        pd.bouncepad = level->bouncepads_high[index];
        break;
    case ENT_VINE:
        pd.vine = level->vines[index];
        break;
    case ENT_LADDER:
        pd.ladder = level->ladders[index];
        break;
    case ENT_ROPE:
        pd.rope = level->ropes[index];
        break;
    default:
        break;
    }
    return pd;
}

/* ------------------------------------------------------------------ */
/* Hit-test: find the topmost entity under a world-space point         */
/* ------------------------------------------------------------------ */

/*
 * hit_test --- Return the frontmost entity whose bounding box contains (wx, wy).
 *
 * Tests all 25 entity types in reverse render order (enemies and hazards
 * first, world geometry last) so that visually topmost entities are
 * selected first when multiple overlap.
 *
 * Returns a Selection with {type, index} of the first hit, or
 * {type=0, index=-1} if nothing is under the cursor.
 */
static Selection hit_test(const LevelDef *level, float wx, float wy)
{
    Selection sel = { 0, -1 };
    float ex, ey;
    int ew, eh;

    /*
     * Test in reverse render order: enemies / hazards / collectibles first
     * (drawn last = on top), then surfaces, then world geometry.
     */

    /* ---- Enemies ---------------------------------------------------- */

    /* Spiders — ground patrol, sit at FLOOR_Y - SPIDER_ART_H */
    for (int i = level->spider_count - 1; i >= 0; i--) {
        ex = level->spiders[i].x;
        ey = (float)(FLOOR_Y - SPIDER_ART_H);
        ew = SPIDER_FRAME_W;
        eh = SPIDER_ART_H;
        if (wx >= ex && wx < ex + ew && wy >= ey && wy < ey + eh) {
            sel.type = ENT_SPIDER;
            sel.index = i;
            return sel;
        }
    }

    /* Jumping spiders — same dimensions as spiders */
    for (int i = level->jumping_spider_count - 1; i >= 0; i--) {
        ex = level->jumping_spiders[i].x;
        ey = (float)(FLOOR_Y - SPIDER_ART_H);
        ew = SPIDER_FRAME_W;
        eh = SPIDER_ART_H;
        if (wx >= ex && wx < ex + ew && wy >= ey && wy < ey + eh) {
            sel.type = ENT_JUMPING_SPIDER;
            sel.index = i;
            return sel;
        }
    }

    /* Birds — sine-wave patrol, y = base_y */
    for (int i = level->bird_count - 1; i >= 0; i--) {
        ex = level->birds[i].x;
        ey = level->birds[i].base_y;
        ew = BIRD_FRAME_W;
        eh = BIRD_ART_H;
        if (wx >= ex && wx < ex + ew && wy >= ey && wy < ey + eh) {
            sel.type = ENT_BIRD;
            sel.index = i;
            return sel;
        }
    }

    /* Faster birds — same dimensions as birds */
    for (int i = level->faster_bird_count - 1; i >= 0; i--) {
        ex = level->faster_birds[i].x;
        ey = level->faster_birds[i].base_y;
        ew = BIRD_FRAME_W;
        eh = BIRD_ART_H;
        if (wx >= ex && wx < ex + ew && wy >= ey && wy < ey + eh) {
            sel.type = ENT_FASTER_BIRD;
            sel.index = i;
            return sel;
        }
    }

    /* Fish — water lane, y derived from water strip position */
    for (int i = level->fish_count - 1; i >= 0; i--) {
        ex = level->fish[i].x;
        ey = (float)(GAME_H - WATER_ART_H) - FISH_FRAME_H / 2.0f;
        ew = FISH_FRAME_W;
        eh = FISH_FRAME_H;
        if (wx >= ex && wx < ex + ew && wy >= ey && wy < ey + eh) {
            sel.type = ENT_FISH;
            sel.index = i;
            return sel;
        }
    }

    /* Faster fish — same dimensions and Y as fish */
    for (int i = level->faster_fish_count - 1; i >= 0; i--) {
        ex = level->faster_fish[i].x;
        ey = (float)(GAME_H - WATER_ART_H) - FISH_FRAME_H / 2.0f;
        ew = FISH_FRAME_W;
        eh = FISH_FRAME_H;
        if (wx >= ex && wx < ex + ew && wy >= ey && wy < ey + eh) {
            sel.type = ENT_FASTER_FISH;
            sel.index = i;
            return sel;
        }
    }

    /* ---- Hazards ---------------------------------------------------- */

    /* Axe traps — centred on host pillar, y from pillar height */
    for (int i = level->axe_trap_count - 1; i >= 0; i--) {
        const AxeTrapPlacement *at = &level->axe_traps[i];
        ex = at->pillar_x + (float)(TILE_SIZE / 2 - AXE_FRAME_W / 2);
        ey = (float)(FLOOR_Y - 3 * TILE_SIZE + 16);
        ew = AXE_FRAME_W;
        eh = AXE_FRAME_H;
        if (wx >= ex && wx < ex + ew && wy >= ey && wy < ey + eh) {
            sel.type = ENT_AXE_TRAP;
            sel.index = i;
            return sel;
        }
    }

    /* Circular saws — horizontal patrol at bridge height */
    for (int i = level->circular_saw_count - 1; i >= 0; i--) {
        ex = level->circular_saws[i].x;
        ey = (float)(FLOOR_Y - 2 * TILE_SIZE + 16 - SAW_DISPLAY_H);
        ew = SAW_DISPLAY_W;
        eh = SAW_DISPLAY_H;
        if (wx >= ex && wx < ex + ew && wy >= ey && wy < ey + eh) {
            sel.type = ENT_CIRCULAR_SAW;
            sel.index = i;
            return sel;
        }
    }

    /* Spike rows — ground-level spikes */
    for (int i = level->spike_row_count - 1; i >= 0; i--) {
        const SpikeRowPlacement *sr = &level->spike_rows[i];
        ex = sr->x;
        ey = (float)(FLOOR_Y - SPIKE_TILE_H);
        ew = sr->count * SPIKE_TILE_W;
        eh = SPIKE_TILE_H;
        if (wx >= ex && wx < ex + ew && wy >= ey && wy < ey + eh) {
            sel.type = ENT_SPIKE_ROW;
            sel.index = i;
            return sel;
        }
    }

    /* Spike platforms — elevated spike strips */
    for (int i = level->spike_platform_count - 1; i >= 0; i--) {
        const SpikePlatformPlacement *sp = &level->spike_platforms[i];
        ex = sp->x;
        ey = sp->y;
        ew = sp->tile_count * SPIKE_PLAT_PIECE_W;
        eh = SPIKE_PLAT_SRC_H;
        if (wx >= ex && wx < ex + ew && wy >= ey && wy < ey + eh) {
            sel.type = ENT_SPIKE_PLATFORM;
            sel.index = i;
            return sel;
        }
    }

    /* Spike blocks — rail-riding hazards, position from rail data */
    for (int i = level->spike_block_count - 1; i >= 0; i--) {
        const SpikeBlockPlacement *sb = &level->spike_blocks[i];
        int ri = sb->rail_index;
        if (ri < 0 || ri >= level->rail_count) continue;
        const RailPlacement *rp = &level->rails[ri];
        if (rp->layout == RAIL_LAYOUT_RECT) {
            ex = (float)rp->x;
            ey = (float)rp->y;
        } else {
            ex = (float)rp->x + sb->t_offset * (float)RAIL_TILE_W;
            ey = (float)rp->y;
        }
        ew = SPIKE_DISPLAY_W;
        eh = SPIKE_DISPLAY_H;
        if (wx >= ex && wx < ex + ew && wy >= ey && wy < ey + eh) {
            sel.type = ENT_SPIKE_BLOCK;
            sel.index = i;
            return sel;
        }
    }

    /* Blue flames — erupting fire hazards, preview centred in sea gap */
    for (int i = level->blue_flame_count - 1; i >= 0; i--) {
        float gap_x = level->blue_flames[i].x;
        ex = gap_x + (float)(SEA_GAP_W - BLUE_FLAME_W) / 2.0f;
        ey = (float)(FLOOR_Y - BLUE_FLAME_H);
        ew = BLUE_FLAME_W;
        eh = BLUE_FLAME_H;
        if (wx >= ex && wx < ex + ew && wy >= ey && wy < ey + eh) {
            sel.type = ENT_BLUE_FLAME;
            sel.index = i;
            return sel;
        }
    }

    /* Fire flames — erupting fire hazards (fire variant), same layout */
    for (int i = level->fire_flame_count - 1; i >= 0; i--) {
        float gap_x = level->fire_flames[i].x;
        ex = gap_x + (float)(SEA_GAP_W - FIRE_FLAME_W) / 2.0f;
        ey = (float)(FLOOR_Y - FIRE_FLAME_H);
        ew = FIRE_FLAME_W;
        eh = FIRE_FLAME_H;
        if (wx >= ex && wx < ex + ew && wy >= ey && wy < ey + eh) {
            sel.type = ENT_FIRE_FLAME;
            sel.index = i;
            return sel;
        }
    }

    /* ---- Collectibles ----------------------------------------------- */

    /* Coins — small 16x16 pickups */
    for (int i = level->coin_count - 1; i >= 0; i--) {
        ex = level->coins[i].x;
        ey = level->coins[i].y;
        ew = COIN_DISPLAY_W;
        eh = COIN_DISPLAY_H;
        if (wx >= ex && wx < ex + ew && wy >= ey && wy < ey + eh) {
            sel.type = ENT_COIN;
            sel.index = i;
            return sel;
        }
    }

    /* Star yellows — 16x16 health pickups */
    for (int i = level->star_yellow_count - 1; i >= 0; i--) {
        ex = level->star_yellows[i].x;
        ey = level->star_yellows[i].y;
        ew = YSTAR_DISPLAY_W;
        eh = YSTAR_DISPLAY_H;
        if (wx >= ex && wx < ex + ew && wy >= ey && wy < ey + eh) {
            sel.type = ENT_STAR_YELLOW;
            sel.index = i;
            return sel;
        }
    }

    /* Star greens — 16x16 health pickups */
    for (int i = level->star_green_count - 1; i >= 0; i--) {
        ex = level->star_greens[i].x;
        ey = level->star_greens[i].y;
        ew = YSTAR_DISPLAY_W;
        eh = YSTAR_DISPLAY_H;
        if (wx >= ex && wx < ex + ew && wy >= ey && wy < ey + eh) {
            sel.type = ENT_STAR_GREEN;
            sel.index = i;
            return sel;
        }
    }

    /* Star reds — 16x16 health pickups */
    for (int i = level->star_red_count - 1; i >= 0; i--) {
        ex = level->star_reds[i].x;
        ey = level->star_reds[i].y;
        ew = YSTAR_DISPLAY_W;
        eh = YSTAR_DISPLAY_H;
        if (wx >= ex && wx < ex + ew && wy >= ey && wy < ey + eh) {
            sel.type = ENT_STAR_RED;
            sel.index = i;
            return sel;
        }
    }

    /* Last star — single 24x24 end-of-level star */
    {
        ex = level->last_star.x;
        ey = level->last_star.y;
        ew = LSTAR_DISPLAY_W;
        eh = LSTAR_DISPLAY_H;
        if (wx >= ex && wx < ex + ew && wy >= ey && wy < ey + eh) {
            sel.type = ENT_LAST_STAR;
            sel.index = 0;
            return sel;
        }
    }

    /* Player spawn — single 48x48 idle frame at spawn position */
    {
        ex = level->player_start_x;
        ey = level->player_start_y;
        ew = PLAYER_SPAWN_W;
        eh = PLAYER_SPAWN_H;
        if (wx >= ex && wx < ex + ew && wy >= ey && wy < ey + eh) {
            sel.type = ENT_PLAYER_SPAWN;
            sel.index = 0;
            return sel;
        }
    }

    /* ---- Surfaces --------------------------------------------------- */

    /* Bouncepads — all three variants sit at FLOOR_Y - BP_SRC_H */
    for (int i = level->bouncepad_small_count - 1; i >= 0; i--) {
        ex = level->bouncepads_small[i].x;
        ey = (float)(FLOOR_Y - BP_SRC_H);
        ew = BP_FRAME_W;
        eh = BP_SRC_H;
        if (wx >= ex && wx < ex + ew && wy >= ey && wy < ey + eh) {
            sel.type = ENT_BOUNCEPAD_SMALL;
            sel.index = i;
            return sel;
        }
    }

    for (int i = level->bouncepad_medium_count - 1; i >= 0; i--) {
        ex = level->bouncepads_medium[i].x;
        ey = (float)(FLOOR_Y - BP_SRC_H);
        ew = BP_FRAME_W;
        eh = BP_SRC_H;
        if (wx >= ex && wx < ex + ew && wy >= ey && wy < ey + eh) {
            sel.type = ENT_BOUNCEPAD_MEDIUM;
            sel.index = i;
            return sel;
        }
    }

    for (int i = level->bouncepad_high_count - 1; i >= 0; i--) {
        ex = level->bouncepads_high[i].x;
        ey = (float)(FLOOR_Y - BP_SRC_H);
        ew = BP_FRAME_W;
        eh = BP_SRC_H;
        if (wx >= ex && wx < ex + ew && wy >= ey && wy < ey + eh) {
            sel.type = ENT_BOUNCEPAD_HIGH;
            sel.index = i;
            return sel;
        }
    }

    /* Float platforms — hovering / crumble / rail platforms */
    for (int i = level->float_platform_count - 1; i >= 0; i--) {
        const FloatPlatformPlacement *fp = &level->float_platforms[i];
        ex = fp->x;
        ey = fp->y;
        ew = fp->tile_count * FPLAT_PIECE_W;
        eh = FPLAT_PIECE_H;
        if (wx >= ex && wx < ex + ew && wy >= ey && wy < ey + eh) {
            sel.type = ENT_FLOAT_PLATFORM;
            sel.index = i;
            return sel;
        }
    }

    /* Bridges — tiled crumble walkways */
    for (int i = level->bridge_count - 1; i >= 0; i--) {
        const BridgePlacement *br = &level->bridges[i];
        ex = br->x;
        ey = br->y;
        ew = br->brick_count * BRIDGE_TILE_W;
        eh = BRIDGE_TILE_H;
        if (wx >= ex && wx < ex + ew && wy >= ey && wy < ey + eh) {
            sel.type = ENT_BRIDGE;
            sel.index = i;
            return sel;
        }
    }

    /* Vines — hanging climbable decoration */
    for (int i = level->vine_count - 1; i >= 0; i--) {
        const VinePlacement *v = &level->vines[i];
        ex = v->x;
        ey = v->y;
        ew = VINE_W;
        eh = (v->tile_count - 1) * VINE_STEP + VINE_H;
        if (wx >= ex && wx < ex + ew && wy >= ey && wy < ey + eh) {
            sel.type = ENT_VINE;
            sel.index = i;
            return sel;
        }
    }

    /* Ladders — climbable stacked tiles */
    for (int i = level->ladder_count - 1; i >= 0; i--) {
        const LadderPlacement *ld = &level->ladders[i];
        ex = ld->x;
        ey = ld->y;
        ew = LADDER_W;
        eh = (ld->tile_count - 1) * LADDER_STEP + LADDER_H;
        if (wx >= ex && wx < ex + ew && wy >= ey && wy < ey + eh) {
            sel.type = ENT_LADDER;
            sel.index = i;
            return sel;
        }
    }

    /* Ropes — climbable stacked tiles */
    for (int i = level->rope_count - 1; i >= 0; i--) {
        const RopePlacement *rp = &level->ropes[i];
        ex = rp->x;
        ey = rp->y;
        ew = ROPE_W;
        eh = (rp->tile_count - 1) * ROPE_STEP + ROPE_H;
        if (wx >= ex && wx < ex + ew && wy >= ey && wy < ey + eh) {
            sel.type = ENT_ROPE;
            sel.index = i;
            return sel;
        }
    }

    /* Rails — 16-px tile paths forming loops or lines */
    for (int i = level->rail_count - 1; i >= 0; i--) {
        const RailPlacement *r = &level->rails[i];
        ex = (float)r->x;
        ey = (float)r->y;
        ew = r->w * RAIL_TILE_W;
        eh = (r->layout == RAIL_LAYOUT_RECT) ? r->h * RAIL_TILE_H : RAIL_TILE_H;
        if (wx >= ex && wx < ex + ew && wy >= ey && wy < ey + eh) {
            sel.type = ENT_RAIL;
            sel.index = i;
            return sel;
        }
    }

    /* ---- World geometry (lowest priority) --------------------------- */

    /* Platforms — ground pillars */
    for (int i = level->platform_count - 1; i >= 0; i--) {
        const PlatformPlacement *p = &level->platforms[i];
        int ptw = (p->tile_width > 0) ? p->tile_width : 1;
        ex = p->x;
        ey = (float)(FLOOR_Y - p->tile_height * TILE_SIZE + 16);
        ew = ptw * TILE_SIZE;
        eh = p->tile_height * TILE_SIZE;
        if (wx >= ex && wx < ex + ew && wy >= ey && wy < ey + eh) {
            sel.type = ENT_PLATFORM;
            sel.index = i;
            return sel;
        }
    }

    /* Sea gaps — holes in the floor */
    for (int i = level->sea_gap_count - 1; i >= 0; i--) {
        ex = (float)level->sea_gaps[i];
        ey = (float)FLOOR_Y;
        ew = SEA_GAP_W;
        eh = GAME_H - FLOOR_Y;
        if (wx >= ex && wx < ex + ew && wy >= ey && wy < ey + eh) {
            sel.type = ENT_SEA_GAP;
            sel.index = i;
            return sel;
        }
    }

    return sel;  /* no hit — index stays -1 */
}

/* ------------------------------------------------------------------ */
/* Internal: delete an entity by type and index                        */
/* ------------------------------------------------------------------ */

/*
 * delete_entity --- Remove one entity from the level and push undo.
 *
 * Snapshots the entity data before removal, compacts the array with
 * memmove, decrements the count, pushes a CMD_DELETE command to the
 * undo stack, and sets the modified flag.
 */
static void delete_entity(EditorState *es, EntityType type, int index)
{
    LevelDef *level = &es->level;

    /* Snapshot entity data before deletion for undo */
    PlacementData before = snapshot_entity(level, type, index);

    /*
     * Remove the entity from its array by shifting trailing elements
     * left by one slot.  Each entity type has its own array and count.
     */
    switch (type) {
    case ENT_PLATFORM:
        array_remove(level->platforms, &level->platform_count,
                     index, sizeof(PlatformPlacement));
        break;
    case ENT_SEA_GAP:
        array_remove(level->sea_gaps, &level->sea_gap_count,
                     index, sizeof(int));
        break;
    case ENT_RAIL:
        array_remove(level->rails, &level->rail_count,
                     index, sizeof(RailPlacement));
        break;
    case ENT_COIN:
        array_remove(level->coins, &level->coin_count,
                     index, sizeof(CoinPlacement));
        break;
    case ENT_STAR_YELLOW:
        array_remove(level->star_yellows, &level->star_yellow_count,
                     index, sizeof(StarYellowPlacement));
        break;
    case ENT_STAR_GREEN:
        array_remove(level->star_greens, &level->star_green_count,
                     index, sizeof(StarGreenPlacement));
        break;
    case ENT_STAR_RED:
        array_remove(level->star_reds, &level->star_red_count,
                     index, sizeof(StarRedPlacement));
        break;
    case ENT_LAST_STAR:
        /*
         * LastStar is a single entity, not an array.  "Deleting" it
         * resets its position to (0, 0) as a sentinel.  There is always
         * exactly one last star in a level.
         */
        level->last_star.x = 0.0f;
        level->last_star.y = 0.0f;
        break;
    case ENT_PLAYER_SPAWN:
        /*
         * Player spawn is a single position.  "Deleting" it resets to
         * (0, 0) as a sentinel, same pattern as last_star.
         */
        level->player_start_x = 0.0f;
        level->player_start_y = 0.0f;
        break;
    case ENT_SPIDER:
        array_remove(level->spiders, &level->spider_count,
                     index, sizeof(SpiderPlacement));
        break;
    case ENT_JUMPING_SPIDER:
        array_remove(level->jumping_spiders, &level->jumping_spider_count,
                     index, sizeof(JumpingSpiderPlacement));
        break;
    case ENT_BIRD:
        array_remove(level->birds, &level->bird_count,
                     index, sizeof(BirdPlacement));
        break;
    case ENT_FASTER_BIRD:
        array_remove(level->faster_birds, &level->faster_bird_count,
                     index, sizeof(BirdPlacement));
        break;
    case ENT_FISH:
        array_remove(level->fish, &level->fish_count,
                     index, sizeof(FishPlacement));
        break;
    case ENT_FASTER_FISH:
        array_remove(level->faster_fish, &level->faster_fish_count,
                     index, sizeof(FishPlacement));
        break;
    case ENT_AXE_TRAP:
        array_remove(level->axe_traps, &level->axe_trap_count,
                     index, sizeof(AxeTrapPlacement));
        break;
    case ENT_CIRCULAR_SAW:
        array_remove(level->circular_saws, &level->circular_saw_count,
                     index, sizeof(CircularSawPlacement));
        break;
    case ENT_SPIKE_ROW:
        array_remove(level->spike_rows, &level->spike_row_count,
                     index, sizeof(SpikeRowPlacement));
        break;
    case ENT_SPIKE_PLATFORM:
        array_remove(level->spike_platforms, &level->spike_platform_count,
                     index, sizeof(SpikePlatformPlacement));
        break;
    case ENT_SPIKE_BLOCK:
        array_remove(level->spike_blocks, &level->spike_block_count,
                     index, sizeof(SpikeBlockPlacement));
        break;
    case ENT_BLUE_FLAME:
        array_remove(level->blue_flames, &level->blue_flame_count,
                     index, sizeof(BlueFlamePlacement));
        break;
    case ENT_FIRE_FLAME:
        array_remove(level->fire_flames, &level->fire_flame_count,
                     index, sizeof(FireFlamePlacement));
        break;
    case ENT_FLOAT_PLATFORM:
        array_remove(level->float_platforms, &level->float_platform_count,
                     index, sizeof(FloatPlatformPlacement));
        break;
    case ENT_BRIDGE:
        array_remove(level->bridges, &level->bridge_count,
                     index, sizeof(BridgePlacement));
        break;
    case ENT_BOUNCEPAD_SMALL:
        array_remove(level->bouncepads_small, &level->bouncepad_small_count,
                     index, sizeof(BouncepadPlacement));
        break;
    case ENT_BOUNCEPAD_MEDIUM:
        array_remove(level->bouncepads_medium, &level->bouncepad_medium_count,
                     index, sizeof(BouncepadPlacement));
        break;
    case ENT_BOUNCEPAD_HIGH:
        array_remove(level->bouncepads_high, &level->bouncepad_high_count,
                     index, sizeof(BouncepadPlacement));
        break;
    case ENT_VINE:
        array_remove(level->vines, &level->vine_count,
                     index, sizeof(VinePlacement));
        break;
    case ENT_LADDER:
        array_remove(level->ladders, &level->ladder_count,
                     index, sizeof(LadderPlacement));
        break;
    case ENT_ROPE:
        array_remove(level->ropes, &level->rope_count,
                     index, sizeof(RopePlacement));
        break;
    default:
        return;
    }

    /* Push undo command — CMD_DELETE stores "before" so undo re-inserts */
    Command cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.type         = CMD_DELETE;
    cmd.entity_type  = (int)type;
    cmd.entity_index = index;
    cmd.before       = before;
    undo_push(es->undo, cmd);

    es->modified = 1;
}

/* ------------------------------------------------------------------ */
/* TOOL_PLACE: create a new entity with sensible defaults              */
/* ------------------------------------------------------------------ */

/*
 * place_entity --- Add one entity of the given type at (world_x, world_y).
 *
 * Checks capacity, fills in default values appropriate for the entity
 * type, increments the array count, and pushes a CMD_PLACE undo command.
 */
static void place_entity(EditorState *es, float world_x, float world_y)
{
    LevelDef *level = &es->level;
    EntityType type  = es->palette_type;

    /* Check capacity — every entity type has a fixed-size array */
    int count = get_count(level, type);
    int max   = get_max_count(type);
    if (count >= max) {
        fprintf(stderr, "Warning: cannot place more — %d/%d capacity reached\n",
                count, max);
        return;
    }

    /*
     * Populate the new entry with sensible defaults.
     * Each entity type needs different fields; the switch below fills
     * in starting position, velocity, patrol bounds, and mode flags.
     */
    int new_index = count;

    switch (type) {
    case ENT_SPIDER: {
        SpiderPlacement sp = {
            .x           = world_x,
            .vx          = 50.0f,
            .patrol_x0   = world_x - 50.0f,
            .patrol_x1   = world_x + 50.0f,
            .frame_index = 0
        };
        level->spiders[new_index] = sp;
        level->spider_count++;
        break;
    }
    case ENT_JUMPING_SPIDER: {
        JumpingSpiderPlacement jsp = {
            .x         = world_x,
            .vx        = 55.0f,
            .patrol_x0 = world_x - 50.0f,
            .patrol_x1 = world_x + 50.0f
        };
        level->jumping_spiders[new_index] = jsp;
        level->jumping_spider_count++;
        break;
    }
    case ENT_BIRD: {
        BirdPlacement bp = {
            .x           = world_x,
            .base_y      = world_y,
            .vx          = 45.0f,
            .patrol_x0   = world_x - 80.0f,
            .patrol_x1   = world_x + 80.0f,
            .frame_index = 0
        };
        level->birds[new_index] = bp;
        level->bird_count++;
        break;
    }
    case ENT_FASTER_BIRD: {
        BirdPlacement fbp = {
            .x           = world_x,
            .base_y      = world_y,
            .vx          = 80.0f,
            .patrol_x0   = world_x - 80.0f,
            .patrol_x1   = world_x + 80.0f,
            .frame_index = 0
        };
        level->faster_birds[new_index] = fbp;
        level->faster_bird_count++;
        break;
    }
    case ENT_FISH: {
        FishPlacement fp = {
            .x         = world_x,
            .vx        = 70.0f,
            .patrol_x0 = world_x - 60.0f,
            .patrol_x1 = world_x + 60.0f
        };
        level->fish[new_index] = fp;
        level->fish_count++;
        break;
    }
    case ENT_FASTER_FISH: {
        FishPlacement ffp = {
            .x         = world_x,
            .vx        = 120.0f,
            .patrol_x0 = world_x - 60.0f,
            .patrol_x1 = world_x + 60.0f
        };
        level->faster_fish[new_index] = ffp;
        level->faster_fish_count++;
        break;
    }
    case ENT_AXE_TRAP: {
        AxeTrapPlacement atp = {
            .pillar_x = world_x,
            .mode     = AXE_MODE_PENDULUM
        };
        level->axe_traps[new_index] = atp;
        level->axe_trap_count++;
        break;
    }
    case ENT_CIRCULAR_SAW: {
        CircularSawPlacement csp = {
            .x         = world_x,
            .patrol_x0 = world_x - 48.0f,
            .patrol_x1 = world_x + 48.0f,
            .direction = 1
        };
        level->circular_saws[new_index] = csp;
        level->circular_saw_count++;
        break;
    }
    case ENT_SPIKE_ROW: {
        SpikeRowPlacement srp = {
            .x     = world_x,
            .count = 3
        };
        level->spike_rows[new_index] = srp;
        level->spike_row_count++;
        break;
    }
    case ENT_SPIKE_PLATFORM: {
        SpikePlatformPlacement spp = {
            .x          = world_x,
            .y          = world_y,
            .tile_count = 3
        };
        level->spike_platforms[new_index] = spp;
        level->spike_platform_count++;
        break;
    }
    case ENT_SPIKE_BLOCK: {
        SpikeBlockPlacement sbp = {
            .rail_index = 0,
            .t_offset   = 0.0f,
            .speed      = 3.0f
        };
        level->spike_blocks[new_index] = sbp;
        level->spike_block_count++;
        break;
    }
    case ENT_BLUE_FLAME: {
        BlueFlamePlacement bfp = {
            .x = world_x
        };
        level->blue_flames[new_index] = bfp;
        level->blue_flame_count++;
        break;
    }
    case ENT_FIRE_FLAME: {
        FireFlamePlacement ffp = {
            .x = world_x
        };
        level->fire_flames[new_index] = ffp;
        level->fire_flame_count++;
        break;
    }
    case ENT_FLOAT_PLATFORM: {
        FloatPlatformPlacement fpp = {
            .mode       = FLOAT_PLATFORM_STATIC,
            .x          = world_x,
            .y          = world_y,
            .tile_count = 3,
            .rail_index = 0,
            .t_offset   = 0.0f,
            .speed      = 0.0f
        };
        level->float_platforms[new_index] = fpp;
        level->float_platform_count++;
        break;
    }
    case ENT_BRIDGE: {
        BridgePlacement brp = {
            .x           = world_x,
            .y           = world_y,
            .brick_count = 8
        };
        level->bridges[new_index] = brp;
        level->bridge_count++;
        break;
    }
    case ENT_BOUNCEPAD_SMALL: {
        BouncepadPlacement bps = {
            .x         = world_x,
            .launch_vy = -380.0f,
            .pad_type  = BOUNCEPAD_GREEN
        };
        level->bouncepads_small[new_index] = bps;
        level->bouncepad_small_count++;
        break;
    }
    case ENT_BOUNCEPAD_MEDIUM: {
        BouncepadPlacement bpm = {
            .x         = world_x,
            .launch_vy = -536.25f,
            .pad_type  = BOUNCEPAD_WOOD
        };
        level->bouncepads_medium[new_index] = bpm;
        level->bouncepad_medium_count++;
        break;
    }
    case ENT_BOUNCEPAD_HIGH: {
        BouncepadPlacement bph = {
            .x         = world_x,
            .launch_vy = -700.0f,
            .pad_type  = BOUNCEPAD_RED
        };
        level->bouncepads_high[new_index] = bph;
        level->bouncepad_high_count++;
        break;
    }
    case ENT_PLATFORM: {
        PlatformPlacement pp = {
            .x           = world_x,
            .tile_height = 2,
            .tile_width  = 1
        };
        level->platforms[new_index] = pp;
        level->platform_count++;
        break;
    }
    case ENT_VINE: {
        VinePlacement vp = {
            .x          = world_x,
            .y          = world_y,
            .tile_count = 3
        };
        level->vines[new_index] = vp;
        level->vine_count++;
        break;
    }
    case ENT_LADDER: {
        LadderPlacement lp = {
            .x          = world_x,
            .y          = world_y,
            .tile_count = 3
        };
        level->ladders[new_index] = lp;
        level->ladder_count++;
        break;
    }
    case ENT_ROPE: {
        RopePlacement rp = {
            .x          = world_x,
            .y          = world_y,
            .tile_count = 3
        };
        level->ropes[new_index] = rp;
        level->rope_count++;
        break;
    }
    case ENT_COIN: {
        CoinPlacement cp = {
            .x = world_x,
            .y = world_y
        };
        level->coins[new_index] = cp;
        level->coin_count++;
        break;
    }
    case ENT_STAR_YELLOW: {
        StarYellowPlacement ysp = {
            .x = world_x,
            .y = world_y
        };
        level->star_yellows[new_index] = ysp;
        level->star_yellow_count++;
        break;
    }
    case ENT_STAR_GREEN: {
        StarGreenPlacement gsp = {
            .x = world_x,
            .y = world_y
        };
        level->star_greens[new_index] = gsp;
        level->star_green_count++;
        break;
    }
    case ENT_STAR_RED: {
        StarRedPlacement rsp = {
            .x = world_x,
            .y = world_y
        };
        level->star_reds[new_index] = rsp;
        level->star_red_count++;
        break;
    }
    case ENT_LAST_STAR: {
        /*
         * LastStar is a single entity — "placing" it overwrites the
         * existing position rather than appending to an array.
         */
        level->last_star.x = world_x;
        level->last_star.y = world_y;
        break;
    }
    case ENT_PLAYER_SPAWN: {
        /*
         * Player spawn is a single position — "placing" it overwrites
         * the existing spawn point, same pattern as last_star.
         */
        level->player_start_x = world_x;
        level->player_start_y = world_y;
        break;
    }
    case ENT_SEA_GAP: {
        /*
         * Sea gaps snap to a 32-px grid (SEA_GAP_W) so they align
         * with the floor tile boundaries.
         */
        int snapped_x = ((int)world_x / SEA_GAP_W) * SEA_GAP_W;
        level->sea_gaps[new_index] = snapped_x;
        level->sea_gap_count++;
        break;
    }
    case ENT_RAIL: {
        RailPlacement rpl = {
            .layout  = RAIL_LAYOUT_RECT,
            .x       = (int)world_x,
            .y       = (int)world_y,
            .w       = 4,
            .h       = 4,
            .end_cap = 0
        };
        level->rails[new_index] = rpl;
        level->rail_count++;
        break;
    }
    default:
        return;  /* unknown type — do nothing */
    }

    /*
     * Push CMD_PLACE to undo stack.
     * "after" holds the newly placed entity data so redo can re-insert it.
     */
    PlacementData after = snapshot_entity(level, type, new_index);
    Command cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.type         = CMD_PLACE;
    cmd.entity_type  = (int)type;
    cmd.entity_index = new_index;
    cmd.after        = after;
    undo_push(es->undo, cmd);

    /* Select the newly placed entity for immediate inspection */
    es->selection.type  = type;
    es->selection.index = new_index;
    es->modified        = 1;
}

/* ================================================================== */
/* Public API                                                          */
/* ================================================================== */

/* ------------------------------------------------------------------ */
/* tools_mouse_down                                                    */
/* ------------------------------------------------------------------ */

/*
 * tools_mouse_down --- Dispatch a left-click to the active tool handler.
 *
 * TOOL_SELECT : hit-test and select/deselect, optionally start a drag.
 * TOOL_PLACE  : stamp a new entity at the click position.
 * TOOL_DELETE : hit-test and delete the clicked entity.
 */
void tools_mouse_down(EditorState *es, float world_x, float world_y)
{
    switch (es->tool) {

    case TOOL_SELECT: {
        Selection hit = hit_test(&es->level, world_x, world_y);
        if (hit.index >= 0) {
            /*
             * Hit an entity — select it and begin a drag so the user
             * can reposition it by holding and moving the mouse.
             */
            es->selection = hit;
            es->dragging  = 1;

            /* Record the entity's current position as drag start */
            get_entity_pos(&es->level, hit.type, hit.index,
                           &es->drag_start_x, &es->drag_start_y);
        } else {
            /* Clicked empty space — clear the current selection */
            es->selection.index = -1;
            es->dragging        = 0;
        }
        break;
    }

    case TOOL_PLACE:
        place_entity(es, world_x, world_y);
        break;

    case TOOL_DELETE: {
        Selection hit = hit_test(&es->level, world_x, world_y);
        if (hit.index >= 0) {
            delete_entity(es, hit.type, hit.index);
            /* Clear selection since the clicked entity is now gone */
            es->selection.index = -1;
        }
        break;
    }
    }
}

/* ------------------------------------------------------------------ */
/* tools_mouse_up                                                      */
/* ------------------------------------------------------------------ */

/*
 * tools_mouse_up --- End a drag operation and record the move for undo.
 *
 * Compares the entity's current position to drag_start_x/y.  If they
 * differ (the user actually moved it), a CMD_MOVE command is pushed
 * with "before" = start position and "after" = end position.
 */
void tools_mouse_up(EditorState *es, float world_x, float world_y)
{
    (void)world_x;
    (void)world_y;

    if (!es->dragging) return;
    es->dragging = 0;

    if (es->selection.index < 0) return;

    /* Read the entity's final position after the drag */
    float end_x, end_y;
    get_entity_pos(&es->level, es->selection.type, es->selection.index,
                   &end_x, &end_y);

    /*
     * Only push an undo command if the entity actually moved.
     * Comparing floats with a small epsilon avoids false positives
     * from floating-point rounding during drag updates.
     */
    float dx = end_x - es->drag_start_x;
    float dy = end_y - es->drag_start_y;
    if (dx * dx + dy * dy < 0.5f) return;  /* less than ~0.7 px — no real move */

    /* Snapshot both the before and after states for undo/redo */
    PlacementData after = snapshot_entity(&es->level, es->selection.type,
                                          es->selection.index);

    /*
     * Temporarily restore the entity to its drag-start position to
     * snapshot the "before" state, then put it back.
     */
    set_entity_pos(&es->level, es->selection.type, es->selection.index,
                   es->drag_start_x, es->drag_start_y);
    PlacementData before = snapshot_entity(&es->level, es->selection.type,
                                           es->selection.index);
    set_entity_pos(&es->level, es->selection.type, es->selection.index,
                   end_x, end_y);

    Command cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.type         = CMD_MOVE;
    cmd.entity_type  = (int)es->selection.type;
    cmd.entity_index = es->selection.index;
    cmd.before       = before;
    cmd.after        = after;
    undo_push(es->undo, cmd);

    es->modified = 1;
}

/* ------------------------------------------------------------------ */
/* tools_mouse_drag                                                    */
/* ------------------------------------------------------------------ */

/*
 * tools_mouse_drag --- Update entity position while dragging.
 *
 * Called on every mouse-motion event while the left button is held.
 * Moves the selected entity to the cursor's world position, optionally
 * snapping to a TILE_SIZE grid when Shift is held.  Clamps to world bounds.
 */
void tools_mouse_drag(EditorState *es, float world_x, float world_y)
{
    if (!es->dragging) return;
    if (es->selection.index < 0) return;

    float nx = world_x;
    float ny = world_y;

    /*
     * Shift-snap: when the Shift key is held, round the position to the
     * nearest TILE_SIZE (48 px) grid point.  This makes alignment easy
     * without needing to toggle a separate grid-snap mode.
     */
    SDL_Keymod mods = SDL_GetModState();
    if (mods & KMOD_SHIFT) {
        nx = (float)((int)(nx / TILE_SIZE) * TILE_SIZE);
        ny = (float)((int)(ny / TILE_SIZE) * TILE_SIZE);
    }

    /* Clamp to world bounds — keep entity within the level area */
    if (nx < 0.0f) nx = 0.0f;
    int ww = (es->level.screen_count > 0 ? es->level.screen_count : 4) * GAME_W;
    if (nx > (float)ww) nx = (float)ww;
    if (ny < 0.0f) ny = 0.0f;
    if (ny > (float)GAME_H) ny = (float)GAME_H;

    set_entity_pos(&es->level, es->selection.type, es->selection.index,
                   nx, ny);
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
    Selection hit = hit_test(&es->level, world_x, world_y);
    if (hit.index < 0) return;

    delete_entity(es, hit.type, hit.index);

    /* If the deleted entity was selected, clear the selection */
    if (es->selection.type == hit.type && es->selection.index == hit.index) {
        es->selection.index = -1;
    }
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
    if (es->selection.index < 0) return;

    delete_entity(es, es->selection.type, es->selection.index);
    es->selection.index = -1;
}
