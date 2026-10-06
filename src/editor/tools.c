/*
 * tools.c --- Implementation of the editor's interactive tools.
 *
 * Implements SELECT, PLACE, MOVE, and DELETE tool interactions for all
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
#include "editor_session.h" /* save-point dirty tracking */
#include "../levels/level_loader.h"

static int tools_can_hit_test(EditorState *es)
{
    char error[128];
    if (level_validate_runtime(&es->level, error, sizeof(error)) == 0) return 1;
    editor_set_status(es, "Correct properties or Undo: %s", error);
    return 0;
}
#include "entity_meta.h" /* Editor display dimensions and rail helpers     */
#include "undo.h"    /* Command, PlacementData, undo_push                 */
#include "../game.h" /* GAME_W, GAME_H, FLOOR_Y, TILE_SIZE, WORLD_W,
                        FLOOR_GAP_W, MAX_* constants                      */

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
    case ENT_FLOOR_GAP:
        *x = (float)level->floor_gaps[index];
        *y = (float)FLOOR_Y;
        break;
    case ENT_CHECKPOINT:
        *x = level->checkpoints[index].x;
        *y = level->checkpoints[index].y;
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
            editor_rail_placement_position_at(rp, sb->t_offset, x, y);
            *x -= (float)SPIKE_DISPLAY_W / 2.0f;
            *y -= (float)SPIKE_DISPLAY_H / 2.0f;
        } else {
            *x = 0.0f;
            *y = 0.0f;
        }
        break;
    }
    case ENT_BLUE_FLAME: {
        float gap_x = level->blue_flames[index].x;
        *x = gap_x + (float)(FLOOR_GAP_W - BLUE_FLAME_W) / 2.0f;
        *y = (float)(FLOOR_Y - BLUE_FLAME_H);
        break;
    }
    case ENT_FIRE_FLAME: {
        float gap_x = level->fire_flames[index].x;
        *x = gap_x + (float)(FLOOR_GAP_W - FIRE_FLAME_W) / 2.0f;
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
    case ENT_FLOOR_GAP:
        level->floor_gaps[index] = (int)x;
        break;
    case ENT_CHECKPOINT:
        level->checkpoints[index].x = x;
        level->checkpoints[index].y = y;
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
        editor_rail_placement_position_at(rp, sb->t_offset, &ex, &ey);
        ex -= (float)SPIKE_DISPLAY_W / 2.0f;
        ey -= (float)SPIKE_DISPLAY_H / 2.0f;
        ew = SPIKE_DISPLAY_W;
        eh = SPIKE_DISPLAY_H;
        if (wx >= ex && wx < ex + ew && wy >= ey && wy < ey + eh) {
            sel.type = ENT_SPIKE_BLOCK;
            sel.index = i;
            return sel;
        }
    }

    /* Blue flames — erupting fire hazards, preview centred in floor gap */
    for (int i = level->blue_flame_count - 1; i >= 0; i--) {
        float gap_x = level->blue_flames[i].x;
        ex = gap_x + (float)(FLOOR_GAP_W - BLUE_FLAME_W) / 2.0f;
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
        ex = gap_x + (float)(FLOOR_GAP_W - FIRE_FLAME_W) / 2.0f;
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

    /* Floor gaps — holes in the floor */
    for (int i = level->floor_gap_count - 1; i >= 0; i--) {
        ex = (float)level->floor_gaps[i];
        ey = (float)FLOOR_Y;
        ew = FLOOR_GAP_W;
        eh = GAME_H - FLOOR_Y;
        if (wx >= ex && wx < ex + ew && wy >= ey && wy < ey + eh) {
            sel.type = ENT_FLOOR_GAP;
            sel.index = i;
            return sel;
        }
    }

    /* Checkpoints are thin markers, but retain a forgiving hit target. */
    for (int i = level->checkpoint_count - 1; i >= 0; i--) {
        ex = level->checkpoints[i].x - 4.0f;
        ey = level->checkpoints[i].y - 20.0f;
        ew = 9;
        eh = 24;
        if (wx >= ex && wx < ex + ew && wy >= ey && wy < ey + eh) {
            sel.type = ENT_CHECKPOINT;
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

    /* Push undo command — CMD_DELETE stores "before" so undo re-inserts */
    memset(&cmd, 0, sizeof(cmd));
    cmd.type         = CMD_DELETE;
    cmd.entity_type  = (int)type;
    cmd.entity_index = index;
    cmd.before       = before;
    undo_push(es->undo, cmd);

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
    case ENT_BLUE_FLAME:
        out->blue_flame.x = world_x;
        return 1;
    case ENT_FIRE_FLAME:
        out->fire_flame.x = world_x;
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
 * place_entity --- Add one entity of the palette type at (world_x, world_y).
 *
 * Checks capacity, builds the default placement, appends it with the shared
 * editor_entity_insert helper, and pushes a CMD_PLACE undo command.  The two
 * singletons are moved instead and record a CMD_MOVE.
 */
static void place_entity(EditorState *es, float world_x, float world_y)
{
    LevelDef *level = &es->level;
    EntityType type  = es->palette_type;
    int singleton = editor_entity_type_is_singleton(type);
    PlacementData before;
    PlacementData after;
    Command cmd;
    int index;

    editor_selection_reconcile(es);

    /* Check capacity — every entity type has a fixed-size array */
    int count = editor_entity_count(level, type);
    int max   = editor_entity_capacity(type);
    if (!singleton && count >= max) {
        fprintf(stderr, "Warning: cannot place more — %d/%d capacity reached\n",
                count, max);
        return;
    }

    if (!default_placement(type, world_x, world_y, &after)) return;

    memset(&before, 0, sizeof(before));
    if (singleton) {
        index = 0;
        before = editor_snapshot_entity(level, type, 0);
        (void)editor_entity_write(level, type, 0, &after);
    } else {
        index = count;  /* append: new entities draw on top of older ones */
        if (editor_entity_insert(level, type, index, &after) != 0) return;
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
    cmd.after        = after;
    undo_push(es->undo, cmd);

    /* Select the newly placed entity for immediate inspection */
    es->selection.type  = type;
    es->selection.index = index;
    editor_refresh_dirty(es);
    if (type == ENT_CHECKPOINT) editor_set_status(es, "Checkpoint placed");
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
    if (!es) return;
    if (!tools_can_hit_test(es)) return;
    editor_selection_reconcile(es);

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

    if (!es || !es->dragging) return;
    es->dragging = 0;

    /* Drag coordinates are bounded, but a property edit may have invalidated
     * structural geometry while the mouse button was held. */
    if (level_validate_counts(&es->level, NULL, 0) != 0) return;

    editor_selection_reconcile(es);
    if (!editor_selection_is_valid(es)) return;

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
    PlacementData after = editor_snapshot_entity(&es->level, es->selection.type,
                                          es->selection.index);

    /*
     * Temporarily restore the entity to its drag-start position to
     * snapshot the "before" state, then put it back.
     */
    set_entity_pos(&es->level, es->selection.type, es->selection.index,
                   es->drag_start_x, es->drag_start_y);
    PlacementData before = editor_snapshot_entity(&es->level, es->selection.type,
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

    editor_refresh_dirty(es);
    if (es->selection.type == ENT_CHECKPOINT)
        editor_set_status(es, "Checkpoint moved");
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
    if (!es || !es->dragging) return;
    editor_selection_reconcile(es);
    if (!editor_selection_is_valid(es)) return;

    float nx = world_x;
    float ny = world_y;

    /*
     * Shift-snap: when the Shift key is held, round the position to the
     * nearest TILE_SIZE (48 px) grid point.  This makes alignment easy
     * without needing to toggle a separate grid-snap mode.
     */
    int mods = IsWindowReady() ? input_modifiers() : 0;
    if (mods & INPUT_SHIFT) {
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
    if (!es) return;
    if (!tools_can_hit_test(es)) return;
    editor_selection_reconcile(es);
    Selection hit = hit_test(&es->level, world_x, world_y);
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
