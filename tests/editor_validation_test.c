#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L /* fork, kill, pipe, sigaction under -std=c11 */
#endif

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shared/graphics.h"
#include "shared/text.h"

#ifdef _WIN32
#include <direct.h>
#include <process.h>
#include <io.h>
#include <sys/stat.h>
#else
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
#include "test_paths.h"  /* TEST_OUT scratch directory */
#include "editor/tools.h"
#include "editor/undo.h"
#include "shared/ui.h"
#include "editor/canvas.h"
#include "editor/editor_playtest.h"
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

static void cleanup_test_preference_root(const char *root,
                                         EditorState *states, int state_count)
{
    if (!root || root[0] == '\0' || !states) return;
    for (int s = 0; s < state_count; s++) {
        for (int i = 0; i < states[s].recovery_entry_count; i++) {
            remove(states[s].recovery_entries[i].metadata_path);
            remove(states[s].recovery_entries[i].snapshot_path);
        }
        remove(states[s].autosave_path);
        remove(states[s].playtest_path);
        remove(states[s].recent_path);
    }
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

static int rejects_bad_runtime_link(void)
{
    LevelDef def;
    EditorValidationReport report;

    fill_valid_minimal(&def);
    def.spike_block_count = 1;
    def.spike_blocks[0].rail_index = 0;

    if (expect_int("bad link result", editor_validate_level(&def, &report), -1) != 0)
        return 1;
    /* The block names a rail that does not exist, and its speed was left
     * at 0: both are reported, the rail first. */
    if (expect_int("bad link errors", report.error_count, 2) != 0) return 1;
    if (expect_location("bad link first", &report.locations[0],
                        "spike_blocks", 0, "rail_index") != 0) return 1;

    return 0;
}

static int accepts_valid_level(void)
{
    LevelDef def;
    EditorValidationReport report;

    fill_valid_minimal(&def);

    if (expect_int("valid result", editor_validate_level(&def, &report), 0) != 0)
        return 1;
    if (expect_int("valid errors", report.error_count, 0) != 0) return 1;
    if (expect_int("valid warnings", report.warning_count, 0) != 0) return 1;

    return 0;
}

static int rejects_bad_count_and_path(void)
{
    LevelDef def;
    EditorValidationReport report;

    fill_valid_minimal(&def);
    def.coin_count = MAX_COINS + 1;
    strncpy(def.music_path, "assets/sounds/levels/missing.wav",
            sizeof(def.music_path) - 1);

    if (expect_int("invalid result", editor_validate_level(&def, &report), -1) != 0)
        return 1;
    if (expect_int("invalid errors", report.error_count, 2) != 0) return 1;

    return 0;
}

static int rejects_missing_phase_and_layer_paths(void)
{
    LevelDef def;
    EditorValidationReport report;

    fill_valid_minimal(&def);
    strncpy(def.next_phase, "levels/missing_next.toml", sizeof(def.next_phase) - 1);
    def.background_layer_count = 1;
    strncpy(def.background_layers[0].path,
            "assets/sprites/backgrounds/missing.png",
            sizeof(def.background_layers[0].path) - 1);
    def.foreground_layer_count = 1;
    strncpy(def.foreground_layers[0].path,
            "assets/sprites/foregrounds/missing.png",
            sizeof(def.foreground_layers[0].path) - 1);
    def.fog_layer_count = 1;
    strncpy(def.fog_layers[0].path,
            "assets/sprites/foregrounds/missing_fog.png",
            sizeof(def.fog_layers[0].path) - 1);

    if (expect_int("missing paths result", editor_validate_level(&def, &report), -1) != 0)
        return 1;
    if (expect_int("missing paths errors", report.error_count, 4) != 0) return 1;

    return 0;
}

static int warns_without_blocking(void)
{
    LevelDef def;
    EditorValidationReport report;

    fill_valid_minimal(&def);
    def.name[0] = '\0';

    if (expect_int("warning result", editor_validate_level(&def, &report), 0) != 0)
        return 1;
    if (expect_int("warning errors", report.error_count, 0) != 0) return 1;
    if (expect_int("warning count", report.warning_count, 1) != 0) return 1;

    return 0;
}

static int rejects_unsafe_asset_paths(void)
{
    LevelDef def;
    EditorValidationReport report;

    fill_valid_minimal(&def);
    strncpy(def.floor_tile_path, "/tmp/grass_tileset.png",
            sizeof(def.floor_tile_path) - 1);
    if (expect_int("unsafe floor path result",
                   editor_validate_level(&def, &report), -1) != 0)
        return 1;
    if (report.error_count < 1) {
        fprintf(stderr, "editor_validation_test: unsafe floor path should report error\n");
        return 1;
    }

    fill_valid_minimal(&def);
    def.platform_count = 1;
    def.platforms[0].x = 64.0f;
    def.platforms[0].tile_height = 1;
    def.platforms[0].tile_width = 1;
    strncpy(def.platforms[0].tile_path, "../secret.png",
            sizeof(def.platforms[0].tile_path) - 1);
    if (expect_int("unsafe platform path result",
                   editor_validate_level(&def, &report), -1) != 0)
        return 1;
    if (report.error_count < 1) {
        fprintf(stderr, "editor_validation_test: unsafe platform path should report error\n");
        return 1;
    }

    fill_valid_minimal(&def);
    strncpy(def.next_phase, "../levels/evil.toml", sizeof(def.next_phase) - 1);
    if (expect_int("unsafe next phase result",
                   editor_validate_level(&def, &report), -1) != 0)
        return 1;
    if (report.error_count < 1) {
        fprintf(stderr, "editor_validation_test: unsafe next phase should report error\n");
        return 1;
    }

    return 0;
}

static int save_and_load_resets_editor_session(void)
{
    EditorState es;
    Command cmd;
    char root[EDITOR_PATH_MAX] = {0};
    int result = 1;

    ensure_out_dir();
    remove(EDITOR_WORKFLOW_LEVEL_PATH);
    memset(&es, 0, sizeof(es));
    memset(&cmd, 0, sizeof(cmd));
    if (make_test_preference_root(root, sizeof(root)) != 0 ||
        editor_set_preference_root(&es, root) != 0) return 1;

    es.undo = undo_create();
    if (!es.undo) {
        fprintf(stderr, "editor_validation_test: undo_create failed\n");
        return 1;
    }

    editor_level_init_defaults(&es.level);
    strncpy(es.level.name, "Workflow Fixture", sizeof(es.level.name) - 1);
    es.level.coin_count = 1;
    es.level.coins[0].x = 64.0f;
    es.level.coins[0].y = 96.0f;
    strncpy(es.file_path, EDITOR_WORKFLOW_LEVEL_PATH,
            sizeof(es.file_path) - 1);
    strncpy(es.recent_path, EDITOR_WORKFLOW_RECENT_PATH,
            sizeof(es.recent_path) - 1);
    es.source_state = EDITOR_SOURCE_EXPECTED_MISSING;
    es.modified = 1;
    if (editor_init_persistence_paths(&es) != 0) goto cleanup;
    es.selection.type = ENT_COIN;
    es.selection.index = 0;

    cmd.type = CMD_PLACE;
    cmd.entity_type = ENT_COIN;
    cmd.entity_index = 0;
    cmd.after.coin = es.level.coins[0];
    undo_push(es.undo, &cmd);

    if (editor_save_current_level(&es) != 0) goto cleanup;
    if (expect_int("saved modified", es.modified, 0) != 0) goto cleanup;
    if (expect_prefix("save status", es.status_message, "Saved ") != 0)
        goto cleanup;
    if (expect_int("saved file exists",
                   editor_file_exists(EDITOR_WORKFLOW_LEVEL_PATH), 1) != 0)
        goto cleanup;

    editor_level_init_defaults(&es.level);
    es.modified = 1;
    es.selection.index = 7;
    undo_push(es.undo, &cmd);
    if (expect_int("dirty undo count", es.undo->top, 2) != 0) goto cleanup;

    if (editor_load_level(&es, EDITOR_WORKFLOW_LEVEL_PATH) != 0) goto cleanup;
    if (expect_string("loaded file path", es.file_path,
                      EDITOR_WORKFLOW_LEVEL_PATH) != 0) goto cleanup;
    if (expect_string("loaded level name", es.level.name,
                      "Workflow Fixture") != 0) goto cleanup;
    if (expect_int("loaded coin count", es.level.coin_count, 1) != 0)
        goto cleanup;
    if (expect_int("loaded modified", es.modified, 0) != 0) goto cleanup;
    if (expect_int("loaded selection", es.selection.index, -1) != 0)
        goto cleanup;
    if (expect_int("loaded undo count", es.undo->top, 0) != 0)
        goto cleanup;
    if (expect_int("recent count after load", es.recent_file_count, 1) != 0)
        goto cleanup;
    if (expect_string("recent first after load", es.recent_files[0],
                      EDITOR_WORKFLOW_LEVEL_PATH) != 0) goto cleanup;

    result = 0;

cleanup:
    undo_destroy(es.undo);
    cleanup_test_preference_root(root, &es, 1);
    remove(EDITOR_WORKFLOW_LEVEL_PATH);
    return result;
}

static int failed_save_preserves_target_and_cleans_temp(void)
{
    EditorState es;
    char sentinel_path[256];
    char temp_path[256];
    FILE *fp;

    ensure_out_dir();
    remove(EDITOR_TEST_FAILED_TARGET);
    snprintf(sentinel_path, sizeof(sentinel_path), "%s/sentinel",
             EDITOR_TEST_FAILED_TARGET);
    remove(sentinel_path);
    remove_directory(EDITOR_TEST_FAILED_TARGET);
#ifdef _WIN32
    if (_mkdir(EDITOR_TEST_FAILED_TARGET) != 0) return 1;
#else
    if (mkdir(EDITOR_TEST_FAILED_TARGET, 0755) != 0) return 1;
#endif
    fp = fopen(sentinel_path, "w");
    if (!fp) {
        remove_directory(EDITOR_TEST_FAILED_TARGET);
        return 1;
    }
    fputs("original target data\n", fp);
    fclose(fp);

    memset(&es, 0, sizeof(es));
    editor_level_init_defaults(&es.level);
    strncpy(es.file_path, EDITOR_TEST_FAILED_TARGET, sizeof(es.file_path) - 1);
    es.source_state = EDITOR_SOURCE_EXPECTED_EXISTING;
    es.modified = 1;
    serializer_temp_path(EDITOR_TEST_FAILED_TARGET, temp_path,
                         sizeof(temp_path));
    remove(temp_path);

    if (expect_int("failed save result", editor_save_current_level(&es), -1) != 0)
        goto cleanup;
    if (expect_int("failed save stays dirty", es.modified, 1) != 0) goto cleanup;
    if (expect_string("failed save keeps path", es.file_path,
                      EDITOR_TEST_FAILED_TARGET) != 0) goto cleanup;
    if (expect_prefix("failed save status", es.status_message,
                      "Save failed: ") != 0) goto cleanup;
    if (expect_int("failed save target preserved", editor_file_exists(sentinel_path), 1) != 0)
        goto cleanup;
    if (expect_int("failed save temp removed", editor_file_exists(temp_path), 0) != 0)
        goto cleanup;

    remove(sentinel_path);
    remove_directory(EDITOR_TEST_FAILED_TARGET);
    return 0;

cleanup:
    remove(temp_path);
    remove(sentinel_path);
    remove_directory(EDITOR_TEST_FAILED_TARGET);
    return 1;
}

static int atomic_save_replaces_and_preserves_on_injected_errors(void)
{
    const char *target = TEST_OUT "editor_atomic_target.toml";
    const char *sentinel = "exact original bytes\n";
    char temp_path[256];
    LevelDef def;

    ensure_out_dir();
    fill_valid_minimal(&def);
    serializer_temp_path(target, temp_path, sizeof(temp_path));
    remove(target);
    remove(temp_path);

    if (write_text_file(target, sentinel) != 0) return 1;
    if (expect_int("atomic successful save", level_save_toml(&def, target), 0) != 0)
        goto cleanup;
    if (expect_int("atomic target replaced", file_equals_text(target, sentinel), 0) != 0)
        goto cleanup;
    if (expect_int("atomic successful temp removed",
                   editor_file_exists(temp_path), 0) != 0)
        goto cleanup;

    if (write_text_file(target, sentinel) != 0) goto cleanup;
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_WRITE);
    if (expect_int("injected write failure", level_save_toml(&def, target), -1) != 0)
        goto reset_failure;
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_NONE);
    if (expect_int("write failure exact target", file_equals_text(target, sentinel), 1) != 0)
        goto cleanup;
    if (expect_int("write failure temp removed",
                   editor_file_exists(temp_path), 0) != 0)
        goto cleanup;

    if (write_text_file(target, sentinel) != 0) goto cleanup;
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_FLUSH);
    if (expect_int("injected flush failure", level_save_toml(&def, target), -1) != 0)
        goto reset_failure;
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_NONE);
    if (expect_int("flush failure exact target", file_equals_text(target, sentinel), 1) != 0)
        goto cleanup;
    if (expect_int("flush failure temp removed",
                   editor_file_exists(temp_path), 0) != 0)
        goto cleanup;

    remove(target);
    return 0;

reset_failure:
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_NONE);
cleanup:
    remove(target);
    remove(temp_path);
    return 1;
}

static int save_policy_and_fingerprint_seams(void)
{
    const char *target = TEST_OUT "editor_save_policy_target.toml";
    const char *external = "external bytes\n";
    const char *sentinel = "existing target\n";
    LevelDef def;
    SerializerFileFingerprint fingerprint;

    ensure_out_dir();
    fill_valid_minimal(&def);
    remove(target);
    if (level_save_toml(&def, target) != 0) return 1;
    if (serializer_fingerprint_utf8(target, &fingerprint) != 1) goto fail;
    if (write_text_file(target, external) != 0) goto fail;
    if (expect_int("external source recheck",
                   level_save_toml_checked(&def, target,
                                           SERIALIZER_SAVE_REPLACE,
                                           &fingerprint), -2) != 0)
        goto fail;
    if (expect_int("external bytes preserved",
                   file_equals_text(target, external), 1) != 0) goto fail;

    if (write_text_file(target, sentinel) != 0) goto fail;
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_TARGET_APPEARED);
    if (expect_int("create-only target appearance",
                   level_save_toml_with_policy(&def, target,
                                               SERIALIZER_SAVE_CREATE_ONLY), -1) != 0)
        goto reset_failure;
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_NONE);
    if (expect_int("create-only preserves appeared target",
                   file_equals_text(target, sentinel), 1) != 0) goto fail;

    remove(target);
    return 0;

reset_failure:
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_NONE);
fail:
    remove(target);
    return 1;
}

/*
 * A folder fsync that fails after rename/link has already installed the
 * file is a warning, not a failed save.  Otherwise the editor would keep
 * the old baseline, and a create-only Save As would fail on every retry
 * because its target now exists.
 */
static int dir_sync_failure_after_install_still_saves(void)
{
#ifdef _WIN32
    return 0;   /* Windows uses MOVEFILE_WRITE_THROUGH; no folder sync. */
#else
    const char *replaced = TEST_OUT "editor_dir_sync_replace.toml";
    const char *created = TEST_OUT "editor_dir_sync_create.toml";
    EditorState es = {0};
    LevelDef def;
    LevelDef reloaded;
    SerializerFileFingerprint on_disk;
    char root[EDITOR_PATH_MAX] = {0};
    int result = 1;

    ensure_out_dir();
    remove(replaced);
    remove(created);
    fill_valid_minimal(&def);
    if (level_save_toml(&def, replaced) != 0) return 1;
    strncpy(def.name, "Synced later", sizeof(def.name) - 1);
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_DIR_SYNC);
    if (expect_int("replace with failed folder sync",
                   level_save_toml(&def, replaced), 0) != 0 ||
        expect_int("replaced file reloads", level_load_toml(replaced, &reloaded), 0) != 0 ||
        expect_string("replaced file updated", reloaded.name, "Synced later") != 0)
        goto cleanup;

    /* Editor Save As (create-only) keeps a correct baseline. */
    if (make_test_preference_root(root, sizeof(root)) != 0 ||
        editor_set_preference_root(&es, root) != 0 ||
        editor_init_persistence_paths(&es) != 0) goto cleanup;
    fill_valid_minimal(&es.level);
    es.modified = 1;
    file_dialog_test_set_save_result(FILE_DIALOG_SELECTED, created);
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_DIR_SYNC);
    if (expect_int("create with failed folder sync",
                   editor_save_current_level_as(&es), 0) != 0 ||
        expect_int("baseline tracks created file",
                   es.source_state, EDITOR_SOURCE_EXPECTED_EXISTING) != 0 ||
        expect_int("created fingerprint", serializer_fingerprint_utf8(created, &on_disk), 1) != 0 ||
        expect_int("baseline matches created bytes",
                   serializer_fingerprint_equal(&es.source_fingerprint, &on_disk), 1) != 0)
        goto cleanup;
    es.level.coin_score++;
    es.modified = 1;
    if (expect_int("next save uses the baseline", editor_save_current_level(&es), 0) != 0)
        goto cleanup;
    result = 0;

cleanup:
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_NONE);
    file_dialog_test_set_save_result(-1, NULL);
    cleanup_test_preference_root(root, &es, 1);
    remove(replaced);
    remove(created);
    return result;
#endif
}

static int unreadable_existing_probe_is_not_missing(void)
{
    const char *target = TEST_OUT "editor_unreadable_target.toml";
    SerializerPathStatus status;

    ensure_out_dir();
    if (write_text_file(target, "unreadable\n") != 0) return 1;
#ifdef _WIN32
    _chmod(target, 0);
#else
    chmod(target, 0000);
#endif
    status = serializer_probe_path_utf8(target);
#ifdef _WIN32
    _chmod(target, _S_IREAD | _S_IWRITE);
#else
    chmod(target, 0644);
#endif
    remove(target);
    return expect_int("unreadable existing probe", status,
                      SERIALIZER_PATH_EXISTING);
}

static int recovery_entries_survive_restart_and_sessions(void)
{
    const char *source = TEST_OUT "editor_manifest_source.toml";
    EditorState first = {0};
    EditorState second = {0};
    EditorState restarted = {0};
    char root[EDITOR_PATH_MAX];
    char first_snapshot[EDITOR_PATH_MAX] = {0};
    char second_snapshot[EDITOR_PATH_MAX] = {0};
    char malformed[EDITOR_PATH_MAX];
    char blocked_root[EDITOR_PATH_MAX] = {0};
    int result = 1;

    ensure_out_dir();
    if (make_test_preference_root(root, sizeof(root)) != 0) return 1;
    remove(source);
    if (write_text_file(source, "name = \"manifest source\"\nscreen_count = 1\n") != 0)
        goto cleanup;
    strncpy(first.file_path, source, sizeof(first.file_path) - 1);
    strncpy(second.file_path, source, sizeof(second.file_path) - 1);
    editor_level_init_defaults(&first.level);
    editor_level_init_defaults(&second.level);
    first.modified = 1;
    second.modified = 1;
    first.last_autosave_ms = (uint32_t)clock_millis() - 30001u;
    second.last_autosave_ms = (uint32_t)clock_millis() - 30001u;
    if (editor_set_preference_root(&first, root) != 0 ||
        editor_set_preference_root(&second, root) != 0 ||
        editor_init_persistence_paths(&first) != 0 ||
        editor_init_persistence_paths(&second) != 0) goto cleanup;
    /* Same-size EDITOR_PATH_MAX arrays: copy the whole terminated path. */
    memcpy(first_snapshot, first.autosave_path, sizeof(first_snapshot));
    memcpy(second_snapshot, second.autosave_path, sizeof(second_snapshot));
    editor_maybe_autosave(&first);
    editor_maybe_autosave(&second);
    if (expect_int("session snapshots differ",
                   strcmp(first_snapshot, second_snapshot) != 0, 1) != 0)
        goto cleanup;
    if (expect_int("first snapshot root", path_stays_in_root(root, first_snapshot), 1) != 0 ||
        expect_int("second snapshot root", path_stays_in_root(root, second_snapshot), 1) != 0 ||
        expect_int("first metadata root",
                   path_stays_in_root(root, first.recovery_entries[0].metadata_path), 1) != 0)
        goto cleanup;

    if (editor_set_preference_root(&restarted, root) != 0 ||
        editor_init_persistence_paths(&restarted) != 0 ||
        expect_int("restart recovery count", restarted.recovery_entry_count, 2) != 0)
        goto cleanup;
    if (expect_int("restart source identity",
                   strcmp(restarted.recovery_entries[0].source_path, source) == 0 ||
                   strcmp(restarted.recovery_entries[1].source_path, source) == 0,
                   1) != 0)
        goto cleanup;

    {
        uint64_t before_hash = editor_document_hash(&restarted.level);
        char before_path[EDITOR_PATH_MAX];
        memcpy(before_path, restarted.file_path, sizeof(before_path));
        editor_test_set_recovery_choice(0);
        if (expect_int("recovery picker cancel", editor_choose_recovery(&restarted), -1) != 0 ||
            expect_int("recovery picker leaves document",
                       editor_document_hash(&restarted.level) == before_hash &&
                       strcmp(restarted.file_path, before_path) == 0, 1) != 0)
            goto cleanup;
    }

    /* snprintf returns the length it needed; a temp root too long for the
     * buffer would test the wrong file, so treat that as a failure. */
    if (snprintf(malformed, sizeof(malformed), "%s/editor_recovery_bad.meta",
                 root) >= (int)sizeof(malformed)) goto cleanup;
    if (write_text_file(malformed, "not-a-recovery-record\n") != 0) goto cleanup;
    if (expect_int("malformed recovery ignored",
                   editor_discover_recoveries(&restarted), 0) != 0 ||
        expect_int("malformed recovery leaves valid entries",
                   restarted.recovery_entry_count, 2) != 0)
        goto cleanup;

    file_dialog_test_set_open_result(FILE_DIALOG_CANCELLED, NULL);
    editor_open_level_file(&restarted);
    if (expect_int("cancelled picker preserves recovery",
                   restarted.recovery_entry_count, 2) != 0)
        goto cleanup;
    file_dialog_test_set_open_result(FILE_DIALOG_SELECTED,
                                     TEST_OUT "editor_missing_open.toml");
    editor_open_level_file(&restarted);
    if (expect_int("failed open preserves recovery",
                   restarted.recovery_entry_count, 2) != 0)
        goto cleanup;

    {
        InputEvent recovery_event;
        char modal_save_path[] = TEST_OUT "editor_recovery_modal_save.toml";
        remove(modal_save_path);
        editor_level_init_defaults(&restarted.level);
        restarted.modified = 1;
        editor_test_set_recovery_choice(1);
        editor_test_set_discard_choice(2);
        file_dialog_test_set_save_result(FILE_DIALOG_SELECTED, modal_save_path);
        memset(&recovery_event, 0, sizeof(recovery_event));
        recovery_event.type = INPUT_KEY_DOWN;
        recovery_event.key = KEY_R;
        recovery_event.mods = INPUT_CTRL;
        editor_handle_event(&restarted, &recovery_event);
        if (expect_int("save during recovery selection preserves entries",
                       restarted.recovery_entry_count, 2) != 0 ||
            expect_string("save during recovery resolves selected source",
                          restarted.file_path, source) != 0)
            goto cleanup;
        remove(modal_save_path);
    }

    if (snprintf(blocked_root, sizeof(blocked_root), "%s/not-a-directory",
                 root) >= (int)sizeof(blocked_root)) goto cleanup;
    if (write_text_file(blocked_root, "block discovery\n") != 0) goto cleanup;
    {
        char saved_root[EDITOR_PATH_MAX];
        memcpy(saved_root, restarted.recovery_root_path, sizeof(saved_root));
        memcpy(restarted.recovery_root_path, blocked_root,
               sizeof(restarted.recovery_root_path));
        editor_retire_current_recovery(&restarted);
        memcpy(restarted.recovery_root_path, saved_root, sizeof(saved_root));
    }
    if (expect_int("discovery failure preserves snapshot",
                   editor_file_exists(first_snapshot), 1) != 0)
        goto cleanup;

    editor_retire_current_recovery(&first);
    editor_retire_current_recovery(&second);
    result = 0;

cleanup:
    file_dialog_test_set_open_result(-1, NULL);
    file_dialog_test_set_save_result(-1, NULL);
    editor_test_set_recovery_choice(-1);
    editor_test_set_discard_choice(-1);
    if (blocked_root[0] != '\0') remove(blocked_root);
    cleanup_test_preference_root(root, (EditorState[]){first, second, restarted}, 3);
    remove(source);
    remove(malformed);
    return result;
}

static int over_capacity_load_preserves_document(void)
{
    EditorState es = {0};
    char path[EDITOR_PATH_MAX + 32];
    LevelDef before;

    editor_level_init_defaults(&es.level);
    strcpy(es.level.name, "untouched");
    before = es.level;
    memset(path, 'x', sizeof(path) - 1);
    path[sizeof(path) - 1] = '\0';
    if (expect_int("over-capacity load", editor_load_level(&es, path), -1) != 0)
        return 1;
    if (expect_int("over-capacity document untouched",
                   memcmp(&es.level, &before, sizeof(before)) == 0, 1) != 0)
        return 1;
    return 0;
}

static int utf8_filename_roundtrip(void)
{
    const char *path = TEST_OUT "editor_é测试_🍊.toml";
    LevelDef before;
    LevelDef after;

    ensure_out_dir();
    remove(path);
    fill_valid_minimal(&before);
    memset(&after, 0, sizeof(after));
    if (expect_int("UTF-8 save", level_save_toml(&before, path), 0) != 0)
        goto cleanup;
    if (expect_int("UTF-8 exists", editor_file_exists(path), 1) != 0)
        goto cleanup;
    if (expect_int("UTF-8 load", level_load_toml(path, &after), 0) != 0)
        goto cleanup;
    if (expect_string("UTF-8 roundtrip name", after.name, before.name) != 0)
        goto cleanup;

    remove(path);
    return 0;

cleanup:
    remove(path);
    return 1;
}

#ifdef _WIN32
static int utf8_wide_path_conversion_roundtrip(void)
{
    const char *original = TEST_OUT "editor_é测试_🍊";
    wchar_t *wide = serializer_utf8_to_wide(original);
    char *roundtrip = wide ? serializer_wide_to_utf8(wide) : NULL;
    int result = expect_int("UTF-8 wide path conversion",
                            roundtrip && strcmp(roundtrip, original) == 0, 1);

    free(roundtrip);
    free(wide);
    return result;
}
#endif

static int recovery_metadata_and_failed_save_contract(void)
{
    const char *recovery = TEST_OUT "editor_recovery_contract.toml";
    const char *valid = TEST_OUT "editor_recovery_valid.toml";
    const char *target = TEST_OUT "editor_recovery_target.toml";
    EditorState es;
    EditorState recovered;
    char metadata[EDITOR_PATH_MAX];

    ensure_out_dir();
    remove(recovery);
    remove(valid);
    remove(target);
    memset(&es, 0, sizeof(es));
    editor_level_init_defaults(&es.level);
    strncpy(es.file_path, target, sizeof(es.file_path) - 1);
    strncpy(es.autosave_path, recovery, sizeof(es.autosave_path) - 1);
    /* This fixture owns an absent destination; reach the injected write failure
     * instead of relying on a headless native confirmation to fail first. */
    es.source_state = EDITOR_SOURCE_EXPECTED_MISSING;

    if (level_save_toml(&es.level, valid) != 0 ||
        level_save_toml_recovery(&es.level, recovery, target) != 0)
        goto cleanup;
    if (level_read_recovery_path(recovery, metadata, sizeof(metadata)) != 1 ||
        expect_string("recovery metadata", metadata, target) != 0)
        goto cleanup;
    editor_retire_matching_recovery(&es, TEST_OUT "other-document.toml");
    if (expect_int("stale recovery retained", editor_file_exists(recovery), 1) != 0)
        goto cleanup;
    if (copy_file_with_bad_recovery_metadata(valid, recovery) != 0)
        goto cleanup;
    memset(&recovered, 0, sizeof(recovered));
    strncpy(recovered.autosave_path, recovery,
            sizeof(recovered.autosave_path) - 1);
    if (expect_int("malformed metadata recovery", editor_recover_autosave(&recovered), 0) != 0)
        goto cleanup;
    if (expect_int("malformed metadata untitled", recovered.file_path[0], '\0') != 0)
        goto cleanup;
    if (expect_int("malformed metadata dirty", recovered.modified, 1) != 0)
        goto cleanup;

    if (level_save_toml_recovery(&es.level, recovery, target) != 0)
        goto cleanup;
    es.modified = 1;
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_WRITE);
    if (expect_int("failed explicit save", editor_save_current_level(&es), -1) != 0) {
        serializer_test_set_failure(SERIALIZER_TEST_FAILURE_NONE);
        goto cleanup;
    }
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_NONE);
    if (expect_int("failed save keeps recovery", editor_file_exists(recovery), 1) != 0)
        goto cleanup;

    editor_retire_matching_recovery(&es, target);
    if (expect_int("matching recovery retired", editor_file_exists(recovery), 0) != 0)
        goto cleanup;

    remove(valid);
    remove(target);
    return 0;

cleanup:
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_NONE);
    remove(recovery);
    remove(valid);
    remove(target);
    return 1;
}

static int playtest_destination_isolated(void)
{
    EditorState es;
    EditorState second = {0};
    char root[EDITOR_PATH_MAX];
    char playtest_path[EDITOR_PATH_MAX];
    uint64_t saved_hash;
    int saved_modified;

    memset(&es, 0, sizeof(es));
    editor_level_init_defaults(&es.level);
    if (make_test_preference_root(root, sizeof(root)) != 0) return 1;
    if (editor_set_preference_root(&es, root) != 0) goto cleanup;
    strncpy(es.file_path, TEST_OUT "active_editor_document.toml",
            sizeof(es.file_path) - 1);
    es.modified = 1;
    es.saved_document_hash = editor_document_hash(&es.level);
    es.saved_document_hash_valid = 1;
    saved_hash = es.saved_document_hash;
    saved_modified = es.modified;
    remove(es.file_path);

    if (editor_init_persistence_paths(&es) != 0 ||
        editor_prepare_playtest_level(&es, playtest_path,
                                      sizeof(playtest_path)) != 0)
        goto cleanup;
    memset(&second, 0, sizeof(second));
    if (editor_set_preference_root(&second, root) != 0 ||
        editor_init_persistence_paths(&second) != 0 ||
        expect_int("untitled recovery paths unique",
                   strcmp(es.autosave_path, second.autosave_path) == 0, 0) != 0)
        goto cleanup;
    if (expect_int("playtest differs from active",
                   strcmp(playtest_path, es.file_path) == 0, 0) != 0)
        goto cleanup;
    if (expect_int("playtest stays in preference root",
                   path_stays_in_root(root, playtest_path), 1) != 0)
        goto cleanup;
    if (expect_int("playtest avoids legacy destination",
                   strcmp(playtest_path, "levels/_playtest.toml") == 0, 0) != 0)
        goto cleanup;
    if (expect_int("playtest file exists", editor_file_exists(playtest_path), 1) != 0)
        goto cleanup;
    if (expect_int("playtest preserves dirty", es.modified, saved_modified) != 0)
        goto cleanup;
    if (expect_int("playtest preserves save point",
                   es.saved_document_hash == saved_hash, 1) != 0)
        goto cleanup;
    if (expect_int("playtest does not create active file",
                   editor_file_exists(es.file_path), 0) != 0)
        goto cleanup;

    editor_retire_playtest_level(&es);
    if (expect_int("playtest retired", editor_file_exists(playtest_path), 0) != 0)
        goto cleanup;
    cleanup_test_preference_root(root, (EditorState[]){es, second}, 2);
    return 0;

cleanup:
    editor_retire_playtest_level(&es);
    editor_retire_playtest_level(&second);
    cleanup_test_preference_root(root, (EditorState[]){es, second}, 2);
    remove(es.file_path);
    return 1;
}

static int invalid_save_preserves_existing_file(void)
{
    EditorState es;
    char temp_path[256];
    char content[64];
    FILE *fp;

    ensure_out_dir();
    remove(EDITOR_TEST_FAILED_TARGET);
    fp = fopen(EDITOR_TEST_FAILED_TARGET, "w");
    if (!fp) return 1;
    fputs("original file data\n", fp);
    fclose(fp);

    memset(&es, 0, sizeof(es));
    editor_level_init_defaults(&es.level);
    es.level.coin_count = MAX_COINS + 1;
    strncpy(es.file_path, EDITOR_TEST_FAILED_TARGET, sizeof(es.file_path) - 1);
    es.modified = 1;
    serializer_temp_path(EDITOR_TEST_FAILED_TARGET, temp_path,
                         sizeof(temp_path));
    remove(temp_path);

    if (expect_int("invalid save result", editor_save_current_level(&es), -1) != 0)
        goto cleanup;
    if (expect_int("invalid save stays dirty", es.modified, 1) != 0)
        goto cleanup;
    if (expect_prefix("invalid save status", es.status_message,
                      "Save blocked: ") != 0)
        goto cleanup;
    fp = fopen(EDITOR_TEST_FAILED_TARGET, "r");
    if (!fp || !fgets(content, sizeof(content), fp)) {
        if (fp) fclose(fp);
        goto cleanup;
    }
    fclose(fp);
    if (expect_string("invalid save preserves file", content,
                      "original file data\n") != 0)
        goto cleanup;
    if (expect_int("invalid save no temp", editor_file_exists(temp_path), 0) != 0)
        goto cleanup;

    remove(EDITOR_TEST_FAILED_TARGET);
    return 0;

cleanup:
    remove(temp_path);
    remove(EDITOR_TEST_FAILED_TARGET);
    return 1;
}

static int autosave_recovery_preserves_destination(void)
{
    const char *destination = EDITOR_TEST_RECOVERY_DEST;
    const char *save_as_path = TEST_OUT "editor_recovered_save_as.toml";
    EditorState es = {0};
    EditorState recovered = {0};
    char root[EDITOR_PATH_MAX] = {0};
    uint64_t recovery_id;

    ensure_out_dir();
    remove(destination);
    remove(save_as_path);
    if (make_test_preference_root(root, sizeof(root)) != 0) return 1;

    editor_level_init_defaults(&es.level);
    es.level.coin_count = 1;
    es.level.coins[0].x = 64.0f;
    es.level.coins[0].y = 96.0f;
    strncpy(es.file_path, destination, sizeof(es.file_path) - 1);
    if (editor_set_preference_root(&es, root) != 0 ||
        editor_init_persistence_paths(&es) != 0) goto cleanup;
    es.modified = 1;
    es.last_autosave_ms = (uint32_t)clock_millis() - 30001u;

    editor_maybe_autosave(&es);
    if (expect_int("autosave exists", editor_file_exists(es.autosave_path), 1) != 0)
        goto cleanup;
    if (expect_int("autosave remains dirty", es.modified, 1) != 0) goto cleanup;
    if (expect_string("autosave keeps destination", es.file_path,
                      destination) != 0) goto cleanup;
    if (expect_prefix("autosave status", es.status_message,
                      "Autosaved recovery copy") != 0) goto cleanup;
    if (expect_int("autosave does not create destination",
                   editor_file_exists(EDITOR_TEST_RECOVERY_DEST), 0) != 0)
        goto cleanup;

    if (editor_set_preference_root(&recovered, root) != 0 ||
        editor_init_persistence_paths(&recovered) != 0 ||
        expect_int("recovery discovery", recovered.recovery_entry_count, 1) != 0)
        goto cleanup;
    recovery_id = recovered.recovery_entries[0].id;
    if (expect_int("recover result", editor_recover_entry_by_id(&recovered,
                                                                  recovery_id), 0) != 0)
        goto cleanup;
    if (expect_int("recovered stays dirty", recovered.modified, 1) != 0)
        goto cleanup;
    if (expect_string("recovered destination", recovered.file_path,
                      destination) != 0) goto cleanup;
    if (expect_int("recovery not recent", recovered.recent_file_count, 0) != 0)
        goto cleanup;
    if (expect_prefix("recovery status", recovered.status_message,
                      "Recovered unsaved changes") != 0) goto cleanup;

    if (expect_int("recovered source unknown",
                   recovered.source_state, EDITOR_SOURCE_UNKNOWN) != 0)
        goto cleanup;
    recovered.level.coins[0].x = 128.0f;
    file_dialog_test_set_save_result(FILE_DIALOG_SELECTED, save_as_path);
    if (editor_save_current_level_as(&recovered) != 0) goto cleanup;
    if (expect_int("recovered Save As clean", recovered.modified, 0) != 0)
        goto cleanup;
    if (expect_int("normal save retires recovery copy",
                   editor_file_exists(es.autosave_path), 0) != 0)
        goto cleanup;

    cleanup_test_preference_root(root, (EditorState[]){es, recovered}, 2);
    remove(destination);
    remove(save_as_path);
    return 0;

cleanup:
    file_dialog_test_set_save_result(-1, NULL);
    cleanup_test_preference_root(root, (EditorState[]){es, recovered}, 2);
    remove(destination);
    remove(save_as_path);
    return 1;
}

static int editor_save_workflows_enforce_baselines(void)
{
    const char *existing = TEST_OUT "editor_save_workflow_existing.toml";
    const char *appearing = TEST_OUT "editor_save_workflow_appearing.toml";
    EditorState es = {0};
    EditorState create_only = {0};
    char root[EDITOR_PATH_MAX] = {0};
    int result = 1;

    ensure_out_dir();
    remove(existing);
    remove(appearing);
    if (write_text_file(existing, "old bytes\n") != 0 ||
        make_test_preference_root(root, sizeof(root)) != 0) goto cleanup;

    editor_level_init_defaults(&es.level);
    es.modified = 1;
    if (editor_set_preference_root(&es, root) != 0 ||
        editor_init_persistence_paths(&es) != 0) goto cleanup;
    file_dialog_test_set_save_result(FILE_DIALOG_SELECTED, existing);
    editor_test_set_overwrite_choice(1);
    if (expect_int("editor Save As existing", editor_save_current_level_as(&es), 0) != 0 ||
        expect_int("editor Save As existing clean", es.modified, 0) != 0)
        goto cleanup;

    es.level.coin_score++;
    if (write_text_file(existing, "external bytes\n") != 0) goto cleanup;
    editor_test_set_external_choice(EDITOR_EXTERNAL_REPLACE);
    if (expect_int("editor normal external replace",
                   editor_save_current_level(&es), 0) != 0)
        goto cleanup;

    editor_level_init_defaults(&create_only.level);
    create_only.modified = 1;
    if (editor_set_preference_root(&create_only, root) != 0 ||
        editor_init_persistence_paths(&create_only) != 0) goto cleanup;
    file_dialog_test_set_save_result(FILE_DIALOG_SELECTED, appearing);
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_TARGET_APPEARED);
    if (expect_int("editor Save As create-only race",
                   editor_save_current_level_as(&create_only), -1) != 0 ||
        expect_int("create-only race keeps untitled",
                   create_only.file_path[0], '\0') != 0)
        goto cleanup;
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_NONE);
    result = 0;

cleanup:
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_NONE);
    file_dialog_test_set_save_result(-1, NULL);
    editor_test_set_overwrite_choice(-1);
    editor_test_set_external_choice((EditorExternalChoice)-1);
    cleanup_test_preference_root(root, (EditorState[]){es, create_only}, 2);
    remove(existing);
    remove(appearing);
    return result;
}

/*
 * When Windows' ReplaceFileW moves the old file away but cannot move the new
 * one in, the temporary file is the only copy of the level.  The save must
 * keep it (never "clean it up"), and the editor must say where it is.  The
 * test seam reports that failure without touching either file.
 */
/*
 * A checked save that finds the file changed on disk at the last moment
 * writes nothing and returns -2.  The status bar used to call that a plain
 * "Save failed"; it now says the file changed and how to go on.
 */
static int changed_on_disk_save_has_its_own_message(void)
{
    const char *target = TEST_OUT "editor_changed_on_disk.toml";
    EditorState es = {0};
    LevelDef def;
    char root[EDITOR_PATH_MAX] = {0};
    int result = 1;

    ensure_out_dir();
    remove(target);
    fill_valid_minimal(&def);
    es.undo = undo_create();
    if (!es.undo || level_save_toml(&def, target) != 0 ||
        make_test_preference_root(root, sizeof(root)) != 0 ||
        editor_set_preference_root(&es, root) != 0 ||
        editor_init_persistence_paths(&es) != 0 ||
        editor_load_level(&es, target) != 0) goto cleanup;
    es.level.coin_score = 31;
    editor_refresh_dirty(&es);
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_SOURCE_CHANGED);
    if (expect_int("changed save refused", editor_save_current_level(&es), -1) != 0 ||
        expect_prefix("changed save explained", es.status_message,
                      "Save stopped: " TEST_OUT "editor_changed_on_disk.toml changed on disk") != 0 ||
        expect_int("changed save keeps edits", es.modified, 1) != 0)
        goto cleanup;
    /* Nothing was written: the file still holds the old level. */
    {
        LevelDef on_disk;
        if (level_load_toml(target, &on_disk) != 0 ||
            expect_int("file untouched", on_disk.coin_score == def.coin_score, 1) != 0)
            goto cleanup;
    }
    result = 0;

cleanup:
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_NONE);
    if (root[0]) cleanup_test_preference_root(root, &es, 1);
    undo_destroy(es.undo);
    remove(target);
    return result;
}

static int stranded_replace_keeps_the_temporary_file(void)
{
    const char *target = TEST_OUT "editor_stranded_target.toml";
    EditorState es = {0};
    LevelDef def;
    LevelDef kept;
    char kept_path[EDITOR_PATH_MAX] = {0};
    char root[EDITOR_PATH_MAX] = {0};
    int result = 1;

    ensure_out_dir();
    remove(target);
    fill_valid_minimal(&def);
    if (level_save_toml(&def, target) != 0) return 1;

    def.coin_score = 77;
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_REPLACE_STRANDED);
    if (expect_int("stranded replace result", level_save_toml(&def, target),
                   SERIALIZER_REPLACE_TEMP_KEPT) != 0) goto cleanup;
    snprintf(kept_path, sizeof(kept_path), "%s", level_save_kept_temp_path());
    if (expect_int("stranded temp named", kept_path[0] != '\0', 1) != 0 ||
        expect_int("stranded temp kept", editor_file_exists(kept_path), 1) != 0 ||
        expect_int("stranded temp loads", level_load_toml(kept_path, &kept), 0) != 0 ||
        expect_int("stranded temp holds the new level", kept.coin_score, 77) != 0)
        goto cleanup;
    remove(kept_path);
    /* The next save starts with a clean report. */
    if (expect_int("normal save after stranding", level_save_toml(&def, target), 0) != 0 ||
        expect_string("kept path cleared", level_save_kept_temp_path(), "") != 0)
        goto cleanup;

    /* The editor names the kept file instead of a plain "Save failed". */
    es.undo = undo_create();
    if (!es.undo || make_test_preference_root(root, sizeof(root)) != 0) goto cleanup;
    if (editor_set_preference_root(&es, root) != 0 ||
        editor_init_persistence_paths(&es) != 0 ||
        editor_load_level(&es, target) != 0) goto cleanup;
    es.level.coin_score = 78;
    editor_refresh_dirty(&es);
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_REPLACE_STRANDED);
    if (expect_int("editor stranded save", editor_save_current_level(&es), -1) != 0 ||
        expect_prefix("editor stranded status", es.status_message,
                      "Save incomplete: your level is safe in ") != 0 ||
        expect_int("editor still modified", es.modified, 1) != 0)
        goto cleanup;
    snprintf(kept_path, sizeof(kept_path), "%s", level_save_kept_temp_path());
    if (expect_int("editor stranded temp kept", editor_file_exists(kept_path), 1) != 0)
        goto cleanup;
    result = 0;

cleanup:
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_NONE);
    if (kept_path[0]) remove(kept_path);
    if (root[0]) cleanup_test_preference_root(root, &es, 1);
    undo_destroy(es.undo);
    remove(target);
    return result;
}

#ifndef _WIN32
/* The permission bits of path, or -1 when it cannot be read. */
static int file_mode(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 ? (int)(st.st_mode & 07777) : -1;
}
#endif

/*
 * A level the user saves is an ordinary document: a new file gets
 * 0666 minus the umask (0644 here), not the owner-only 0600 that only the
 * editor's private recovery and playtest copies need.  Replacing a file
 * keeps its permissions and group.
 */
static int saved_levels_get_ordinary_permissions(void)
{
#ifdef _WIN32
    return 0;   /* Windows files have no POSIX mode bits. */
#else
    const char *fresh = TEST_OUT "perm_fresh.toml";
    const char *created = TEST_OUT "perm_created.toml";
    const char *copied = TEST_OUT "perm_copied.toml";
    const char *private_copy = TEST_OUT "perm_private.toml";
    const char *recovery = TEST_OUT "perm_recovery.toml";
    const char *existing = TEST_OUT "perm_existing.toml";
    const char *all[] = {fresh, created, copied, private_copy, recovery, existing};
    struct stat before, after;
    LevelDef def;
    int result = 1;
    mode_t old_mask = umask(022);

    ensure_out_dir();
    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++) remove(all[i]);
    fill_valid_minimal(&def);

    if (level_save_toml(&def, fresh) != 0 ||
        expect_int("new level mode", file_mode(fresh), 0644) != 0) goto cleanup;
    if (level_save_toml_with_policy(&def, created, SERIALIZER_SAVE_CREATE_ONLY) != 0 ||
        expect_int("create-only level mode", file_mode(created), 0644) != 0) goto cleanup;
    /* Drives without hard links copy into the claimed name instead. */
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_NO_HARD_LINKS);
    if (level_save_toml_with_policy(&def, copied, SERIALIZER_SAVE_CREATE_ONLY) != 0 ||
        expect_int("copied level mode", file_mode(copied), 0644) != 0) goto cleanup;
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_NONE);
    if (level_save_toml_private(&def, private_copy) != 0 ||
        expect_int("playtest copy mode", file_mode(private_copy), 0600) != 0) goto cleanup;
    if (level_save_toml_recovery(&def, recovery, fresh) != 0 ||
        expect_int("recovery copy mode", file_mode(recovery), 0600) != 0) goto cleanup;

    /* Replacing keeps the old file's mode and group, private or not. */
    if (write_text_file(existing, "old\n") != 0 || chmod(existing, 0640) != 0 ||
        stat(existing, &before) != 0) goto cleanup;
    if (level_save_toml(&def, existing) != 0 || stat(existing, &after) != 0 ||
        expect_int("replaced mode kept", (int)(after.st_mode & 07777), 0640) != 0 ||
        expect_int("replaced group kept", after.st_gid == before.st_gid, 1) != 0)
        goto cleanup;
    if (level_save_toml_private(&def, existing) != 0 ||
        expect_int("private replace keeps mode", file_mode(existing), 0640) != 0)
        goto cleanup;
    result = 0;

cleanup:
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_NONE);
    umask(old_mask);
    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++) remove(all[i]);
    return result;
#endif
}

static int autosave_backs_off_and_snapshots_last_valid_level(void)
{
    EditorState es;
    LevelDef recovered;
    const uint32_t long_ago = (uint32_t)clock_millis() - 30001u;

    ensure_out_dir();
    memset(&es, 0, sizeof(es));
    editor_level_init_defaults(&es.level);
    strncpy(es.autosave_path, TEST_OUT "autosave/test_editor_autosave.toml",
            sizeof(es.autosave_path) - 1);
    ensure_autosave_dir();
    remove(es.autosave_path);

    /* An invalid level with no valid version yet writes nothing. */
    es.modified = 1;
    es.last_autosave_ms = long_ago;
    es.level.coin_count = MAX_COINS + 1;
    editor_maybe_autosave(&es);
    if (expect_string("invalid autosave status", es.status_message,
                      "Autosave skipped: level has validation errors") != 0 ||
        expect_int("invalid autosave not written",
                   editor_file_exists(es.autosave_path), 0) != 0)
        return 1;

    /* The attempt consumed the interval: no retry on the next frame. */
    es.level.coin_count = 1;
    es.level.coins[0] = (CoinPlacement){64.0f, 96.0f};
    editor_maybe_autosave(&es);
    if (expect_int("no retry before interval",
                   editor_file_exists(es.autosave_path), 0) != 0) return 1;

    /* A valid level is written and remembered... */
    es.last_autosave_ms = long_ago;
    editor_maybe_autosave(&es);
    if (expect_int("valid autosave written",
                   editor_file_exists(es.autosave_path), 1) != 0) return 1;

    /* ...so when it later breaks, crash recovery still gets a loadable
     * snapshot: the last valid version. */
    es.level.coin_count = MAX_COINS + 1;
    remove(es.autosave_path);
    es.status_message[0] = '\0';
    es.last_autosave_ms = long_ago;
    editor_maybe_autosave(&es);
    memset(&recovered, 0, sizeof(recovered));
    if (expect_int("last valid snapshot written",
                   editor_file_exists(es.autosave_path), 1) != 0 ||
        expect_int("last valid snapshot loads",
                   level_load_toml(es.autosave_path, &recovered), 0) != 0 ||
        expect_int("snapshot is last valid version", recovered.coin_count, 1) != 0 ||
        expect_prefix("last valid status", es.status_message,
                      "Autosaved last valid version") != 0) return 1;

    /* Routine autosave messages do not replace a fresh, unread message. */
    es.level.coin_count = 1;
    editor_set_status(&es, "Cannot place Coin: limit reached");
    es.last_autosave_ms = long_ago;
    editor_maybe_autosave(&es);
    if (expect_string("fresh status kept", es.status_message,
                      "Cannot place Coin: limit reached") != 0) return 1;

    /* Failures report once, then wait a full interval before retrying. */
    strncpy(es.autosave_path, TEST_OUT "no_such_dir/never/autosave.toml",
            sizeof(es.autosave_path) - 1);
    es.last_autosave_ms = long_ago;
    editor_maybe_autosave(&es);
    if (expect_prefix("failure status", es.status_message, "Autosave failed") != 0)
        return 1;
    editor_set_status(&es, "Other message");
    editor_maybe_autosave(&es);
    if (expect_string("failure backs off", es.status_message, "Other message") != 0)
        return 1;

    remove(TEST_OUT "autosave/test_editor_autosave.toml");
    return 0;
}

static int loads_recent_files_with_trim_and_limit(void)
{
    EditorState es;
    FILE *fp;

    ensure_out_dir();
    fp = fopen(EDITOR_WORKFLOW_RECENT_PATH, "w");
    if (!fp) {
        fprintf(stderr, "editor_validation_test: cannot write recent file\n");
        return 1;
    }
    fprintf(fp, "levels/a.toml\n\nlevels/b.toml\r\nlevels/c.toml\n");
    fprintf(fp, "levels/d.toml\nlevels/e.toml\nlevels/f.toml\n");
    fclose(fp);

    memset(&es, 0, sizeof(es));
    strncpy(es.recent_path, EDITOR_WORKFLOW_RECENT_PATH,
            sizeof(es.recent_path) - 1);
    editor_load_recent_files(&es);

    if (expect_int("recent count", es.recent_file_count, 5) != 0) return 1;
    if (expect_string("recent 0", es.recent_files[0], "levels/a.toml") != 0)
        return 1;
    if (expect_string("recent 1", es.recent_files[1], "levels/b.toml") != 0)
        return 1;
    if (expect_string("recent 4", es.recent_files[4], "levels/e.toml") != 0)
        return 1;

    remove(EDITOR_WORKFLOW_RECENT_PATH);
    return 0;
}

static int recent_files_skip_overlong_lines_and_line_breaks(void)
{
    const char *level_path = TEST_OUT "test_editor_recent_level.toml";
    EditorState es = {0};
    LevelDef fixture;
    char root[EDITOR_PATH_MAX] = {0};
    FILE *fp;
    int result = 1;

    /* An over-long line is skipped without swallowing the next one, and a
     * stored path with an embedded line break is ignored. */
    ensure_out_dir();
    fp = fopen(EDITOR_WORKFLOW_RECENT_PATH, "wb");
    if (!fp) return 1;
    fputs("levels/a.toml\n", fp);
    for (int i = 0; i < EDITOR_PATH_MAX; i++) fputc('x', fp);
    fputs("\nlevels/b.toml\nlevels/c\rd.toml\nlevels/e.toml\n", fp);
    fclose(fp);
    strncpy(es.recent_path, EDITOR_WORKFLOW_RECENT_PATH, sizeof(es.recent_path) - 1);
    editor_load_recent_files(&es);
    if (expect_int("recent after long line", es.recent_file_count, 3) != 0 ||
        expect_string("recent keeps a", es.recent_files[0], "levels/a.toml") != 0 ||
        expect_string("recent keeps b", es.recent_files[1], "levels/b.toml") != 0 ||
        expect_string("recent skips CR path", es.recent_files[2], "levels/e.toml") != 0)
        goto cleanup;

    /* Saving never writes a path containing a line break. */
    strcpy(es.recent_files[1], "levels/evil\nlevels/injected.toml");
    editor_level_init_defaults(&fixture);
    es.undo = undo_create();
    if (!es.undo || level_save_toml(&fixture, level_path) != 0 ||
        make_test_preference_root(root, sizeof(root)) != 0 ||
        editor_set_preference_root(&es, root) != 0 ||
        editor_load_level(&es, level_path) != 0) goto cleanup;
    {
        EditorState reloaded = {0};
        strncpy(reloaded.recent_path, EDITOR_WORKFLOW_RECENT_PATH,
                sizeof(reloaded.recent_path) - 1);
        editor_load_recent_files(&reloaded);
        if (expect_int("saved recent count", reloaded.recent_file_count, 3) != 0 ||
            expect_string("saved recent newest", reloaded.recent_files[0], level_path) != 0 ||
            expect_string("line break path dropped", reloaded.recent_files[2],
                          "levels/e.toml") != 0) goto cleanup;
    }
    result = 0;

cleanup:
    cleanup_test_preference_root(root, &es, 1);
    undo_destroy(es.undo);
    remove(level_path);
    remove(EDITOR_WORKFLOW_RECENT_PATH);
    return result;
}

/*
 * editor_document_hash lists LevelDef fields by hand; a forgotten field
 * would make edits to it invisible to dirty tracking (no "*", no save
 * prompt).  Entity coverage iterates the palette table, so a new entity
 * type is checked automatically; each element's *content* must matter,
 * not just the array count.
 */
static int expect_hash_changes(const char *name, uint64_t before,
                               const LevelDef *level)
{
    if (editor_document_hash(level) == before) {
        fprintf(stderr, "editor_validation_test: document hash ignores %s\n", name);
        return 1;
    }
    return 0;
}

/*
 * entity_meta.c's table is filled with designated initializers, so a type
 * left out of it compiles (its row is all zeros) unless it is the last one.
 * Check every row here: names, a capacity, a preview, and storage that
 * points at that type's own array (adding one entity changes only its own
 * count).
 */
static int entity_table_has_a_row_for_every_type(void)
{
    static LevelDef level;
    PlacementData data;
    EntityTextures textures;
    Texture2D fake_texture;

    memset(&data, 0, sizeof(data));
    for (size_t i = 0; i < sizeof(textures) / sizeof(Texture2D *); i++)
        ((Texture2D **)&textures)[i] = &fake_texture;

    for (int t = 0; t < ENT_COUNT; t++) {
        EntityType type = (EntityType)t;
        const EditorEntityPreview *preview = editor_entity_preview(type);

        if (strcmp(editor_entity_type_name(type), "Unknown") == 0 ||
            strcmp(editor_entity_palette_name(type), "Unknown") == 0 ||
            editor_entity_capacity(type) <= 0 || !preview) {
            fprintf(stderr, "editor_validation_test: entity type %d has no table row\n", t);
            return 1;
        }
        if (preview->texture != EDITOR_NO_TEXTURE &&
            editor_entity_texture(&textures, type) != &fake_texture) {
            fprintf(stderr, "editor_validation_test: %s preview texture is not an EntityTextures member\n",
                    editor_entity_type_name(type));
            return 1;
        }
        if (editor_entity_type_is_singleton(type)) continue;

        memset(&level, 0, sizeof(level));
        if (editor_entity_insert(&level, type, 0, &data) != 0) return 1;
        for (int other = 0; other < ENT_COUNT; other++) {
            int expected = (other == t) ? 1 : 0;
            if (editor_entity_type_is_singleton((EntityType)other)) continue;
            if (editor_entity_count(&level, (EntityType)other) != expected) {
                fprintf(stderr, "editor_validation_test: inserting a %s changed the %s count\n",
                        editor_entity_type_name(type),
                        editor_entity_type_name((EntityType)other));
                return 1;
            }
        }
    }
    if (editor_entity_preview(ENT_COUNT) != NULL ||
        editor_entity_texture(&textures, ENT_COUNT) != NULL) return 1;
    return 0;
}

static int document_hash_covers_every_entity_and_config_field(void)
{
    LevelDef level;
    uint64_t before;
    PlacementData first, second;

    memset(&first, 0x11, sizeof(first));
    memset(&second, 0x22, sizeof(second));

    for (int p = 0; p < editor_entity_palette_entry_count(); p++) {
        EntityType type = editor_entity_palette_entry_type(p);
        const char *name = editor_entity_type_name(type);

        editor_level_init_defaults(&level);
        before = editor_document_hash(&level);
        if (editor_entity_type_is_singleton(type)) {
            if (editor_entity_write(&level, type, 0, &first) != 0) return 1;
        } else if (editor_entity_insert(&level, type, 0, &first) != 0) {
            return 1;
        }
        if (expect_hash_changes(name, before, &level) != 0) return 1;

        before = editor_document_hash(&level);
        if (editor_entity_write(&level, type, 0, &second) != 0 ||
            expect_hash_changes(name, before, &level) != 0) return 1;
    }

    /* Level-wide settings: take the hash, change one field, compare. */
    editor_level_init_defaults(&level);
    before = editor_document_hash(&level); level.name[0] = 'Z';
    if (expect_hash_changes("name", before, &level)) return 1;
    before = editor_document_hash(&level); strcpy(level.description, "d");
    if (expect_hash_changes("description", before, &level)) return 1;
    before = editor_document_hash(&level); strcpy(level.generated_by, "g");
    if (expect_hash_changes("generated_by", before, &level)) return 1;
    before = editor_document_hash(&level); level.screen_count++;
    if (expect_hash_changes("screen_count", before, &level)) return 1;
    before = editor_document_hash(&level); strcpy(level.next_phase, "levels/x.toml");
    if (expect_hash_changes("next_phase", before, &level)) return 1;
    before = editor_document_hash(&level); level.background_layer_count = 1;
    if (expect_hash_changes("background_layer_count", before, &level)) return 1;
    before = editor_document_hash(&level); level.background_layers[0].path[0] = 'b';
    if (expect_hash_changes("background_layers.path", before, &level)) return 1;
    before = editor_document_hash(&level); level.background_layers[0].speed = 0.5f;
    if (expect_hash_changes("background_layers.speed", before, &level)) return 1;
    before = editor_document_hash(&level); level.foreground_layer_count = 1;
    if (expect_hash_changes("foreground_layer_count", before, &level)) return 1;
    before = editor_document_hash(&level); level.foreground_layers[0].path[0] = 'f';
    if (expect_hash_changes("foreground_layers.path", before, &level)) return 1;
    before = editor_document_hash(&level); level.foreground_layers[0].speed = 0.5f;
    if (expect_hash_changes("foreground_layers.speed", before, &level)) return 1;
    before = editor_document_hash(&level); level.fog_layer_count = 1;
    if (expect_hash_changes("fog_layer_count", before, &level)) return 1;
    before = editor_document_hash(&level); level.fog_layers[0].path[0] = 'o';
    if (expect_hash_changes("fog_layers.path", before, &level)) return 1;
    before = editor_document_hash(&level); level.fog_layers[0].speed = 0.5f;
    if (expect_hash_changes("fog_layers.speed", before, &level)) return 1;
    before = editor_document_hash(&level); level.player_start_x += 1.0f;
    if (expect_hash_changes("player_start_x", before, &level)) return 1;
    before = editor_document_hash(&level); level.player_start_y += 1.0f;
    if (expect_hash_changes("player_start_y", before, &level)) return 1;
    before = editor_document_hash(&level); strcpy(level.music_path, "m");
    if (expect_hash_changes("music_path", before, &level)) return 1;
    before = editor_document_hash(&level); level.music_volume++;
    if (expect_hash_changes("music_volume", before, &level)) return 1;
    before = editor_document_hash(&level); level.floor_tile_path[0] = 'X';
    if (expect_hash_changes("floor_tile_path", before, &level)) return 1;
    before = editor_document_hash(&level); level.initial_hearts++;
    if (expect_hash_changes("initial_hearts", before, &level)) return 1;
    before = editor_document_hash(&level); level.initial_lives++;
    if (expect_hash_changes("initial_lives", before, &level)) return 1;
    before = editor_document_hash(&level); level.score_per_life++;
    if (expect_hash_changes("score_per_life", before, &level)) return 1;
    before = editor_document_hash(&level); level.coin_score++;
    if (expect_hash_changes("coin_score", before, &level)) return 1;

    /* physics is a block of floats: change each one in turn. */
    for (size_t i = 0; i < sizeof(level.physics) / sizeof(float); i++) {
        float value;
        char label[32];
        before = editor_document_hash(&level);
        memcpy(&value, (char *)&level.physics + i * sizeof(float), sizeof(value));
        value += 1.0f;
        memcpy((char *)&level.physics + i * sizeof(float), &value, sizeof(value));
        snprintf(label, sizeof(label), "physics float %zu", i);
        if (expect_hash_changes(label, before, &level) != 0) return 1;
    }
    return 0;
}

/* Feed `output` to file_dialog_read_path as if a picker had printed it. */
static int read_picker_output(const char *output, char *buf, int buf_size)
{
    FILE *fp = tmpfile();
    int result;
    if (!fp) return -100;
    fputs(output, fp);
    rewind(fp);
    result = file_dialog_read_path(fp, buf, buf_size);
    fclose(fp);
    return result;
}

static int dialog_quoting_and_picked_paths_stay_literal(void)
{
    char buf[64];
    char *quoted;
    int failed = 0;

    /* PowerShell: ASCII and Unicode single quotes are all doubled, so the
     * file name stays one literal string instead of ending it early. */
    quoted = dialog_quote_powershell("x\xE2\x80\x99;Start-Process calc;\xE2\x80\x98.toml");
    failed |= !quoted || expect_string("powershell smart quotes", quoted,
        "'x\xE2\x80\x99\xE2\x80\x99;Start-Process calc;\xE2\x80\x98\xE2\x80\x98.toml'");
    free(quoted);
    quoted = dialog_quote_powershell("it's \xE2\x80\x9A\xE2\x80\x9B \xE2\x80\x9C");
    failed |= !quoted || expect_string("powershell all marks", quoted,
        "'it''s \xE2\x80\x9A\xE2\x80\x9A\xE2\x80\x9B\xE2\x80\x9B \xE2\x80\x9C'");
    free(quoted);
    /* zenity: exit 1 is Cancel unless the extra button printed its label. */
    {
        const char *const labels[3] = {"Save", "Discard", "Cancel"};
        failed |= expect_int("zenity ok", dialog_zenity_selection(0, "", labels, 3, 0, 2, 1), 0);
        failed |= expect_int("zenity cancel", dialog_zenity_selection(1, "", labels, 3, 0, 2, 1), 2);
        failed |= expect_int("zenity extra", dialog_zenity_selection(1, "Discard", labels, 3, 0, 2, 1), 1);
        failed |= expect_int("zenity two-button ignores label",
                             dialog_zenity_selection(1, "Discard", labels, 2, 0, 1, -1), 1);
        failed |= expect_int("zenity failure", dialog_zenity_selection(5, "", labels, 3, 0, 2, 1), -1);
    }
    quoted = dialog_quote_posix("it's <b>&amp;");
    failed |= !quoted || expect_string("posix quote", quoted, "'it'\\''s <b>&amp;'");
    free(quoted);
    if (failed) return 1;

    /* A picked name containing a line break is refused, never truncated.
     * "\r\n" is a line ending only on Windows; elsewhere the '\r' belongs
     * to the name ("a.toml\r" is a different file from "a.toml"). */
    if (expect_int("plain path", read_picker_output("/tmp/a.toml\n", buf, sizeof(buf)),
                   FILE_DIALOG_SELECTED) != 0 ||
        expect_string("plain path text", buf, "/tmp/a.toml") != 0 ||
#ifdef _WIN32
        expect_int("crlf path", read_picker_output("C:\\a.toml\r\n", buf, sizeof(buf)),
                   FILE_DIALOG_SELECTED) != 0 ||
        expect_string("crlf path text", buf, "C:\\a.toml") != 0 ||
#else
        expect_int("crlf path", read_picker_output("/tmp/a.toml\r\n", buf, sizeof(buf)),
                   FILE_DIALOG_INVALID_PATH) != 0 ||
#endif
        expect_int("trailing carriage return without newline",
                   read_picker_output("/tmp/a.toml\r", buf, sizeof(buf)),
                   FILE_DIALOG_INVALID_PATH) != 0 ||
        expect_int("newline in name",
                   read_picker_output("/tmp/evil\nreal.toml\n", buf, sizeof(buf)),
                   FILE_DIALOG_INVALID_PATH) != 0 ||
        expect_int("carriage return in name",
                   read_picker_output("/tmp/evil\rreal.toml\n", buf, sizeof(buf)),
                   FILE_DIALOG_INVALID_PATH) != 0 ||
        expect_int("no output", read_picker_output("", buf, sizeof(buf)),
                   FILE_DIALOG_CANCELLED) != 0) return 1;

    {
        EditorState es = {0};
        file_dialog_test_set_open_result(FILE_DIALOG_INVALID_PATH, NULL);
        editor_open_level_file(&es);
        if (expect_string("open line break status", es.status_message,
                          "Open failed: file names with line breaks are not supported") != 0)
            return 1;
    }
    return 0;
}

static int property_command_undo_redo(void)
{
    EditorState es;
    Command cmd;

    memset(&es, 0, sizeof(es));
    editor_level_init_defaults(&es.level);
    es.level.coin_count = 1;
    es.level.coins[0].x = 24.0f;
    es.level.coins[0].y = 48.0f;
    es.selection.type = ENT_COIN;
    es.selection.index = 0;
    es.undo = undo_create();
    editor_set_document_save_point(&es);

    editor_begin_change_tracking(&es, EDITOR_CHANGE_ENTITY);
    editor_capture_change_before(&es, -1);
    es.level.coins[0].x = 96.0f;
    editor_commit_change(&es);

    if (expect_float_value("property forward", es.level.coins[0].x, 96.0f) != 0)
        goto fail;
    if (expect_int("property dirty", es.modified, 1) != 0) goto fail;
    if (expect_int("property undo available", es.undo->top, 1) != 0) goto fail;

    if (!undo_pop(es.undo, &cmd)) goto fail;
    editor_apply_undo_command(&es, &cmd, 1);
    editor_refresh_dirty(&es);
    if (expect_float_value("property undo", es.level.coins[0].x, 24.0f) != 0)
        goto fail;
    if (expect_int("property undo clean", es.modified, 0) != 0) goto fail;

    if (!redo_pop(es.undo, &cmd)) goto fail;
    editor_apply_undo_command(&es, &cmd, 0);
    editor_refresh_dirty(&es);
    if (expect_float_value("property redo", es.level.coins[0].x, 96.0f) != 0)
        goto fail;
    if (expect_int("property redo dirty", es.modified, 1) != 0) goto fail;

    undo_destroy(es.undo);
    return 0;

fail:
    undo_destroy(es.undo);
    return 1;
}

static int config_command_preserves_entity_edit(void)
{
    EditorState es;
    Command cmd;

    memset(&es, 0, sizeof(es));
    editor_level_init_defaults(&es.level);
    es.level.coin_count = 1;
    es.level.coins[0].x = 32.0f;
    es.selection.type = ENT_COIN;
    es.selection.index = 0;
    es.undo = undo_create();
    editor_set_document_save_point(&es);

    editor_begin_change_tracking(&es, EDITOR_CHANGE_ENTITY);
    editor_capture_change_before(&es, -1);
    es.level.coins[0].x = 128.0f;
    editor_commit_change(&es);

    editor_begin_change_tracking(&es, EDITOR_CHANGE_CONFIG);
    editor_capture_change_before(&es, -1);
    es.level.music_volume = 77;
    memset(es.level.description,'x',sizeof(es.level.description)-1);
    es.level.description[sizeof(es.level.description)-1]='\0';
    editor_commit_change(&es);

    if (!undo_pop(es.undo, &cmd)) goto fail;
    editor_apply_undo_command(&es, &cmd, 1);
    editor_refresh_dirty(&es);
    if (expect_float_value("config undo keeps entity", es.level.coins[0].x,
                           128.0f) != 0)
        goto fail;
    if (expect_int("config undo restores config", es.level.music_volume, 0) != 0)
        goto fail;
    if (expect_string("long metadata undo", es.level.description, "") != 0) goto fail;

    if (!redo_pop(es.undo, &cmd)) goto fail;
    editor_apply_undo_command(&es, &cmd, 0);
    editor_refresh_dirty(&es);
    if (expect_int("config redo", es.level.music_volume, 77) != 0) goto fail;
    if (expect_int("long metadata redo length", (int)strlen(es.level.description), LEVEL_DESCRIPTION_CAPACITY-1) != 0) goto fail;

    undo_destroy(es.undo);
    return 0;

fail:
    undo_destroy(es.undo);
    return 1;
}

static int last_star_text_property_undo_redo(void)
{
    EditorState es;
    Command cmd;

    memset(&es, 0, sizeof(es));
    editor_level_init_defaults(&es.level);
    strncpy(es.level.next_phase, "levels/old.toml",
            sizeof(es.level.next_phase) - 1);
    es.selection.type = ENT_LAST_STAR;
    es.selection.index = 0;
    es.undo = undo_create();
    editor_set_document_save_point(&es);

    editor_begin_change_tracking(&es, EDITOR_CHANGE_ENTITY);
    editor_capture_change_before(&es, EDITOR_LAST_STAR_NEXT_PHASE_WIDGET);
    strncpy(es.level.next_phase, "levels/new.toml",
            sizeof(es.level.next_phase) - 1);
    editor_commit_change(&es);
    if (expect_int("last star text command", es.undo->top, 1) != 0) goto fail;

    if (!undo_pop(es.undo, &cmd)) goto fail;
    editor_apply_undo_command(&es, &cmd, 1);
    if (expect_string("last star text undo", es.level.next_phase,
                      "levels/old.toml") != 0)
        goto fail;
    if (expect_int("last star text undo clean", es.modified, 0) != 0) goto fail;

    if (!redo_pop(es.undo, &cmd)) goto fail;
    editor_apply_undo_command(&es, &cmd, 0);
    if (expect_string("last star text redo", es.level.next_phase,
                      "levels/new.toml") != 0)
        goto fail;
    if (expect_int("last star text redo dirty", es.modified, 1) != 0) goto fail;

    undo_destroy(es.undo);
    return 0;

fail:
    undo_destroy(es.undo);
    return 1;
}

static int dirty_save_point_tracks_undo_redo(void)
{
    EditorState es;
    Command cmd;

    memset(&es, 0, sizeof(es));
    editor_level_init_defaults(&es.level);
    es.level.coin_count = 1;
    es.level.coins[0].x = 16.0f;
    es.selection.type = ENT_COIN;
    es.selection.index = 0;
    es.undo = undo_create();
    editor_set_document_save_point(&es);

    editor_begin_change_tracking(&es, EDITOR_CHANGE_ENTITY);
    editor_capture_change_before(&es, -1);
    es.level.coins[0].x = 64.0f;
    editor_commit_change(&es);
    if (expect_int("save point edit dirty", es.modified, 1) != 0) goto fail;

    if (!undo_pop(es.undo, &cmd)) goto fail;
    editor_apply_undo_command(&es, &cmd, 1);
    editor_refresh_dirty(&es);
    if (expect_int("save point undo clean", es.modified, 0) != 0) goto fail;

    if (!redo_pop(es.undo, &cmd)) goto fail;
    editor_apply_undo_command(&es, &cmd, 0);
    editor_refresh_dirty(&es);
    if (expect_int("save point redo dirty", es.modified, 1) != 0) goto fail;

    undo_destroy(es.undo);
    return 0;

fail:
    undo_destroy(es.undo);
    return 1;
}

static int selection_structural_mutations_are_safe(void)
{
    EditorState es;
    Command cmd;

    memset(&es, 0, sizeof(es));
    editor_level_init_defaults(&es.level);
    es.undo = undo_create();
    if (!es.undo) return 1;

    es.level.coin_count = 2;
    es.level.coins[0].x = 16.0f;
    es.level.coins[0].y = 16.0f;
    es.level.coins[1].x = 64.0f;
    es.level.coins[1].y = 16.0f;
    es.selection.type = ENT_COIN;
    es.selection.index = 1;
    es.tool = TOOL_DELETE;

    tools_mouse_down(&es, 16.0f, 16.0f);
    if (expect_int("delete shifts selected index", es.selection.index, 0) != 0)
        goto fail;
    if (expect_float_value("delete keeps selected entity", es.level.coins[0].x,
                           64.0f) != 0)
        goto fail;

    if (!undo_pop(es.undo, &cmd)) goto fail;
    editor_apply_undo_command(&es, &cmd, 1);
    if (expect_int("undo delete shifts selection back", es.selection.index, 1) != 0)
        goto fail;
    if (!editor_selection_is_valid(&es)) goto fail;

    if (!redo_pop(es.undo, &cmd)) goto fail;
    editor_apply_undo_command(&es, &cmd, 0);
    if (expect_int("redo delete shifts selection", es.selection.index, 0) != 0)
        goto fail;
    if (!editor_selection_is_valid(&es)) goto fail;

    undo_clear(es.undo);
    es.level.coin_count = 0;
    es.selection.index = -1;
    es.tool = TOOL_PLACE;
    es.palette_type = ENT_COIN;
    tools_mouse_down(&es, 120.0f, 48.0f);
    if (expect_int("place selects new entity", es.selection.index, 0) != 0)
        goto fail;
    if (!editor_selection_is_valid(&es)) goto fail;

    if (!undo_pop(es.undo, &cmd)) goto fail;
    editor_apply_undo_command(&es, &cmd, 1);
    if (expect_int("undo selected place clears selection",
                   es.selection.index, -1) != 0)
        goto fail;
    if (expect_int("undo selected place count", es.level.coin_count, 0) != 0)
        goto fail;

    if (!redo_pop(es.undo, &cmd)) goto fail;
    editor_apply_undo_command(&es, &cmd, 0);
    if (expect_int("redo place selects valid entity", es.selection.index, 0) != 0)
        goto fail;

    editor_copy_selected(&es);
    editor_paste_clipboard(&es);
    if (expect_int("paste count", es.level.coin_count, 2) != 0) goto fail;
    if (!editor_selection_is_valid(&es)) goto fail;
    if (!undo_pop(es.undo, &cmd)) goto fail;
    editor_apply_undo_command(&es, &cmd, 1);
    if (expect_int("undo paste count", es.level.coin_count, 1) != 0) goto fail;
    if (expect_int("undo paste selection safe", es.selection.index, -1) != 0)
        goto fail;

    es.selection.type = ENT_COIN;
    es.selection.index = 99;
    editor_selection_reconcile(&es);
    if (expect_int("invalid selection reconciled", es.selection.index, -1) != 0)
        goto fail;

    undo_destroy(es.undo);
    return 0;

fail:
    undo_destroy(es.undo);
    return 1;
}

static int checkpoint_editor_mutations_are_reversible(void)
{
    EditorState es = {0};
    Command cmd;

    editor_level_init_defaults(&es.level);
    es.undo = undo_create();
    es.tool = TOOL_PLACE;
    es.palette_type = ENT_CHECKPOINT;
    tools_mouse_down(&es, 120.0f, 180.0f);
    if (expect_int("checkpoint place count", es.level.checkpoint_count, 1) != 0)
        goto fail;
    if (expect_float_value("checkpoint place y", es.level.checkpoints[0].y, 180.0f) != 0)
        goto fail;

    es.tool = TOOL_SELECT;
    tools_mouse_down(&es, 120.0f, 180.0f);
    if (expect_int("checkpoint select", es.selection.index, 0) != 0) goto fail;
    tools_mouse_drag(&es, 160.0f, 200.0f);
    tools_mouse_up(&es, 160.0f, 200.0f);
    if (expect_float_value("checkpoint drag", es.level.checkpoints[0].x, 160.0f) != 0)
        goto fail;

    editor_copy_selected(&es);
    editor_paste_clipboard(&es);
    if (expect_int("checkpoint paste count", es.level.checkpoint_count, 2) != 0)
        goto fail;
    if (!undo_pop(es.undo, &cmd)) goto fail;
    editor_apply_undo_command(&es, &cmd, 1);
    if (expect_int("checkpoint undo paste", es.level.checkpoint_count, 1) != 0)
        goto fail;
    if (!redo_pop(es.undo, &cmd)) goto fail;
    editor_apply_undo_command(&es, &cmd, 0);
    if (expect_int("checkpoint redo paste", es.level.checkpoint_count, 2) != 0)
        goto fail;

    es.selection.type = ENT_CHECKPOINT;
    es.selection.index = 1;
    tools_delete_selected(&es);
    if (expect_int("checkpoint delete count", es.level.checkpoint_count, 1) != 0)
        goto fail;

    undo_destroy(es.undo);
    return 0;
fail:
    undo_destroy(es.undo);
    return 1;
}

/* Three valid rails; a spike block rides rail 2, a float platform rail 1. */
static void fill_rail_level(LevelDef *level)
{
    editor_level_init_defaults(level);
    level->rail_count = 3;
    for (int i = 0; i < 3; i++)
        level->rails[i] = (RailPlacement){RAIL_LAYOUT_RECT, 32 + i * 128, 32, 4, 4, 0};
    level->spike_block_count = 1;
    level->spike_blocks[0] = (SpikeBlockPlacement){2, 1.5f, 3.0f};
    level->float_platform_count = 1;
    level->float_platforms[0] = (FloatPlatformPlacement){
        FLOAT_PLATFORM_RAIL, 0.0f, 0.0f, 3, 1, 2.0f, 1.0f};
}

static int level_is_valid(const char *name, const LevelDef *level)
{
    char error[128];
    if (level_validate_runtime(level, error, sizeof(error)) != 0) {
        fprintf(stderr, "editor_validation_test: %s invalid: %s\n", name, error);
        return 1;
    }
    return 0;
}

/*
 * A Static or Crumble float platform keeps its old rail_index, and deleting
 * rails renumbers only the references that are in use.  Switching such a
 * platform back to Rail must not leave it on a rail that no longer exists
 * (the level would stop validating), and must say which rail it rides.
 */
static int float_platform_rail_switch_rechecks_its_rail(void)
{
    EditorState es = {0};

    fill_rail_level(&es.level);
    es.undo = undo_create();
    if (!es.undo) return 1;

    /* Rail 1 -> Static, then delete rails 0 and 1 (the spike block rides
     * rail 2, which becomes rail 0).  The platform still says rail 1. */
    if (editor_set_float_platform_mode(&es, 0, FLOAT_PLATFORM_STATIC) != 0) goto fail;
    es.level.float_platforms[0].x = 400.0f;
    es.level.float_platforms[0].y = 100.0f;
    es.selection.type = ENT_RAIL;
    es.selection.index = 0;
    tools_delete_selected(&es);
    es.selection.index = 0;
    tools_delete_selected(&es);
    if (expect_int("two rails deleted", es.level.rail_count, 1) != 0 ||
        expect_int("static platform kept its number",
                   es.level.float_platforms[0].rail_index, 1) != 0) goto fail;

    /* Back to Rail: the missing rail is replaced by the nearest one. */
    if (expect_int("switch to rail", editor_set_float_platform_mode(&es, 0,
                   FLOAT_PLATFORM_RAIL), 0) != 0 ||
        expect_int("re-picked rail", es.level.float_platforms[0].rail_index, 0) != 0 ||
        expect_prefix("re-pick explained", es.status_message,
                      "Float Platform rides rail 0 (its rail 1 no longer exists)") != 0 ||
        level_is_valid("after switch to rail", &es.level) != 0) goto fail;

    /* A number that still names a rail is kept, and announced. */
    if (editor_set_float_platform_mode(&es, 0, FLOAT_PLATFORM_CRUMBLE) != 0 ||
        editor_set_float_platform_mode(&es, 0, FLOAT_PLATFORM_RAIL) != 0 ||
        expect_prefix("kept rail announced", es.status_message,
                      "Float Platform rides rail 0; edit rail_index") != 0) goto fail;

    /* Without any rail the switch is refused and nothing changes. */
    if (editor_set_float_platform_mode(&es, 0, FLOAT_PLATFORM_STATIC) != 0) goto fail;
    es.level.spike_block_count = 0;
    es.level.rail_count = 0;
    if (expect_int("no rail refused", editor_set_float_platform_mode(&es, 0,
                   FLOAT_PLATFORM_RAIL), -1) != 0 ||
        expect_int("mode unchanged", (int)es.level.float_platforms[0].mode,
                   (int)FLOAT_PLATFORM_STATIC) != 0 ||
        expect_prefix("no rail explained", es.status_message,
                      "Cannot switch Float Platform to Rail: place a rail first") != 0)
        goto fail;
    undo_destroy(es.undo);
    return 0;
fail:
    undo_destroy(es.undo);
    return 1;
}

/*
 * The derived positions and path rules live once (level.h, level_validate.h)
 * and the game, the validator and the editor all call them.  Pin their
 * values, and check the editor's hit test uses the same platform top.
 */
static int shared_level_rules_have_one_answer(void)
{
    LevelDef level;
    EditorRect r;

    editor_level_init_defaults(&level);
    level.platform_count = 1;
    level.platforms[0] = (PlatformPlacement){100.0f, 2, 1, ""};
    if (expect_float_value("2-tile platform top", level_platform_top_y(2),
                           (float)(FLOOR_Y - 2 * TILE_SIZE + 16)) != 0 ||
        expect_float_value("default axe y", level_axe_trap_y(&(AxeTrapPlacement){0}),
                           level_platform_top_y(3)) != 0 ||
        expect_float_value("custom axe y",
                           level_axe_trap_y(&(AxeTrapPlacement){.y = 50.0f}), 50.0f) != 0 ||
        expect_float_value("default saw y",
                           level_circular_saw_y(&(CircularSawPlacement){0}),
                           level_platform_top_y(2) - SAW_DISPLAY_H) != 0 ||
        !editor_entity_bounds(&level, ENT_PLATFORM, 0, &r) ||
        expect_float_value("hit test platform top", r.y, level_platform_top_y(2)) != 0)
        return 1;
    if (expect_int("parent segment", level_path_has_parent_segment("assets/../x.png"), 1) != 0 ||
        expect_int("leading parent", level_path_has_parent_segment("../x.png"), 1) != 0 ||
        expect_int("dots in a name", level_path_has_parent_segment("a/..b/c..png"), 0) != 0 ||
        expect_int("control byte", level_path_has_control_char("a\nb"), 1) != 0 ||
        expect_int("plain path", level_path_has_control_char("assets/a.png"), 0) != 0)
        return 1;
    return 0;
}

static int expect_location(const char *name, const LevelIssueLocation *where,
                           const char *path, int index, const char *field)
{
    if (strcmp(where->path, path) != 0 || where->index != index ||
        strcmp(where->field, field) != 0) {
        fprintf(stderr, "editor_validation_test: %s got {%s, %d, %s} expected {%s, %d, %s}\n",
                name, where->path, where->index, where->field, path, index, field);
        return 1;
    }
    return 0;
}

/*
 * Validation messages carry a structured location (spec N-001) so the
 * editor can take the designer to the problem.  The validator promises
 * that every message starts with the TOML path of the bad value; these
 * cases pin that promise for each family of checks, including the ones
 * that used to print "spiders[].frame_index" without the element.
 */
static int validation_errors_report_where_they_are(void)
{
    LevelDef base, level;
    LevelIssueLocation where;
    char err[256];

    if (level_issue_location_parse("coins[3].x is 9.00 (expected 0..1)", &where) != 1 ||
        expect_location("parse element field", &where, "coins", 3, "x") != 0 ||
        level_issue_location_parse("screen_count is 0 (expected 1..99)", &where) != 1 ||
        expect_location("parse key", &where, "screen_count", -1, "") != 0 ||
        level_issue_location_parse("rails[2] has negative origin", &where) != 1 ||
        expect_location("parse element", &where, "rails", 2, "") != 0 ||
        expect_int("not a path", level_issue_location_parse("LevelDef is NULL", &where), 0) != 0 ||
        expect_int("half a path", level_issue_location_parse("coins[x].y bad", &where), 0) != 0 ||
        expect_location("cleared on failure", &where, "", -1, "") != 0)
        return 1;

    editor_level_init_defaults(&base);
    base.coin_count = 2;
    base.coins[0] = (CoinPlacement){100.0f, 100.0f};
    base.coins[1] = (CoinPlacement){140.0f, 100.0f};
    base.spider_count = 1;
    base.spiders[0] = (SpiderPlacement){300.0f, 50.0f, 250.0f, 350.0f, 0};
    base.axe_trap_count = 1;
    base.axe_traps[0] = (AxeTrapPlacement){.pillar_x = 500.0f, .y = 0.0f, .mode = AXE_MODE_PENDULUM};
    base.checkpoint_count = 1;
    base.checkpoints[0] = (CheckpointPlacement){600.0f, 100.0f};
    if (level_is_valid("location base", &base) != 0 ||
        expect_int("valid has no location",
                   level_validate_runtime_at(&base, err, sizeof(err), &where), 0) != 0 ||
        expect_location("valid location", &where, "", -1, "") != 0) return 1;

    level = base;
    level.coins[1].x = 99999.0f;
    if (level_validate_runtime_at(&level, err, sizeof(err), &where) == 0 ||
        expect_location("coin x", &where, "coins", 1, "x") != 0) return 1;
    level = base;
    level.spiders[0].frame_index = 99;
    if (level_validate_runtime_at(&level, err, sizeof(err), &where) == 0 ||
        expect_location("spider frame", &where, "spiders", 0, "frame_index") != 0) return 1;
    level = base;
    level.spiders[0].vx = 1.0e9f;
    if (level_validate_runtime_at(&level, err, sizeof(err), &where) == 0 ||
        expect_location("spider vx", &where, "spiders", 0, "vx") != 0) return 1;
    level = base;
    level.axe_traps[0].y = 5000.0f;
    if (level_validate_runtime_at(&level, err, sizeof(err), &where) == 0 ||
        expect_location("axe y", &where, "axe_traps", 0, "y") != 0) return 1;
    level = base;
    level.checkpoints[0].x = 1.0f;          /* behind the player start */
    if (level_validate_runtime_at(&level, err, sizeof(err), &where) == 0 ||
        expect_location("checkpoint x", &where, "checkpoints", 0, "x") != 0) return 1;
    level = base;
    level.screen_count = 500;
    if (level_validate_runtime_at(&level, err, sizeof(err), &where) == 0 ||
        expect_location("screens", &where, "screen_count", -1, "") != 0) return 1;
    level = base;
    level.physics.air_friction = NAN;
    if (level_validate_runtime_at(&level, err, sizeof(err), &where) == 0 ||
        expect_location("physics", &where, "physics", -1, "air_friction") != 0) return 1;
    level = base;
    level.player_start_x = 99999.0f;
    if (level_validate_runtime_at(&level, err, sizeof(err), &where) == 0 ||
        expect_location("player start", &where, "player_start", -1, "x") != 0) return 1;

    /* The editor's report keeps the location next to each message, and
     * maps TOML names back to its entity types. */
    {
        EditorValidationReport report;
        level = base;
        level.coins[1].x = 99999.0f;
        level.name[0] = '\0';
        (void)editor_validate_level(&level, &report);
        if (expect_int("report messages", report.message_count >= 2, 1) != 0 ||
            expect_location("report error", &report.locations[0], "coins", 1, "x") != 0 ||
            expect_location("report warning", &report.locations[report.message_count - 1],
                            "name", -1, "") != 0 ||
            expect_int("coins type", editor_entity_type_for_toml("coins"), ENT_COIN) != 0 ||
            expect_int("checkpoint type", editor_entity_type_for_toml("checkpoints"),
                       ENT_CHECKPOINT) != 0 ||
            expect_int("spawn type", editor_entity_type_for_toml("player_start"),
                       ENT_PLAYER_SPAWN) != 0 ||
            expect_int("config key", editor_entity_type_for_toml("screen_count"), ENT_COUNT) != 0)
            return 1;
    }
    return 0;
}

/* Every error level_validate_runtime_each reported, in order. */
typedef struct {
    int count;
    char first[256];
    LevelIssueLocation where[16];
} CollectedIssues;

static void collect_issue(void *context, const char *message,
                          const LevelIssueLocation *where)
{
    CollectedIssues *issues = context;
    if (issues->count == 0) snprintf(issues->first, sizeof(issues->first), "%s", message);
    if (issues->count < 16) issues->where[issues->count] = *where;
    issues->count++;
}

/*
 * The editor lists every runtime error at once, while the game keeps its
 * single-error API: the first error collected is word for word the one
 * level_validate_runtime reports.  Checks that need an earlier value to be
 * good (t_offset needs a valid rail_index) are skipped instead of reading
 * past an array, and a bad array count stops everything.
 */
static int validation_reports_every_runtime_error(void)
{
    LevelDef level;
    CollectedIssues issues;
    EditorValidationReport report;
    char err[256];

    editor_level_init_defaults(&level);
    level.coin_count = 2;
    level.coins[0] = (CoinPlacement){100.0f, 100.0f};
    level.coins[1] = (CoinPlacement){99999.0f, 100.0f};      /* 1: coins[1].x  */
    level.spider_count = 1;
    level.spiders[0] = (SpiderPlacement){300.0f, 0.0f, 250.0f, 350.0f, 0};  /* vx 0 */
    level.axe_trap_count = 1;
    level.axe_traps[0] = (AxeTrapPlacement){.pillar_x = 500.0f, .mode = (AxeTrapMode)7};
    level.spike_block_count = 1;                               /* no rails at all */
    level.spike_blocks[0] = (SpikeBlockPlacement){5, 1.0f, 3.0f};
    level.physics.air_friction = NAN;

    memset(&issues, 0, sizeof(issues));
    if (expect_int("errors found",
                   level_validate_runtime_each(&level, collect_issue, &issues), 5) != 0 ||
        expect_int("errors reported", issues.count, 5) != 0 ||
        level_validate_runtime(&level, err, sizeof(err)) == 0 ||
        expect_int("first matches the game", strcmp(issues.first, err), 0) != 0 ||
        expect_location("physics first", &issues.where[0], "physics", -1, "air_friction") != 0 ||
        expect_location("spider vx", &issues.where[1], "spiders", 0, "vx") != 0 ||
        expect_location("coin x", &issues.where[2], "coins", 1, "x") != 0 ||
        expect_location("rider rail", &issues.where[3], "spike_blocks", 0, "rail_index") != 0 ||
        expect_location("axe mode", &issues.where[4], "axe_traps", 0, "mode") != 0)
        return 1;

    /* The editor's report lists them all, each with its location. */
    (void)editor_validate_level(&level, &report);
    if (expect_int("report errors", report.error_count, 5) != 0 ||
        expect_location("report first", &report.locations[0], "physics", -1, "air_friction") != 0 ||
        expect_location("report last", &report.locations[4], "axe_traps", 0, "mode") != 0 ||
        expect_int("nothing hidden", editor_validation_hidden_count(&report), 0) != 0)
        return 1;

    /* More errors than the list holds: the counts keep them all. */
    editor_level_init_defaults(&level);
    level.coin_count = EDITOR_VALIDATION_MAX_MESSAGES + 4;
    for (int i = 0; i < level.coin_count; i++)
        level.coins[i] = (CoinPlacement){-5.0f, 100.0f};
    (void)editor_validate_level(&level, &report);
    if (expect_int("listed", report.message_count, EDITOR_VALIDATION_MAX_MESSAGES) != 0 ||
        expect_int("counted", report.error_count, EDITOR_VALIDATION_MAX_MESSAGES + 4) != 0 ||
        expect_int("hidden", editor_validation_hidden_count(&report) >= 4, 1) != 0)
        return 1;

    /* A bad count is reported alone: the arrays cannot be walked. */
    level.coin_count = MAX_COINS + 1;
    memset(&issues, 0, sizeof(issues));
    if (expect_int("count stops", level_validate_runtime_each(&level, collect_issue, &issues), 1) != 0 ||
        expect_location("count location", &issues.where[0], "coin_count", -1, "") != 0)
        return 1;

    /* A valid level reports nothing. */
    editor_level_init_defaults(&level);
    memset(&issues, 0, sizeof(issues));
    if (expect_int("valid", level_validate_runtime_each(&level, collect_issue, &issues), 0) != 0 ||
        expect_int("valid calls", issues.count, 0) != 0)
        return 1;
    return 0;
}

/*
 * A rail copied together with the spike block riding it pastes as a new
 * rail with the copy riding the NEW rail; deleting the pair is one undo
 * step even though the rail must go after its rider.
 */
static int group_copy_and_delete_keep_riders_with_their_rail(void)
{
    EditorState es = {0};
    LevelDef original;
    Command cmd;
    int group;

    editor_level_init_defaults(&es.level);
    es.level.rail_count = 2;
    es.level.rails[0] = (RailPlacement){RAIL_LAYOUT_RECT, 32, 32, 4, 4, 0};
    es.level.rails[1] = (RailPlacement){RAIL_LAYOUT_RECT, 400, 32, 4, 4, 0};
    es.level.spike_block_count = 1;
    es.level.spike_blocks[0] = (SpikeBlockPlacement){1, 1.0f, 3.0f};
    es.undo = undo_create();
    if (!es.undo || level_is_valid("group rail base", &es.level) != 0) goto fail;
    original = es.level;
    editor_set_document_save_point(&es);

    (void)editor_select_items(&es, (Selection[]){{ENT_SPIKE_BLOCK, 0}, {ENT_RAIL, 1}}, 2);
    editor_copy_selected(&es);
    editor_paste_clipboard(&es);
    if (expect_int("pasted rail", es.level.rail_count, 3) != 0 ||
        expect_int("pasted rider", es.level.spike_block_count, 2) != 0 ||
        expect_int("copy rides the copied rail", es.level.spike_blocks[1].rail_index, 2) != 0 ||
        expect_int("original keeps its rail", es.level.spike_blocks[0].rail_index, 1) != 0 ||
        expect_int("both copies selected", editor_selection_count(&es), 2) != 0 ||
        level_is_valid("after group paste", &es.level) != 0) goto fail;

    /* Undo the paste as one step (the editor pops a whole group). */
    group = undo_top_group(es.undo);
    while (undo_top_group(es.undo) == group && group != 0 && undo_pop(es.undo, &cmd))
        editor_apply_undo_command(&es, &cmd, 1);
    /* (The hash covers what is in use; removed slots may keep old bytes.) */
    if (expect_int("paste undone",
                   editor_document_hash(&es.level) == editor_document_hash(&original), 1) != 0)
        goto fail;

    /* Delete the rail together with its rider: allowed, and one step. */
    (void)editor_select_items(&es, (Selection[]){{ENT_RAIL, 1}, {ENT_SPIKE_BLOCK, 0}}, 2);
    tools_delete_selected(&es);
    if (expect_int("pair deleted rails", es.level.rail_count, 1) != 0 ||
        expect_int("pair deleted riders", es.level.spike_block_count, 0) != 0) goto fail;
    group = undo_top_group(es.undo);
    while (undo_top_group(es.undo) == group && group != 0 && undo_pop(es.undo, &cmd))
        editor_apply_undo_command(&es, &cmd, 1);
    if (expect_int("delete undone exactly",
                   editor_document_hash(&es.level) == editor_document_hash(&original), 1) != 0)
        goto fail;

    /* A group paste that cannot complete adds nothing at all. */
    es.level.coin_count = MAX_COINS - 1;
    for (int i = 0; i < MAX_COINS - 1; i++) es.level.coins[i] = (CoinPlacement){10.0f, 10.0f};
    (void)editor_select_items(&es, (Selection[]){{ENT_COIN, 0}, {ENT_COIN, 1}}, 2);
    editor_copy_selected(&es);
    {
        int top = es.undo->top;
        editor_paste_clipboard(&es);
        if (expect_int("all or nothing", es.level.coin_count, MAX_COINS - 1) != 0 ||
            expect_int("no history left", es.undo->top, top) != 0 ||
            expect_int("nothing to redo", es.undo->redo_top, 0) != 0) goto fail;
    }
    undo_destroy(es.undo);
    return 0;
fail:
    undo_destroy(es.undo);
    return 1;
}

static int rail_deletion_keeps_references_valid(void)
{
    EditorState es = {0};
    LevelDef original;
    Command cmd;

    fill_rail_level(&es.level);
    original = es.level;
    es.undo = undo_create();
    if (!es.undo) return 1;
    editor_set_document_save_point(&es);

    /* A referenced rail is refused with an explanation. */
    es.selection.type = ENT_RAIL;
    es.selection.index = 1;
    tools_delete_selected(&es);
    if (expect_int("referenced rail kept", es.level.rail_count, 3) != 0 ||
        expect_prefix("referenced rail status", es.status_message,
                      "Rail 1 is used by 0 spike blocks and 1 float platform") != 0 ||
        expect_int("refusal records no history", es.undo->top, 0) != 0) goto fail;

    /* An unused rail is removed and later references shift down. */
    es.selection.index = 0;
    tools_delete_selected(&es);
    if (expect_int("unused rail removed", es.level.rail_count, 2) != 0 ||
        expect_int("spike block follows rail", es.level.spike_blocks[0].rail_index, 1) != 0 ||
        expect_int("float platform follows rail",
                   es.level.float_platforms[0].rail_index, 0) != 0 ||
        level_is_valid("after rail delete", &es.level) != 0) goto fail;

    /* Undo restores the exact document, so the save point is clean again. */
    if (!undo_pop(es.undo, &cmd)) goto fail;
    editor_apply_undo_command(&es, &cmd, 1);
    if (expect_int("undo rail count", es.level.rail_count, 3) != 0 ||
        expect_int("undo restores bytes",
                   memcmp(&es.level, &original, sizeof(original)) == 0, 1) != 0 ||
        expect_int("undo rail clean", es.modified, 0) != 0) goto fail;

    if (!redo_pop(es.undo, &cmd)) goto fail;
    editor_apply_undo_command(&es, &cmd, 0);
    if (expect_int("redo spike index", es.level.spike_blocks[0].rail_index, 1) != 0 ||
        level_is_valid("after rail redo", &es.level) != 0) goto fail;

    /* A copy made before the delete still pastes onto the rail it rode:
     * undo the delete, copy the spike block (rail 2), redo the delete
     * (that rail is now index 1), then paste. */
    if (!undo_pop(es.undo, &cmd)) goto fail;
    editor_apply_undo_command(&es, &cmd, 1);
    es.selection.type = ENT_SPIKE_BLOCK;
    es.selection.index = 0;
    editor_copy_selected(&es);
    if (!redo_pop(es.undo, &cmd)) goto fail;
    editor_apply_undo_command(&es, &cmd, 0);
    editor_paste_clipboard(&es);
    if (expect_int("pasted copy added", es.level.spike_block_count, 2) != 0 ||
        expect_int("pasted copy follows its rail",
                   es.level.spike_blocks[1].rail_index, 1) != 0 ||
        level_is_valid("after rail-rider paste", &es.level) != 0) goto fail;

    undo_destroy(es.undo);
    return 0;
fail:
    undo_destroy(es.undo);
    return 1;
}

/* Press on (x, y), move to (x + dx, y + dy), release. */
static void drag_by(EditorState *es, float x, float y, float dx, float dy)
{
    tools_mouse_down(es, x, y);
    tools_mouse_drag(es, x + dx, y + dy);
    tools_mouse_up(es, x + dx, y + dy);
}

static int undo_last(EditorState *es)
{
    Command cmd;
    if (!undo_pop(es->undo, &cmd)) return 1;
    editor_apply_undo_command(es, &cmd, 1);
    return 0;
}

static int drag_round_trips_and_follows_grab_point(void)
{
    EditorState es = {0};
    EditorRect r;
    InputEvent event;

    editor_level_init_defaults(&es.level);
    es.undo = undo_create();
    if (!es.undo) return 1;
    es.camera.zoom = 2.0f;
    es.tool = TOOL_SELECT;
    es.level.floor_gap_count = 3;
    es.level.floor_gaps[0] = 64;
    es.level.floor_gaps[1] = 96;
    es.level.floor_gaps[2] = 320;
    es.level.blue_flame_count = 1;
    es.level.blue_flames[0].x = 64.0f;
    es.level.axe_trap_count = 1;
    es.level.axe_traps[0] = (AxeTrapPlacement){200.0f, 0.0f, AXE_MODE_PENDULUM};
    es.level.coin_count = 1;
    es.level.coins[0] = (CoinPlacement){100.0f, 100.0f};
    es.level.spider_count = 1;
    es.level.spiders[0] = (SpiderPlacement){100.0f, 50.0f, 50.0f, 150.0f, 0};
    editor_set_document_save_point(&es);

    /* A flame is drawn centred in its gap; dragging must move the gap by
     * exactly the cursor distance, and undo must return to the save point. */
    if (!editor_entity_bounds(&es.level, ENT_BLUE_FLAME, 0, &r)) goto fail;
    drag_by(&es, r.x + 10.0f, r.y + 10.0f, 32.0f, 0.0f);
    if (expect_float_value("flame drag exact", es.level.blue_flames[0].x, 96.0f) != 0 ||
        undo_last(&es) != 0 ||
        expect_float_value("flame undo", es.level.blue_flames[0].x, 64.0f) != 0 ||
        expect_int("flame undo clean", es.modified, 0) != 0) goto fail;

    /* A flame erupts from a gap, so a drag between gaps lands on the
     * nearest one: 64 + 150 = 214 is 106 px from 320 and 118 px from 96;
     * 64 + 100 = 164 is closer to 96. */
    drag_by(&es, r.x + 10.0f, r.y + 10.0f, 150.0f, 0.0f);
    if (expect_float_value("flame drag snaps to nearest gap",
                           es.level.blue_flames[0].x, 320.0f) != 0 ||
        undo_last(&es) != 0) goto fail;
    drag_by(&es, r.x + 10.0f, r.y + 10.0f, 100.0f, 0.0f);
    if (expect_float_value("flame drag snaps back to closer gap",
                           es.level.blue_flames[0].x, 96.0f) != 0 ||
        undo_last(&es) != 0 ||
        expect_int("flame snap undo clean", es.modified, 0) != 0) goto fail;

    /* Axe y = 0 means "default height"; a horizontal move keeps the 0. */
    if (!editor_entity_bounds(&es.level, ENT_AXE_TRAP, 0, &r)) goto fail;
    drag_by(&es, r.x + 5.0f, r.y + 5.0f, 40.0f, 0.0f);
    if (expect_float_value("axe drag x", es.level.axe_traps[0].pillar_x, 240.0f) != 0 ||
        expect_float_value("axe default y kept", es.level.axe_traps[0].y, 0.0f) != 0 ||
        undo_last(&es) != 0 ||
        expect_int("axe undo clean", es.modified, 0) != 0) goto fail;
    drag_by(&es, r.x + 5.0f, r.y + 5.0f, 0.0f, 10.0f);
    if (expect_float_value("axe vertical drag", es.level.axe_traps[0].y,
                           level_axe_trap_y(&(AxeTrapPlacement){0}) + 10.0f) != 0 ||
        undo_last(&es) != 0 || expect_int("axe y undo clean", es.modified, 0) != 0)
        goto fail;
    /* Moving it up to exactly y = 0 must not read back as "default". */
    drag_by(&es, r.x + 5.0f, r.y + 5.0f, 0.0f,
            -level_axe_trap_y(&(AxeTrapPlacement){0}));
    if (expect_int("axe at top keeps a custom y", es.level.axe_traps[0].y > 0.0f, 1) != 0 ||
        undo_last(&es) != 0) goto fail;

    /* A click, or a wobble under the threshold, changes nothing. */
    tools_mouse_down(&es, 104.0f, 104.0f);
    tools_mouse_drag(&es, 104.5f, 104.0f);
    tools_mouse_up(&es, 104.5f, 104.0f);
    if (expect_int("click records nothing", es.undo->top, 0) != 0 ||
        expect_float_value("click keeps coin", es.level.coins[0].x, 100.0f) != 0 ||
        expect_int("click selects coin", es.selection.type, ENT_COIN) != 0) goto fail;

    /* The grabbed point follows the cursor: no jump to the corner. */
    drag_by(&es, 104.0f, 104.0f, 10.0f, 0.0f);
    if (expect_float_value("grab offset x", es.level.coins[0].x, 110.0f) != 0 ||
        expect_float_value("grab offset y", es.level.coins[0].y, 100.0f) != 0 ||
        undo_last(&es) != 0) goto fail;

    /* Spiders take their patrol range along and stay inside the world. */
    drag_by(&es, 110.0f, 245.0f, 40.0f, 0.0f);
    if (expect_float_value("spider x", es.level.spiders[0].x, 140.0f) != 0 ||
        expect_float_value("spider x0", es.level.spiders[0].patrol_x0, 90.0f) != 0 ||
        expect_float_value("spider x1", es.level.spiders[0].patrol_x1, 190.0f) != 0)
        goto fail;
    drag_by(&es, 150.0f, 245.0f, 5000.0f, 0.0f);
    if (expect_float_value("spider range clamped", es.level.spiders[0].patrol_x1,
                           editor_world_width(&es.level)) != 0 ||
        level_is_valid("after far spider drag", &es.level) != 0) goto fail;
    undo_clear(es.undo);
    editor_set_document_save_point(&es);

    /* Undo is ignored mid-drag; Esc cancels the move without history. */
    tools_mouse_down(&es, 104.0f, 104.0f);
    tools_mouse_drag(&es, 140.0f, 104.0f);
    memset(&event, 0, sizeof(event));
    event.type = INPUT_KEY_DOWN;
    event.key = KEY_Z;
    event.mods = INPUT_CTRL;
    editor_handle_event(&es, &event);
    if (expect_float_value("undo blocked during drag", es.level.coins[0].x, 136.0f) != 0 ||
        expect_prefix("drag blocks keys", es.status_message, "Release the mouse") != 0)
        goto fail;
    event.key = KEY_ESCAPE;
    event.mods = 0;
    editor_handle_event(&es, &event);
    if (expect_float_value("esc restores", es.level.coins[0].x, 100.0f) != 0 ||
        expect_int("esc ends drag", es.dragging, 0) != 0 ||
        expect_int("esc records nothing", es.undo->top, 0) != 0) goto fail;

    /* A level broken while the button is held puts the entity back. */
    tools_mouse_down(&es, 104.0f, 104.0f);
    tools_mouse_drag(&es, 140.0f, 104.0f);
    es.level.music_volume = 999;
    tools_mouse_up(&es, 140.0f, 104.0f);
    es.level.music_volume = 0;
    if (expect_float_value("invalid release restores", es.level.coins[0].x, 100.0f) != 0 ||
        expect_int("invalid release records nothing", es.undo->top, 0) != 0) goto fail;

    undo_destroy(es.undo);
    return 0;
fail:
    undo_destroy(es.undo);
    return 1;
}

/*
 * Every editor mutation must leave a level that passes validation: for each
 * palette type, place at the world corners, paste repeatedly and drag far
 * outside the world.  Refusals are fine; an invalid level is not.
 */
static int editor_mutations_keep_level_valid(void)
{
    char name[96];

    for (int p = 0; p < editor_entity_palette_entry_count(); p++) {
        EntityType type = editor_entity_palette_entry_type(p);
        EditorState es = {0};
        float world_w;
        /* x = -1 stands for "half a pixel before the world's right edge". */
        const float points[3][2] = {
            {0.5f, 0.5f}, {200.0f, 150.0f}, {-1.0f, (float)GAME_H - 0.5f}
        };

        editor_level_init_defaults(&es.level);
        es.level.rail_count = 1;  /* lets spike blocks attach */
        es.level.rails[0] = (RailPlacement){RAIL_LAYOUT_RECT, 32, 32, 4, 4, 0};
        /* Lets flames erupt; a gap at each end so every corner has one near. */
        es.level.floor_gap_count = 2;
        es.level.floor_gaps[0] = 0;
        es.level.floor_gaps[1] = (int)editor_world_width(&es.level) - FLOOR_GAP_W;
        es.undo = undo_create();
        if (!es.undo) return 1;
        es.camera.zoom = 1.0f;
        es.palette_type = type;
        world_w = editor_world_width(&es.level);

        for (int i = 0; i < 3; i++) {
            float x = points[i][0] < 0.0f ? world_w - 0.5f : points[i][0];
            es.tool = TOOL_PLACE;
            tools_mouse_down(&es, x, points[i][1]);
            snprintf(name, sizeof(name), "place %s at %d",
                     editor_entity_type_name(type), i);
            if (level_is_valid(name, &es.level) != 0) goto fail;
        }
        if (es.selection.index < 0) {
            fprintf(stderr, "editor_validation_test: %s was never placed (%s)\n",
                    editor_entity_type_name(type), es.status_message);
            goto fail;
        }

        editor_copy_selected(&es);
        for (int i = 0; i < 4; i++) {
            editor_paste_clipboard(&es);
            snprintf(name, sizeof(name), "paste %s #%d",
                     editor_entity_type_name(type), i);
            if (level_is_valid(name, &es.level) != 0) goto fail;
        }

        {
            EditorRect r;
            if (editor_entity_bounds(&es.level, es.selection.type,
                                     es.selection.index, &r)) {
                es.tool = TOOL_SELECT;
                drag_by(&es, r.x + 1.0f, r.y + 1.0f, 9000.0f, 9000.0f);
                drag_by(&es, r.x + 1.0f, r.y + 1.0f, -9000.0f, -9000.0f);
                snprintf(name, sizeof(name), "drag %s",
                         editor_entity_type_name(type));
                if (level_is_valid(name, &es.level) != 0) goto fail;
            }
        }
        undo_destroy(es.undo);
        continue;
fail:
        undo_destroy(es.undo);
        return 1;
    }
    return 0;
}

static int refused_mutations_explain_why(void)
{
    EditorState es = {0};

    editor_level_init_defaults(&es.level);
    es.undo = undo_create();
    if (!es.undo) return 1;

    /* Rail riders need a rail. */
    es.tool = TOOL_PLACE;
    es.palette_type = ENT_SPIKE_BLOCK;
    tools_mouse_down(&es, 100.0f, 100.0f);
    if (expect_int("no rail no spike block", es.level.spike_block_count, 0) != 0 ||
        expect_string("no rail status", es.status_message,
                      "Cannot place Spike Block: place a rail first") != 0) goto fail;

    /* Flames erupt from a floor gap; with none there is nowhere to go. */
    es.palette_type = ENT_FIRE_FLAME;
    tools_mouse_down(&es, 100.0f, 100.0f);
    if (expect_int("no gap no flame", es.level.fire_flame_count, 0) != 0 ||
        expect_string("no gap status", es.status_message,
                      "Cannot place Fire Flame: place a floor gap first") != 0) goto fail;
    /* With gaps, a click lands the flame on the gap under the cursor
     * (the click is the flame's centre, like the ghost preview). */
    es.level.floor_gap_count = 2;
    es.level.floor_gaps[0] = 64;
    es.level.floor_gaps[1] = 96;
    tools_mouse_down(&es, 96.0f + FLOOR_GAP_W / 2.0f + 5.0f, 100.0f);
    if (expect_int("flame on gap placed", es.level.fire_flame_count, 1) != 0 ||
        expect_float_value("flame on clicked gap", es.level.fire_flames[0].x, 96.0f) != 0)
        goto fail;
    /* Deleting the gap a flame stands on is refused with the rule. */
    es.tool = TOOL_SELECT;
    es.selection.type = ENT_FLOOR_GAP;
    es.selection.index = 1;
    tools_delete_selected(&es);
    if (expect_int("gap under flame kept", es.level.floor_gap_count, 2) != 0 ||
        expect_prefix("gap under flame status", es.status_message,
                      "Cannot delete Floor Gap: fire_flames[0].x") != 0) goto fail;
    es.level.fire_flame_count = 0;
    es.level.floor_gap_count = 0;
    undo_clear(es.undo);
    es.tool = TOOL_PLACE;

    /* A full array is reported instead of silently ignored. */
    es.palette_type = ENT_COIN;
    es.level.coin_count = MAX_COINS;
    for (int i = 0; i < MAX_COINS; i++)
        es.level.coins[i] = (CoinPlacement){10.0f, 10.0f};
    tools_mouse_down(&es, 100.0f, 100.0f);
    if (expect_int("full coins", es.level.coin_count, MAX_COINS) != 0 ||
        expect_prefix("full coin status", es.status_message,
                      "Cannot place Coin: limit of") != 0) goto fail;
    es.selection.type = ENT_COIN;
    es.selection.index = 0;
    editor_copy_selected(&es);
    editor_paste_clipboard(&es);
    if (expect_prefix("full paste status", es.status_message,
                      "Cannot paste Coin: limit of") != 0) goto fail;
    es.level.coin_count = 0;

    /* Rules involving other entities are checked after clamping. */
    es.palette_type = ENT_CHECKPOINT;
    tools_mouse_down(&es, 20.0f, 100.0f);
    if (expect_int("checkpoint behind start refused", es.level.checkpoint_count, 0) != 0 ||
        expect_prefix("checkpoint status", es.status_message,
                      "Cannot place Checkpoint here:") != 0) goto fail;

    /* A clipboard from another level cannot ride a rail this level lacks:
     * the copy remembers its rail's shape, and no rail here matches it. */
    es.clipboard_count = 1;
    memset(&es.clipboard[0], 0, sizeof(es.clipboard[0]));
    es.clipboard[0].type = ENT_SPIKE_BLOCK;
    es.clipboard[0].data.spike_block = (SpikeBlockPlacement){2, 0.0f, 3.0f};
    es.clipboard[0].rail_index = -1;     /* copied in another document */
    es.clipboard[0].rail_item = -1;
    es.clipboard[0].has_rail = 1;
    es.clipboard[0].rail = (RailPlacement){RAIL_LAYOUT_HORIZ, 200, 80, 6, 1, 1};
    es.level.rail_count = 1;
    es.level.rails[0] = (RailPlacement){RAIL_LAYOUT_RECT, 32, 32, 4, 4, 0};
    editor_paste_clipboard(&es);
    if (expect_int("missing rail paste refused", es.level.spike_block_count, 0) != 0 ||
        expect_string("missing rail status", es.status_message,
                      "Paste blocked: the copied Spike Block's rail is not in this level") != 0)
        goto fail;

    /* The same copy re-attaches by rail shape, whatever index it had: the
     * stored index 2 becomes 0, where the matching rail lives here. */
    es.clipboard[0].rail = es.level.rails[0];
    es.clipboard[0].data.spike_block = (SpikeBlockPlacement){2, 11.5f, 3.0f};
    /* A t_offset past the end of a loop also wraps onto the rail. */
    editor_paste_clipboard(&es);
    if (expect_int("wrapped paste count", es.level.spike_block_count, 1) != 0 ||
        expect_int("pasted rider re-attached by shape",
                   es.level.spike_blocks[0].rail_index, 0) != 0 ||
        level_is_valid("wrapped spike block", &es.level) != 0) goto fail;

    undo_destroy(es.undo);
    return 0;
fail:
    undo_destroy(es.undo);
    return 1;
}

static int camera_scrolls_vertically_and_stays_clamped(void)
{
    EditorState es = {0};
    InputEvent event;
    float wx, wy;
    int canvas_bottom = TOOLBAR_H + CANVAS_H - 1;

    editor_level_init_defaults(&es.level);
    es.undo = undo_create();
    if (!es.undo) return 1;
    es.camera.zoom = 2.0f;

    /* Ctrl+wheel near the bottom zooms to 3x around the cursor, so the
     * floor that was under the cursor stays on screen. */
    memset(&event, 0, sizeof(event));
    event.type = INPUT_WHEEL;
    event.x = 200;
    event.y = canvas_bottom;
    event.wheel = 1.0f;
    event.mods = INPUT_CTRL;
    canvas_screen_to_world(&es, event.x, event.y, &wx, &wy);
    editor_handle_event(&es, &event);
    {
        float after_x, after_y;
        canvas_screen_to_world(&es, event.x, event.y, &after_x, &after_y);
        if (expect_float_value("zoom preset", es.camera.zoom, 3.0f) != 0 ||
            expect_float_value("zoom keeps cursor x", after_x, wx) != 0 ||
            expect_int("zoom scrolls down", es.camera.y > 0.0f, 1) != 0) goto fail;
    }
    {
        float floor_screen = ((float)FLOOR_Y - es.camera.y) * es.camera.zoom + TOOLBAR_H;
        if (expect_int("floor visible at 3x", floor_screen >= TOOLBAR_H &&
                       floor_screen <= TOOLBAR_H + CANVAS_H, 1) != 0) goto fail;
    }

    /* Shift+wheel pans vertically and clamps at the world's bottom edge. */
    event.mods = INPUT_SHIFT;
    event.wheel = -50.0f;
    editor_handle_event(&es, &event);
    if (expect_float_value("vertical clamp",
                           es.camera.y, (float)GAME_H - CANVAS_H / es.camera.zoom) != 0)
        goto fail;

    /* macOS reports Shift+wheel as horizontal scroll; it still pans up/down. */
    event.wheel = 0.0f;
    event.wheel_x = 50.0f;
    editor_handle_event(&es, &event);
    if (expect_float_value("shift horizontal wheel pans vertically", es.camera.y, 0.0f) != 0)
        goto fail;
    event.wheel = -50.0f;
    event.wheel_x = 0.0f;
    editor_handle_event(&es, &event);

    /* Clicks use the vertical offset too. */
    es.tool = TOOL_PLACE;
    es.palette_type = ENT_COIN;
    event.type = INPUT_MOUSE_DOWN;
    event.button = MOUSE_BUTTON_LEFT;
    event.mods = 0;
    event.x = 90;
    event.y = TOOLBAR_H + 30;
    editor_handle_event(&es, &event);
    if (expect_int("coin placed", es.level.coin_count, 1) != 0 ||
        expect_float_value("click world y", es.level.coins[0].y,
                           es.camera.y + 30.0f / es.camera.zoom) != 0) goto fail;

    /* Right-click deletes, but not during a left-button drag: deleting
     * would shift the array under the drag and overwrite another entity. */
    es.tool = TOOL_SELECT;
    tools_mouse_down(&es, es.level.coins[0].x + 4.0f, es.level.coins[0].y + 4.0f);
    tools_mouse_drag(&es, es.level.coins[0].x + 40.0f, es.level.coins[0].y + 4.0f);
    event.button = MOUSE_BUTTON_RIGHT;
    editor_handle_event(&es, &event);
    if (expect_int("right-click ignored during drag", es.level.coin_count, 1) != 0)
        goto fail;
    tools_mouse_up(&es, es.level.coins[0].x, es.level.coins[0].y);
    event.type = INPUT_MOUSE_UP;
    editor_handle_event(&es, &event);

    /* Zooming out or shrinking the level re-clamps the camera. */
    es.camera.x = 1000.0f;
    canvas_set_zoom(&es, 1.0f, 0, TOOLBAR_H);
    if (expect_float_value("1x has no vertical scroll", es.camera.y, 0.0f) != 0) goto fail;
    es.level.screen_count = 3;
    canvas_clamp_camera(&es);
    if (expect_float_value("screen shrink clamps x", es.camera.x,
                           3.0f * GAME_W - CANVAS_W) != 0) goto fail;

    undo_destroy(es.undo);
    return 0;
fail:
    undo_destroy(es.undo);
    return 1;
}

static int playtest_blocks_editing_and_stop_cleans_up(void)
{
    EditorState es = {0};
    InputEvent event;

    editor_level_init_defaults(&es.level);
    es.undo = undo_create();
    if (!es.undo) return 1;
    es.camera.zoom = 1.0f;
    es.playing = 1;

    /* Canvas clicks and shortcuts do not edit while the game runs. */
    es.tool = TOOL_PLACE;
    es.palette_type = ENT_COIN;
    memset(&event, 0, sizeof(event));
    event.type = INPUT_MOUSE_DOWN;
    event.button = MOUSE_BUTTON_LEFT;
    event.x = 100;
    event.y = TOOLBAR_H + 100;
    editor_handle_event(&es, &event);
    if (expect_int("no place while playing", es.level.coin_count, 0) != 0 ||
        expect_int("stop button still sees click", es.ui.mouse_clicked, 1) != 0)
        goto fail;
    event.type = INPUT_KEY_DOWN;
    event.key = KEY_V;
    event.mods = INPUT_CTRL;
    es.clipboard_count = 1;
    es.clipboard[0].type = ENT_COIN;
    es.clipboard[0].data.coin = (CoinPlacement){50.0f, 50.0f};
    editor_handle_event(&es, &event);
    if (expect_int("no paste while playing", es.level.coin_count, 0) != 0 ||
        expect_prefix("playing status", es.status_message, "Playtest running") != 0)
        goto fail;

#ifndef _WIN32
    {
        /* A game that ignores SIGTERM is killed after the bounded wait,
         * reaped (no zombie), and the private level file is removed. */
        const char *level_path = TEST_OUT "test_editor_playtest_stop.toml";
        int ready[2];
        char byte = 0;
        pid_t pid;

        if (write_text_file(level_path, "format_version = 1\n") != 0 ||
            pipe(ready) != 0) goto fail;
        pid = fork();
        if (pid < 0) goto fail;
        if (pid == 0) {
            struct sigaction ignore;
            memset(&ignore, 0, sizeof(ignore));
            ignore.sa_handler = SIG_IGN;
            sigaction(SIGTERM, &ignore, NULL);
            if (write(ready[1], "r", 1) != 1) _exit(1);
            for (;;) pause();
        }
        close(ready[1]);
        /* Wait until the child ignores SIGTERM, so the SIGKILL path runs. */
        if (read(ready[0], &byte, 1) != 1) byte = 0;
        close(ready[0]);

        es.play_pid = (int)pid;
        strncpy(es.playtest_path, level_path, sizeof(es.playtest_path) - 1);
        editor_stop_play(&es);
        if (expect_int("stop clears playing", es.playing, 0) != 0 ||
            expect_int("stop clears pid", es.play_pid, 0) != 0 ||
            expect_int("child reaped", waitpid(pid, NULL, WNOHANG) == -1 &&
                       errno == ECHILD, 1) != 0 ||
            expect_int("playtest file removed", editor_file_exists(level_path), 0) != 0) {
            kill(pid, SIGKILL);
            waitpid(pid, NULL, 0);
            remove(level_path);
            goto fail;
        }
    }
#endif

    undo_destroy(es.undo);
    return 0;
fail:
    undo_destroy(es.undo);
    return 1;
}

/* Load hook: append a TOML comment, as if another program saved the file
 * while the editor was parsing it.  Only the first `touches_left` calls
 * touch the file. */
static int touches_left;
static void touch_level_during_load(const char *path)
{
    FILE *fp;
    if (touches_left <= 0) return;
    touches_left--;
    fp = fopen(path, "ab");
    if (!fp) return;
    fputs("# changed by another program\n", fp);
    fclose(fp);
}

static int load_fingerprints_the_bytes_it_parsed(void)
{
    const char *path = TEST_OUT "test_editor_load_race.toml";
    EditorState es = {0};
    LevelDef fixture;
    SerializerFileFingerprint on_disk;
    char root[EDITOR_PATH_MAX] = {0};
    int result = 1;

    ensure_out_dir();
    editor_level_init_defaults(&fixture);
    if (level_save_toml(&fixture, path) != 0 ||
        make_test_preference_root(root, sizeof(root)) != 0 ||
        editor_set_preference_root(&es, root) != 0) return 1;
    es.undo = undo_create();
    if (!es.undo) return 1;

    /* One change during the first read: the retry reads stable bytes and
     * the Save baseline matches the file as it is now. */
    touches_left = 1;
    editor_test_set_load_hook(touch_level_during_load);
    if (expect_int("load retries after change", editor_load_level(&es, path), 0) != 0 ||
        serializer_fingerprint_utf8(path, &on_disk) != 1 ||
        expect_int("baseline matches file",
                   serializer_fingerprint_equal(&es.source_fingerprint, &on_disk), 1) != 0)
        goto cleanup;

    /* A file that keeps changing is refused rather than half-trusted. */
    strncpy(es.level.name, "Kept", sizeof(es.level.name) - 1);
    touches_left = 2;
    if (expect_int("changing file refused", editor_load_level(&es, path), -1) != 0 ||
        expect_prefix("changing file status", es.status_message, "Load failed:") != 0 ||
        expect_int("status explains", strstr(es.status_message, "kept changing") != NULL, 1) != 0 ||
        expect_string("document untouched", es.level.name, "Kept") != 0) goto cleanup;
    result = 0;

cleanup:
    editor_test_set_load_hook(NULL);
    cleanup_test_preference_root(root, &es, 1);
    undo_destroy(es.undo);
    remove(path);
    return result;
}

static int recovery_metadata_keeps_longest_source_path(void)
{
    EditorState es = {0};
    EditorState restarted = {0};
    char root[EDITOR_PATH_MAX] = {0};
    char long_path[EDITOR_PATH_MAX];
    size_t length = 1020;  /* just under EDITOR_PATH_MAX - 1 */
    int result = 1;

    ensure_out_dir();
    memcpy(long_path, TEST_OUT, sizeof(TEST_OUT) - 1);
    memset(long_path + sizeof(TEST_OUT) - 1, 'p', length - (sizeof(TEST_OUT) - 1) - 5);
    memcpy(long_path + length - 5, ".toml", 6);
    if (strlen(long_path) != length || !editor_path_fits(long_path)) return 1;

    if (make_test_preference_root(root, sizeof(root)) != 0 ||
        editor_set_preference_root(&es, root) != 0 ||
        editor_set_preference_root(&restarted, root) != 0) return 1;
    editor_level_init_defaults(&es.level);
    memcpy(es.file_path, long_path, length + 1);
    if (editor_init_persistence_paths(&es) != 0) goto cleanup;
    es.modified = 1;
    es.last_autosave_ms = (uint32_t)clock_millis() - 30001u;
    editor_maybe_autosave(&es);

    /* A restarted editor must rediscover it with the full source path. */
    if (editor_init_persistence_paths(&restarted) != 0 ||
        expect_int("long path entry found", restarted.recovery_entry_count, 1) != 0 ||
        expect_int("long path round trip",
                   strcmp(restarted.recovery_entries[0].source_path, long_path) == 0,
                   1) != 0) goto cleanup;
    result = 0;

cleanup:
    cleanup_test_preference_root(root, (EditorState[]){es, restarted}, 2);
    return result;
}

typedef struct {
    TextFont *font;
    int drawing;
} EditorWidgetTestContext;

static int editor_widget_test_context_init(EditorWidgetTestContext *context)
{
    memset(context, 0, sizeof(*context));
    if (display_open(320, 240, "editor state test", 1)) return -1;
    context->font = font_load();
    if (!context->font) return -1;
    BeginDrawing();
    context->drawing = 1;
    return 0;
}

static void editor_widget_test_context_cleanup(EditorWidgetTestContext *context)
{
    if (context->font) {
        font_unload(context->font);
        context->font = NULL;
    }
    if (context->drawing) { EndDrawing(); context->drawing = 0; }
    if (IsWindowReady()) CloseWindow();
}

static int widget_commit_paths_preserve_values(void)
{
    EditorWidgetTestContext context = {0};
    UIState ui;
    EditorState es;
    char long_text[512];
    int changed;

    if (editor_widget_test_context_init(&context) != 0) {
        editor_widget_test_context_cleanup(&context);
        fprintf(stderr, "editor_validation_test: widget raylib setup failed\n");
        return 1;
    }

    ui_init(&ui, context.font);
    memset(long_text, 'x', 200);
    long_text[200] = '\0';
    ui_begin_frame(&ui);
    ui.mouse_clicked = 1;
    ui.mouse_x = 4;
    ui.mouse_y = 4;
    (void)ui_text_field(&ui, 1, 0, 0, 300, long_text, (int)sizeof(long_text));
    ui_begin_frame(&ui);
    ui.key_return = 1;
    changed = ui_text_field(&ui, 1, 0, 0, 300, long_text, (int)sizeof(long_text));
    if (expect_int("long text no-op commit", changed, 0) != 0 ||
        expect_int("long text remains lossless", (int)strlen(long_text), 200) != 0)
        goto fail;

    {
        int integer = 7;
        ui_begin_frame(&ui);
        ui.mouse_clicked = 1;
        ui.mouse_x = 4;
        ui.mouse_y = 28;
        (void)ui_int_field(&ui, 2, 0, 24, 120, &integer);
        ui_queue_text_input(&ui, "-");
        ui.key_return = 1;
        changed = ui_int_field(&ui, 2, 0, 24, 120, &integer);
        if (expect_int("invalid integer rejected", changed, 0) != 0 ||
            expect_int("invalid integer preserved", integer, 7) != 0)
            goto fail;
        ui_cancel_active_edit(&ui);
    }

    {
        float precise = 0.08f;
        ui_begin_frame(&ui);
        ui.mouse_clicked = 1;
        ui.mouse_x = 4;
        ui.mouse_y = 52;
        (void)ui_float_field(&ui, 3, 0, 48, 120, &precise);
        ui_begin_frame(&ui);
        ui.key_return = 1;
        changed = ui_float_field(&ui, 3, 0, 48, 120, &precise);
        if (expect_int("0.08 no-op commit", changed, 0) != 0 ||
            expect_float_value("0.08 precision", precise, 0.08f) != 0)
            goto fail;
    }

    memset(&es, 0, sizeof(es));
    editor_level_init_defaults(&es.level);
    es.level.coin_count = 1;
    es.level.coins[0].x = 0.0f;
    es.level.coins[0].y = 32.0f;
    es.selection.type = ENT_COIN;
    es.selection.index = 0;
    es.undo = undo_create();
    if (!es.undo) goto fail;
    editor_set_document_save_point(&es);

    editor_begin_change_tracking(&es, EDITOR_CHANGE_ENTITY);
    ui.before_change = editor_before_change;
    ui.before_change_context = &es;
    ui_begin_frame(&ui);
    ui.mouse_clicked = 1;
    ui.mouse_x = 4;
    ui.mouse_y = 76;
    (void)ui_float_field(&ui, 4, 0, 72, 120, &es.level.coins[0].x);
    ui_queue_text_input(&ui, "0.16");
    ui.key_return = 1;
    changed = ui_float_field(&ui, 4, 0, 72, 120, &es.level.coins[0].x);
    if (changed) editor_commit_change(&es);
    if (expect_int("float widget command", es.undo->top, 1) != 0 ||
        expect_float_value("float widget value", es.level.coins[0].x, 0.16f) != 0)
        goto fail_with_undo;

    undo_clear(es.undo);
    es.level.spike_row_count = 1;
    es.level.spike_rows[0].count = 0;
    es.selection.type = ENT_SPIKE_ROW;
    es.selection.index = 0;
    editor_set_document_save_point(&es);
    editor_begin_change_tracking(&es, EDITOR_CHANGE_ENTITY);
    ui.before_change = editor_before_change;
    ui.before_change_context = &es;
    ui_begin_frame(&ui);
    ui.mouse_clicked = 1;
    ui.mouse_x = 4;
    ui.mouse_y = 100;
    (void)ui_int_field(&ui, 5, 0, 96, 120, &es.level.spike_rows[0].count);
    ui_queue_text_input(&ui, "7");
    ui.key_return = 1;
    changed = ui_int_field(&ui, 5, 0, 96, 120, &es.level.spike_rows[0].count);
    if (changed) editor_commit_change(&es);
    if (expect_int("integer widget command", es.undo->top, 1) != 0 ||
        expect_int("integer widget value", es.level.spike_rows[0].count, 7) != 0)
        goto fail_with_undo;

    undo_clear(es.undo);
    es.level.rail_count = 1;
    es.level.rails[0].layout = RAIL_LAYOUT_RECT;
    es.selection.type = ENT_RAIL;
    es.selection.index = 0;
    editor_set_document_save_point(&es);
    editor_begin_change_tracking(&es, EDITOR_CHANGE_ENTITY);
    ui.before_change = editor_before_change;
    ui.before_change_context = &es;
    {
        static const char *options[] = { "Rect", "Horiz" };
        int selected = 0;
        ui_begin_frame(&ui);
        ui.mouse_clicked = 1;
        ui.mouse_x = 4;
        ui.mouse_y = 124;
        (void)ui_dropdown(&ui, 6, 0, 120, 120, options, 2, &selected);
        ui_begin_frame(&ui);
        ui.mouse_clicked = 1;
        ui.mouse_x = 4;
        ui.mouse_y = 161;
        if (ui_dropdown(&ui, 6, 0, 120, 120, options, 2, &selected)) {
            es.level.rails[0].layout = (RailLayout)selected;
            editor_commit_change(&es);
        }
    }
    if (expect_int("dropdown widget command", es.undo->top, 1) != 0 ||
        expect_int("dropdown widget value", es.level.rails[0].layout,
                   RAIL_LAYOUT_HORIZ) != 0)
        goto fail_with_undo;

    undo_clear(es.undo);
    es.level.description[0] = '\0';
    editor_set_document_save_point(&es);
    editor_begin_change_tracking(&es, EDITOR_CHANGE_CONFIG);
    ui.before_change = editor_before_change;
    ui.before_change_context = &es;
    ui_begin_frame(&ui);
    ui.mouse_clicked = 1;
    ui.mouse_x = 4;
    ui.mouse_y = 188;
    (void)ui_text_field(&ui, 7, 0, 184, 300, es.level.description,
                        (int)sizeof(es.level.description));
    ui_queue_text_input(&ui, "after");
    ui.key_return = 1;
    changed = ui_text_field(&ui, 7, 0, 184, 300, es.level.description,
                            (int)sizeof(es.level.description));
    if (changed) editor_commit_change(&es);
    if (expect_int("text widget config command", es.undo->top, 1) != 0 ||
        expect_string("text widget config value", es.level.description, "after") != 0)
        goto fail_with_undo;

    texture_unload(es.textures.floor_tile);
    undo_destroy(es.undo);
    ui_cleanup(&ui);
    editor_widget_test_context_cleanup(&context);
    return 0;

fail_with_undo:
    texture_unload(es.textures.floor_tile);
    undo_destroy(es.undo);
fail:
    ui_cleanup(&ui);
    editor_widget_test_context_cleanup(&context);
    return 1;
}

static int config_preview_sync_preserves_old_texture(void)
{
    EditorWidgetTestContext context = {0};
    EditorState es;
    Texture2D *old_texture;

    if (editor_widget_test_context_init(&context) != 0) {
        editor_widget_test_context_cleanup(&context);
        return 1;
    }

    memset(&es, 0, sizeof(es));
    old_texture = texture_load("assets/sprites/levels/grass_tileset.png");
    if (!old_texture) goto fail;
    es.textures.floor_tile = old_texture;
    strcpy(es.level.floor_tile_path, "assets/sprites/levels/does_not_exist.png");
    editor_sync_config_resources(&es);
    if (es.textures.floor_tile != old_texture) goto fail;

    /* A config edit that keeps the paths (here a rename) reloads nothing;
     * the replacement is loaded before the old one is freed, so a reload
     * would always produce a different pointer. */
    strcpy(es.level.floor_tile_path, "assets/sprites/levels/leaf_tileset.png");
    editor_sync_config_resources(&es);
    old_texture = es.textures.floor_tile;
    if (!old_texture) goto fail;
    strcpy(es.level.name, "Renamed");
    editor_sync_config_resources(&es);
    if (es.textures.floor_tile != old_texture) goto fail;
    strcpy(es.level.floor_tile_path, "assets/sprites/levels/grass_tileset.png");
    editor_sync_config_resources(&es);
    if (es.textures.floor_tile == old_texture) goto fail;

    texture_unload(es.textures.floor_tile);
    es.textures.floor_tile = NULL;
    editor_widget_test_context_cleanup(&context);
    return 0;

fail:
    texture_unload(es.textures.floor_tile);
    editor_widget_test_context_cleanup(&context);
    return 1;
}

static int staged_edit_save_and_quit_boundaries(void)
{
    const char *target = TEST_OUT "editor_staged_command.toml";
    EditorState save_state = {0};
    EditorState quit_state = {0};
    EditorState selection_state = {0};
    EditorWidgetTestContext context = {0};
    char root[EDITOR_PATH_MAX] = {0};
    LevelDef initial;
    InputEvent event;

    ensure_out_dir();
    if (editor_widget_test_context_init(&context) != 0) return 1;
    if (make_test_preference_root(root, sizeof(root)) != 0) goto fail;
    editor_level_init_defaults(&initial);
    initial.coin_count = 1;
    initial.coins[0].x = 0.0f;
    initial.coins[0].y = 32.0f;
    remove(target);
    if (level_save_toml(&initial, target) != 0) goto fail;

    save_state.level = initial;
    ui_init(&save_state.ui, context.font);
    save_state.undo = undo_create();
    strncpy(save_state.file_path, target, sizeof(save_state.file_path) - 1);
    if (!save_state.undo || editor_set_preference_root(&save_state, root) != 0 ||
        editor_init_persistence_paths(&save_state) != 0 ||
        serializer_fingerprint_utf8(target,
                                                        &save_state.source_fingerprint) != 1)
        goto fail;
    save_state.source_state = EDITOR_SOURCE_EXPECTED_EXISTING;
    editor_set_document_save_point(&save_state);
    save_state.selection.type = ENT_COIN;
    save_state.selection.index = 0;
    ui_begin_frame(&save_state.ui);
    save_state.ui.mouse_clicked = 1;
    save_state.ui.mouse_x = 4;
    save_state.ui.mouse_y = 4;
    (void)ui_float_field(&save_state.ui, 301, 0, 0, 120,
                         &save_state.level.coins[0].x);
    memset(&event, 0, sizeof(event));
    event.type = INPUT_TEXT;
    strncpy(event.text, "7", sizeof(event.text) - 1);
    editor_handle_event(&save_state, &event);
    event.text[0] = '2';
    event.text[1] = '\0';
    editor_handle_event(&save_state, &event);
    memset(&event, 0, sizeof(event));
    event.type = INPUT_KEY_DOWN;
    event.key = KEY_S;
    event.mods = INPUT_CTRL;
    editor_test_set_finish_field_choice(1);
    editor_handle_event(&save_state, &event);
    if (expect_prefix("staged Save succeeds", save_state.status_message,
                      "Saved ") != 0 ||
        expect_float_value("staged Save value", save_state.level.coins[0].x,
                           72.0f) != 0 ||
        expect_int("staged Save clean", save_state.modified, 0) != 0 ||
        expect_int("staged Save command", save_state.undo->top, 1) != 0)
        goto fail;

    editor_level_init_defaults(&quit_state.level);
    quit_state.level.coin_count = 1;
    quit_state.level.coins[0].x = 0.0f;
    ui_init(&quit_state.ui, context.font);
    quit_state.undo = undo_create();
    if (!quit_state.undo || editor_set_preference_root(&quit_state, root) != 0 ||
        editor_init_persistence_paths(&quit_state) != 0) goto fail;
    editor_set_document_save_point(&quit_state);
    quit_state.selection.type = ENT_COIN;
    quit_state.selection.index = 0;
    ui_begin_frame(&quit_state.ui);
    quit_state.ui.mouse_clicked = 1;
    quit_state.ui.mouse_x = 4;
    quit_state.ui.mouse_y = 4;
    (void)ui_float_field(&quit_state.ui, 301, 0, 0, 120,
                         &quit_state.level.coins[0].x);
    memset(&event, 0, sizeof(event));
    event.type = INPUT_TEXT;
    strncpy(event.text, "8", sizeof(event.text) - 1);
    editor_handle_event(&quit_state, &event);
    event.text[0] = '0';
    event.text[1] = '\0';
    editor_handle_event(&quit_state, &event);
    editor_test_set_finish_field_choice(1);
    editor_test_set_discard_choice(1);
    memset(&event, 0, sizeof(event));
    event.type = INPUT_QUIT;
    quit_state.running = 1;
    editor_handle_event(&quit_state, &event);
    if (expect_int("staged Quit confirmation", quit_state.running, 0) != 0 ||
        expect_float_value("staged Quit value", quit_state.level.coins[0].x,
                           80.0f) != 0 ||
        expect_int("staged Quit field closed", quit_state.ui.active_id, 0) != 0)
        goto fail;

    editor_level_init_defaults(&selection_state.level);
    selection_state.level.coin_count = 1;
    selection_state.level.coins[0].x = 0.0f;
    selection_state.undo = undo_create();
    ui_init(&selection_state.ui, context.font);
    if (!selection_state.undo) goto fail;
    editor_set_document_save_point(&selection_state);
    selection_state.selection.type = ENT_COIN;
    selection_state.selection.index = 0;
    ui_begin_frame(&selection_state.ui);
    selection_state.ui.mouse_clicked = 1;
    selection_state.ui.mouse_x = 4;
    selection_state.ui.mouse_y = 4;
    (void)ui_float_field(&selection_state.ui, 301, 0, 0, 120,
                         &selection_state.level.coins[0].x);
    memset(&event, 0, sizeof(event));
    event.type = INPUT_TEXT;
    strncpy(event.text, "9", sizeof(event.text) - 1);
    editor_handle_event(&selection_state, &event);
    memset(&event, 0, sizeof(event));
    event.type = INPUT_KEY_DOWN;
    event.key = KEY_TWO;
    editor_handle_event(&selection_state, &event);
    if (expect_float_value("typing leaves value staged",
                           selection_state.level.coins[0].x, 0.0f) != 0 ||
        expect_int("typing does not select a tool", selection_state.tool, TOOL_SELECT) != 0 ||
        expect_int("typing keeps active field", selection_state.ui.active_id, 301) != 0) goto fail;

    undo_destroy(save_state.undo);
    undo_destroy(quit_state.undo);
    undo_destroy(selection_state.undo);
    ui_cleanup(&save_state.ui);
    ui_cleanup(&quit_state.ui);
    ui_cleanup(&selection_state.ui);
    cleanup_test_preference_root(root,
                                 (EditorState[]){save_state, quit_state,
                                                 selection_state}, 3);
    editor_widget_test_context_cleanup(&context);
    remove(target);
    return 0;

fail:
    editor_test_set_finish_field_choice(-1);
    editor_test_set_discard_choice(-1);
    cleanup_test_preference_root(root,
                                 (EditorState[]){save_state, quit_state,
                                                 selection_state}, 3);
    undo_destroy(save_state.undo);
    undo_destroy(quit_state.undo);
    undo_destroy(selection_state.undo);
    ui_cleanup(&save_state.ui);
    ui_cleanup(&quit_state.ui);
    ui_cleanup(&selection_state.ui);
    editor_widget_test_context_cleanup(&context);
    remove(target);
    return 1;
}

/*
 * Undo groups: entries pushed between undo_group_begin/end share a number
 * and are undone as one step by the editor; amending folds nudges into a
 * step; and a full history drops a whole group, never half of one.
 */
static int undo_groups_stay_whole(void)
{
    UndoStack *stack = undo_create();
    Command cmd;
    PlacementData later;
    int failed = 1;
    int group;

    if (!stack) return 1;
    memset(&cmd, 0, sizeof(cmd));
    cmd.type = CMD_MOVE;
    cmd.entity_type = ENT_COIN;
    undo_push(stack, &cmd);                       /* a step of its own */
    group = undo_group_begin(stack);
    for (int i = 0; i < 3; i++) {
        cmd.entity_index = i;
        undo_push(stack, &cmd);
    }
    undo_group_end(stack);
    if (expect_int("group number", group > 0, 1) != 0 ||
        expect_int("top is the group", undo_top_group(stack), group) != 0) goto done;

    memset(&later, 0, sizeof(later));
    later.coin.x = 42.0f;
    if (expect_int("amend found", undo_amend_after(stack, group, ENT_COIN, 1, &later), 1) != 0 ||
        expect_int("amend other entity", undo_amend_after(stack, group, ENT_STAR_RED, 1, &later), 0) != 0 ||
        expect_float_value("amended after", stack->commands[stack->top - 2].after.coin.x, 42.0f) != 0)
        goto done;
    if (!undo_pop(stack, &cmd) || expect_int("popped group", cmd.group, group) != 0 ||
        expect_int("redo top group", redo_top_group(stack), group) != 0) goto done;
    undo_clear(stack);

    /* Fill the history so the oldest step is a group of three. */
    group = undo_group_begin(stack);
    for (int i = 0; i < 3; i++) undo_push(stack, &cmd);
    undo_group_end(stack);
    cmd.group = 0;
    for (int i = 0; i < UNDO_MAX - 3; i++) undo_push(stack, &cmd);
    if (expect_int("history full", stack->top, UNDO_MAX) != 0) goto done;
    undo_push(stack, &cmd);                       /* evicts the whole group */
    if (expect_int("group evicted whole", stack->top, UNDO_MAX - 2) != 0 ||
        expect_int("no group remnant", stack->commands[0].group, 0) != 0) goto done;
    failed = 0;
done:
    undo_destroy(stack);
    return failed;
}

static int orphan_recovery_does_not_poison_discovery(void)
{
    EditorState es = {0};
    char root[EDITOR_PATH_MAX], metadata[EDITOR_PATH_MAX];
    if (make_test_preference_root(root, sizeof(root))) return 1;
    if (editor_set_preference_root(&es, root) || editor_init_persistence_paths(&es)) return 1;
    fill_valid_minimal(&es.level);
    es.modified = 1;
    es.last_autosave_ms = (uint32_t)clock_millis() - 30001;
    editor_maybe_autosave(&es);
    if (es.recovery_entry_count != 1) return 1;
    strcpy(metadata, es.recovery_entries[0].metadata_path);
    remove(es.autosave_path);
    int result = expect_int("orphan snapshot skipped", editor_discover_recoveries(&es), 0) ||
                 expect_int("orphan not offered", es.recovery_entry_count, 0);
    remove(metadata);
    cleanup_test_preference_root(root, &es, 1);
    return result;
}

/*
 * Write one recovery pair by hand: editor_recovery_<id>.toml (any bytes;
 * discovery only checks it exists) and its .meta naming `owner_pid`
 * (0 writes the old version-1 line, which has no owner).
 */
static int write_recovery_pair(const char *root, unsigned long long id,
                               unsigned long owner_pid, int with_meta)
{
    char path[EDITOR_PATH_MAX];
    char line[160];

    snprintf(path, sizeof(path), "%s/editor_recovery_%016llx.toml", root, id);
    if (write_text_file(path, "name = \"left over\"\n") != 0) return -1;
    if (!with_meta) return 0;
    snprintf(path, sizeof(path), "%s/editor_recovery_%016llx.meta", root, id);
    if (owner_pid)
        snprintf(line, sizeof(line), "2\t%016llx\t1700000000\t%lu\t-\n", id, owner_pid);
    else
        snprintf(line, sizeof(line), "1\t%016llx\t1700000000\t-\n", id);
    return write_text_file(path, line);
}

static int recovery_pair_exists(const char *root, unsigned long long id,
                                const char *suffix)
{
    char path[EDITOR_PATH_MAX];
    snprintf(path, sizeof(path), "%s/editor_recovery_%016llx%s", root, id, suffix);
    return editor_file_exists(path);
}

static void remove_recovery_pair(const char *root, unsigned long long id)
{
    char path[EDITOR_PATH_MAX];
    snprintf(path, sizeof(path), "%s/editor_recovery_%016llx.toml", root, id);
    remove(path);
    snprintf(path, sizeof(path), "%s/editor_recovery_%016llx.meta", root, id);
    remove(path);
}

/*
 * The recovery folder used to fill up for good: with 32 leftover copies
 * autosave failed forever with a generic message, left a .toml without a
 * .meta that nothing cleaned, and a second editor offered the first one's
 * live snapshot as if it had crashed.
 */
static int recovery_folder_stays_manageable(void)
{
    EditorState es = {0};
    char root[EDITOR_PATH_MAX] = {0};
    const unsigned long long first_id = 0x1000;
    int result = 1;

    if (make_test_preference_root(root, sizeof(root)) != 0) return 1;

    /* A .toml whose .meta is gone is swept at start-up. */
    if (write_recovery_pair(root, 0xabc, 0, 0) != 0) goto cleanup;
    if (editor_set_preference_root(&es, root) != 0 ||
        editor_init_persistence_paths(&es) != 0 ||
        expect_int("orphan swept", recovery_pair_exists(root, 0xabc, ".toml"), 0) != 0)
        goto cleanup;

    /* Old copies fill every slot: autosave says so, and leaves no orphan. */
    for (int i = 0; i < EDITOR_MAX_RECOVERY_ENTRIES; i++)
        if (write_recovery_pair(root, first_id + (unsigned long long)i, 0, 1) != 0)
            goto cleanup;
    fill_valid_minimal(&es.level);
    es.modified = 1;
    es.last_autosave_ms = (uint32_t)clock_millis() - 30001u;
    editor_maybe_autosave(&es);
    if (expect_prefix("full folder explained", es.status_message,
                      "Autosave paused: 32 old recovery copies fill the folder") != 0 ||
        expect_int("no orphan from the full folder",
                   editor_file_exists(es.autosave_path), 0) != 0) goto cleanup;

    /* Discard from the picker deletes that copy's files, then Cancel. */
    if (editor_discover_recoveries(&es) != 0 ||
        expect_int("all copies offered", es.recovery_entry_count,
                   EDITOR_MAX_RECOVERY_ENTRIES) != 0) goto cleanup;
    {
        unsigned long long discarded = (unsigned long long)es.recovery_entries[0].id;
        editor_test_set_recovery_choice(EDITOR_RECOVERY_DISCARD);
        editor_test_queue_recovery_choice(EDITOR_RECOVERY_CANCEL);
        if (expect_int("picker after discard", editor_choose_recovery(&es), -1) != 0 ||
            expect_int("one copy fewer", es.recovery_entry_count,
                       EDITOR_MAX_RECOVERY_ENTRIES - 1) != 0 ||
            expect_int("discarded snapshot gone",
                       recovery_pair_exists(root, discarded, ".toml"), 0) != 0 ||
            expect_int("discarded metadata gone",
                       recovery_pair_exists(root, discarded, ".meta"), 0) != 0)
            goto cleanup;
    }
    /* With a free slot the next autosave works again.  (A fresh status
     * line is not overwritten by routine autosave news, so clear it.) */
    es.status_message[0] = '\0';
    es.last_autosave_ms = (uint32_t)clock_millis() - 30001u;
    editor_maybe_autosave(&es);
    if (expect_string("autosave resumes", es.status_message, "Autosaved recovery copy") != 0 ||
        expect_int("own snapshot written", editor_file_exists(es.autosave_path), 1) != 0)
        goto cleanup;
    for (int i = 0; i < EDITOR_MAX_RECOVERY_ENTRIES; i++)
        remove_recovery_pair(root, first_id + (unsigned long long)i);
    editor_retire_current_recovery(&es);

    /* Discarding the only copy ends the picker. */
    if (write_recovery_pair(root, 0x2000, 0, 1) != 0 ||
        editor_discover_recoveries(&es) != 0 ||
        expect_int("single copy offered", es.recovery_entry_count, 1) != 0) goto cleanup;
    editor_test_set_recovery_choice(EDITOR_RECOVERY_DISCARD);
    if (expect_int("discard only copy", editor_choose_recovery(&es), -1) != 0 ||
        expect_int("nothing left", es.recovery_entry_count, 0) != 0 ||
        expect_string("discard reported", es.status_message, "Recovery copy discarded") != 0)
        goto cleanup;

#ifndef _WIN32
    /* A copy owned by another running editor (our parent process stands in
     * for one) is live work and is not offered; one whose owner has exited
     * is a crash leftover and is. */
    {
        pid_t child = fork();
        if (child < 0) goto cleanup;
        if (child == 0) _exit(0);
        if (waitpid(child, NULL, 0) != child) goto cleanup;
        if (write_recovery_pair(root, 0x3000, (unsigned long)getppid(), 1) != 0 ||
            write_recovery_pair(root, 0x3001, (unsigned long)child, 1) != 0 ||
            editor_discover_recoveries(&es) != 0 ||
            expect_int("only the crashed copy offered", es.recovery_entry_count, 1) != 0 ||
            expect_int("crashed copy id", (int)(es.recovery_entries[0].id == 0x3001), 1) != 0)
            goto cleanup;
        /* The live copy is not an orphan either: its files stay. */
        if (expect_int("live copy kept", recovery_pair_exists(root, 0x3000, ".toml"), 1) != 0)
            goto cleanup;
    }
#endif
    result = 0;

cleanup:
    editor_test_set_recovery_choice(-1);
    for (int i = 0; i < EDITOR_MAX_RECOVERY_ENTRIES; i++)
        remove_recovery_pair(root, first_id + (unsigned long long)i);
    remove_recovery_pair(root, 0xabc);
    remove_recovery_pair(root, 0x2000);
    remove_recovery_pair(root, 0x3000);
    remove_recovery_pair(root, 0x3001);
    cleanup_test_preference_root(root, &es, 1);
    return result;
}

static int invalid_drafts_do_not_build_unsafe_previews(void)
{
    EditorWidgetTestContext context;
    EditorState es = {0};
    if (editor_widget_test_context_init(&context) != 0) return 1;
    ui_init(&es.ui, context.font);
    ui_label(&es.ui, 0, 0, "cached label");
    Texture2D *cached = es.ui.text_cache[0].texture;
    if (!cached) { editor_widget_test_context_cleanup(&context); return 1; }
    for (int i = 0; i < 100; i++) ui_label(&es.ui, 0, 0, "cached label");
    if (es.ui.text_cache[0].texture != cached || es.ui.text_cache[1].texture) {
        ui_cleanup(&es.ui);
        editor_widget_test_context_cleanup(&context);
        return 1;
    }
    if (getenv("MANGO_BENCHMARK")) {
        const int rounds=2000;
        double start=GetTime();
        for (int i=0;i<rounds;i++) ui_label(&es.ui,0,0,"cached label");
        rlDrawRenderBatchActive();
        double warm=GetTime()-start;
        start=GetTime();
        for (int i=0;i<rounds;i++) { ui_cleanup(&es.ui); ui_label(&es.ui,0,0,"cached label"); }
        rlDrawRenderBatchActive();
        double cold=GetTime()-start;
        double frequency=1;
        printf("text benchmark: %d labels cached=%.3fms uncached=%.3fms ratio=%.2fx\n",rounds,
               warm*1000.0/frequency,cold*1000.0/frequency,(double)cold/(double)(warm?warm:1));
    }
    char text[4] = "";
    es.ui.active_id = 1;
    es.ui.edit_type = UI_EDIT_TEXT;
    es.ui.edit_target = text;
    es.ui.edit_target_size = sizeof(text);
    ui_queue_text_input(&es.ui, "éé");
    if (ui_apply_active_edit(&es.ui) != 2 || strcmp(text, "é")) {
        ui_cleanup(&es.ui);
        editor_widget_test_context_cleanup(&context);
        return 1;
    }
    es.ui.mouse_clicked = 1;
    es.ui.mouse_x = es.ui.mouse_y = 4;
    (void)ui_text_field(&es.ui, 1, 0, 0, 120, text, sizeof(text));
    es.ui.mouse_clicked = 0;
    es.ui.key_backspace = 1;
    (void)ui_text_field(&es.ui, 1, 0, 0, 120, text, sizeof(text));
    (void)ui_apply_active_edit(&es.ui);
    es.ui.key_backspace = 0;
    if (text[0] != '\0') {
        ui_cleanup(&es.ui);
        editor_widget_test_context_cleanup(&context);
        return 1;
    }
    es.camera.zoom = 2.0f;
    editor_level_init_defaults(&es.level);
    es.level.rail_count = 1;
    es.level.rails[0] = (RailPlacement){RAIL_LAYOUT_RECT, 80, 40, 1000, 3, 1};
    canvas_render(&es);
    float x, y;
    editor_rail_placement_position_at(&es.level.rails[0], 1e30f, &x, &y);
    if (es.level.rails[0].w != 1000 || x != 0.0f || y != 0.0f) {
        editor_widget_test_context_cleanup(&context);
        return 1;
    }
    es.level.rail_count = 0;
    es.level.screen_count = 2147483647;
    canvas_render(&es);
    es.config_open = 1;
    level_config_render(&es, TOOLBAR_H, 600, 1000);
    ui_cleanup(&es.ui);
    ui_cleanup(&es.ui); /* teardown is idempotent */
    editor_widget_test_context_cleanup(&context);
    return 0;
}

static int compact_history_owns_config_snapshots(void)
{
    UndoStack *stack = undo_create();
    if (!stack) return 1;
    Command command = {0}, popped;
    command.type = CMD_CONFIG;
    int failed = 0;
    for (int i = 0; i < UNDO_MAX + 2; i++) {
        command.config_before.screen_count = i;
        command.config_after.screen_count = i + 1;
        if (!undo_push(stack, &command)) { failed = 1; break; }
    }
    if (!failed && (stack->top != UNDO_MAX || !undo_pop(stack, &popped) ||
        popped.config_after.screen_count != UNDO_MAX + 2 || !undo_pop(stack, &popped) ||
        !redo_pop(stack, &popped))) failed = 1;
    command.type = CMD_PLACE;
    if (!undo_push(stack, &command) || stack->redo_top != 0) failed = 1;
    undo_clear(stack);
    if (stack->top || stack->redo_top || sizeof(UndoStack) > 512 * 1024) failed = 1;
    printf("undo storage: %zu bytes plus config snapshots only when used\n", sizeof(UndoStack));
    undo_destroy(stack);
    return failed;
}

/* Long paths shown in the status bar, title and recent list keep their file
 * name: the tail is kept, prefixed with "...", and never splits a UTF-8
 * character. */
static int display_paths_keep_the_file_name(void)
{
    char out[12];

    editor_path_for_display("a/b.toml", out, sizeof(out));
    if (expect_string("short path unchanged", out, "a/b.toml") != 0) return 1;

    editor_path_for_display("levels/long_name.toml", out, sizeof(out));
    /* 12-byte buffer: "..." + last 8 bytes + NUL. */
    if (expect_string("long path keeps tail", out, "...ame.toml") != 0) return 1;

    /* "\xc3\xa9" is UTF-8 for "e acute". This 17-byte path keeps its last
     * 8 bytes starting at byte 9, the second half of the third character,
     * so the helper must step forward to the fourth character. */
    editor_path_for_display("dir/\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9.toml", out, sizeof(out));
    if (expect_string("display path keeps whole UTF-8 characters", out,
                      "...\xc3\xa9.toml") != 0) return 1;
    return 0;
}

/*
 * Text fields keep only valid UTF-8.  Pasted bytes (or a lone surrogate
 * from a text event) that are not valid UTF-8 would be saved into the level
 * and make it unloadable, so each bad byte is dropped and the valid
 * characters around it are kept.
 */
static int text_fields_drop_invalid_utf8(void)
{
    static const struct {
        const char *typed;
        const char *kept;
    } cases[] = {
        {"a\xc0\xaf" "b", "ab"},                         /* overlong '/'          */
        {"a\xed\xa0\x80" "b", "ab"},                     /* surrogate U+D800      */
        {"a\xf4\x90\x80\x80" "b", "ab"},                 /* above U+10FFFF        */
        {"a\xe2\x82" "b", "ab"},                         /* truncated euro sign   */
        {"a\x80" "b", "ab"},                             /* stray continuation    */
        {"\xc3\xa9\xe2\x82\xac\xf0\x9f\xa5\xad", "\xc3\xa9\xe2\x82\xac\xf0\x9f\xa5\xad"},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        UIState ui;
        char text[32] = "";

        memset(&ui, 0, sizeof(ui));
        ui.active_id = 1;
        ui.edit_type = UI_EDIT_TEXT;
        ui.edit_target = text;
        ui.edit_target_size = sizeof(text);
        ui_queue_text_input(&ui, cases[i].typed);
        (void)ui_apply_active_edit(&ui);
        if (expect_string("text field keeps valid UTF-8", text, cases[i].kept) != 0)
            return 1;
    }
    return 0;
}

/*
 * Symlinks are followed only for the document the user opened.  Save As
 * onto a planted link must not touch the file it names; Save of a level
 * opened through a link updates that file, but not after the link is
 * repointed somewhere the user never looked.
 */
static int symlinks_are_followed_only_for_the_opened_document(void)
{
#ifdef _WIN32
    return 0;
#else
    const char *victim = TEST_OUT "editor_symlink_victim.toml";
    const char *planted = TEST_OUT "editor_symlink_planted.toml";
    const char *document = TEST_OUT "editor_symlink_document.toml";
    const char *opened = TEST_OUT "editor_symlink_opened.toml";
    char alias_root[EDITOR_PATH_MAX];
    char private_alias[EDITOR_PATH_MAX];
    char planted_autosave[EDITOR_PATH_MAX] = {0};
    char root[EDITOR_PATH_MAX] = {0};
    EditorState es = {0};
    LevelDef fixture;
    LevelDef reloaded;
    struct stat link_stat;
    int result = 1;

    ensure_out_dir();
    remove(planted);
    remove(opened);
    snprintf(alias_root, sizeof(alias_root), TEST_OUT "editor_symlink_alias_%ld",
             (long)getpid());
    remove(alias_root);
    editor_level_init_defaults(&fixture);
    if (write_text_file(victim, "victim bytes\n") != 0 ||
        symlink("editor_symlink_victim.toml", planted) != 0 ||
        level_save_toml(&fixture, document) != 0 ||
        symlink("editor_symlink_document.toml", opened) != 0 ||
        make_test_preference_root(root, sizeof(root)) != 0 ||
        editor_set_preference_root(&es, root) != 0 ||
        editor_init_persistence_paths(&es) != 0) goto cleanup;
    es.undo = undo_create();
    if (!es.undo) goto cleanup;

    /* Save As onto a planted link: refused, victim and link untouched. */
    editor_level_init_defaults(&es.level);
    es.modified = 1;
    file_dialog_test_set_save_result(FILE_DIALOG_SELECTED, planted);
    editor_test_set_overwrite_choice(1);
    if (expect_int("Save As onto symlink refused",
                   editor_save_current_level_as(&es), -1) != 0 ||
        expect_prefix("Save As symlink status", es.status_message,
                      "Save failed:") != 0 ||
        expect_int("victim untouched", file_equals_text(victim, "victim bytes\n"), 1) != 0 ||
        expect_int("planted link kept", lstat(planted, &link_stat) == 0 &&
                   S_ISLNK(link_stat.st_mode), 1) != 0) goto cleanup;

    /* Save As to a private file spelled through a symlinked folder.
     * Loading below picks a new autosave path, so remember this one. */
    {
        const char *base = strrchr(es.autosave_path, '/');
        int written = snprintf(private_alias, sizeof(private_alias), "%s%s",
                               alias_root, base ? base : "/missing");
        if (written < 0 || (size_t)written >= sizeof(private_alias)) goto cleanup;
    }
    memcpy(planted_autosave, es.autosave_path, strlen(es.autosave_path) + 1);
    if (write_text_file(planted_autosave, "autosave bytes\n") != 0 ||
        symlink(root + strlen(TEST_OUT), alias_root) != 0) goto cleanup;
    file_dialog_test_set_save_result(FILE_DIALOG_SELECTED, private_alias);
    if (expect_int("Save As to aliased private file refused",
                   editor_save_current_level_as(&es), -1) != 0 ||
        expect_string("aliased private status", es.status_message,
                      "Save failed: private editor path") != 0 ||
        expect_int("autosave untouched",
                   file_equals_text(planted_autosave, "autosave bytes\n"), 1) != 0)
        goto cleanup;

    /* Save of a level opened through a link updates the link's target. */
    if (expect_int("open through symlink", editor_load_level(&es, opened), 0) != 0)
        goto cleanup;
    strncpy(es.level.name, "Through link", sizeof(es.level.name) - 1);
    es.modified = 1;
    if (expect_int("save through opened symlink",
                   editor_save_current_level(&es), 0) != 0 ||
        expect_int("opened link kept", lstat(opened, &link_stat) == 0 &&
                   S_ISLNK(link_stat.st_mode), 1) != 0 ||
        expect_int("target reload", level_load_toml(document, &reloaded), 0) != 0 ||
        expect_string("target updated", reloaded.name, "Through link") != 0)
        goto cleanup;

    /* The same link repointed at another file is no longer followed. */
    if (remove(opened) != 0 ||
        symlink("editor_symlink_victim.toml", opened) != 0) goto cleanup;
    es.level.coin_score++;
    es.modified = 1;
    editor_test_set_external_choice(EDITOR_EXTERNAL_REPLACE);
    if (expect_int("repointed link refused", editor_save_current_level(&es), -1) != 0 ||
        expect_prefix("repointed link status", es.status_message, "Save failed:") != 0 ||
        expect_int("victim still untouched",
                   file_equals_text(victim, "victim bytes\n"), 1) != 0) goto cleanup;
    result = 0;

cleanup:
    file_dialog_test_set_save_result(-1, NULL);
    editor_test_set_overwrite_choice(-1);
    editor_test_set_external_choice((EditorExternalChoice)-1);
    if (planted_autosave[0] != '\0') remove(planted_autosave);
    cleanup_test_preference_root(root, &es, 1);
    undo_destroy(es.undo);
    remove(alias_root);
    remove(planted);
    remove(opened);
    remove(victim);
    remove(document);
    return result;
#endif
}

/* ---- Level Config panel driven like a real frame ------------------- */

/*
 * Rows of the Level Config panel when it starts at TOOLBAR_H, is not
 * scrolled, the validation report is empty and there are no recent files
 * (the layout written out in level_config_render).  The layer-button test
 * first checks "+ Add" really is at CFG_FIRST_LAYER_Y, so a layout change
 * fails loudly there instead of making these tests click empty space.
 */
#define CFG_X              CANVAS_W
#define CFG_NAME_Y         (TOOLBAR_H + 64)
#define CFG_SCREENS_Y      (TOOLBAR_H + 136)
#define CFG_MUSIC_Y        (TOOLBAR_H + 190)
#define CFG_HEARTS_Y       (TOOLBAR_H + 272)
#define CFG_FIRST_LAYER_Y  (TOOLBAR_H + 368)

static int config_state_init(EditorState *es, TextFont *font, char *root,
                             size_t root_size)
{
    memset(es, 0, sizeof(*es));
    editor_level_init_defaults(&es->level);
    ui_init(&es->ui, font);
    es->ui.before_command = editor_before_command;
    es->ui.before_command_context = es;
    es->config_open = 1;
    es->selection.index = -1;
    es->tool = TOOL_SELECT;
    es->camera.zoom = 2.0f;
    es->undo = undo_create();
    if (!es->undo) return -1;
    if (root && (make_test_preference_root(root, root_size) != 0 ||
                 editor_set_preference_root(es, root) != 0)) return -1;
    editor_sync_config_resources(es);
    editor_set_document_save_point(es);
    return 0;
}

static void config_state_cleanup(EditorState *es, const char *root)
{
    texture_unload(es->textures.sky);
    texture_unload(es->textures.floor_tile);
    texture_unload(es->textures.water);
    es->textures.sky = es->textures.floor_tile = es->textures.water = NULL;
    undo_destroy(es->undo);
    es->undo = NULL;
    ui_cleanup(&es->ui);
    if (root && root[0]) cleanup_test_preference_root(root, es, 1);
    editor_test_set_finish_field_choice(-1);
}

/* One editor frame of the Level Config panel: optional typed text, then an
 * optional press at (mx, my) routed the way editor_handle_event routes it. */
static void config_frame(EditorState *es, const char *text, int click,
                         int mx, int my)
{
    ui_begin_frame(&es->ui);
    if (text) ui_queue_text_input(&es->ui, text);
    if (click) (void)ui_press(&es->ui);
    es->ui.mouse_x = mx;
    es->ui.mouse_y = my;
    /* total == visible, so any earlier scroll clamps back to 0. */
    level_config_render(es, TOOLBAR_H, EDITOR_H - TOOLBAR_H, EDITOR_H - TOOLBAR_H);
}

static int redo_last(EditorState *es)
{
    Command cmd;
    if (!redo_pop(es->undo, &cmd)) return 1;
    editor_apply_undo_command(es, &cmd, 0);
    return 0;
}

/*
 * Clicking a layer button while a field is mid-edit first finishes that
 * edit.  The button must still be its own undo step, mark the document
 * modified and refresh the preview, whether the edit is applied or
 * discarded.
 */
static int layer_buttons_record_their_own_undo_step(void)
{
    EditorWidgetTestContext context;
    EditorState es;
    char root[EDITOR_PATH_MAX] = {0};
    const int add_x = CFG_X + 20, remove_x = CFG_X + 120;
    int result = 1;

    if (editor_widget_test_context_init(&context) != 0) {
        editor_widget_test_context_cleanup(&context);
        return 1;
    }
    g_plx_open = 1;
    if (config_state_init(&es, context.font, root, sizeof(root)) != 0) goto done;

    /* Baseline: "+ Add" with no field active adds one undoable layer. */
    config_frame(&es, NULL, 1, add_x, CFG_FIRST_LAYER_Y + 5);
    if (expect_int("plain add layer", es.level.background_layer_count, 1) != 0 ||
        expect_int("plain add undo", es.undo->top, 1) != 0 ||
        expect_int("plain add modified", es.modified, 1) != 0 ||
        undo_last(&es) != 0 ||
        expect_int("plain add undone", es.level.background_layer_count, 0) != 0 ||
        expect_int("plain add clean", es.modified, 0) != 0) goto done;

    /* Name mid-edit, then "+ Add", choosing Apply: two separate steps. */
    config_frame(&es, NULL, 1, CFG_X + 60, CFG_NAME_Y + 4);
    config_frame(&es, "2", 0, CFG_X + 60, CFG_NAME_Y + 4);
    if (expect_int("name field active", es.ui.active_id, 9000) != 0) goto done;
    editor_test_set_finish_field_choice(1);
    config_frame(&es, NULL, 1, add_x, CFG_FIRST_LAYER_Y + 5);
    if (expect_string("applied name", es.level.name, "Untitled2") != 0 ||
        expect_int("applied add layer", es.level.background_layer_count, 1) != 0 ||
        expect_int("name and layer are two steps", es.undo->top, 2) != 0 ||
        expect_int("applied add modified", es.modified, 1) != 0 ||
        expect_int("sky preview refreshed", es.textures.sky != NULL, 1) != 0)
        goto done;
    if (undo_last(&es) != 0 ||
        expect_int("undo removes only the layer", es.level.background_layer_count, 0) != 0 ||
        expect_string("undo keeps the name", es.level.name, "Untitled2") != 0 ||
        undo_last(&es) != 0 ||
        expect_string("second undo restores the name", es.level.name, "Untitled") != 0 ||
        redo_last(&es) != 0 || redo_last(&es) != 0 ||
        expect_int("redo restores the layer", es.level.background_layer_count, 1) != 0 ||
        expect_string("redo restores the name", es.level.name, "Untitled2") != 0)
        goto done;

    /* Name mid-edit, then "- Remove Last" with no prompt answer at all:
     * a valid value is applied silently (no dialog), as its own step. */
    config_frame(&es, NULL, 1, CFG_X + 60, CFG_NAME_Y + 4);
    config_frame(&es, "X", 0, CFG_X + 60, CFG_NAME_Y + 4);
    config_frame(&es, NULL, 1, remove_x, CFG_FIRST_LAYER_Y + 20 + 5);
    if (expect_string("silently applied name", es.level.name, "Untitled2X") != 0 ||
        expect_int("silent remove layer", es.level.background_layer_count, 0) != 0 ||
        expect_int("silent name and remove are two steps", es.undo->top, 4) != 0 ||
        expect_int("silent remove modified", es.modified, 1) != 0 ||
        expect_int("sky preview cleared", es.textures.sky == NULL, 1) != 0 ||
        undo_last(&es) != 0 ||
        expect_int("undo restores removed layer", es.level.background_layer_count, 1) != 0 ||
        expect_string("undo keeps the applied name", es.level.name, "Untitled2X") != 0)
        goto done;
    result = 0;

done:
    g_plx_open = 0;
    config_state_cleanup(&es, root);
    editor_widget_test_context_cleanup(&context);
    return result;
}

/*
 * Range limits apply on every commit path.  Typing 0 and pressing Return
 * always clamped; typing 0 and clicking the canvas (Apply) used to store 0,
 * which the game reads as "4 screens" / "3 hearts".
 */
/*
 * Leaving a field used to ask Apply / Discard / Block every time, even for
 * a perfectly good value.  A valid value is now applied without a prompt;
 * only a value that cannot be stored asks, and its answer decides between
 * keeping the edit open (the command waits) and dropping the typed text.
 */
static int leaving_a_field_applies_valid_values_silently(void)
{
    EditorWidgetTestContext context;
    EditorState es;
    InputEvent canvas_click;
    char root[EDITOR_PATH_MAX] = {0};
    int result = 1;

    if (editor_widget_test_context_init(&context) != 0) {
        editor_widget_test_context_cleanup(&context);
        return 1;
    }
    if (config_state_init(&es, context.font, root, sizeof(root)) != 0) goto done;
    memset(&canvas_click, 0, sizeof(canvas_click));
    canvas_click.type = INPUT_MOUSE_DOWN;
    canvas_click.button = MOUSE_BUTTON_LEFT;
    canvas_click.x = 100;
    canvas_click.y = TOOLBAR_H + 100;

    /* Valid: no canned answer is armed, so a prompt would fail the test
     * (dialog_choice has no display here and reports an error). */
    config_frame(&es, NULL, 1, CFG_X + 90, CFG_SCREENS_Y + 4);
    if (expect_int("screens active", es.ui.active_id, 9011) != 0) goto done;
    strcpy(es.ui.edit_buf, "6");
    es.ui.edit_cursor = 1;
    editor_handle_event(&es, &canvas_click);
    if (expect_int("valid value applied", es.level.screen_count, 6) != 0 ||
        expect_int("valid value field closed", es.ui.active_id, 0) != 0 ||
        expect_int("valid value one undo step", es.undo->top, 1) != 0) goto done;

    /* Clicking straight into another field applies the first one too. */
    config_frame(&es, NULL, 1, CFG_X + 90, CFG_SCREENS_Y + 4);
    strcpy(es.ui.edit_buf, "7");
    es.ui.edit_cursor = 1;
    config_frame(&es, NULL, 1, CFG_X + 80, CFG_HEARTS_Y + 4);
    if (expect_int("field to field applies", es.level.screen_count, 7) != 0 ||
        expect_int("second field active", es.ui.active_id, 9006) != 0) goto done;
    ui_cancel_active_edit(&es.ui);

    /* Invalid: "Keep Editing" keeps the field open and the command waits. */
    config_frame(&es, NULL, 1, CFG_X + 90, CFG_SCREENS_Y + 4);
    strcpy(es.ui.edit_buf, "-");
    es.ui.edit_cursor = 1;
    editor_test_set_finish_field_choice(0);
    if (expect_int("invalid blocks", editor_finish_field_edit(&es), 0) != 0 ||
        expect_int("invalid stays active", es.ui.active_id, 9011) != 0 ||
        expect_prefix("invalid explained", es.status_message,
                      "Command blocked: invalid field value") != 0) goto done;
    /* ...and "Discard" drops the typed text and lets the command run. */
    editor_test_set_finish_field_choice(2);
    if (expect_int("discard proceeds", editor_finish_field_edit(&es), 1) != 0 ||
        expect_int("discard closes", es.ui.active_id, 0) != 0 ||
        expect_int("discard keeps value", es.level.screen_count, 7) != 0) goto done;
    result = 0;

done:
    config_state_cleanup(&es, root);
    editor_widget_test_context_cleanup(&context);
    return result;
}

static int field_limits_apply_on_every_commit_path(void)
{
    EditorWidgetTestContext context;
    EditorState es;
    InputEvent event;
    char root[EDITOR_PATH_MAX] = {0};
    int result = 1;

    if (editor_widget_test_context_init(&context) != 0) {
        editor_widget_test_context_cleanup(&context);
        return 1;
    }
    if (config_state_init(&es, context.font, root, sizeof(root)) != 0) goto done;

    memset(&event, 0, sizeof(event));
    event.type = INPUT_MOUSE_DOWN;
    event.button = MOUSE_BUTTON_LEFT;
    event.x = 100;
    event.y = TOOLBAR_H + 100;

    /* Screens: type 0, then click the canvas and choose Apply. */
    config_frame(&es, NULL, 1, CFG_X + 90, CFG_SCREENS_Y + 4);
    if (expect_int("screens active", es.ui.active_id, 9011) != 0) goto done;
    strcpy(es.ui.edit_buf, "0");
    es.ui.edit_cursor = 1;
    editor_test_set_finish_field_choice(1);
    editor_handle_event(&es, &event);
    if (expect_int("applied screens clamp", es.level.screen_count, 1) != 0 ||
        expect_int("applied screens undo", es.undo->top, 1) != 0) goto done;

    /* Hearts: the same through the finish dialog... */
    es.level.initial_hearts = 3;
    config_frame(&es, NULL, 1, CFG_X + 80, CFG_HEARTS_Y + 4);
    if (expect_int("hearts active", es.ui.active_id, 9006) != 0) goto done;
    strcpy(es.ui.edit_buf, "0");
    es.ui.edit_cursor = 1;
    editor_test_set_finish_field_choice(1);
    editor_handle_event(&es, &event);
    if (expect_int("applied hearts clamp", es.level.initial_hearts, 1) != 0) goto done;

    /* Music volume accepts the whole 0..128 range the format allows (it
     * used to stop at 99). */
    config_frame(&es, NULL, 1, CFG_X + 60, CFG_MUSIC_Y + 22 + 4);
    if (expect_int("volume active", es.ui.active_id, 9003) != 0) goto done;
    strcpy(es.ui.edit_buf, "128");
    es.ui.edit_cursor = 3;
    if (expect_int("volume applied", editor_finish_field_edit(&es), 1) != 0 ||
        expect_int("volume reaches 128", es.level.music_volume, 128) != 0) goto done;
    config_frame(&es, NULL, 1, CFG_X + 60, CFG_MUSIC_Y + 22 + 4);
    strcpy(es.ui.edit_buf, "500");
    es.ui.edit_cursor = 3;
    if (expect_int("loud volume applied", editor_finish_field_edit(&es), 1) != 0 ||
        expect_int("volume clamps to 128", es.level.music_volume, 128) != 0) goto done;

    /* ...and with Return, which already clamped. */
    config_frame(&es, NULL, 1, CFG_X + 80, CFG_HEARTS_Y + 4);
    strcpy(es.ui.edit_buf, "9");
    es.ui.edit_cursor = 1;
    ui_begin_frame(&es.ui);
    es.ui.key_return = 1;
    level_config_render(&es, TOOLBAR_H, EDITOR_H - TOOLBAR_H, EDITOR_H - TOOLBAR_H);
    if (expect_int("returned hearts clamp", es.level.initial_hearts, 3) != 0) goto done;

    /* A stored value already outside the limits (a hand-edited 0 meaning
     * "default") survives a no-op confirmation. */
    es.level.screen_count = 0;
    config_frame(&es, NULL, 1, CFG_X + 90, CFG_SCREENS_Y + 4);
    editor_test_set_finish_field_choice(1);
    if (expect_int("no-op finish", editor_finish_field_edit(&es), 1) != 0 ||
        expect_int("no-op keeps stored 0", es.level.screen_count, 0) != 0) goto done;

    /* ui_int_field_limited on its own: clamp, and round to the step. */
    {
        static const struct { const char *typed; int expected; } cases[] = {
            {"37", 32}, {"40", 48}, {"-5", 0}, {"99999", 1568}, {"1570", 1568},
        };
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            int gap = 16;
            ui_begin_frame(&es.ui);
            es.ui.mouse_clicked = 1;
            es.ui.mouse_x = 4;
            es.ui.mouse_y = 4;
            (void)ui_int_field_limited(&es.ui, 77, 0, 0, 120, &gap, 0, 1568, 16);
            strcpy(es.ui.edit_buf, cases[i].typed);
            es.ui.edit_cursor = (int)strlen(cases[i].typed);
            ui_begin_frame(&es.ui);
            es.ui.key_return = 1;
            (void)ui_int_field_limited(&es.ui, 77, 0, 0, 120, &gap, 0, 1568, 16);
            if (expect_int(cases[i].typed, gap, cases[i].expected) != 0) goto done;
        }
    }
    result = 0;

done:
    config_state_cleanup(&es, root);
    editor_widget_test_context_cleanup(&context);
    return result;
}

/* One editor frame of the entity properties panel placed at PROPS_Y. */
#define PROPS_Y        (TOOLBAR_H + 100)
#define PROPS_ROW(n)   (PROPS_Y + 36 + (n) * 24 + 4)  /* n-th field row */
#define PROPS_FIELD_X  (CFG_X + 8 + 80 + 10)
static void props_frame(EditorState *es, int click, int row_y)
{
    ui_begin_frame(&es->ui);
    if (click) (void)ui_press(&es->ui);
    es->ui.mouse_x = PROPS_FIELD_X;
    es->ui.mouse_y = row_y;
    properties_render(es, PROPS_Y, 300);
}

/* Type text into the property field on row_y and press Return. */
static void props_type(EditorState *es, int row_y, const char *text)
{
    props_frame(es, 1, row_y);
    strcpy(es->ui.edit_buf, text);
    es->ui.edit_cursor = (int)strlen(text);
    ui_begin_frame(&es->ui);
    es->ui.key_return = 1;
    properties_render(es, PROPS_Y, 300);
}

/*
 * Property fields keep motion values inside the validator's limits, and
 * switching a float platform to Rail gives it a usable speed.
 */
static int motion_fields_stay_within_validator_limits(void)
{
    EditorWidgetTestContext context;
    EditorState es;
    int result = 1;

    if (editor_widget_test_context_init(&context) != 0) {
        editor_widget_test_context_cleanup(&context);
        return 1;
    }
    if (config_state_init(&es, context.font, NULL, 0) != 0) goto done;
    es.panel_open = 1;
    es.level.rail_count = 1;
    es.level.rails[0] = (RailPlacement){RAIL_LAYOUT_RECT, 32, 32, 4, 4, 0};
    es.level.float_platform_count = 1;
    es.level.float_platforms[0] = (FloatPlatformPlacement){
        FLOAT_PLATFORM_STATIC, 100.0f, 100.0f, 3, 0, 0.0f, 0.0f};
    es.level.bouncepad_small_count = 1;
    es.level.bouncepads_small[0] = (BouncepadPlacement){200.0f, -380.0f, BOUNCEPAD_GREEN};
    es.level.spider_count = 1;
    es.level.spiders[0] = (SpiderPlacement){300.0f, 50.0f, 250.0f, 350.0f, 0};

    /* Float platform: mode Static -> Rail (third option). */
    es.selection.type = ENT_FLOAT_PLATFORM;
    es.selection.index = 0;
    props_frame(&es, 1, PROPS_ROW(0));
    props_frame(&es, 1, PROPS_ROW(0) + 20 * 3);
    if (expect_int("platform now rides a rail", es.level.float_platforms[0].mode,
                   FLOAT_PLATFORM_RAIL) != 0 ||
        expect_float_value("rail platform speed", es.level.float_platforms[0].speed,
                           3.0f) != 0) goto done;
    props_type(&es, PROPS_ROW(6), "999");
    if (expect_float_value("rail speed capped", es.level.float_platforms[0].speed,
                           30.0f) != 0) goto done;

    /* Bouncepad: a launch weaker than a jump is raised to the jump. */
    es.selection.type = ENT_BOUNCEPAD_SMALL;
    props_type(&es, PROPS_ROW(2), "-100");
    if (expect_float_value("launch at least a jump",
                           es.level.bouncepads_small[0].launch_vy, -325.0f) != 0)
        goto done;

    /* Spider: the patrol range cannot get narrower than the sprite. */
    es.selection.type = ENT_SPIDER;
    props_type(&es, PROPS_ROW(3), "260");
    if (expect_float_value("patrol_x1 keeps a sprite width",
                           es.level.spiders[0].patrol_x1, 314.0f) != 0) goto done;

    /* Spider vx: a speed past MAX_PATROL_SPEED is limited to it, and the
     * status bar says so... */
    props_type(&es, PROPS_ROW(1), "-99999");
    if (expect_float_value("vx limited", es.level.spiders[0].vx,
                           -(float)MAX_PATROL_SPEED) != 0 ||
        expect_int("limit reported", strstr(es.status_message, "limited") != NULL, 1) != 0)
        goto done;
    /* ...and 0, which would freeze the spider, is refused: nothing is
     * stored and the field stays open with the reason in the status bar. */
    props_type(&es, PROPS_ROW(1), "0");
    if (expect_float_value("vx kept", es.level.spiders[0].vx,
                           -(float)MAX_PATROL_SPEED) != 0 ||
        expect_int("field still open", es.ui.active_id != 0, 1) != 0 ||
        expect_int("zero reported", strstr(es.status_message, "0 is not allowed") != NULL, 1) != 0)
        goto done;
    ui_cancel_active_edit(&es.ui);
    if (level_is_valid("vx edits keep the level valid", &es.level) != 0) goto done;

    /* Every newly placed enemy starts with a valid patrol speed. */
    {
        static const EntityType enemies[] = {
            ENT_SPIDER, ENT_JUMPING_SPIDER, ENT_BIRD, ENT_FASTER_BIRD,
            ENT_FISH, ENT_FASTER_FISH,
        };
        es.tool = TOOL_PLACE;
        for (size_t i = 0; i < sizeof(enemies) / sizeof(enemies[0]); i++) {
            es.palette_type = enemies[i];
            tools_mouse_down(&es, 700.0f + 60.0f * (float)i, 150.0f);
            tools_mouse_up(&es, 700.0f + 60.0f * (float)i, 150.0f);
        }
        if (expect_int("enemies placed", es.level.spider_count + es.level.jumping_spider_count +
                       es.level.bird_count + es.level.faster_bird_count +
                       es.level.fish_count + es.level.faster_fish_count, 7) != 0 ||
            level_is_valid("placed enemies", &es.level) != 0)
            goto done;
    }
    result = 0;

done:
    config_state_cleanup(&es, NULL);
    editor_widget_test_context_cleanup(&context);
    return result;
}

/*
 * An open dropdown list owns the next press: it must not also place on the
 * canvas or reach a widget drawn under the list, before or after it.  A
 * list whose dropdown is no longer drawn closes by itself.
 */
static int open_dropdown_owns_the_next_click(void)
{
    static const char *zoom[] = {"Zoom: 1x", "Zoom: 2x", "Zoom: 3x", "Zoom: 5x"};
    EditorWidgetTestContext context;
    EditorState es;
    InputEvent down, up;
    int sel = 1;
    int result = 1;

    if (editor_widget_test_context_init(&context) != 0) {
        editor_widget_test_context_cleanup(&context);
        return 1;
    }
    if (config_state_init(&es, context.font, NULL, 0) != 0) goto done;
    es.tool = TOOL_PLACE;
    es.palette_type = ENT_COIN;

    memset(&down, 0, sizeof(down));
    down.type = INPUT_MOUSE_DOWN;
    down.button = MOUSE_BUTTON_LEFT;
    down.x = 380;            /* inside the toolbar zoom list's third row */
    down.y = 70;
    up = down;
    up.type = INPUT_MOUSE_UP;

    /* Baseline: with no list open, this press places a coin. */
    ui_begin_frame(&es.ui);
    editor_handle_event(&es, &down);
    editor_handle_event(&es, &up);
    if (expect_int("baseline place", es.level.coin_count, 1) != 0) goto done;

    /* Open the zoom dropdown where the toolbar draws it. */
    ui_begin_frame(&es.ui);
    es.ui.mouse_clicked = 1;
    es.ui.mouse_x = 360;
    es.ui.mouse_y = 10;
    (void)ui_dropdown(&es.ui, 8888, 344, 6, 80, zoom, 4, &sel);
    if (expect_int("zoom list open", es.ui.dropdown_open_id, 8888) != 0) goto done;

    /* Choose "Zoom: 3x" over the canvas. */
    ui_begin_frame(&es.ui);
    editor_handle_event(&es, &down);
    editor_handle_event(&es, &up);
    es.ui.mouse_x = down.x;
    es.ui.mouse_y = down.y;
    if (expect_int("button before list", ui_button(&es.ui, 344, 60, 80, 20, "a"), 0) != 0 ||
        expect_int("zoom picked",
                   ui_dropdown(&es.ui, 8888, 344, 6, 80, zoom, 4, &sel), 1) != 0 ||
        expect_int("zoom option", sel, 2) != 0 ||
        expect_int("button after list", ui_button(&es.ui, 344, 60, 80, 20, "b"), 0) != 0 ||
        expect_int("no coin under the list", es.level.coin_count, 1) != 0 ||
        expect_int("zoom list closed", es.ui.dropdown_open_id, 0) != 0) goto done;

    /* A press outside an open list only closes it. */
    ui_begin_frame(&es.ui);
    es.ui.mouse_clicked = 1;
    es.ui.mouse_x = 360;
    es.ui.mouse_y = 10;
    (void)ui_dropdown(&es.ui, 8888, 344, 6, 80, zoom, 4, &sel);
    ui_begin_frame(&es.ui);
    down.x = 600;
    up.x = 600;
    editor_handle_event(&es, &down);
    editor_handle_event(&es, &up);
    es.ui.mouse_x = down.x;
    es.ui.mouse_y = down.y;
    if (expect_int("outside press is not a pick",
                   ui_dropdown(&es.ui, 8888, 344, 6, 80, zoom, 4, &sel), 0) != 0 ||
        expect_int("outside press closes", es.ui.dropdown_open_id, 0) != 0 ||
        expect_int("outside press places nothing", es.level.coin_count, 1) != 0)
        goto done;

    /* A list whose dropdown stops being drawn closes on the next frame. */
    ui_begin_frame(&es.ui);
    es.ui.mouse_clicked = 1;
    es.ui.mouse_x = 360;
    es.ui.mouse_y = 10;
    (void)ui_dropdown(&es.ui, 8888, 344, 6, 80, zoom, 4, &sel);
    ui_begin_frame(&es.ui);          /* frame without the dropdown */
    ui_begin_frame(&es.ui);
    editor_handle_event(&es, &down);
    editor_handle_event(&es, &up);
    if (expect_int("stale list closed", es.ui.dropdown_open_id, 0) != 0 ||
        expect_int("canvas works again", es.level.coin_count, 2) != 0) goto done;
    result = 0;

done:
    config_state_cleanup(&es, NULL);
    editor_widget_test_context_cleanup(&context);
    return result;
}

/*
 * A dropdown whose stored value is none of its options (a hand-edited
 * path) must still let the designer pick option 0, "(none)" here.
 */
static int dropdowns_accept_any_option_for_unknown_values(void)
{
    EditorWidgetTestContext context;
    EditorState es;
    int result = 1;

    if (editor_widget_test_context_init(&context) != 0) {
        editor_widget_test_context_cleanup(&context);
        return 1;
    }
    if (config_state_init(&es, context.font, NULL, 0) != 0) goto done;
    strcpy(es.level.music_path, "assets/sounds/levels/custom.wav");
    editor_set_document_save_point(&es);

    config_frame(&es, NULL, 1, CFG_X + 200, CFG_MUSIC_Y + 4);
    if (expect_int("music list open", es.ui.dropdown_open_id, 9009) != 0) goto done;
    config_frame(&es, NULL, 1, CFG_X + 200, CFG_MUSIC_Y + 20 + 4);
    if (expect_string("custom music set to none", es.level.music_path, "") != 0 ||
        expect_int("none choice undoable", es.undo->top, 1) != 0 ||
        expect_int("none choice modified", es.modified, 1) != 0) goto done;
    result = 0;

done:
    config_state_cleanup(&es, NULL);
    editor_widget_test_context_cleanup(&context);
    return result;
}

/* New must not keep showing the previous level's sky/floor/water. */
static int new_level_resets_previews(void)
{
    EditorWidgetTestContext context;
    EditorState es;
    char root[EDITOR_PATH_MAX] = {0};
    int result = 1;

    if (editor_widget_test_context_init(&context) != 0) {
        editor_widget_test_context_cleanup(&context);
        return 1;
    }
    if (config_state_init(&es, context.font, root, sizeof(root)) != 0) goto done;
    if (expect_int("defaults have no sky", es.textures.sky == NULL, 1) != 0 ||
        expect_int("defaults have no water", es.textures.water == NULL, 1) != 0 ||
        expect_int("defaults have a floor", es.textures.floor_tile != NULL, 1) != 0)
        goto done;

    es.level.background_layer_count = 1;
    strcpy(es.level.background_layers[0].path,
           "assets/sprites/backgrounds/castle_pillars.png");
    es.level.foreground_layer_count = 1;
    strcpy(es.level.foreground_layers[0].path,
           "assets/sprites/foregrounds/water.png");
    strcpy(es.level.floor_tile_path, "assets/sprites/levels/stone_tileset.png");
    editor_sync_config_resources(&es);
    if (expect_int("level sky", es.textures.sky != NULL, 1) != 0 ||
        expect_int("level water", es.textures.water != NULL, 1) != 0) goto done;

    editor_reset_new_level(&es);
    if (expect_int("new level drops sky", es.textures.sky == NULL, 1) != 0 ||
        expect_int("new level drops water", es.textures.water == NULL, 1) != 0 ||
        expect_string("new level floor preview", es.preview_floor_path,
                      "assets/sprites/levels/grass_tileset.png") != 0) goto done;
    result = 0;

done:
    config_state_cleanup(&es, root);
    editor_widget_test_context_cleanup(&context);
    return result;
}

/*
 * A copied rail rider pastes onto the rail it rode: still after that rail
 * moved, never onto an identical rail beside it, and onto a same-shaped
 * rail only in another document.
 */
/*
 * Each Ctrl+V used to offset the same clipboard snapshot, so the second,
 * third... copies all landed on the first copy's spot, and a second floor
 * gap was even refused as a duplicate.  Now every paste steps once more.
 */
static int repeated_paste_steps_each_copy(void)
{
    EditorState es = {0};

    editor_level_init_defaults(&es.level);
    es.undo = undo_create();
    if (!es.undo) return 1;
    es.level.coin_count = 1;
    es.level.coins[0] = (CoinPlacement){100.0f, 100.0f};
    es.level.floor_gap_count = 1;
    es.level.floor_gaps[0] = 320;

    es.selection.type = ENT_COIN;
    es.selection.index = 0;
    editor_copy_selected(&es);
    for (int i = 0; i < 3; i++) editor_paste_clipboard(&es);
    if (expect_int("three coin copies", es.level.coin_count, 4) != 0 ||
        expect_float_value("first copy x", es.level.coins[1].x, 124.0f) != 0 ||
        expect_float_value("second copy x", es.level.coins[2].x, 148.0f) != 0 ||
        expect_float_value("third copy x", es.level.coins[3].x, 172.0f) != 0 ||
        expect_float_value("third copy y", es.level.coins[3].y, 172.0f) != 0)
        goto fail;

    es.selection.type = ENT_FLOOR_GAP;
    es.selection.index = 0;
    editor_copy_selected(&es);
    editor_paste_clipboard(&es);
    editor_paste_clipboard(&es);
    if (expect_int("two gap copies", es.level.floor_gap_count, 3) != 0 ||
        expect_int("first gap copy", es.level.floor_gaps[1], 320 + FLOOR_GAP_W) != 0 ||
        expect_int("second gap copy", es.level.floor_gaps[2], 320 + 2 * FLOOR_GAP_W) != 0 ||
        level_is_valid("after repeated paste", &es.level) != 0) goto fail;

    /* A fresh copy starts stepping from the copied entity again. */
    es.selection.type = ENT_COIN;
    es.selection.index = 0;
    editor_copy_selected(&es);
    editor_paste_clipboard(&es);
    if (expect_float_value("fresh copy restarts", es.level.coins[4].x, 124.0f) != 0)
        goto fail;
    undo_destroy(es.undo);
    return 0;
fail:
    undo_destroy(es.undo);
    return 1;
}

static int rail_rider_paste_follows_its_rail(void)
{
    EditorState es = {0};
    char root[EDITOR_PATH_MAX] = {0};
    int result = 1;

    editor_level_init_defaults(&es.level);
    es.level.rail_count = 3;
    es.level.rails[0] = (RailPlacement){RAIL_LAYOUT_RECT, 32, 32, 4, 4, 0};
    es.level.rails[1] = es.level.rails[0];        /* identical twin */
    es.level.rails[2] = (RailPlacement){RAIL_LAYOUT_RECT, 400, 32, 4, 4, 0};
    es.level.spike_block_count = 1;
    es.level.spike_blocks[0] = (SpikeBlockPlacement){1, 1.0f, 3.0f};
    es.undo = undo_create();
    if (!es.undo || make_test_preference_root(root, sizeof(root)) != 0 ||
        editor_set_preference_root(&es, root) != 0) goto done;
    editor_set_document_save_point(&es);

    es.selection.type = ENT_SPIKE_BLOCK;
    es.selection.index = 0;
    editor_copy_selected(&es);
    editor_paste_clipboard(&es);
    if (expect_int("twin rail paste", es.level.spike_block_count, 2) != 0 ||
        expect_int("paste rides the copied twin", es.level.spike_blocks[1].rail_index, 1) != 0)
        goto done;

    /* Move the rail (as its x field or a drag would), then paste again. */
    es.level.rails[1].x += 16;
    editor_paste_clipboard(&es);
    if (expect_int("moved rail paste", es.level.spike_block_count, 3) != 0 ||
        expect_int("paste follows the moved rail", es.level.spike_blocks[2].rail_index, 1) != 0)
        goto done;

    /* Deleting an earlier, unused rail renumbers the copied one. */
    es.selection.type = ENT_RAIL;
    es.selection.index = 0;
    tools_delete_selected(&es);
    if (expect_int("unused rail deleted", es.level.rail_count, 2) != 0) goto done;
    editor_paste_clipboard(&es);
    if (expect_int("renumbered rail paste", es.level.spike_block_count, 4) != 0 ||
        expect_int("paste follows renumbered rail", es.level.spike_blocks[3].rail_index, 0) != 0)
        goto done;

    /* In a new document the rail is matched by its copied shape. */
    editor_reset_new_level(&es);
    es.level.rail_count = 2;
    es.level.rails[0] = (RailPlacement){RAIL_LAYOUT_RECT, 400, 32, 4, 4, 0};
    es.level.rails[1] = (RailPlacement){RAIL_LAYOUT_RECT, 32, 32, 4, 4, 0};
    editor_paste_clipboard(&es);
    if (expect_int("other document paste", es.level.spike_block_count, 1) != 0 ||
        expect_int("other document matches shape", es.level.spike_blocks[0].rail_index, 1) != 0)
        goto done;
    result = 0;

done:
    cleanup_test_preference_root(root, &es, 1);
    undo_destroy(es.undo);
    return result;
}

/*
 * The editor only saves levels that validate, and must be able to open
 * what it saved.  A rail-mode float platform's x/y are not range-checked
 * (the rail places it), so they can hold FLT_MAX, which "%.9g" used to
 * write as 3.40282347e+38: past FLT_MAX, so the loader refused the file.
 */
static int extreme_floats_round_trip_through_save(void)
{
    const char *path = TEST_OUT "editor_extreme_float_roundtrip.toml";
    LevelDef def, loaded;
    char error[128];
    static const float samples[] = {
        FLT_MAX, -FLT_MAX, 3.4028233e38f, FLT_MIN, 1e-45f, 0.08f, 536.2f, -380.0f,
    };

    for (size_t i = 0; i < sizeof(samples) / sizeof(samples[0]); i++) {
        const char *text = fmt_float(samples[i]);
        double back = strtod(text, NULL);
        if (fabs(back) > FLT_MAX || (float)back != samples[i]) {
            fprintf(stderr, "editor_validation_test: fmt_float(%.9g) wrote %s\n",
                    (double)samples[i], text);
            return 1;
        }
    }

    ensure_out_dir();
    editor_level_init_defaults(&def);
    def.rail_count = 1;
    def.rails[0] = (RailPlacement){RAIL_LAYOUT_RECT, 32, 32, 4, 4, 0};
    def.float_platform_count = 1;
    def.float_platforms[0] = (FloatPlatformPlacement){
        FLOAT_PLATFORM_RAIL, FLT_MAX, -FLT_MAX, 3, 0, 2.0f, 1.0f};
    if (level_validate_runtime(&def, error, sizeof(error)) != 0) {
        fprintf(stderr, "editor_validation_test: extreme fixture invalid: %s\n", error);
        return 1;
    }
    remove(path);
    if (expect_int("extreme save", level_save_toml(&def, path), 0) != 0 ||
        expect_int("extreme load", level_load_toml(path, &loaded), 0) != 0 ||
        expect_int("FLT_MAX x kept", loaded.float_platforms[0].x == FLT_MAX, 1) != 0 ||
        expect_int("-FLT_MAX y kept", loaded.float_platforms[0].y == -FLT_MAX, 1) != 0) {
        remove(path);
        return 1;
    }
    remove(path);
    return 0;
}

/*
 * Save As to a new file uses link() for its no-clobber install.  On a
 * filesystem without hard links it must still create the file, and still
 * never replace one that is already there.
 */
static int create_only_save_without_hard_links(void)
{
#ifdef _WIN32
    return 0;   /* Windows installs with MoveFileExW, which never clobbers. */
#else
    const char *target = TEST_OUT "editor_no_hard_links.toml";
    const char *sentinel = "existing target\n";
    char temp[EDITOR_PATH_MAX];
    LevelDef def, loaded;
    int result = 1;

    ensure_out_dir();
    fill_valid_minimal(&def);
    remove(target);
    serializer_temp_path(target, temp, sizeof(temp));

    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_NO_HARD_LINKS);
    if (expect_int("create without links",
                   level_save_toml_with_policy(&def, target,
                                               SERIALIZER_SAVE_CREATE_ONLY), 0) != 0 ||
        expect_int("created file loads", level_load_toml(target, &loaded), 0) != 0 ||
        expect_string("created file content", loaded.name, def.name) != 0) goto done;
    if (expect_int("temp removed", serializer_probe_path_utf8(temp),
                   SERIALIZER_PATH_MISSING) != 0) goto done;

    if (write_text_file(target, sentinel) != 0 ||
        write_text_file(temp, "new bytes\n") != 0) goto done;
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_NO_HARD_LINKS);
    if (expect_int("existing target refused", serializer_create_file(temp, target), -1) != 0 ||
        expect_int("existing target kept", file_equals_text(target, sentinel), 1) != 0)
        goto done;
    result = 0;

done:
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_NONE);
    remove(temp);
    remove(target);
    return result;
#endif
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    ensure_out_dir();
    if (compact_history_owns_config_snapshots()) return 1;
    if (orphan_recovery_does_not_poison_discovery()) return 1;
    if (undo_groups_stay_whole()) return 1;
    if (recovery_folder_stays_manageable()) return 1;
    char binary_path[EDITOR_PATH_MAX];
    if (editor_playtest_binary_path(binary_path, sizeof(binary_path))) return 1;
    if (editor_playtest_binary_path(binary_path, 2) != -1) return 1;
    if (invalid_drafts_do_not_build_unsafe_previews() != 0) return 1;
    if (accepts_valid_level() != 0) return 1;
    if (rejects_bad_count_and_path() != 0) return 1;
    if (rejects_missing_phase_and_layer_paths() != 0) return 1;
    if (rejects_bad_runtime_link() != 0) return 1;
    if (warns_without_blocking() != 0) return 1;
    if (rejects_unsafe_asset_paths() != 0) return 1;
    if (save_and_load_resets_editor_session() != 0) return 1;
    if (invalid_save_preserves_existing_file() != 0) return 1;
    if (atomic_save_replaces_and_preserves_on_injected_errors() != 0) return 1;
    if (save_policy_and_fingerprint_seams() != 0) return 1;
    if (dir_sync_failure_after_install_still_saves() != 0) return 1;
    if (unreadable_existing_probe_is_not_missing() != 0) return 1;
    if (recovery_entries_survive_restart_and_sessions() != 0) return 1;
    if (over_capacity_load_preserves_document() != 0) return 1;
    if (utf8_filename_roundtrip() != 0) return 1;
#ifdef _WIN32
    if (utf8_wide_path_conversion_roundtrip() != 0) return 1;
#endif
    if (failed_save_preserves_target_and_cleans_temp() != 0) return 1;
    if (autosave_recovery_preserves_destination() != 0) return 1;
    if (editor_save_workflows_enforce_baselines() != 0) return 1;
    if (stranded_replace_keeps_the_temporary_file() != 0) return 1;
    if (changed_on_disk_save_has_its_own_message() != 0) return 1;
    if (saved_levels_get_ordinary_permissions() != 0) return 1;
    if (symlinks_are_followed_only_for_the_opened_document() != 0) return 1;
    if (text_fields_drop_invalid_utf8() != 0) return 1;
    if (recovery_metadata_and_failed_save_contract() != 0) return 1;
    if (playtest_destination_isolated() != 0) return 1;
    if (autosave_backs_off_and_snapshots_last_valid_level() != 0) return 1;
    if (loads_recent_files_with_trim_and_limit() != 0) return 1;
    if (property_command_undo_redo() != 0) return 1;
    if (last_star_text_property_undo_redo() != 0) return 1;
    if (config_command_preserves_entity_edit() != 0) return 1;
    if (dirty_save_point_tracks_undo_redo() != 0) return 1;
    if (selection_structural_mutations_are_safe() != 0) return 1;
    if (checkpoint_editor_mutations_are_reversible() != 0) return 1;
    if (rail_deletion_keeps_references_valid() != 0) return 1;
    if (group_copy_and_delete_keep_riders_with_their_rail() != 0) return 1;
    if (shared_level_rules_have_one_answer() != 0) return 1;
    if (validation_errors_report_where_they_are() != 0) return 1;
    if (validation_reports_every_runtime_error() != 0) return 1;
    if (float_platform_rail_switch_rechecks_its_rail() != 0) return 1;
    if (drag_round_trips_and_follows_grab_point() != 0) return 1;
    if (editor_mutations_keep_level_valid() != 0) return 1;
    if (refused_mutations_explain_why() != 0) return 1;
    if (camera_scrolls_vertically_and_stays_clamped() != 0) return 1;
    if (playtest_blocks_editing_and_stop_cleans_up() != 0) return 1;
    if (load_fingerprints_the_bytes_it_parsed() != 0) return 1;
    if (recovery_metadata_keeps_longest_source_path() != 0) return 1;
    if (recent_files_skip_overlong_lines_and_line_breaks() != 0) return 1;
    if (dialog_quoting_and_picked_paths_stay_literal() != 0) return 1;
    if (entity_table_has_a_row_for_every_type() != 0) return 1;
    if (document_hash_covers_every_entity_and_config_field() != 0) return 1;
    if (widget_commit_paths_preserve_values() != 0) return 1;
    if (config_preview_sync_preserves_old_texture() != 0) return 1;
    if (staged_edit_save_and_quit_boundaries() != 0) return 1;
    if (display_paths_keep_the_file_name() != 0) return 1;
    if (layer_buttons_record_their_own_undo_step() != 0) return 1;
    if (field_limits_apply_on_every_commit_path() != 0) return 1;
    if (leaving_a_field_applies_valid_values_silently() != 0) return 1;
    if (motion_fields_stay_within_validator_limits() != 0) return 1;
    if (open_dropdown_owns_the_next_click() != 0) return 1;
    if (dropdowns_accept_any_option_for_unknown_values() != 0) return 1;
    if (new_level_resets_previews() != 0) return 1;
    if (rail_rider_paste_follows_its_rail() != 0) return 1;
    if (repeated_paste_steps_each_copy() != 0) return 1;
    if (extreme_floats_round_trip_through_save() != 0) return 1;
    if (create_only_save_without_hard_links() != 0) return 1;

    puts("editor_validation_test: ok");
    return 0;
}
