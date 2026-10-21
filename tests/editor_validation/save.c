/* Included by tests/editor_validation_test.c. Not compiled alone: the functions are
 * static and use the helpers in that file.
 */
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

