#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L /* fork, kill, pipe, sigaction under -std=c11 */
#endif

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shared/graphics.h"
#include <rlgl.h>  /* rlDrawRenderBatchActive */
#include "shared/text.h"

#include <time.h>

#ifdef _WIN32
#include <direct.h>
#include <process.h>
#include <io.h>
#include <sys/stat.h>
#include <sys/utime.h>  /* _utime: backdate a file */
#else
#include <utime.h>      /* utime: backdate a file */
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "editor/editor.h"
#include "editor/editor_clipboard.h"
#include "editor/editor_events.h"
#include "editor/editor_files.h"
#include "editor/editor_recovery.h"
#include "editor/editor_session.h"
#include "editor/editor_undo_apply.h"
#include "editor/editor_validation.h"
#include "editor/entity_meta.h"
#include "editor/file_dialog.h"
#include "shared/serializer.h"
#include "shared/serializer_emit.h"
#include "shared/serializer_io.h"
#include "test_folders.h"
#include "test_paths.h"  /* TEST_OUT scratch directory */
#include "check.h"  /* test_run_cases */
#include "editor/tools.h"
#include "editor/tools_internal.h"  /* delete_entity */
#include "editor/undo.h"
#include "shared/ui.h"
#include "editor/canvas.h"
#include "editor/editor_playtest.h"
#include "editor/editor_campaign.h"
#include "editor/properties.h"
#include "editor/hit_test.h"
#include "levels/level_loader.h"
#include "levels/level_validate.h"

#define EDITOR_WORKFLOW_LEVEL_PATH TEST_OUT "test_editor_workflow_level.toml"
#define EDITOR_WORKFLOW_RECENT_PATH TEST_OUT "editor_recent.txt"
#define EDITOR_TEST_AUTOSAVE_PATH TEST_OUT "autosave/test_editor_autosave.toml"
#define EDITOR_TEST_RECOVERY_DEST TEST_OUT "test_editor_recovery_destination.toml"
#define EDITOR_TEST_FAILED_TARGET TEST_OUT "test_editor_failed_target.toml"

static int expect_int(const char *name, int actual, int expected)
{
    if (actual != expected) {
        fprintf(stderr, "editor_validation_test: %s got %d expected %d\n",
                name, actual, expected);
        return 1;
    }
    return 0;
}

static int expect_string(const char *name, const char *actual,
                         const char *expected)
{
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "editor_validation_test: %s got '%s' expected '%s'\n",
                name, actual, expected);
        return 1;
    }
    return 0;
}

/* Compare a document path with `relative`, a path as a test wrote it. The
 * editor knows every document by its absolute path (the working folder
 * joined to a relative one), so that is what is compared. */
static int expect_document_path(const char *name, const char *actual,
                                const char *relative)
{
    char absolute[EDITOR_PATH_MAX];
    if (editor_absolute_path(relative, absolute, sizeof(absolute)) != 0) return 1;
    return expect_string(name, actual, absolute);
}

static int expect_prefix(const char *name, const char *actual,
                         const char *expected)
{
    size_t len = strlen(expected);

    if (strncmp(actual, expected, len) != 0) {
        fprintf(stderr, "editor_validation_test: %s got '%s' expected prefix '%s'\n",
                name, actual, expected);
        return 1;
    }
    return 0;
}

static int expect_float_value(const char *name, float actual, float expected)
{
    float diff = actual - expected;
    if (diff < 0.0f) diff = -diff;
    if (diff > 0.001f) {
        fprintf(stderr, "editor_validation_test: %s got %.3f expected %.3f\n",
                name, actual, expected);
        return 1;
    }
    return 0;
}

static void ensure_out_dir(void)
{
#ifdef _WIN32
    _mkdir(MANGO_TEST_OUTDIR);
#else
    mkdir(MANGO_TEST_OUTDIR, 0755);
#endif
}

static void ensure_autosave_dir(void)
{
#ifdef _WIN32
    _mkdir(TEST_OUT "autosave");
#else
    mkdir(TEST_OUT "autosave", 0755);
#endif
}

static void remove_directory(const char *path)
{
#ifdef _WIN32
    (void)_rmdir(path);
#else
    (void)rmdir(path);
#endif
}

static int make_test_preference_root(char *root, size_t root_size)
{
    unsigned long process_id;

#ifdef _WIN32
    process_id = (unsigned long)_getpid();
#else
    process_id = (unsigned long)getpid();
#endif
    if (snprintf(root, root_size, TEST_OUT "editor_pref_test_%lu", process_id) < 0)
        return -1;
    remove_directory(root);
#ifdef _WIN32
    return _mkdir(root);
#else
    return mkdir(root, 0755);
#endif
}

/* Delete the files one editor kept in its preference folder. */
static void remove_state_files(const EditorState *state)
{
    for (int i = 0; i < state->recovery_entry_count; i++) {
        remove(state->recovery_entries[i].metadata_path);
        remove(state->recovery_entries[i].snapshot_path);
    }
    remove(state->autosave_path);
    remove(state->playtest_path);
    remove(state->recent_path);
    remove(state->settings_path);
}

static void cleanup_test_preference_root(const char *root,
                                         EditorState *states, int state_count)
{
    if (!root || root[0] == '\0' || !states) return;
    for (int s = 0; s < state_count; s++) remove_state_files(&states[s]);
    remove_directory(root);
}

/* The same for editors that are not neighbours in one array. */
static void cleanup_test_preference_root_of(const char *root,
                                            EditorState *const *states, int state_count)
{
    if (!root || root[0] == '\0' || !states) return;
    for (int s = 0; s < state_count; s++) remove_state_files(states[s]);
    remove_directory(root);
}

static int path_stays_in_root(const char *root, const char *path)
{
    size_t root_length;

    if (!root || !path) return 0;
    root_length = strlen(root);
    return strncmp(root, path, root_length) == 0 &&
           (path[root_length] == '/' || path[root_length] == '\\');
}

static void serializer_temp_path(const char *path, char *buf, size_t buf_size)
{
#ifdef _WIN32
    snprintf(buf, buf_size, "%s.tmp.%lu", path, (unsigned long)_getpid());
#else
    snprintf(buf, buf_size, "%s.tmp.%ld", path, (long)getpid());
#endif
}

static void fill_valid_minimal(LevelDef *def)
{
    level_def_init_defaults(def);
    strncpy(def->name, "Validation Fixture", sizeof(def->name) - 1);
    def->screen_count = 1;
    strncpy(def->floor_tile_path, "assets/sprites/levels/grass_tileset.png",
            sizeof(def->floor_tile_path) - 1);
    def->last_star.x = 100.0f;
    def->last_star.y = 100.0f;
}

static int write_text_file(const char *path, const char *text)
{
    FILE *fp = fopen(path, "wb");
    int result;

    if (!fp) return -1;
    result = fputs(text, fp) == EOF ? -1 : 0;
    if (fclose(fp) != 0) result = -1;
    return result;
}

static int file_equals_text(const char *path, const char *expected)
{
    FILE *fp;
    char actual[256];
    size_t length;

    fp = fopen(path, "rb");
    if (!fp) return 0;
    length = fread(actual, 1, sizeof(actual) - 1, fp);
    if (ferror(fp) || !feof(fp)) {
        fclose(fp);
        return 0;
    }
    actual[length] = '\0';
    fclose(fp);
    return strcmp(actual, expected) == 0;
}

static int copy_file_with_bad_recovery_metadata(const char *source,
                                                const char *destination)
{
    FILE *src = fopen(source, "rb");
    FILE *dst;
    char buffer[512];
    size_t count;

    if (!src) return -1;
    dst = fopen(destination, "wb");
    if (!dst) {
        fclose(src);
        return -1;
    }
    if (fputs("# super_mango_recovery_path = \"unterminated\n", dst) == EOF) {
        fclose(src);
        fclose(dst);
        return -1;
    }
    while ((count = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        if (fwrite(buffer, 1, count, dst) != count) {
            fclose(src);
            fclose(dst);
            return -1;
        }
    }
    if (ferror(src) || fclose(src) != 0 || fclose(dst) != 0) return -1;
    return 0;
}

static int expect_location(const char *name, const LevelIssueLocation *where,
                           const char *path, int index, const char *field);


/* The cases live in tests/editor_validation/ and are included here so they stay
 * one program. Do not compile those files on their own.
 */
#include "editor_validation/rules.c"
#include "editor_validation/save.c"
#include "editor_validation/recovery.c"
#include "editor_validation/autosave.c"
#include "editor_validation/document.c"
#include "editor_validation/campaign.c"
#include "editor_validation/mutations.c"
#include "editor_validation/session.c"
#include "editor_validation/widgets.c"


/* The game binary path helper: the full path fits, and a buffer too small
 * for it is refused instead of truncated. */
static int playtest_binary_path_fits_or_is_refused(void)
{
    char binary_path[EDITOR_PATH_MAX];
    if (editor_playtest_binary_path(binary_path, sizeof(binary_path))) return 1;
    if (editor_playtest_binary_path(binary_path, 2) != -1) return 1;
    return 0;
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    ensure_out_dir();
    static const TestCase cases[] = {
#define CASE TEST_CASE
        CASE(compact_history_owns_config_snapshots),
        CASE(orphan_recovery_does_not_poison_discovery),
        CASE(undo_groups_stay_whole),
        CASE(group_steps_cost_one_slot_each),
        CASE(recovery_folder_stays_manageable),
        CASE(playtest_binary_path_fits_or_is_refused),
        CASE(invalid_drafts_do_not_build_unsafe_previews),
        CASE(accepts_valid_level),
        CASE(rejects_bad_count_and_path),
        CASE(rejects_missing_phase_and_layer_paths),
        CASE(rejects_bad_runtime_link),
        CASE(warns_without_blocking),
        CASE(rejects_unsafe_asset_paths),
        CASE(editor_checks_do_not_repeat_runtime_errors),
        CASE(save_and_load_resets_editor_session),
        CASE(save_refuses_a_path_with_control_characters),
        CASE(invalid_save_preserves_existing_file),
        CASE(atomic_save_replaces_and_preserves_on_injected_errors),
        CASE(save_policy_and_fingerprint_seams),
        CASE(dir_sync_failure_after_install_still_saves),
        CASE(unreadable_existing_probe_is_not_missing),
        CASE(recovery_entries_survive_restart_and_sessions),
        CASE(over_capacity_load_preserves_document),
        CASE(utf8_filename_roundtrip),
#ifdef _WIN32
        CASE(utf8_wide_path_conversion_roundtrip),
#endif
        CASE(failed_save_preserves_target_and_cleans_temp),
        CASE(autosave_recovery_preserves_destination),
        CASE(editor_save_workflows_enforce_baselines),
        CASE(stranded_replace_keeps_the_temporary_file),
        CASE(changed_on_disk_save_has_its_own_message),
        CASE(saved_levels_get_ordinary_permissions),
        CASE(symlinks_are_followed_only_for_the_opened_document),
        CASE(text_fields_drop_invalid_utf8),
        CASE(recovery_metadata_and_failed_save_contract),
        CASE(playtest_destination_isolated),
        CASE(autosave_backs_off_and_snapshots_last_valid_level),
        CASE(loads_recent_files_with_trim_and_limit),
        CASE(property_command_undo_redo),
        CASE(last_star_text_property_undo_redo),
        CASE(config_command_preserves_entity_edit),
        CASE(dirty_save_point_tracks_undo_redo),
        CASE(selection_structural_mutations_are_safe),
        CASE(checkpoint_editor_mutations_are_reversible),
        CASE(rail_deletion_keeps_references_valid),
        CASE(group_copy_and_delete_keep_riders_with_their_rail),
        CASE(shared_level_rules_have_one_answer),
        CASE(validation_errors_report_where_they_are),
        CASE(validation_reports_every_runtime_error),
        CASE(campaign_view_edits_and_saves_the_manifest),
        CASE(campaign_save_writes_everything_before_replacing),
        CASE(campaign_save_keeps_a_manifest_changed_by_hand),
        CASE(campaign_names_a_nameless_level_by_its_file),
        CASE(float_platform_rail_switch_rechecks_its_rail),
        CASE(drag_round_trips_and_follows_grab_point),
        CASE(editor_mutations_keep_level_valid),
        CASE(refused_mutations_explain_why),
        CASE(camera_scrolls_vertically_and_stays_clamped),
        CASE(playtest_blocks_editing_and_stop_cleans_up),
        CASE(load_fingerprints_the_bytes_it_parsed),
        CASE(documents_are_known_by_absolute_path),
        CASE(status_bar_names_files_usefully),
        CASE(recovery_metadata_keeps_longest_source_path),
        CASE(apply_loaded_level_rejects_long_path),
        CASE(recent_files_skip_overlong_lines_and_line_breaks),
        CASE(editor_settings_survive_a_restart),
        CASE(dialog_quoting_and_picked_paths_stay_literal),
        CASE(entity_table_has_a_row_for_every_type),
        CASE(document_hash_covers_every_entity_and_config_field),
        CASE(widget_commit_paths_preserve_values),
        CASE(config_preview_sync_preserves_old_texture),
        CASE(staged_edit_save_and_quit_boundaries),
        CASE(display_paths_keep_the_file_name),
        CASE(layer_buttons_record_their_own_undo_step),
        CASE(field_limits_apply_on_every_commit_path),
        CASE(leaving_a_field_applies_valid_values_silently),
        CASE(motion_fields_stay_within_validator_limits),
        CASE(open_dropdown_owns_the_next_click),
        CASE(dropdowns_accept_any_option_for_unknown_values),
        CASE(dropdown_near_the_bottom_opens_upward),
        CASE(new_level_resets_previews),
        CASE(rail_rider_paste_follows_its_rail),
        CASE(repeated_paste_steps_each_copy),
        CASE(paste_moves_like_a_drag),
        CASE(extreme_floats_round_trip_through_save),
        CASE(create_only_save_without_hard_links),
#undef CASE
    };
    return test_run_cases("editor_validation", cases, sizeof(cases) / sizeof(cases[0]),
                          (TestHooks){0}, argc, argv);
}
