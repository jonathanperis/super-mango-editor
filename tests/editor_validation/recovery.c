/* Included by tests/editor_validation_test.c. Not compiled alone: the functions are
 * static and use the helpers in that file.
 */
static int recovery_entries_survive_restart_and_sessions_with(EditorState *first,
                                                              EditorState *second,
                                                              EditorState *restarted)
{
    const char *source = TEST_OUT "editor_manifest_source.toml";
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
    strncpy(first->file_path, source, sizeof(first->file_path) - 1);
    strncpy(second->file_path, source, sizeof(second->file_path) - 1);
    editor_level_init_defaults(&first->level);
    editor_level_init_defaults(&second->level);
    first->modified = 1;
    second->modified = 1;
    first->last_autosave_ms = (uint32_t)clock_millis() - 30001u;
    second->last_autosave_ms = (uint32_t)clock_millis() - 30001u;
    if (editor_set_preference_root(first, root) != 0 ||
        editor_set_preference_root(second, root) != 0 ||
        editor_init_persistence_paths(first) != 0 ||
        editor_init_persistence_paths(second) != 0) goto cleanup;
    /* Same-size EDITOR_PATH_MAX arrays: copy the whole terminated path. */
    memcpy(first_snapshot, first->autosave_path, sizeof(first_snapshot));
    memcpy(second_snapshot, second->autosave_path, sizeof(second_snapshot));
    editor_maybe_autosave(first);
    editor_maybe_autosave(second);
    if (expect_int("session snapshots differ",
                   strcmp(first_snapshot, second_snapshot) != 0, 1) != 0)
        goto cleanup;
    if (expect_int("first snapshot root", path_stays_in_root(root, first_snapshot), 1) != 0 ||
        expect_int("second snapshot root", path_stays_in_root(root, second_snapshot), 1) != 0 ||
        expect_int("first metadata root",
                   path_stays_in_root(root, first->recovery_entries[0].metadata_path), 1) != 0)
        goto cleanup;

    if (editor_set_preference_root(restarted, root) != 0 ||
        editor_init_persistence_paths(restarted) != 0 ||
        expect_int("restart recovery count", restarted->recovery_entry_count, 2) != 0)
        goto cleanup;
    if (expect_int("restart source identity",
                   strcmp(restarted->recovery_entries[0].source_path, source) == 0 ||
                   strcmp(restarted->recovery_entries[1].source_path, source) == 0,
                   1) != 0)
        goto cleanup;

    {
        uint64_t before_hash = editor_document_hash(&restarted->level);
        char before_path[EDITOR_PATH_MAX];
        memcpy(before_path, restarted->file_path, sizeof(before_path));
        editor_test_set_recovery_choice(0);
        if (expect_int("recovery picker cancel", editor_choose_recovery(restarted), -1) != 0 ||
            expect_int("recovery picker leaves document",
                       editor_document_hash(&restarted->level) == before_hash &&
                       strcmp(restarted->file_path, before_path) == 0, 1) != 0)
            goto cleanup;
    }

    /* snprintf returns the length it needed; a temp root too long for the
     * buffer would test the wrong file, so treat that as a failure. */
    if (snprintf(malformed, sizeof(malformed), "%s/editor_recovery_bad.meta",
                 root) >= (int)sizeof(malformed)) goto cleanup;
    if (write_text_file(malformed, "not-a-recovery-record\n") != 0) goto cleanup;
    if (expect_int("malformed recovery ignored",
                   editor_discover_recoveries(restarted), 0) != 0 ||
        expect_int("malformed recovery leaves valid entries",
                   restarted->recovery_entry_count, 2) != 0)
        goto cleanup;

    file_dialog_test_set_open_result(FILE_DIALOG_CANCELLED, NULL);
    editor_open_level_file(restarted);
    if (expect_int("cancelled picker preserves recovery",
                   restarted->recovery_entry_count, 2) != 0)
        goto cleanup;
    file_dialog_test_set_open_result(FILE_DIALOG_SELECTED,
                                     TEST_OUT "editor_missing_open.toml");
    editor_open_level_file(restarted);
    if (expect_int("failed open preserves recovery",
                   restarted->recovery_entry_count, 2) != 0)
        goto cleanup;

    {
        InputEvent recovery_event;
        char modal_save_path[] = TEST_OUT "editor_recovery_modal_save.toml";
        remove(modal_save_path);
        editor_level_init_defaults(&restarted->level);
        restarted->modified = 1;
        editor_test_set_recovery_choice(1);
        editor_test_set_discard_choice(2);
        file_dialog_test_set_save_result(FILE_DIALOG_SELECTED, modal_save_path);
        memset(&recovery_event, 0, sizeof(recovery_event));
        recovery_event.type = INPUT_KEY_DOWN;
        recovery_event.key = KEY_R;
        recovery_event.mods = INPUT_CTRL;
        editor_handle_event(restarted, &recovery_event);
        if (expect_int("save during recovery selection preserves entries",
                       restarted->recovery_entry_count, 2) != 0 ||
            expect_string("save during recovery resolves selected source",
                          restarted->file_path, source) != 0)
            goto cleanup;
        remove(modal_save_path);
    }

    if (snprintf(blocked_root, sizeof(blocked_root), "%s/not-a-directory",
                 root) >= (int)sizeof(blocked_root)) goto cleanup;
    if (write_text_file(blocked_root, "block discovery\n") != 0) goto cleanup;
    {
        char saved_root[EDITOR_PATH_MAX];
        memcpy(saved_root, restarted->recovery_root_path, sizeof(saved_root));
        memcpy(restarted->recovery_root_path, blocked_root,
               sizeof(restarted->recovery_root_path));
        editor_retire_current_recovery(restarted);
        memcpy(restarted->recovery_root_path, saved_root, sizeof(saved_root));
    }
    if (expect_int("discovery failure preserves snapshot",
                   editor_file_exists(first_snapshot), 1) != 0)
        goto cleanup;

    editor_retire_current_recovery(first);
    editor_retire_current_recovery(second);
    result = 0;

cleanup:
    file_dialog_test_set_open_result(-1, NULL);
    file_dialog_test_set_save_result(-1, NULL);
    editor_test_set_recovery_choice(-1);
    editor_test_set_discard_choice(-1);
    if (blocked_root[0] != '\0') remove(blocked_root);
    cleanup_test_preference_root_of(root, (EditorState *[]){first, second, restarted}, 3);
    remove(source);
    remove(malformed);
    return result;
}

/* An EditorState is about 200 KB and Windows gives a thread 1 MB of stack,
 * so the 3 this test needs come from the heap. */
static int recovery_entries_survive_restart_and_sessions(void)
{
    EditorState *states = calloc(3, sizeof(*states));
    int result;

    if (!states) return 1;
    result = recovery_entries_survive_restart_and_sessions_with(&states[0], &states[1], &states[2]);
    free(states);
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

static int recovery_metadata_and_failed_save_contract_with(EditorState *es,
                                                           EditorState *recovered)
{
    const char *recovery = TEST_OUT "editor_recovery_contract.toml";
    const char *valid = TEST_OUT "editor_recovery_valid.toml";
    const char *target = TEST_OUT "editor_recovery_target.toml";
    char metadata[EDITOR_PATH_MAX];

    ensure_out_dir();
    remove(recovery);
    remove(valid);
    remove(target);
    memset(es, 0, sizeof(*es));
    editor_level_init_defaults(&es->level);
    strncpy(es->file_path, target, sizeof(es->file_path) - 1);
    strncpy(es->autosave_path, recovery, sizeof(es->autosave_path) - 1);
    /* This fixture owns an absent destination; reach the injected write failure
     * instead of relying on a headless native confirmation to fail first. */
    es->source_state = EDITOR_SOURCE_EXPECTED_MISSING;

    if (level_save_toml(&es->level, valid) != 0 ||
        level_save_toml_recovery(&es->level, recovery, target) != 0)
        goto cleanup;
    if (level_read_recovery_path(recovery, metadata, sizeof(metadata)) != 1 ||
        expect_string("recovery metadata", metadata, target) != 0)
        goto cleanup;
    editor_retire_matching_recovery(es, TEST_OUT "other-document.toml");
    if (expect_int("stale recovery retained", editor_file_exists(recovery), 1) != 0)
        goto cleanup;
    if (copy_file_with_bad_recovery_metadata(valid, recovery) != 0)
        goto cleanup;
    memset(recovered, 0, sizeof(*recovered));
    strncpy(recovered->autosave_path, recovery,
            sizeof(recovered->autosave_path) - 1);
    if (expect_int("malformed metadata recovery", editor_recover_autosave(recovered), 0) != 0)
        goto cleanup;
    if (expect_int("malformed metadata untitled", recovered->file_path[0], '\0') != 0)
        goto cleanup;
    if (expect_int("malformed metadata dirty", recovered->modified, 1) != 0)
        goto cleanup;

    if (level_save_toml_recovery(&es->level, recovery, target) != 0)
        goto cleanup;
    es->modified = 1;
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_WRITE);
    if (expect_int("failed explicit save", editor_save_current_level(es), -1) != 0) {
        serializer_test_set_failure(SERIALIZER_TEST_FAILURE_NONE);
        goto cleanup;
    }
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_NONE);
    if (expect_int("failed save keeps recovery", editor_file_exists(recovery), 1) != 0)
        goto cleanup;

    editor_retire_matching_recovery(es, target);
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

/* An EditorState is about 200 KB and Windows gives a thread 1 MB of stack,
 * so the 2 this test needs come from the heap. */
static int recovery_metadata_and_failed_save_contract(void)
{
    EditorState *states = calloc(2, sizeof(*states));
    int result;

    if (!states) return 1;
    result = recovery_metadata_and_failed_save_contract_with(&states[0], &states[1]);
    free(states);
    return result;
}

static int playtest_destination_isolated_with(EditorState *es, EditorState *second)
{
    char root[EDITOR_PATH_MAX];
    char playtest_path[EDITOR_PATH_MAX];
    uint64_t saved_hash;
    int saved_modified;

    memset(es, 0, sizeof(*es));
    editor_level_init_defaults(&es->level);
    if (make_test_preference_root(root, sizeof(root)) != 0) return 1;
    if (editor_set_preference_root(es, root) != 0) goto cleanup;
    strncpy(es->file_path, TEST_OUT "active_editor_document.toml",
            sizeof(es->file_path) - 1);
    es->modified = 1;
    es->saved_document_hash = editor_document_hash(&es->level);
    es->saved_document_hash_valid = 1;
    saved_hash = es->saved_document_hash;
    saved_modified = es->modified;
    remove(es->file_path);

    if (editor_init_persistence_paths(es) != 0 ||
        editor_prepare_playtest_level(es, playtest_path,
                                      sizeof(playtest_path)) != 0)
        goto cleanup;
    memset(second, 0, sizeof(*second));
    if (editor_set_preference_root(second, root) != 0 ||
        editor_init_persistence_paths(second) != 0 ||
        expect_int("untitled recovery paths unique",
                   strcmp(es->autosave_path, second->autosave_path) == 0, 0) != 0)
        goto cleanup;
    if (expect_int("playtest differs from active",
                   strcmp(playtest_path, es->file_path) == 0, 0) != 0)
        goto cleanup;
    if (expect_int("playtest stays in preference root",
                   path_stays_in_root(root, playtest_path), 1) != 0)
        goto cleanup;
    if (expect_int("playtest avoids legacy destination",
                   strcmp(playtest_path, "levels/_playtest.toml") == 0, 0) != 0)
        goto cleanup;
    if (expect_int("playtest file exists", editor_file_exists(playtest_path), 1) != 0)
        goto cleanup;
    if (expect_int("playtest preserves dirty", es->modified, saved_modified) != 0)
        goto cleanup;
    if (expect_int("playtest preserves save point",
                   es->saved_document_hash == saved_hash, 1) != 0)
        goto cleanup;
    if (expect_int("playtest does not create active file",
                   editor_file_exists(es->file_path), 0) != 0)
        goto cleanup;

    editor_retire_playtest_level(es);
    if (expect_int("playtest retired", editor_file_exists(playtest_path), 0) != 0)
        goto cleanup;
    cleanup_test_preference_root_of(root, (EditorState *[]){es, second}, 2);
    return 0;

cleanup:
    editor_retire_playtest_level(es);
    editor_retire_playtest_level(second);
    cleanup_test_preference_root_of(root, (EditorState *[]){es, second}, 2);
    remove(es->file_path);
    return 1;
}

/* An EditorState is about 200 KB and Windows gives a thread 1 MB of stack,
 * so the 2 this test needs come from the heap. */
static int playtest_destination_isolated(void)
{
    EditorState *states = calloc(2, sizeof(*states));
    int result;

    if (!states) return 1;
    result = playtest_destination_isolated_with(&states[0], &states[1]);
    free(states);
    return result;
}

