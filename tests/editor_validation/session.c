/* Included by tests/editor_validation_test.c. Not compiled alone: the functions are
 * static and use the helpers in that file.
 */
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

static int recovery_metadata_keeps_longest_source_path_with(EditorState *es,
                                                            EditorState *restarted)
{
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
        editor_set_preference_root(es, root) != 0 ||
        editor_set_preference_root(restarted, root) != 0) return 1;
    editor_level_init_defaults(&es->level);
    memcpy(es->file_path, long_path, length + 1);
    if (editor_init_persistence_paths(es) != 0) goto cleanup;
    es->modified = 1;
    es->last_autosave_ms = (uint32_t)clock_millis() - 30001u;
    editor_maybe_autosave(es);

    /* A restarted editor must rediscover it with the full source path. */
    if (editor_init_persistence_paths(restarted) != 0 ||
        expect_int("long path entry found", restarted->recovery_entry_count, 1) != 0 ||
        expect_int("long path round trip",
                   strcmp(restarted->recovery_entries[0].source_path, long_path) == 0,
                   1) != 0) goto cleanup;
    result = 0;

cleanup:
    cleanup_test_preference_root_of(root, (EditorState *[]){es, restarted}, 2);
    return result;
}

/* An EditorState is about 200 KB and Windows gives a thread 1 MB of stack,
 * so the 2 this test needs come from the heap. */
static int recovery_metadata_keeps_longest_source_path(void)
{
    EditorState *states = calloc(2, sizeof(*states));
    int result;

    if (!states) return 1;
    result = recovery_metadata_keeps_longest_source_path_with(&states[0], &states[1]);
    free(states);
    return result;
}

typedef struct {
    TextFont *font;
    int drawing;
} EditorWidgetTestContext;

static int editor_widget_test_context_init(EditorWidgetTestContext *context)
{
    memset(context, 0, sizeof(*context));
    if (display_open(320, 240, "editor state test", 1, 0)) return -1;
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

static int staged_edit_save_and_quit_boundaries_with(EditorState *save_state,
                                                     EditorState *quit_state,
                                                     EditorState *selection_state)
{
    const char *target = TEST_OUT "editor_staged_command.toml";
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

    save_state->level = initial;
    ui_init(&save_state->ui, context.font);
    save_state->undo = undo_create();
    strncpy(save_state->file_path, target, sizeof(save_state->file_path) - 1);
    if (!save_state->undo || editor_set_preference_root(save_state, root) != 0 ||
        editor_init_persistence_paths(save_state) != 0 ||
        serializer_fingerprint_utf8(target,
                                                        &save_state->source_fingerprint) != 1)
        goto fail;
    save_state->source_state = EDITOR_SOURCE_EXPECTED_EXISTING;
    editor_set_document_save_point(save_state);
    save_state->selection.type = ENT_COIN;
    save_state->selection.index = 0;
    ui_begin_frame(&save_state->ui);
    save_state->ui.mouse_clicked = 1;
    save_state->ui.mouse_x = 4;
    save_state->ui.mouse_y = 4;
    (void)ui_float_field(&save_state->ui, 301, 0, 0, 120,
                         &save_state->level.coins[0].x);
    memset(&event, 0, sizeof(event));
    event.type = INPUT_TEXT;
    strncpy(event.text, "7", sizeof(event.text) - 1);
    editor_handle_event(save_state, &event);
    event.text[0] = '2';
    event.text[1] = '\0';
    editor_handle_event(save_state, &event);
    memset(&event, 0, sizeof(event));
    event.type = INPUT_KEY_DOWN;
    event.key = KEY_S;
    event.mods = INPUT_CTRL;
    editor_test_set_finish_field_choice(1);
    editor_handle_event(save_state, &event);
    if (expect_prefix("staged Save succeeds", save_state->status_message,
                      "Saved ") != 0 ||
        expect_float_value("staged Save value", save_state->level.coins[0].x,
                           72.0f) != 0 ||
        expect_int("staged Save clean", save_state->modified, 0) != 0 ||
        expect_int("staged Save command", save_state->undo->top, 1) != 0)
        goto fail;

    editor_level_init_defaults(&quit_state->level);
    quit_state->level.coin_count = 1;
    quit_state->level.coins[0].x = 0.0f;
    ui_init(&quit_state->ui, context.font);
    quit_state->undo = undo_create();
    if (!quit_state->undo || editor_set_preference_root(quit_state, root) != 0 ||
        editor_init_persistence_paths(quit_state) != 0) goto fail;
    editor_set_document_save_point(quit_state);
    quit_state->selection.type = ENT_COIN;
    quit_state->selection.index = 0;
    ui_begin_frame(&quit_state->ui);
    quit_state->ui.mouse_clicked = 1;
    quit_state->ui.mouse_x = 4;
    quit_state->ui.mouse_y = 4;
    (void)ui_float_field(&quit_state->ui, 301, 0, 0, 120,
                         &quit_state->level.coins[0].x);
    memset(&event, 0, sizeof(event));
    event.type = INPUT_TEXT;
    strncpy(event.text, "8", sizeof(event.text) - 1);
    editor_handle_event(quit_state, &event);
    event.text[0] = '0';
    event.text[1] = '\0';
    editor_handle_event(quit_state, &event);
    editor_test_set_finish_field_choice(1);
    editor_test_set_discard_choice(1);
    memset(&event, 0, sizeof(event));
    event.type = INPUT_QUIT;
    quit_state->running = 1;
    editor_handle_event(quit_state, &event);
    if (expect_int("staged Quit confirmation", quit_state->running, 0) != 0 ||
        expect_float_value("staged Quit value", quit_state->level.coins[0].x,
                           80.0f) != 0 ||
        expect_int("staged Quit field closed", quit_state->ui.active_id, 0) != 0)
        goto fail;

    editor_level_init_defaults(&selection_state->level);
    selection_state->level.coin_count = 1;
    selection_state->level.coins[0].x = 0.0f;
    selection_state->undo = undo_create();
    ui_init(&selection_state->ui, context.font);
    if (!selection_state->undo) goto fail;
    editor_set_document_save_point(selection_state);
    selection_state->selection.type = ENT_COIN;
    selection_state->selection.index = 0;
    ui_begin_frame(&selection_state->ui);
    selection_state->ui.mouse_clicked = 1;
    selection_state->ui.mouse_x = 4;
    selection_state->ui.mouse_y = 4;
    (void)ui_float_field(&selection_state->ui, 301, 0, 0, 120,
                         &selection_state->level.coins[0].x);
    memset(&event, 0, sizeof(event));
    event.type = INPUT_TEXT;
    strncpy(event.text, "9", sizeof(event.text) - 1);
    editor_handle_event(selection_state, &event);
    memset(&event, 0, sizeof(event));
    event.type = INPUT_KEY_DOWN;
    event.key = KEY_TWO;
    editor_handle_event(selection_state, &event);
    if (expect_float_value("typing leaves value staged",
                           selection_state->level.coins[0].x, 0.0f) != 0 ||
        expect_int("typing does not select a tool", selection_state->tool, TOOL_SELECT) != 0 ||
        expect_int("typing keeps active field", selection_state->ui.active_id, 301) != 0) goto fail;

    undo_destroy(save_state->undo);
    undo_destroy(quit_state->undo);
    undo_destroy(selection_state->undo);
    ui_cleanup(&save_state->ui);
    ui_cleanup(&quit_state->ui);
    ui_cleanup(&selection_state->ui);
    cleanup_test_preference_root_of(root,
                                    (EditorState *[]){save_state, quit_state,
                                                      selection_state}, 3);
    editor_widget_test_context_cleanup(&context);
    remove(target);
    return 0;

fail:
    editor_test_set_finish_field_choice(-1);
    editor_test_set_discard_choice(-1);
    cleanup_test_preference_root_of(root,
                                    (EditorState *[]){save_state, quit_state,
                                                      selection_state}, 3);
    undo_destroy(save_state->undo);
    undo_destroy(quit_state->undo);
    undo_destroy(selection_state->undo);
    ui_cleanup(&save_state->ui);
    ui_cleanup(&quit_state->ui);
    ui_cleanup(&selection_state->ui);
    editor_widget_test_context_cleanup(&context);
    remove(target);
    return 1;
}

/* An EditorState is about 200 KB and Windows gives a thread 1 MB of stack,
 * so the 3 this test needs come from the heap. */
static int staged_edit_save_and_quit_boundaries(void)
{
    EditorState *states = calloc(3, sizeof(*states));
    int result;

    if (!states) return 1;
    result = staged_edit_save_and_quit_boundaries_with(&states[0], &states[1], &states[2]);
    free(states);
    return result;
}

/*
 * Undo groups: entries pushed between undo_group_begin/end share a number,
 * live in one step (one history slot) and are undone as one step by the
 * editor; amending folds nudges into a step; a full history drops a whole
 * group, never half of one; and a new edit after a partly undone group
 * keeps only the part still applied.
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
    group = undo_group_begin(stack, 3);
    for (int i = 0; i < 3; i++) {
        cmd.entity_index = i;
        undo_push(stack, &cmd);
    }
    undo_group_end(stack);
    if (expect_int("group number", group > 0, 1) != 0 ||
        expect_int("top is the group", undo_top_group(stack), group) != 0 ||
        expect_int("group is one slot", stack->top, 2) != 0 ||
        expect_int("group holds three", stack->steps[1].count, 3) != 0) goto done;

    memset(&later, 0, sizeof(later));
    later.coin.x = 42.0f;
    if (expect_int("amend found", undo_amend_after(stack, group, ENT_COIN, 1, &later), 1) != 0 ||
        expect_int("amend other entity", undo_amend_after(stack, group, ENT_STAR_RED, 1, &later), 0) != 0)
        goto done;
    /* Undo meets the group newest entry first, redo oldest first. */
    if (!undo_pop(stack, &cmd) || expect_int("popped group", cmd.group, group) != 0 ||
        expect_int("newest first", cmd.entity_index, 2) != 0 ||
        expect_int("rest of group still on top", undo_top_group(stack), group) != 0 ||
        expect_int("redo top group", redo_top_group(stack), group) != 0 ||
        !undo_pop(stack, &cmd) ||
        expect_float_value("amended after", cmd.after.coin.x, 42.0f) != 0 ||
        !redo_pop(stack, &cmd) || expect_int("redo oldest undone first", cmd.entity_index, 1) != 0)
        goto done;
    /* A new edit now forgets the undone entry 2 and keeps entries 0 and 1. */
    cmd.group = 0;
    cmd.entity_index = 7;
    undo_push(stack, &cmd);
    if (expect_int("redo cleared", stack->redo_top, 0) != 0 ||
        expect_int("partly undone group kept", stack->steps[1].count, 2) != 0 ||
        expect_int("three steps", stack->top, 3) != 0) goto done;
    undo_clear(stack);

    /* A group asking for more room than a step may hold is refused. */
    if (expect_int("oversized group refused", undo_group_begin(stack, UNDO_GROUP_MAX + 1), 0) != 0)
        goto done;

    /* Fill the history so the oldest step is a group of three. */
    group = undo_group_begin(stack, 3);
    for (int i = 0; i < 3; i++) undo_push(stack, &cmd);
    undo_group_end(stack);
    cmd.group = 0;
    for (int i = 0; i < UNDO_MAX - 1; i++) undo_push(stack, &cmd);
    if (expect_int("history full", stack->top, UNDO_MAX) != 0 ||
        expect_int("oldest is the group", stack->steps[0].group, group) != 0) goto done;
    undo_push(stack, &cmd);                       /* evicts the whole group */
    if (expect_int("group evicted whole", stack->top, UNDO_MAX) != 0 ||
        expect_int("no group remnant", stack->steps[0].group, 0) != 0 ||
        expect_int("oldest step is lone", stack->steps[0].count, 1) != 0) goto done;
    failed = 0;
done:
    undo_destroy(stack);
    return failed;
}

/* Undo (redo = 0) or redo one whole step the way the editor does. */
static int history_step(EditorState *es, int redo)
{
    Command cmd;
    int group;

    if (!(redo ? redo_pop(es->undo, &cmd) : undo_pop(es->undo, &cmd))) return 1;
    group = cmd.group;
    editor_apply_undo_command(es, &cmd, !redo);
    while (group != 0 &&
           (redo ? redo_top_group(es->undo) : undo_top_group(es->undo)) == group &&
           (redo ? redo_pop(es->undo, &cmd) : undo_pop(es->undo, &cmd)))
        editor_apply_undo_command(es, &cmd, !redo);
    return 0;
}

/*
 * A group costs one history slot, made through the editor itself: a drag
 * of 64 selected coins is one step, 256 such steps all fit, and undoing
 * every one then redoing every one lands on exactly the same coins.  When
 * the memory for a group cannot be had, the move, nudge, delete or paste
 * changes nothing.
 */
static int group_steps_cost_one_slot_each(void)
{
    static Selection all[EDITOR_MAX_SELECTION];
    static LevelDef original, final_level, before;
    static EditorState es;
    EditorRect r;
    Command cmd;
    int failed = 1;

    memset(&es, 0, sizeof(es));
    editor_level_init_defaults(&es.level);
    es.undo = undo_create();
    if (!es.undo) return 1;
    es.camera.zoom = 1.0f;
    es.tool = TOOL_SELECT;
    es.level.coin_count = EDITOR_MAX_SELECTION;
    for (int i = 0; i < EDITOR_MAX_SELECTION; i++) {
        es.level.coins[i] = (CoinPlacement){64.0f + (float)(i % 16) * 32.0f,
                                            64.0f + (float)(i / 16) * 32.0f};
        all[i] = (Selection){ENT_COIN, i};
    }
    if (level_is_valid("64 coins", &es.level) != 0) goto done;
    original = es.level;
    editor_set_document_save_point(&es);
    (void)editor_select_items(&es, all, EDITOR_MAX_SELECTION);
    if (expect_int("all selected", editor_selection_count(&es), EDITOR_MAX_SELECTION) != 0)
        goto done;

    /* UNDO_MAX drags of the whole selection: 16 px right, then 8 px back. */
    for (int step = 0; step < UNDO_MAX; step++) {
        if (!editor_entity_bounds(&es.level, ENT_COIN, 0, &r)) goto done;
        drag_by(&es, r.x + 2.0f, r.y + 2.0f, step % 2 ? -8.0f : 16.0f, 0.0f);
        if (step == 0 &&
            (expect_int("64-entity move is one slot", es.undo->top, 1) != 0 ||
             expect_int("one step holds all 64", es.undo->steps[0].count,
                        EDITOR_MAX_SELECTION) != 0)) goto done;
    }
    if (expect_int("256 group steps fit", es.undo->top, UNDO_MAX) != 0 ||
        expect_float_value("drags landed", es.level.coins[0].x,
                           64.0f + 8.0f * (float)(UNDO_MAX / 2)) != 0)
        goto done;
    final_level = es.level;

    for (int step = 0; step < UNDO_MAX; step++)
        if (history_step(&es, 0) != 0) goto done;
    if (expect_int("everything undone", es.undo->top, 0) != 0 ||
        expect_int("everything redoable", es.undo->redo_top, UNDO_MAX) != 0 ||
        expect_int("undo round-trips exactly",
                   memcmp(es.level.coins, original.coins, sizeof(original.coins)), 0) != 0)
        goto done;
    for (int step = 0; step < UNDO_MAX; step++)
        if (history_step(&es, 1) != 0) goto done;
    if (expect_int("everything redone", es.undo->top, UNDO_MAX) != 0 ||
        expect_int("redo round-trips exactly",
                   memcmp(es.level.coins, final_level.coins, sizeof(final_level.coins)), 0) != 0)
        goto done;

    /* One more group drops the oldest step whole. */
    if (!editor_entity_bounds(&es.level, ENT_COIN, 0, &r)) goto done;
    drag_by(&es, r.x + 2.0f, r.y + 2.0f, 16.0f, 0.0f);
    if (expect_int("still full", es.undo->top, UNDO_MAX) != 0 ||
        expect_int("oldest kept whole", es.undo->steps[0].count, EDITOR_MAX_SELECTION) != 0)
        goto done;

    /* No memory for the history: each group action is refused untouched. */
    before = es.level;
    undo_test_limit_allocations(0);
    if (!editor_entity_bounds(&es.level, ENT_COIN, 0, &r)) goto done;
    drag_by(&es, r.x + 2.0f, r.y + 2.0f, 16.0f, 0.0f);
    if (expect_int("refused drag put back",
                   memcmp(es.level.coins, before.coins, sizeof(before.coins)), 0) != 0 ||
        expect_string("refused drag says why", es.status_message,
                      "Move cancelled: cannot allocate undo history") != 0) goto done;
    tools_nudge_selection(&es, 1.0f, 0.0f);
    if (expect_int("refused nudge put back",
                   memcmp(es.level.coins, before.coins, sizeof(before.coins)), 0) != 0 ||
        expect_string("refused nudge says why", es.status_message,
                      "Nothing moved: cannot allocate undo history") != 0) goto done;
    tools_delete_selected(&es);
    if (expect_int("refused delete keeps coins", es.level.coin_count, EDITOR_MAX_SELECTION) != 0 ||
        expect_string("refused delete says why", es.status_message,
                      "Delete cancelled: cannot allocate undo history") != 0) goto done;
    /* A lone entity edit lives inside its step, so it still records. */
    memset(&cmd, 0, sizeof(cmd));
    cmd.type = CMD_MOVE;
    cmd.entity_type = ENT_COIN;
    if (expect_int("lone push needs no memory", undo_push(es.undo, &cmd), 1) != 0) goto done;
    cmd.type = CMD_CONFIG;
    if (expect_int("config push refused", undo_push(es.undo, &cmd), 0) != 0 ||
        expect_int("history kept", es.undo->top, UNDO_MAX) != 0) goto done;
    undo_test_limit_allocations(-1);

    /* Same for a paste: without room for the copies, nothing is added. */
    es.level.coin_count = 2;
    (void)editor_select_items(&es, all, 2);
    editor_copy_selected(&es);
    undo_test_limit_allocations(0);
    editor_paste_clipboard(&es);
    undo_test_limit_allocations(-1);
    if (expect_int("refused paste adds nothing", es.level.coin_count, 2) != 0 ||
        expect_string("refused paste says why", es.status_message,
                      "Paste cancelled: cannot allocate undo history") != 0) goto done;
    failed = 0;
done:
    undo_test_limit_allocations(-1);
    undo_destroy(es.undo);
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

/* Make a file look `seconds` old, as if written that long ago. */
static int backdate_file(const char *path, long seconds)
{
#ifdef _WIN32
    struct _utimbuf times;
    times.actime = times.modtime = time(NULL) - seconds;
    return _utime(path, &times);
#else
    struct utimbuf times;
    times.actime = times.modtime = time(NULL) - seconds;
    return utime(path, &times);
#endif
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

    /* A .toml whose .meta is gone is swept at start-up, once it is old
     * enough; one written moments ago may be another editor's first
     * snapshot, whose .meta comes right after, and stays. */
    if (write_recovery_pair(root, 0xabc, 0, 0) != 0 ||
        write_recovery_pair(root, 0xabd, 0, 0) != 0) goto cleanup;
    {
        char path[EDITOR_PATH_MAX + 64];   /* root, then the file name */
        snprintf(path, sizeof(path), "%s/editor_recovery_%016llx.toml", root, 0xabcull);
        if (backdate_file(path, 10 * 60) != 0) goto cleanup;
    }
    if (editor_set_preference_root(&es, root) != 0 ||
        editor_init_persistence_paths(&es) != 0 ||
        expect_int("orphan swept", recovery_pair_exists(root, 0xabc, ".toml"), 0) != 0 ||
        expect_int("fresh orphan kept", recovery_pair_exists(root, 0xabd, ".toml"), 1) != 0)
        goto cleanup;
    remove_recovery_pair(root, 0xabd);

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

    /* Discard all deletes every copy offered and ends the picker. */
    if (write_recovery_pair(root, 0x2001, 0, 1) != 0 ||
        write_recovery_pair(root, 0x2002, 0, 1) != 0 ||
        write_recovery_pair(root, 0x2003, 0, 1) != 0 ||
        editor_discover_recoveries(&es) != 0 ||
        expect_int("three copies offered", es.recovery_entry_count, 3) != 0) goto cleanup;
    editor_test_set_recovery_choice(EDITOR_RECOVERY_DISCARD_ALL);
    if (expect_int("discard all", editor_choose_recovery(&es), -1) != 0 ||
        expect_int("none left", es.recovery_entry_count, 0) != 0 ||
        expect_string("discard all reported", es.status_message,
                      "Discarded 3 recovery copies") != 0 ||
        expect_int("snapshot gone", recovery_pair_exists(root, 0x2002, ".toml"), 0) != 0 ||
        expect_int("metadata gone", recovery_pair_exists(root, 0x2003, ".meta"), 0) != 0)
        goto cleanup;

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
#if defined(__linux__) || defined(__APPLE__)
    /* Version 3 also records when the owner started.  The same pid with a
     * different start time is another program that got the number after
     * the editor crashed: that copy is offered.  (Linux and macOS read
     * start times in their own ways, so both run this; it is not built for
     * Windows because it needs the parent's pid from getppid().) */
    {
        unsigned long parent = (unsigned long)getppid();
        unsigned long long start = (unsigned long long)editor_process_start_time(parent);
        char path[EDITOR_PATH_MAX + 64];   /* root, then the file name */
        char line[160];
        int ok = 1;
        if (expect_int("start time known", start != 0, 1) != 0) goto cleanup;
        for (int k = 0; k < 2; k++) {
            unsigned long long id = 0x3002ull + (unsigned long long)k;
            snprintf(path, sizeof(path), "%s/editor_recovery_%016llx.toml", root, id);
            ok = ok && write_text_file(path, "name = \"left over\"\n") == 0;
            snprintf(path, sizeof(path), "%s/editor_recovery_%016llx.meta", root, id);
            snprintf(line, sizeof(line), "3\t%016llx\t1700000000\t%lu\t%llu\t-\n",
                     id, parent, start + (unsigned long long)k);
            ok = ok && write_text_file(path, line) == 0;
        }
        if (!ok || editor_discover_recoveries(&es) != 0 ||
            expect_int("crashed and reused-pid copies offered", es.recovery_entry_count, 2) != 0 ||
            expect_int("live v3 copy hidden",
                       es.recovery_entries[0].id != 0x3002 && es.recovery_entries[1].id != 0x3002, 1) != 0 ||
            expect_int("reused pid offered",
                       es.recovery_entries[0].id == 0x3003 || es.recovery_entries[1].id == 0x3003, 1) != 0)
            goto cleanup;
    }
#endif
    result = 0;

cleanup:
    editor_test_set_recovery_choice(-1);
    for (int i = 0; i < EDITOR_MAX_RECOVERY_ENTRIES; i++)
        remove_recovery_pair(root, first_id + (unsigned long long)i);
    remove_recovery_pair(root, 0xabc);
    remove_recovery_pair(root, 0xabd);
    remove_recovery_pair(root, 0x2000);
    for (unsigned long long id = 0x2001; id <= 0x2003; id++) remove_recovery_pair(root, id);
    remove_recovery_pair(root, 0x3000);
    remove_recovery_pair(root, 0x3001);
    remove_recovery_pair(root, 0x3002);
    remove_recovery_pair(root, 0x3003);
    cleanup_test_preference_root(root, &es, 1);
    return result;
}

static int invalid_drafts_do_not_build_unsafe_previews(void)
{
    EditorWidgetTestContext context;
    EditorState es = {0};
    if (editor_widget_test_context_init(&context) != 0) return 1;
    ui_init(&es.ui, context.font);
    /* Labels draw from the font atlas: repeated labels leave nothing behind. */
    for (int i = 0; i < 100; i++) ui_label(&es.ui, 0, 0, "repeated label");
    if (getenv("MANGO_BENCHMARK")) {
        const int rounds = 2000;
        double start = GetTime();
        for (int i = 0; i < rounds; i++) ui_label(&es.ui, 0, 0, "repeated label");
        rlDrawRenderBatchActive();
        printf("text benchmark: %d labels %.3fms\n", rounds, (GetTime() - start) * 1000.0);
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
