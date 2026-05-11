/*
 * editor_undo_apply.c — Apply undo/redo command snapshots to LevelDef.
 */

#include "editor_undo_apply.h"

/*
 * editor_apply_undo_command — Apply or reverse an undo command on the level.
 *
 * The undo system stores before/after snapshots for every action. Undo applies
 * the before snapshot; redo applies the after snapshot. Place/delete commands
 * insert or remove array entries, while move/property commands overwrite data.
 */
void editor_apply_undo_command(EditorState *es, const Command *cmd, int reverse)
{
    #define APPLY_ARRAY(arr, cnt, union_field, max_count) \
        do { \
            int idx = cmd->entity_index; \
            if (cmd->type == CMD_PLACE) { \
                if (reverse) { \
                    if (idx >= 0 && idx < cnt) { \
                        for (int i = idx; i < cnt - 1; i++) \
                            arr[i] = arr[i + 1]; \
                        cnt--; \
                    } \
                } else { \
                    if (cnt < max_count && idx >= 0 && idx <= cnt) { \
                        for (int i = cnt; i > idx; i--) \
                            arr[i] = arr[i - 1]; \
                        arr[idx] = cmd->after.union_field; \
                        cnt++; \
                    } \
                } \
            } else if (cmd->type == CMD_DELETE) { \
                if (reverse) { \
                    if (cnt < max_count && idx >= 0 && idx <= cnt) { \
                        for (int i = cnt; i > idx; i--) \
                            arr[i] = arr[i - 1]; \
                        arr[idx] = cmd->before.union_field; \
                        cnt++; \
                    } \
                } else { \
                    if (idx >= 0 && idx < cnt) { \
                        for (int i = idx; i < cnt - 1; i++) \
                            arr[i] = arr[i + 1]; \
                        cnt--; \
                    } \
                } \
            } else { \
                if (idx >= 0 && idx < cnt) { \
                    arr[idx] = reverse ? cmd->before.union_field \
                                       : cmd->after.union_field; \
                } \
            } \
        } while (0)

    switch (cmd->entity_type) {
    case ENT_COIN:
        APPLY_ARRAY(es->level.coins, es->level.coin_count,
                     coin, MAX_COINS);
        break;

    case ENT_STAR_YELLOW:
        APPLY_ARRAY(es->level.star_yellows, es->level.star_yellow_count,
                     star_yellow, MAX_STAR_YELLOWS);
        break;

    case ENT_STAR_GREEN:
        APPLY_ARRAY(es->level.star_greens, es->level.star_green_count,
                     star_green, MAX_STAR_GREENS);
        break;

    case ENT_STAR_RED:
        APPLY_ARRAY(es->level.star_reds, es->level.star_red_count,
                     star_red, MAX_STAR_REDS);
        break;

    case ENT_LAST_STAR:
        es->level.last_star = reverse ? cmd->before.last_star
                                      : cmd->after.last_star;
        break;

    case ENT_PLAYER_SPAWN:
        if (reverse) {
            es->level.player_start_x = cmd->before.last_star.x;
            es->level.player_start_y = cmd->before.last_star.y;
        } else {
            es->level.player_start_x = cmd->after.last_star.x;
            es->level.player_start_y = cmd->after.last_star.y;
        }
        break;

    case ENT_SPIDER:
        APPLY_ARRAY(es->level.spiders, es->level.spider_count,
                     spider, MAX_SPIDERS);
        break;

    case ENT_JUMPING_SPIDER:
        APPLY_ARRAY(es->level.jumping_spiders,
                     es->level.jumping_spider_count,
                     jumping_spider, MAX_JUMPING_SPIDERS);
        break;

    case ENT_BIRD:
        APPLY_ARRAY(es->level.birds, es->level.bird_count,
                     bird, MAX_BIRDS);
        break;

    case ENT_FASTER_BIRD:
        APPLY_ARRAY(es->level.faster_birds, es->level.faster_bird_count,
                     bird, MAX_FASTER_BIRDS);
        break;

    case ENT_FISH:
        APPLY_ARRAY(es->level.fish, es->level.fish_count,
                     fish, MAX_FISH);
        break;

    case ENT_FASTER_FISH:
        APPLY_ARRAY(es->level.faster_fish, es->level.faster_fish_count,
                     fish, MAX_FASTER_FISH);
        break;

    case ENT_AXE_TRAP:
        APPLY_ARRAY(es->level.axe_traps, es->level.axe_trap_count,
                     axe_trap, MAX_AXE_TRAPS);
        break;

    case ENT_CIRCULAR_SAW:
        APPLY_ARRAY(es->level.circular_saws, es->level.circular_saw_count,
                     circular_saw, MAX_CIRCULAR_SAWS);
        break;

    case ENT_SPIKE_ROW:
        APPLY_ARRAY(es->level.spike_rows, es->level.spike_row_count,
                     spike_row, MAX_SPIKE_ROWS);
        break;

    case ENT_SPIKE_PLATFORM:
        APPLY_ARRAY(es->level.spike_platforms,
                     es->level.spike_platform_count,
                     spike_platform, MAX_SPIKE_PLATFORMS);
        break;

    case ENT_SPIKE_BLOCK:
        APPLY_ARRAY(es->level.spike_blocks, es->level.spike_block_count,
                     spike_block, MAX_SPIKE_BLOCKS);
        break;

    case ENT_BLUE_FLAME:
        APPLY_ARRAY(es->level.blue_flames, es->level.blue_flame_count,
                     blue_flame, MAX_BLUE_FLAMES);
        break;

    case ENT_FIRE_FLAME:
        APPLY_ARRAY(es->level.fire_flames, es->level.fire_flame_count,
                     fire_flame, MAX_BLUE_FLAMES);
        break;

    case ENT_FLOAT_PLATFORM:
        APPLY_ARRAY(es->level.float_platforms,
                     es->level.float_platform_count,
                     float_platform, MAX_FLOAT_PLATFORMS);
        break;

    case ENT_BRIDGE:
        APPLY_ARRAY(es->level.bridges, es->level.bridge_count,
                     bridge, MAX_BRIDGES);
        break;

    case ENT_BOUNCEPAD_SMALL:
        APPLY_ARRAY(es->level.bouncepads_small,
                     es->level.bouncepad_small_count,
                     bouncepad, MAX_BOUNCEPADS_SMALL);
        break;

    case ENT_BOUNCEPAD_MEDIUM:
        APPLY_ARRAY(es->level.bouncepads_medium,
                     es->level.bouncepad_medium_count,
                     bouncepad, MAX_BOUNCEPADS_MEDIUM);
        break;

    case ENT_BOUNCEPAD_HIGH:
        APPLY_ARRAY(es->level.bouncepads_high,
                     es->level.bouncepad_high_count,
                     bouncepad, MAX_BOUNCEPADS_HIGH);
        break;

    case ENT_PLATFORM:
        APPLY_ARRAY(es->level.platforms, es->level.platform_count,
                     platform, MAX_PLATFORMS);
        break;

    case ENT_VINE:
        APPLY_ARRAY(es->level.vines, es->level.vine_count,
                     vine, MAX_VINES);
        break;

    case ENT_LADDER:
        APPLY_ARRAY(es->level.ladders, es->level.ladder_count,
                     ladder, MAX_LADDERS);
        break;

    case ENT_ROPE:
        APPLY_ARRAY(es->level.ropes, es->level.rope_count,
                     rope, MAX_ROPES);
        break;

    case ENT_RAIL:
        APPLY_ARRAY(es->level.rails, es->level.rail_count,
                     rail, MAX_RAILS);
        break;

    case ENT_FLOOR_GAP:
        APPLY_ARRAY(es->level.floor_gaps, es->level.floor_gap_count,
                     floor_gap, MAX_FLOOR_GAPS);
        break;

    default:
        break;
    }

    #undef APPLY_ARRAY
}
