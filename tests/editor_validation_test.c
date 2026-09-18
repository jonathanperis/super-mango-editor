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
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "editor/editor.h"
#include "editor/editor_clipboard.h"
#include "editor/editor_events.h"
#include "editor/editor_files.h"
#include "editor/editor_session.h"
#include "editor/editor_undo_apply.h"
#include "editor/editor_validation.h"
#include "editor/entity_meta.h"
#include "editor/file_dialog.h"
#include "shared/serializer.h"
#include "shared/serializer_io.h"
#include "editor/tools.h"
#include "editor/undo.h"
#include "shared/ui.h"
#include "editor/canvas.h"
#include "editor/editor_playtest.h"
#include "editor/properties.h"

#define EDITOR_WORKFLOW_LEVEL_PATH "out/test_editor_workflow_level.toml"
#define EDITOR_WORKFLOW_RECENT_PATH "out/editor_recent.txt"
#define EDITOR_TEST_AUTOSAVE_PATH "out/autosave/test_editor_autosave.toml"
#define EDITOR_TEST_RECOVERY_DEST "out/test_editor_recovery_destination.toml"
#define EDITOR_TEST_FAILED_TARGET "out/test_editor_failed_target.toml"

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
    _mkdir("out");
#else
    mkdir("out", 0755);
#endif
}

static void ensure_autosave_dir(void)
{
#ifdef _WIN32
    _mkdir("out/autosave");
#else
    mkdir("out/autosave", 0755);
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
    if (snprintf(root, root_size, "out/editor_pref_test_%lu", process_id) < 0)
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

static int rejects_bad_runtime_link(void)
{
    LevelDef def;
    EditorValidationReport report;

    fill_valid_minimal(&def);
    def.spike_block_count = 1;
    def.spike_blocks[0].rail_index = 0;

    if (expect_int("bad link result", editor_validate_level(&def, &report), -1) != 0)
        return 1;
    if (expect_int("bad link errors", report.error_count, 1) != 0) return 1;

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
    undo_push(es.undo, cmd);

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
    undo_push(es.undo, cmd);
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
    const char *target = "out/editor_atomic_target.toml";
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
    const char *target = "out/editor_save_policy_target.toml";
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

static int unreadable_existing_probe_is_not_missing(void)
{
    const char *target = "out/editor_unreadable_target.toml";
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
    const char *source = "out/editor_manifest_source.toml";
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
    strncpy(first_snapshot, first.autosave_path, sizeof(first_snapshot) - 1);
    strncpy(second_snapshot, second.autosave_path, sizeof(second_snapshot) - 1);
    first_snapshot[sizeof(first_snapshot) - 1] = '\0';
    second_snapshot[sizeof(second_snapshot) - 1] = '\0';
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

    snprintf(malformed, sizeof(malformed), "%s/editor_recovery_bad.meta", root);
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
                                     "out/editor_missing_open.toml");
    editor_open_level_file(&restarted);
    if (expect_int("failed open preserves recovery",
                   restarted.recovery_entry_count, 2) != 0)
        goto cleanup;

    {
        InputEvent recovery_event;
        char modal_save_path[] = "out/editor_recovery_modal_save.toml";
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

    snprintf(blocked_root, sizeof(blocked_root), "%s/not-a-directory", root);
    if (write_text_file(blocked_root, "block discovery\n") != 0) goto cleanup;
    {
        char saved_root[EDITOR_PATH_MAX];
        memcpy(saved_root, restarted.recovery_root_path, sizeof(saved_root));
        strncpy(restarted.recovery_root_path, blocked_root,
                sizeof(restarted.recovery_root_path) - 1);
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
    const char *path = "out/editor_é测试_🍊.toml";
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
    const char *original = "out/editor_é测试_🍊";
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
    const char *recovery = "out/editor_recovery_contract.toml";
    const char *valid = "out/editor_recovery_valid.toml";
    const char *target = "out/editor_recovery_target.toml";
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
    editor_retire_matching_recovery(&es, "out/other-document.toml");
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
    strncpy(es.file_path, "out/active_editor_document.toml",
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
    const char *save_as_path = "out/editor_recovered_save_as.toml";
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
    const char *existing = "out/editor_save_workflow_existing.toml";
    const char *appearing = "out/editor_save_workflow_appearing.toml";
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

static int invalid_autosave_does_not_consume_autosave_interval(void)
{
    EditorState es;

    ensure_out_dir();
    memset(&es, 0, sizeof(es));
    editor_level_init_defaults(&es.level);
    strncpy(es.autosave_path, "out/autosave/test_editor_autosave.toml",
            sizeof(es.autosave_path) - 1);
    ensure_autosave_dir();
    remove(es.autosave_path);

    es.modified = 1;
    es.last_autosave_ms = (uint32_t)clock_millis() - 30001u;
    es.level.coin_count = MAX_COINS + 1;

    editor_maybe_autosave(&es);
    if (expect_string("invalid autosave status", es.status_message,
                      "Autosave skipped: level has validation errors") != 0)
        return 1;
    if (expect_int("invalid autosave not written",
                   editor_file_exists(es.autosave_path), 0) != 0)
        return 1;

    es.level.coin_count = 0;
    editor_maybe_autosave(&es);
    if (expect_int("fixed autosave written immediately",
                   editor_file_exists(es.autosave_path), 1) != 0)
        return 1;

    remove(es.autosave_path);
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

typedef struct {
    TextFont *font;
    int drawing;
} EditorWidgetTestContext;

static int editor_widget_test_context_init(EditorWidgetTestContext *context)
{
    memset(context, 0, sizeof(*context));
    if (display_open(320, 240, "editor state test", 1)) return -1;
    context->font = font_load("assets/fonts/round9x13.ttf", 13);
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
    const char *target = "out/editor_staged_command.toml";
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
        if (!undo_push(stack, command)) { failed = 1; break; }
    }
    if (!failed && (stack->top != UNDO_MAX || !undo_pop(stack, &popped) ||
        popped.config_after.screen_count != UNDO_MAX + 2 || !undo_pop(stack, &popped) ||
        !redo_pop(stack, &popped))) failed = 1;
    command.type = CMD_PLACE;
    if (!undo_push(stack, command) || stack->redo_top != 0) failed = 1;
    undo_clear(stack);
    if (stack->top || stack->redo_top || sizeof(UndoStack) > 512 * 1024) failed = 1;
    printf("undo storage: %zu bytes plus config snapshots only when used\n", sizeof(UndoStack));
    undo_destroy(stack);
    return failed;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    ensure_out_dir();
    if (compact_history_owns_config_snapshots()) return 1;
    if (orphan_recovery_does_not_poison_discovery()) return 1;
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
    if (recovery_metadata_and_failed_save_contract() != 0) return 1;
    if (playtest_destination_isolated() != 0) return 1;
    if (invalid_autosave_does_not_consume_autosave_interval() != 0) return 1;
    if (loads_recent_files_with_trim_and_limit() != 0) return 1;
    if (property_command_undo_redo() != 0) return 1;
    if (last_star_text_property_undo_redo() != 0) return 1;
    if (config_command_preserves_entity_edit() != 0) return 1;
    if (dirty_save_point_tracks_undo_redo() != 0) return 1;
    if (selection_structural_mutations_are_safe() != 0) return 1;
    if (checkpoint_editor_mutations_are_reversible() != 0) return 1;
    if (widget_commit_paths_preserve_values() != 0) return 1;
    if (config_preview_sync_preserves_old_texture() != 0) return 1;
    if (staged_edit_save_and_quit_boundaries() != 0) return 1;

    puts("editor_validation_test: ok");
    return 0;
}
