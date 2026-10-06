/*
 * editor_undo_apply.c — Apply undo/redo command snapshots to LevelDef.
 */

#include "editor_undo_apply.h"

#include <string.h>

#include "entity_meta.h"
#include "editor_files.h"
#include "editor_session.h"

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
                /* All three buffers are char[256] holding terminated text,
                 * so copying the whole array is exact; the assertions keep
                 * that true if one size ever changes. */
                _Static_assert(sizeof(cmd.property_text_before) ==
                               sizeof(es->pending_text_before),
                               "undo text matches pending text");
                _Static_assert(sizeof(cmd.property_text_after) ==
                               sizeof(es->level.next_phase),
                               "undo text matches next_phase");
                memcpy(cmd.property_text_before, es->pending_text_before,
                       sizeof(cmd.property_text_before));
                memcpy(cmd.property_text_after, es->level.next_phase,
                       sizeof(cmd.property_text_after));
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
            if (!undo_push(es->undo, cmd)) {
                editor_apply_config_snapshot(&es->level, &cmd.config_before);
                editor_set_status(es, "Config edit cancelled: cannot allocate undo history");
            }
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
 * Structural undo steps.  A CMD_PLACE is undone by removing the entity and
 * redone by inserting it again; CMD_DELETE is the mirror image.  Both use
 * the same insert/remove helpers as the tools, so rail references are
 * renumbered identically in every direction.
 */
static void editor_undo_insert(EditorState *es, EntityType type, int index,
                               const PlacementData *data, int select_inserted)
{
    if (editor_entity_insert(&es->level, type, index, data) == 0)
        editor_selection_after_insert(es, type, index, select_inserted);
}

static void editor_undo_remove(EditorState *es, EntityType type, int index)
{
    if (editor_entity_remove(&es->level, type, index) == 0)
        editor_selection_after_remove(es, type, index);
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
    EntityType type;
    int index;

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
    type = (EntityType)cmd->entity_type;
    index = cmd->entity_index;

    if (editor_entity_type_is_singleton(type)) {
        /* Last Star and Player Spawn always exist; "place" and "delete"
         * only move them, so every command is a plain overwrite. */
        (void)editor_entity_write(&es->level, type, 0,
                                  reverse ? &cmd->before : &cmd->after);
        if (type == ENT_LAST_STAR && cmd->type == CMD_PROPERTY &&
            cmd->property_field == EDITOR_LAST_STAR_NEXT_PHASE_WIDGET) {
            const char *text = reverse ? cmd->property_text_before
                                       : cmd->property_text_after;
            strncpy(es->level.next_phase, text,
                    sizeof(es->level.next_phase) - 1);
            es->level.next_phase[sizeof(es->level.next_phase) - 1] = '\0';
        }
    } else if (cmd->type == CMD_PLACE) {
        if (reverse) editor_undo_remove(es, type, index);
        else         editor_undo_insert(es, type, index, &cmd->after, 1);
    } else if (cmd->type == CMD_DELETE) {
        if (reverse) editor_undo_insert(es, type, index, &cmd->before, 0);
        else         editor_undo_remove(es, type, index);
    } else {
        (void)editor_entity_write(&es->level, type, index,
                                  reverse ? &cmd->before : &cmd->after);
    }

    editor_selection_reconcile(es);
    editor_refresh_dirty(es);
}
