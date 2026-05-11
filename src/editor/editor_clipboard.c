/*
 * editor_clipboard.c — Editor clipboard copy/paste helpers.
 */

#include "editor_clipboard.h"

#include <string.h>  /* memset */

#include "../surfaces/rail.h" /* RAIL_TILE_W */
#include "undo.h"   /* Command, undo_push */

static void push_singleton_move(EditorState *es,
                                EntityType type,
                                LastStarPlacement before,
                                LastStarPlacement after)
{
    Command cmd;

    memset(&cmd, 0, sizeof(cmd));
    cmd.type = CMD_MOVE;
    cmd.entity_type = (int)type;
    cmd.entity_index = 0;
    cmd.before.last_star = before;
    cmd.after.last_star = after;
    undo_push(es->undo, cmd);
}

/*
 * editor_copy_selected — Snapshot the currently selected entity into the clipboard.
 *
 * Stores the entity type and a PlacementData union so paste can recreate it.
 */
void editor_copy_selected(EditorState *es)
{
    if (es->selection.index < 0) return;

    EntityType t = es->selection.type;
    int i = es->selection.index;

    es->clipboard_type = t;
    es->has_clipboard = 1;

    switch (t) {
    case ENT_FLOOR_GAP:
        es->clipboard_data.floor_gap = es->level.floor_gaps[i];
        break;
    case ENT_RAIL:
        es->clipboard_data.rail = es->level.rails[i];
        break;
    case ENT_PLATFORM:
        es->clipboard_data.platform = es->level.platforms[i];
        break;
    case ENT_COIN:
        es->clipboard_data.coin = es->level.coins[i];
        break;
    case ENT_STAR_YELLOW:
        es->clipboard_data.star_yellow = es->level.star_yellows[i];
        break;
    case ENT_STAR_GREEN:
        es->clipboard_data.star_green = es->level.star_greens[i];
        break;
    case ENT_STAR_RED:
        es->clipboard_data.star_red = es->level.star_reds[i];
        break;
    case ENT_LAST_STAR:
        es->clipboard_data.last_star = es->level.last_star;
        break;
    case ENT_PLAYER_SPAWN: {
        LastStarPlacement p = { es->level.player_start_x,
                                es->level.player_start_y };
        es->clipboard_data.last_star = p;
        break;
    }
    case ENT_SPIDER:
        es->clipboard_data.spider = es->level.spiders[i];
        break;
    case ENT_JUMPING_SPIDER:
        es->clipboard_data.jumping_spider = es->level.jumping_spiders[i];
        break;
    case ENT_BIRD:
        es->clipboard_data.bird = es->level.birds[i];
        break;
    case ENT_FASTER_BIRD:
        es->clipboard_data.bird = es->level.faster_birds[i];
        break;
    case ENT_FISH:
        es->clipboard_data.fish = es->level.fish[i];
        break;
    case ENT_FASTER_FISH:
        es->clipboard_data.fish = es->level.faster_fish[i];
        break;
    case ENT_AXE_TRAP:
        es->clipboard_data.axe_trap = es->level.axe_traps[i];
        break;
    case ENT_CIRCULAR_SAW:
        es->clipboard_data.circular_saw = es->level.circular_saws[i];
        break;
    case ENT_SPIKE_ROW:
        es->clipboard_data.spike_row = es->level.spike_rows[i];
        break;
    case ENT_SPIKE_PLATFORM:
        es->clipboard_data.spike_platform = es->level.spike_platforms[i];
        break;
    case ENT_SPIKE_BLOCK:
        es->clipboard_data.spike_block = es->level.spike_blocks[i];
        break;
    case ENT_BLUE_FLAME:
        es->clipboard_data.blue_flame = es->level.blue_flames[i];
        break;
    case ENT_FIRE_FLAME:
        es->clipboard_data.fire_flame = es->level.fire_flames[i];
        break;
    case ENT_FLOAT_PLATFORM:
        es->clipboard_data.float_platform = es->level.float_platforms[i];
        break;
    case ENT_BRIDGE:
        es->clipboard_data.bridge = es->level.bridges[i];
        break;
    case ENT_BOUNCEPAD_SMALL:
        es->clipboard_data.bouncepad = es->level.bouncepads_small[i];
        break;
    case ENT_BOUNCEPAD_MEDIUM:
        es->clipboard_data.bouncepad = es->level.bouncepads_medium[i];
        break;
    case ENT_BOUNCEPAD_HIGH:
        es->clipboard_data.bouncepad = es->level.bouncepads_high[i];
        break;
    case ENT_VINE:
        es->clipboard_data.vine = es->level.vines[i];
        break;
    case ENT_LADDER:
        es->clipboard_data.ladder = es->level.ladders[i];
        break;
    case ENT_ROPE:
        es->clipboard_data.rope = es->level.ropes[i];
        break;
    default:
        es->has_clipboard = 0;
        break;
    }
}

/*
 * editor_paste_clipboard — Create a new entity from the clipboard data.
 *
 * Inserts a copy of the last Ctrl+C'd entity into the level, offset by
 * 24px right and 24px down so it doesn't overlap the original. The new
 * entity is auto-selected for immediate repositioning.
 */
void editor_paste_clipboard(EditorState *es)
{
    if (!es->has_clipboard) return;

    EntityType t = es->clipboard_type;
    PlacementData d = es->clipboard_data;

#define PASTE_OFFSET 24.0f
#define PASTE_INTO(array, count_field, max, data_field)                    \
    do {                                                                   \
        if (es->level.count_field >= (max)) break;                         \
        int idx = es->level.count_field;                                   \
        es->level.array[idx] = d.data_field;                               \
        es->level.count_field++;                                           \
        es->selection.type = t;                                            \
        es->selection.index = idx;                                         \
        es->modified = 1;                                                  \
        Command cmd;                                                       \
        memset(&cmd, 0, sizeof(cmd));                                      \
        cmd.type = CMD_PLACE;                                              \
        cmd.entity_type = (int)t;                                          \
        cmd.entity_index = idx;                                            \
        cmd.after.data_field = es->level.array[idx];                       \
        undo_push(es->undo, cmd);                                          \
    } while (0)

    switch (t) {
    case ENT_COIN:
        d.coin.x += PASTE_OFFSET;
        d.coin.y += PASTE_OFFSET;
        PASTE_INTO(coins, coin_count, MAX_COINS, coin);
        break;
    case ENT_STAR_YELLOW:
        d.star_yellow.x += PASTE_OFFSET;
        d.star_yellow.y += PASTE_OFFSET;
        PASTE_INTO(star_yellows, star_yellow_count, MAX_STAR_YELLOWS, star_yellow);
        break;
    case ENT_STAR_GREEN:
        d.star_green.x += PASTE_OFFSET;
        d.star_green.y += PASTE_OFFSET;
        PASTE_INTO(star_greens, star_green_count, MAX_STAR_GREENS, star_green);
        break;
    case ENT_STAR_RED:
        d.star_red.x += PASTE_OFFSET;
        d.star_red.y += PASTE_OFFSET;
        PASTE_INTO(star_reds, star_red_count, MAX_STAR_REDS, star_red);
        break;
    case ENT_LAST_STAR: {
        LastStarPlacement before = es->level.last_star;
        d.last_star.x += PASTE_OFFSET;
        d.last_star.y += PASTE_OFFSET;
        push_singleton_move(es, t, before, d.last_star);
        es->level.last_star = d.last_star;
        es->selection.type = t;
        es->selection.index = 0;
        es->modified = 1;
        break;
    }
    case ENT_PLAYER_SPAWN: {
        LastStarPlacement before = { es->level.player_start_x,
                                     es->level.player_start_y };
        LastStarPlacement after = { d.last_star.x + PASTE_OFFSET,
                                    d.last_star.y + PASTE_OFFSET };
        push_singleton_move(es, t, before, after);
        es->level.player_start_x = after.x;
        es->level.player_start_y = after.y;
        es->selection.type = t;
        es->selection.index = 0;
        es->modified = 1;
        break;
    }
    case ENT_SPIDER:
        d.spider.x += PASTE_OFFSET;
        d.spider.patrol_x0 += PASTE_OFFSET;
        d.spider.patrol_x1 += PASTE_OFFSET;
        PASTE_INTO(spiders, spider_count, MAX_SPIDERS, spider);
        break;
    case ENT_JUMPING_SPIDER:
        d.jumping_spider.x += PASTE_OFFSET;
        d.jumping_spider.patrol_x0 += PASTE_OFFSET;
        d.jumping_spider.patrol_x1 += PASTE_OFFSET;
        PASTE_INTO(jumping_spiders, jumping_spider_count, MAX_JUMPING_SPIDERS, jumping_spider);
        break;
    case ENT_BIRD:
        d.bird.x += PASTE_OFFSET;
        d.bird.patrol_x0 += PASTE_OFFSET;
        d.bird.patrol_x1 += PASTE_OFFSET;
        PASTE_INTO(birds, bird_count, MAX_BIRDS, bird);
        break;
    case ENT_FASTER_BIRD:
        d.bird.x += PASTE_OFFSET;
        d.bird.patrol_x0 += PASTE_OFFSET;
        d.bird.patrol_x1 += PASTE_OFFSET;
        PASTE_INTO(faster_birds, faster_bird_count, MAX_FASTER_BIRDS, bird);
        break;
    case ENT_FISH:
        d.fish.x += PASTE_OFFSET;
        d.fish.patrol_x0 += PASTE_OFFSET;
        d.fish.patrol_x1 += PASTE_OFFSET;
        PASTE_INTO(fish, fish_count, MAX_FISH, fish);
        break;
    case ENT_FASTER_FISH:
        d.fish.x += PASTE_OFFSET;
        d.fish.patrol_x0 += PASTE_OFFSET;
        d.fish.patrol_x1 += PASTE_OFFSET;
        PASTE_INTO(faster_fish, faster_fish_count, MAX_FASTER_FISH, fish);
        break;
    case ENT_AXE_TRAP:
        d.axe_trap.pillar_x += PASTE_OFFSET;
        PASTE_INTO(axe_traps, axe_trap_count, MAX_AXE_TRAPS, axe_trap);
        break;
    case ENT_CIRCULAR_SAW:
        d.circular_saw.x += PASTE_OFFSET;
        d.circular_saw.patrol_x0 += PASTE_OFFSET;
        d.circular_saw.patrol_x1 += PASTE_OFFSET;
        PASTE_INTO(circular_saws, circular_saw_count, MAX_CIRCULAR_SAWS, circular_saw);
        break;
    case ENT_SPIKE_ROW:
        d.spike_row.x += PASTE_OFFSET;
        PASTE_INTO(spike_rows, spike_row_count, MAX_SPIKE_ROWS, spike_row);
        break;
    case ENT_SPIKE_PLATFORM:
        d.spike_platform.x += PASTE_OFFSET;
        d.spike_platform.y += PASTE_OFFSET;
        PASTE_INTO(spike_platforms, spike_platform_count, MAX_SPIKE_PLATFORMS, spike_platform);
        break;
    case ENT_SPIKE_BLOCK:
        d.spike_block.t_offset += PASTE_OFFSET / (float)RAIL_TILE_W;
        PASTE_INTO(spike_blocks, spike_block_count, MAX_SPIKE_BLOCKS, spike_block);
        break;
    case ENT_BLUE_FLAME:
        d.blue_flame.x += PASTE_OFFSET;
        PASTE_INTO(blue_flames, blue_flame_count, MAX_BLUE_FLAMES, blue_flame);
        break;
    case ENT_FIRE_FLAME:
        d.fire_flame.x += PASTE_OFFSET;
        PASTE_INTO(fire_flames, fire_flame_count, MAX_BLUE_FLAMES, fire_flame);
        break;
    case ENT_FLOAT_PLATFORM:
        d.float_platform.x += PASTE_OFFSET;
        d.float_platform.y += PASTE_OFFSET;
        PASTE_INTO(float_platforms, float_platform_count, MAX_FLOAT_PLATFORMS, float_platform);
        break;
    case ENT_BRIDGE:
        d.bridge.x += PASTE_OFFSET;
        PASTE_INTO(bridges, bridge_count, MAX_BRIDGES, bridge);
        break;
    case ENT_BOUNCEPAD_SMALL:
        d.bouncepad.x += PASTE_OFFSET;
        PASTE_INTO(bouncepads_small, bouncepad_small_count, MAX_BOUNCEPADS_SMALL, bouncepad);
        break;
    case ENT_BOUNCEPAD_MEDIUM:
        d.bouncepad.x += PASTE_OFFSET;
        PASTE_INTO(bouncepads_medium, bouncepad_medium_count, MAX_BOUNCEPADS_MEDIUM, bouncepad);
        break;
    case ENT_BOUNCEPAD_HIGH:
        d.bouncepad.x += PASTE_OFFSET;
        PASTE_INTO(bouncepads_high, bouncepad_high_count, MAX_BOUNCEPADS_HIGH, bouncepad);
        break;
    case ENT_PLATFORM:
        d.platform.x += PASTE_OFFSET;
        PASTE_INTO(platforms, platform_count, MAX_PLATFORMS, platform);
        break;
    case ENT_VINE:
        d.vine.x += PASTE_OFFSET;
        PASTE_INTO(vines, vine_count, MAX_VINES, vine);
        break;
    case ENT_LADDER:
        d.ladder.x += PASTE_OFFSET;
        PASTE_INTO(ladders, ladder_count, MAX_LADDERS, ladder);
        break;
    case ENT_ROPE:
        d.rope.x += PASTE_OFFSET;
        PASTE_INTO(ropes, rope_count, MAX_ROPES, rope);
        break;
    case ENT_FLOOR_GAP:
        d.floor_gap += FLOOR_GAP_W;
        if (es->level.floor_gap_count < MAX_FLOOR_GAPS) {
            int idx = es->level.floor_gap_count;
            es->level.floor_gaps[idx] = d.floor_gap;
            es->level.floor_gap_count++;
            es->selection.type = t;
            es->selection.index = idx;
            es->modified = 1;
            Command cmd;
            memset(&cmd, 0, sizeof(cmd));
            cmd.type = CMD_PLACE;
            cmd.entity_type = (int)t;
            cmd.entity_index = idx;
            cmd.after.floor_gap = d.floor_gap;
            undo_push(es->undo, cmd);
        }
        break;
    case ENT_RAIL:
        d.rail.x += (int)PASTE_OFFSET;
        PASTE_INTO(rails, rail_count, MAX_RAILS, rail);
        break;
    default:
        break;
    }

#undef PASTE_INTO
#undef PASTE_OFFSET
}
