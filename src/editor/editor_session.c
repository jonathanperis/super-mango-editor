/*
 * editor_session.c — Editor session state helpers.
 */

#include "editor_session.h"

#include "file_dialog.h"
#include <stdarg.h>       /* va_list */
#include <stdio.h>        /* fprintf, snprintf, stderr, vsnprintf */
#include <string.h>       /* memset, strncpy */

#include "editor_validation.h" /* editor_validate_level */
#include "editor_files.h"      /* recovery retirement */
#include "editor_undo_apply.h" /* staged property/config command capture */
#include "undo.h"              /* undo_clear */

static int editor_test_finish_choice = -1;
static int editor_test_discard_choice = -1;
static int editor_test_overwrite_choice = -1;
static EditorExternalChoice editor_test_external_choice = (EditorExternalChoice)-1;

void editor_test_set_finish_field_choice(int button_id)
{
    editor_test_finish_choice = button_id;
}

void editor_test_set_discard_choice(int button_id)
{
    editor_test_discard_choice = button_id;
}

void editor_test_set_overwrite_choice(int button_id)
{
    editor_test_overwrite_choice = button_id;
}

void editor_test_set_external_choice(EditorExternalChoice choice)
{
    editor_test_external_choice = choice;
}

void editor_set_status(EditorState *es, const char *fmt, ...)
{
    va_list ap;

    if (!es || !fmt) return;
    va_start(ap, fmt);
    vsnprintf(es->status_message, sizeof(es->status_message), fmt, ap);
    va_end(ap);
}

static void editor_hash_bytes(uint64_t *hash, const void *data, size_t size)
{
    const unsigned char *bytes = (const unsigned char *)data;

    for (size_t i = 0; i < size; i++) {
        *hash ^= bytes[i];
        *hash *= UINT64_C(1099511628211);
    }
}

static void editor_hash_string(uint64_t *hash, const char *value)
{
    size_t length = value ? strlen(value) : 0;

    editor_hash_bytes(hash, &length, sizeof(length));
    editor_hash_bytes(hash, value, length);
}

#define EDITOR_HASH_SCALAR(hash, value) \
    editor_hash_bytes((hash), &(value), sizeof(value))

#define EDITOR_HASH_ARRAY(hash, level, array, count, maximum) do { \
    int editor_hash_count = (level)->count; \
    if (editor_hash_count < 0) editor_hash_count = 0; \
    if (editor_hash_count > (maximum)) editor_hash_count = (maximum); \
    editor_hash_bytes((hash), &editor_hash_count, sizeof(editor_hash_count)); \
    for (int editor_hash_i = 0; editor_hash_i < editor_hash_count; editor_hash_i++) \
        editor_hash_bytes((hash), &(level)->array[editor_hash_i], \
                          sizeof((level)->array[editor_hash_i])); \
} while (0)

uint64_t editor_document_hash(const LevelDef *level)
{
    uint64_t hash = UINT64_C(1469598103934665603);

    if (!level) return hash;

    editor_hash_string(&hash, level->name);
    editor_hash_string(&hash, level->description);
    editor_hash_string(&hash, level->generated_by);
    EDITOR_HASH_SCALAR(&hash, level->screen_count);
    EDITOR_HASH_ARRAY(&hash, level, floor_gaps, floor_gap_count, MAX_FLOOR_GAPS);
    EDITOR_HASH_ARRAY(&hash, level, checkpoints, checkpoint_count, MAX_CHECKPOINTS);
    EDITOR_HASH_ARRAY(&hash, level, rails, rail_count, MAX_RAILS);
    EDITOR_HASH_ARRAY(&hash, level, platforms, platform_count, MAX_PLATFORMS);
    EDITOR_HASH_ARRAY(&hash, level, coins, coin_count, MAX_COINS);
    EDITOR_HASH_ARRAY(&hash, level, star_yellows, star_yellow_count, MAX_STAR_YELLOWS);
    EDITOR_HASH_ARRAY(&hash, level, star_greens, star_green_count, MAX_STAR_GREENS);
    EDITOR_HASH_ARRAY(&hash, level, star_reds, star_red_count, MAX_STAR_REDS);
    editor_hash_bytes(&hash, &level->last_star, sizeof(level->last_star));
    editor_hash_string(&hash, level->next_phase);
    EDITOR_HASH_ARRAY(&hash, level, spiders, spider_count, MAX_SPIDERS);
    EDITOR_HASH_ARRAY(&hash, level, jumping_spiders, jumping_spider_count,
                      MAX_JUMPING_SPIDERS);
    EDITOR_HASH_ARRAY(&hash, level, birds, bird_count, MAX_BIRDS);
    EDITOR_HASH_ARRAY(&hash, level, faster_birds, faster_bird_count, MAX_FASTER_BIRDS);
    EDITOR_HASH_ARRAY(&hash, level, fish, fish_count, MAX_FISH);
    EDITOR_HASH_ARRAY(&hash, level, faster_fish, faster_fish_count, MAX_FASTER_FISH);
    EDITOR_HASH_ARRAY(&hash, level, axe_traps, axe_trap_count, MAX_AXE_TRAPS);
    EDITOR_HASH_ARRAY(&hash, level, circular_saws, circular_saw_count, MAX_CIRCULAR_SAWS);
    EDITOR_HASH_ARRAY(&hash, level, spike_rows, spike_row_count, MAX_SPIKE_ROWS);
    EDITOR_HASH_ARRAY(&hash, level, spike_platforms, spike_platform_count,
                      MAX_SPIKE_PLATFORMS);
    EDITOR_HASH_ARRAY(&hash, level, spike_blocks, spike_block_count, MAX_SPIKE_BLOCKS);
    EDITOR_HASH_ARRAY(&hash, level, blue_flames, blue_flame_count, MAX_BLUE_FLAMES);
    EDITOR_HASH_ARRAY(&hash, level, fire_flames, fire_flame_count, MAX_FIRE_FLAMES);
    EDITOR_HASH_ARRAY(&hash, level, float_platforms, float_platform_count,
                      MAX_FLOAT_PLATFORMS);
    EDITOR_HASH_ARRAY(&hash, level, bridges, bridge_count, MAX_BRIDGES);
    EDITOR_HASH_ARRAY(&hash, level, bouncepads_small, bouncepad_small_count,
                      MAX_BOUNCEPADS_SMALL);
    EDITOR_HASH_ARRAY(&hash, level, bouncepads_medium, bouncepad_medium_count,
                      MAX_BOUNCEPADS_MEDIUM);
    EDITOR_HASH_ARRAY(&hash, level, bouncepads_high, bouncepad_high_count,
                      MAX_BOUNCEPADS_HIGH);
    EDITOR_HASH_ARRAY(&hash, level, vines, vine_count, MAX_VINES);
    EDITOR_HASH_ARRAY(&hash, level, ladders, ladder_count, MAX_LADDERS);
    EDITOR_HASH_ARRAY(&hash, level, ropes, rope_count, MAX_ROPES);

    EDITOR_HASH_ARRAY(&hash, level, background_layers, background_layer_count,
                      MAX_BACKGROUND_LAYERS);
    EDITOR_HASH_ARRAY(&hash, level, foreground_layers, foreground_layer_count,
                      MAX_BACKGROUND_LAYERS);
    EDITOR_HASH_ARRAY(&hash, level, fog_layers, fog_layer_count, MAX_FOG_TEXTURES);
    EDITOR_HASH_SCALAR(&hash, level->player_start_x);
    EDITOR_HASH_SCALAR(&hash, level->player_start_y);
    editor_hash_string(&hash, level->music_path);
    EDITOR_HASH_SCALAR(&hash, level->music_volume);
    editor_hash_string(&hash, level->floor_tile_path);
    EDITOR_HASH_SCALAR(&hash, level->initial_hearts);
    EDITOR_HASH_SCALAR(&hash, level->initial_lives);
    EDITOR_HASH_SCALAR(&hash, level->score_per_life);
    EDITOR_HASH_SCALAR(&hash, level->coin_score);
    editor_hash_bytes(&hash, &level->physics, sizeof(level->physics));
    return hash;
}

#undef EDITOR_HASH_ARRAY
#undef EDITOR_HASH_SCALAR

void editor_set_document_save_point(EditorState *es)
{
    if (!es) return;
    es->saved_document_hash = editor_document_hash(&es->level);
    es->saved_document_hash_valid = 1;
    es->modified = 0;
    editor_update_window_title(es);
}

void editor_set_recovered_dirty(EditorState *es)
{
    if (!es) return;
    es->saved_document_hash = ~editor_document_hash(&es->level);
    es->saved_document_hash_valid = 1;
    es->modified = 1;
    editor_update_window_title(es);
}

void editor_refresh_dirty(EditorState *es)
{
    if (!es) return;
    if (es->saved_document_hash_valid) {
        es->modified = editor_document_hash(&es->level) !=
                       es->saved_document_hash;
    } else {
        es->modified = 1;
    }
    editor_update_window_title(es);
}

int editor_finish_field_edit(EditorState *es)
{
    const char *buttons[] = {"Block", "Apply", "Discard"};
    int button_id = 0;
    int result;
    int kind;

    if (!es || es->ui.active_id == 0) return 1;

    if (editor_test_finish_choice >= 0) {
        button_id = editor_test_finish_choice;
        editor_test_finish_choice = -1;
    } else if (dialog_choice("Finish Field Edit", "Field edit is still active. Choose Apply, Discard, or Block.",
                             buttons, 3, 1, 0, &button_id) != 0) {
        editor_set_status(es, "Command blocked: field edit confirmation failed");
        return 0;
    }
    if (button_id == 0) {
        editor_set_status(es, "Command blocked: field edit remains active");
        return 0;
    }
    if (button_id == 2) {
        ui_cancel_active_edit(&es->ui);
        editor_end_change_tracking(es);
        return 1;
    }

    kind = es->ui.active_id >= 9000 ? EDITOR_CHANGE_CONFIG
                                    : EDITOR_CHANGE_ENTITY;
    editor_begin_change_tracking(es, kind);
    es->ui.before_change = editor_before_change;
    es->ui.before_change_context = es;
    result = ui_apply_active_edit(&es->ui);
    if (result == 0) {
        editor_end_change_tracking(es);
        editor_set_status(es, "Command blocked: invalid field value");
        return 0;
    }
    if (result == 2) editor_commit_change(es);
    editor_end_change_tracking(es);
    return 1;
}

int editor_before_command(void *context)
{
    return editor_finish_field_edit((EditorState *)context);
}

void editor_level_init_defaults(LevelDef *level)
{
    if (!level) return;

    level_def_init_defaults(level);
    strncpy(level->name, "Untitled", sizeof(level->name) - 1);
    level->screen_count = 4;
    level->player_start_x = 48.0f;
    level->player_start_y = 205.0f;
    level->last_star.x = 145.0f;
    level->last_star.y = 167.0f;
    strncpy(level->floor_tile_path, "assets/sprites/levels/grass_tileset.png",
            sizeof(level->floor_tile_path) - 1);
}

void editor_update_window_title(EditorState *es)
{
    char title[300];

    if (!es || !IsWindowReady()) return;

    if (es->file_path[0] != '\0') {
        snprintf(title, sizeof(title), "Super Mango Editor - %s%s",
                 es->file_path, es->modified ? " *" : "");
    } else {
        snprintf(title, sizeof(title), "Super Mango Editor%s",
                 es->modified ? " *" : "");
    }
    SetWindowTitle(title);
}

void editor_reset_new_level(EditorState *es)
{
    if (!es || !editor_finish_field_edit(es)) return;
    editor_retire_current_recovery(es);
    editor_level_init_defaults(&es->level);
    es->file_path[0] = '\0';
    memset(&es->source_fingerprint, 0, sizeof(es->source_fingerprint));
    es->source_state = EDITOR_SOURCE_UNKNOWN;
    es->recovery_original_path[0] = '\0';
    (void)editor_set_recovery_document(es, NULL);
    undo_clear(es->undo);
    editor_set_document_save_point(es);
    es->selection.index = -1;
    editor_set_status(es, "New level");
    editor_update_window_title(es);
}

int editor_can_persist(EditorState *es, const char *action)
{
    editor_validate_level(&es->level, &es->validation_report);
    if (es->validation_report.error_count > 0) {
        const char *msg = es->validation_report.message_count > 0
                        ? es->validation_report.messages[0]
                        : "validation failed";
        editor_set_status(es, "%s blocked: %s", action, msg);
        fprintf(stderr, "%s blocked: %s\n", action, msg);
        return 0;
    }
    return 1;
}

int editor_confirm_discard_changes(EditorState *es, const char *action)
{
    const char *buttons[] = {"Cancel", "Discard", "Save"};
    char message[256];
    int button_id = 0;

    if (!es || !editor_finish_field_edit(es)) return 0;
    if (!es->modified) return 1;

    snprintf(message, sizeof(message),
             "Unsaved changes will be lost. Discard changes and %s?", action);

    if (editor_test_discard_choice >= 0) {
        button_id = editor_test_discard_choice;
        editor_test_discard_choice = -1;
    } else if (dialog_choice("Unsaved Changes", message, buttons, 3, 2, 0, &button_id) != 0) {
        editor_set_status(es, "%s cancelled: confirmation failed", action);
        return 0;
    }

    if (button_id == 1) return 1;
    if (button_id == 2) {
        /* Destructive action proceeds only after a successful save. */
        return editor_save_current_level(es) == 0;
    }

    editor_set_status(es, "%s cancelled", action);
    return 0;
}

int editor_confirm_overwrite(EditorState *es, const char *path)
{
    const char *buttons[] = {"Cancel", "Replace"};
    char message[256];
    int button_id = 0;

    if (!es || !path) return 0;

    if (editor_test_overwrite_choice >= 0) {
        int result = editor_test_overwrite_choice;
        editor_test_overwrite_choice = -1;
        return result != 0;
    }

    snprintf(message, sizeof(message), "Replace existing file?\n%s", path);
    if (dialog_choice("Replace Existing File?", message, buttons, 2, 1, 0, &button_id) != 0) {
        editor_set_status(es, "Save failed: confirmation failed");
        return 0;
    }

    return button_id == 1;
}

EditorExternalChoice editor_confirm_external_change(EditorState *es)
{
    const char *buttons[] = {"Cancel", "Replace", "Save As"};
    int button_id = 0;

    if (!es) return EDITOR_EXTERNAL_CANCEL;
    if (editor_test_external_choice != (EditorExternalChoice)-1) {
        EditorExternalChoice result = editor_test_external_choice;
        editor_test_external_choice = (EditorExternalChoice)-1;
        return result;
    }
    if (dialog_choice("File Changed Externally", "The source file changed on disk. Replace it, save as another file, or cancel.",
                      buttons, 3, 2, 0, &button_id) != 0) {
        editor_set_status(es, "Save cancelled: external change confirmation failed");
        return EDITOR_EXTERNAL_CANCEL;
    }
    if (button_id == 1) return EDITOR_EXTERNAL_REPLACE;
    if (button_id == 2) return EDITOR_EXTERNAL_SAVE_AS;
    editor_set_status(es, "Save cancelled: file changed externally");
    return EDITOR_EXTERNAL_CANCEL;
}
