/* Included by tests/editor_validation_test.c. Not compiled alone: the functions are
 * static and use the helpers in that file.
 */
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

static int autosave_recovery_preserves_destination_with(EditorState *es,
                                                        EditorState *recovered)
{
    const char *destination = EDITOR_TEST_RECOVERY_DEST;
    const char *save_as_path = TEST_OUT "editor_recovered_save_as.toml";
    char root[EDITOR_PATH_MAX] = {0};
    uint64_t recovery_id;

    ensure_out_dir();
    remove(destination);
    remove(save_as_path);
    if (make_test_preference_root(root, sizeof(root)) != 0) return 1;

    editor_level_init_defaults(&es->level);
    es->level.coin_count = 1;
    es->level.coins[0].x = 64.0f;
    es->level.coins[0].y = 96.0f;
    strncpy(es->file_path, destination, sizeof(es->file_path) - 1);
    if (editor_set_preference_root(es, root) != 0 ||
        editor_init_persistence_paths(es) != 0) goto cleanup;
    es->modified = 1;
    es->last_autosave_ms = (uint32_t)clock_millis() - 30001u;

    editor_maybe_autosave(es);
    if (expect_int("autosave exists", editor_file_exists(es->autosave_path), 1) != 0)
        goto cleanup;
    if (expect_int("autosave remains dirty", es->modified, 1) != 0) goto cleanup;
    if (expect_string("autosave keeps destination", es->file_path,
                      destination) != 0) goto cleanup;
    if (expect_prefix("autosave status", es->status_message,
                      "Autosaved recovery copy") != 0) goto cleanup;
    if (expect_int("autosave does not create destination",
                   editor_file_exists(EDITOR_TEST_RECOVERY_DEST), 0) != 0)
        goto cleanup;

    if (editor_set_preference_root(recovered, root) != 0 ||
        editor_init_persistence_paths(recovered) != 0 ||
        expect_int("recovery discovery", recovered->recovery_entry_count, 1) != 0)
        goto cleanup;
    recovery_id = recovered->recovery_entries[0].id;
    if (expect_int("recover result", editor_recover_entry_by_id(recovered,
                                                                  recovery_id), 0) != 0)
        goto cleanup;
    if (expect_int("recovered stays dirty", recovered->modified, 1) != 0)
        goto cleanup;
    if (expect_string("recovered destination", recovered->file_path,
                      destination) != 0) goto cleanup;
    if (expect_int("recovery not recent", recovered->recent_file_count, 0) != 0)
        goto cleanup;
    if (expect_prefix("recovery status", recovered->status_message,
                      "Recovered unsaved changes") != 0) goto cleanup;

    if (expect_int("recovered source unknown",
                   recovered->source_state, EDITOR_SOURCE_UNKNOWN) != 0)
        goto cleanup;
    recovered->level.coins[0].x = 128.0f;
    file_dialog_test_set_save_result(FILE_DIALOG_SELECTED, save_as_path);
    if (editor_save_current_level_as(recovered) != 0) goto cleanup;
    if (expect_int("recovered Save As clean", recovered->modified, 0) != 0)
        goto cleanup;
    if (expect_int("normal save retires recovery copy",
                   editor_file_exists(es->autosave_path), 0) != 0)
        goto cleanup;

    cleanup_test_preference_root_of(root, (EditorState *[]){es, recovered}, 2);
    remove(destination);
    remove(save_as_path);
    return 0;

cleanup:
    file_dialog_test_set_save_result(-1, NULL);
    cleanup_test_preference_root_of(root, (EditorState *[]){es, recovered}, 2);
    remove(destination);
    remove(save_as_path);
    return 1;
}

/* An EditorState is about 200 KB and Windows gives a thread 1 MB of stack,
 * so the 2 this test needs come from the heap. */
static int autosave_recovery_preserves_destination(void)
{
    EditorState *states = calloc(2, sizeof(*states));
    int result;

    if (!states) return 1;
    result = autosave_recovery_preserves_destination_with(&states[0], &states[1]);
    free(states);
    return result;
}

static int editor_save_workflows_enforce_baselines_with(EditorState *es,
                                                        EditorState *create_only)
{
    const char *existing = TEST_OUT "editor_save_workflow_existing.toml";
    const char *appearing = TEST_OUT "editor_save_workflow_appearing.toml";
    char root[EDITOR_PATH_MAX] = {0};
    int result = 1;

    ensure_out_dir();
    remove(existing);
    remove(appearing);
    if (write_text_file(existing, "old bytes\n") != 0 ||
        make_test_preference_root(root, sizeof(root)) != 0) goto cleanup;

    editor_level_init_defaults(&es->level);
    es->modified = 1;
    if (editor_set_preference_root(es, root) != 0 ||
        editor_init_persistence_paths(es) != 0) goto cleanup;
    file_dialog_test_set_save_result(FILE_DIALOG_SELECTED, existing);
    editor_test_set_overwrite_choice(1);
    if (expect_int("editor Save As existing", editor_save_current_level_as(es), 0) != 0 ||
        expect_int("editor Save As existing clean", es->modified, 0) != 0)
        goto cleanup;

    es->level.coin_score++;
    if (write_text_file(existing, "external bytes\n") != 0) goto cleanup;
    editor_test_set_external_choice(EDITOR_EXTERNAL_REPLACE);
    if (expect_int("editor normal external replace",
                   editor_save_current_level(es), 0) != 0)
        goto cleanup;

    editor_level_init_defaults(&create_only->level);
    create_only->modified = 1;
    if (editor_set_preference_root(create_only, root) != 0 ||
        editor_init_persistence_paths(create_only) != 0) goto cleanup;
    file_dialog_test_set_save_result(FILE_DIALOG_SELECTED, appearing);
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_TARGET_APPEARED);
    if (expect_int("editor Save As create-only race",
                   editor_save_current_level_as(create_only), -1) != 0 ||
        expect_int("create-only race keeps untitled",
                   create_only->file_path[0], '\0') != 0)
        goto cleanup;
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_NONE);
    result = 0;

cleanup:
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_NONE);
    file_dialog_test_set_save_result(-1, NULL);
    editor_test_set_overwrite_choice(-1);
    editor_test_set_external_choice((EditorExternalChoice)-1);
    cleanup_test_preference_root_of(root, (EditorState *[]){es, create_only}, 2);
    remove(existing);
    remove(appearing);
    return result;
}

/* An EditorState is about 200 KB and Windows gives a thread 1 MB of stack,
 * so the 2 this test needs come from the heap. */
static int editor_save_workflows_enforce_baselines(void)
{
    EditorState *states = calloc(2, sizeof(*states));
    int result;

    if (!states) return 1;
    result = editor_save_workflows_enforce_baselines_with(&states[0], &states[1]);
    free(states);
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

/* The Contrast setting survives a restart, and the settings reader skips
 * what it does not understand instead of failing. */
static int editor_settings_survive_a_restart(void)
{
    char root[EDITOR_PATH_MAX] = {0};
    EditorState first = {0}, next = {0};
    EditorState *const states[] = {&first, &next};
    FILE *fp;
    int result = 1;

    ensure_out_dir();
    if (make_test_preference_root(root, sizeof(root)) != 0 ||
        editor_set_preference_root(&first, root) != 0 ||
        editor_init_persistence_paths(&first) != 0) goto cleanup;
    ui_init(&first.ui, NULL);
    editor_set_high_contrast(&first, 1);
    if (expect_int("settings saved", editor_save_settings(&first), 0) != 0) goto cleanup;

    if (editor_set_preference_root(&next, root) != 0 ||
        editor_init_persistence_paths(&next) != 0) goto cleanup;
    ui_init(&next.ui, NULL);
    editor_load_settings(&next);
    if (expect_int("contrast restored", next.high_contrast, 1) != 0 ||
        expect_int("contrast style", next.ui.style.border_width,
                   UI_STYLE_HIGH_CONTRAST.border_width) != 0) goto cleanup;

    /* Unknown keys, bad values and an over-long line are skipped; the last
     * valid high_contrast line wins. */
    fp = fopen(next.settings_path, "wb");
    if (!fp) goto cleanup;
    fputs("unknown=7\nhigh_contrast=yes\n", fp);
    for (int i = 0; i < 200; i++) fputc('x', fp);
    fputs("high_contrast=1\nhigh_contrast=0\n", fp);
    fclose(fp);
    editor_set_high_contrast(&next, 1);
    editor_load_settings(&next);
    if (expect_int("contrast off from file", next.high_contrast, 0) != 0 ||
        expect_int("default style back", next.ui.style.border_width, 0) != 0) goto cleanup;

    /* Without a settings path (render smoke) saving reports failure and
     * loading changes nothing. */
    {
        EditorState bare = {0};
        ui_init(&bare.ui, NULL);
        if (expect_int("no path save", editor_save_settings(&bare), -1) != 0) goto cleanup;
        editor_load_settings(&bare);
        if (expect_int("no path keeps default", bare.high_contrast, 0) != 0) goto cleanup;
    }
    result = 0;
cleanup:
    cleanup_test_preference_root_of(root, states, 2);
    return result;
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
