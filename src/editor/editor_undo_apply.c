/*
 * editor_undo_apply.c — Apply undo/redo command snapshots to LevelDef.
 */

#include "editor_undo_apply.h"

#include <string.h>

#include "entity_meta.h"
#include "editor_files.h"
#include "editor_session.h"

PlacementData editor_snapshot_entity(const LevelDef *level,
                                     EntityType type, int index)
{
    PlacementData pd;

    memset(&pd, 0, sizeof(pd));
    if (!level || index < 0 || index >= editor_entity_count(level, type))
        return pd;

    switch (type) {
    case ENT_PLATFORM:         pd.platform = level->platforms[index]; break;
    case ENT_FLOOR_GAP:        pd.floor_gap = level->floor_gaps[index]; break;
    case ENT_CHECKPOINT:       pd.checkpoint = level->checkpoints[index]; break;
    case ENT_RAIL:             pd.rail = level->rails[index]; break;
    case ENT_COIN:             pd.coin = level->coins[index]; break;
    case ENT_STAR_YELLOW:      pd.star_yellow = level->star_yellows[index]; break;
    case ENT_STAR_GREEN:       pd.star_green = level->star_greens[index]; break;
    case ENT_STAR_RED:         pd.star_red = level->star_reds[index]; break;
    case ENT_LAST_STAR:        pd.last_star = level->last_star; break;
    case ENT_PLAYER_SPAWN:
        pd.last_star.x = level->player_start_x;
        pd.last_star.y = level->player_start_y;
        break;
    case ENT_SPIDER:           pd.spider = level->spiders[index]; break;
    case ENT_JUMPING_SPIDER:  pd.jumping_spider = level->jumping_spiders[index]; break;
    case ENT_BIRD:             pd.bird = level->birds[index]; break;
    case ENT_FASTER_BIRD:      pd.bird = level->faster_birds[index]; break;
    case ENT_FISH:             pd.fish = level->fish[index]; break;
    case ENT_FASTER_FISH:      pd.fish = level->faster_fish[index]; break;
    case ENT_AXE_TRAP:         pd.axe_trap = level->axe_traps[index]; break;
    case ENT_CIRCULAR_SAW:     pd.circular_saw = level->circular_saws[index]; break;
    case ENT_SPIKE_ROW:        pd.spike_row = level->spike_rows[index]; break;
    case ENT_SPIKE_PLATFORM:   pd.spike_platform = level->spike_platforms[index]; break;
    case ENT_SPIKE_BLOCK:      pd.spike_block = level->spike_blocks[index]; break;
    case ENT_BLUE_FLAME:       pd.blue_flame = level->blue_flames[index]; break;
    case ENT_FIRE_FLAME:       pd.fire_flame = level->fire_flames[index]; break;
    case ENT_FLOAT_PLATFORM:   pd.float_platform = level->float_platforms[index]; break;
    case ENT_BRIDGE:            pd.bridge = level->bridges[index]; break;
    case ENT_BOUNCEPAD_SMALL:  pd.bouncepad = level->bouncepads_small[index]; break;
    case ENT_BOUNCEPAD_MEDIUM: pd.bouncepad = level->bouncepads_medium[index]; break;
    case ENT_BOUNCEPAD_HIGH:   pd.bouncepad = level->bouncepads_high[index]; break;
    case ENT_VINE:              pd.vine = level->vines[index]; break;
    case ENT_LADDER:            pd.ladder = level->ladders[index]; break;
    case ENT_ROPE:              pd.rope = level->ropes[index]; break;
    case ENT_COUNT:             break;
    }
    return pd;
}

LevelConfigSnapshot editor_snapshot_config(const LevelDef *level)
{
    LevelConfigSnapshot snapshot;

    memset(&snapshot, 0, sizeof(snapshot));
    if (!level) return snapshot;

    memcpy(snapshot.name, level->name, sizeof(snapshot.name));
    memcpy(snapshot.description, level->description, sizeof(snapshot.description));
    memcpy(snapshot.generated_by, level->generated_by, sizeof(snapshot.generated_by));
    snapshot.screen_count = level->screen_count;
    memcpy(snapshot.next_phase, level->next_phase, sizeof(snapshot.next_phase));
    memcpy(snapshot.background_layers, level->background_layers,
           sizeof(snapshot.background_layers));
    snapshot.background_layer_count = level->background_layer_count;
    memcpy(snapshot.foreground_layers, level->foreground_layers,
           sizeof(snapshot.foreground_layers));
    snapshot.foreground_layer_count = level->foreground_layer_count;
    memcpy(snapshot.fog_layers, level->fog_layers, sizeof(snapshot.fog_layers));
    snapshot.fog_layer_count = level->fog_layer_count;
    memcpy(snapshot.music_path, level->music_path, sizeof(snapshot.music_path));
    snapshot.music_volume = level->music_volume;
    memcpy(snapshot.floor_tile_path, level->floor_tile_path,
           sizeof(snapshot.floor_tile_path));
    snapshot.initial_hearts = level->initial_hearts;
    snapshot.initial_lives = level->initial_lives;
    snapshot.score_per_life = level->score_per_life;
    snapshot.coin_score = level->coin_score;
    snapshot.physics.walk_max_speed = level->physics.walk_max_speed;
    snapshot.physics.run_max_speed = level->physics.run_max_speed;
    snapshot.physics.walk_ground_accel = level->physics.walk_ground_accel;
    snapshot.physics.run_ground_accel = level->physics.run_ground_accel;
    snapshot.physics.ground_friction = level->physics.ground_friction;
    snapshot.physics.ground_counter_accel = level->physics.ground_counter_accel;
    snapshot.physics.air_accel_walk = level->physics.air_accel_walk;
    snapshot.physics.air_accel_run = level->physics.air_accel_run;
    snapshot.physics.air_friction = level->physics.air_friction;
    snapshot.physics.cam_lookahead_vx_factor = level->physics.cam_lookahead_vx_factor;
    snapshot.physics.cam_lookahead_max = level->physics.cam_lookahead_max;
    return snapshot;
}

static void editor_apply_config_snapshot(LevelDef *level,
                                         const LevelConfigSnapshot *snapshot)
{
    memcpy(level->name, snapshot->name, sizeof(level->name));
    memcpy(level->description, snapshot->description, sizeof(level->description));
    memcpy(level->generated_by, snapshot->generated_by, sizeof(level->generated_by));
    level->screen_count = snapshot->screen_count;
    memcpy(level->next_phase, snapshot->next_phase, sizeof(level->next_phase));
    memcpy(level->background_layers, snapshot->background_layers,
           sizeof(level->background_layers));
    level->background_layer_count = snapshot->background_layer_count;
    memcpy(level->foreground_layers, snapshot->foreground_layers,
           sizeof(level->foreground_layers));
    level->foreground_layer_count = snapshot->foreground_layer_count;
    memcpy(level->fog_layers, snapshot->fog_layers, sizeof(level->fog_layers));
    level->fog_layer_count = snapshot->fog_layer_count;
    memcpy(level->music_path, snapshot->music_path, sizeof(level->music_path));
    level->music_volume = snapshot->music_volume;
    memcpy(level->floor_tile_path, snapshot->floor_tile_path,
           sizeof(level->floor_tile_path));
    level->initial_hearts = snapshot->initial_hearts;
    level->initial_lives = snapshot->initial_lives;
    level->score_per_life = snapshot->score_per_life;
    level->coin_score = snapshot->coin_score;
    level->physics.walk_max_speed = snapshot->physics.walk_max_speed;
    level->physics.run_max_speed = snapshot->physics.run_max_speed;
    level->physics.walk_ground_accel = snapshot->physics.walk_ground_accel;
    level->physics.run_ground_accel = snapshot->physics.run_ground_accel;
    level->physics.ground_friction = snapshot->physics.ground_friction;
    level->physics.ground_counter_accel = snapshot->physics.ground_counter_accel;
    level->physics.air_accel_walk = snapshot->physics.air_accel_walk;
    level->physics.air_accel_run = snapshot->physics.air_accel_run;
    level->physics.air_friction = snapshot->physics.air_friction;
    level->physics.cam_lookahead_vx_factor = snapshot->physics.cam_lookahead_vx_factor;
    level->physics.cam_lookahead_max = snapshot->physics.cam_lookahead_max;
}

void editor_begin_change_tracking(EditorState *es, int kind)
{
    if (!es) return;
    es->change_tracking_kind = kind;
    es->pending_change_valid = 0;
    es->pending_widget_id = 0;
}

void editor_capture_change_before(EditorState *es, int widget_id)
{
    if (!es || es->pending_change_valid) return;

    if (es->change_tracking_kind == EDITOR_CHANGE_ENTITY) {
        editor_selection_reconcile(es);
        if (!editor_selection_is_valid(es)) return;
        es->pending_widget_id = widget_id > 0 ? widget_id : 0;
        es->pending_entity_type = (int)es->selection.type;
        es->pending_entity_index = es->selection.index;
        es->pending_entity_before = editor_snapshot_entity(
            &es->level, es->selection.type, es->selection.index);
        if (widget_id == EDITOR_LAST_STAR_NEXT_PHASE_WIDGET &&
            es->selection.type == ENT_LAST_STAR) {
            strncpy(es->pending_text_before, es->level.next_phase,
                    sizeof(es->pending_text_before) - 1);
            es->pending_text_before[sizeof(es->pending_text_before) - 1] = '\0';
        }
        es->pending_change_valid = 1;
    } else if (es->change_tracking_kind == EDITOR_CHANGE_CONFIG) {
        es->pending_config_before = editor_snapshot_config(&es->level);
        es->pending_change_valid = 1;
    }
}

void editor_before_change(void *context, int widget_id)
{
    editor_capture_change_before((EditorState *)context, widget_id);
}

void editor_commit_change(EditorState *es)
{
    Command cmd;

    if (!es || !es->pending_change_valid) return;
    memset(&cmd, 0, sizeof(cmd));

    if (es->change_tracking_kind == EDITOR_CHANGE_ENTITY) {
        EntityType type = (EntityType)es->pending_entity_type;
        PlacementData after = editor_snapshot_entity(
            &es->level, type, es->pending_entity_index);
        int text_changed = es->pending_widget_id ==
                           EDITOR_LAST_STAR_NEXT_PHASE_WIDGET &&
                           strcmp(es->pending_text_before,
                                  es->level.next_phase) != 0;
        if (memcmp(&es->pending_entity_before, &after, sizeof(after)) != 0 ||
            text_changed) {
            cmd.type = CMD_PROPERTY;
            cmd.entity_type = es->pending_entity_type;
            cmd.entity_index = es->pending_entity_index;
            cmd.before = es->pending_entity_before;
            cmd.after = after;
            cmd.property_field = es->pending_widget_id;
            if (text_changed) {
                strncpy(cmd.property_text_before, es->pending_text_before,
                        sizeof(cmd.property_text_before) - 1);
                strncpy(cmd.property_text_after, es->level.next_phase,
                        sizeof(cmd.property_text_after) - 1);
                cmd.property_text_before[sizeof(cmd.property_text_before) - 1] = '\0';
                cmd.property_text_after[sizeof(cmd.property_text_after) - 1] = '\0';
            }
            undo_push(es->undo, cmd);
            editor_refresh_dirty(es);
        }
    } else if (es->change_tracking_kind == EDITOR_CHANGE_CONFIG) {
        LevelConfigSnapshot after = editor_snapshot_config(&es->level);
        if (memcmp(&es->pending_config_before, &after, sizeof(after)) != 0) {
            cmd.type = CMD_CONFIG;
            cmd.config_before = es->pending_config_before;
            cmd.config_after = after;
            undo_push(es->undo, cmd);
            editor_sync_config_resources(es);
            editor_refresh_dirty(es);
        }
    }

    es->pending_change_valid = 0;
    es->pending_widget_id = 0;
}

void editor_end_change_tracking(EditorState *es)
{
    if (!es) return;
    es->change_tracking_kind = 0;
    es->pending_change_valid = 0;
    es->pending_widget_id = 0;
    es->ui.before_change = NULL;
    es->ui.before_change_context = NULL;
}

/*
 * editor_apply_undo_command — Apply or reverse an undo command on the level.
 *
 * The undo system stores before/after snapshots for every action. Undo applies
 * the before snapshot; redo applies the after snapshot. Place/delete commands
 * insert or remove array entries, while move/property commands overwrite data.
 */
void editor_apply_undo_command(EditorState *es, const Command *cmd, int reverse)
{
    if (!es || !cmd) return;

    if (cmd->type == CMD_CONFIG) {
        editor_apply_config_snapshot(&es->level,
                                     reverse ? &cmd->config_before
                                             : &cmd->config_after);
        editor_selection_reconcile(es);
        editor_sync_config_resources(es);
        editor_refresh_dirty(es);
        return;
    }

    if (cmd->entity_type < 0 || cmd->entity_type >= ENT_COUNT) return;

    #define LEVEL_ARRAY_LEN(arr) ((int)(sizeof(arr) / sizeof((arr)[0])))
    #define APPLY_ARRAY(arr, cnt, union_field, max_count) \
        do { \
            int idx = cmd->entity_index; \
            if (cmd->type == CMD_PLACE) { \
                if (reverse) { \
                    if (idx >= 0 && idx < cnt) { \
                        for (int i = idx; i < cnt - 1; i++) \
                            arr[i] = arr[i + 1]; \
                        cnt--; \
                        editor_selection_after_remove(es, \
                                                      (EntityType)cmd->entity_type, idx); \
                    } \
                } else { \
                    if (cnt < max_count && idx >= 0 && idx <= cnt) { \
                        for (int i = cnt; i > idx; i--) \
                            arr[i] = arr[i - 1]; \
                        arr[idx] = cmd->after.union_field; \
                        cnt++; \
                        editor_selection_after_insert(es, \
                                                      (EntityType)cmd->entity_type, idx, 1); \
                    } \
                } \
            } else if (cmd->type == CMD_DELETE) { \
                if (reverse) { \
                    if (cnt < max_count && idx >= 0 && idx <= cnt) { \
                        for (int i = cnt; i > idx; i--) \
                            arr[i] = arr[i - 1]; \
                        arr[idx] = cmd->before.union_field; \
                        cnt++; \
                        editor_selection_after_insert(es, \
                                                      (EntityType)cmd->entity_type, idx, 0); \
                    } \
                } else { \
                    if (idx >= 0 && idx < cnt) { \
                        for (int i = idx; i < cnt - 1; i++) \
                            arr[i] = arr[i + 1]; \
                        cnt--; \
                        editor_selection_after_remove(es, \
                                                      (EntityType)cmd->entity_type, idx); \
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
        if (cmd->type == CMD_PROPERTY &&
            cmd->property_field == EDITOR_LAST_STAR_NEXT_PHASE_WIDGET) {
            const char *text = reverse ? cmd->property_text_before
                                       : cmd->property_text_after;
            strncpy(es->level.next_phase, text,
                    sizeof(es->level.next_phase) - 1);
            es->level.next_phase[sizeof(es->level.next_phase) - 1] = '\0';
        }
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
                     blue_flame, LEVEL_ARRAY_LEN(es->level.blue_flames));
        break;

    case ENT_FIRE_FLAME:
        APPLY_ARRAY(es->level.fire_flames, es->level.fire_flame_count,
                     fire_flame, LEVEL_ARRAY_LEN(es->level.fire_flames));
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

    case ENT_CHECKPOINT:
        APPLY_ARRAY(es->level.checkpoints, es->level.checkpoint_count,
                     checkpoint, MAX_CHECKPOINTS);
        break;

    default:
        break;
    }

    editor_selection_reconcile(es);
    editor_refresh_dirty(es);

    #undef APPLY_ARRAY
    #undef LEVEL_ARRAY_LEN
}
