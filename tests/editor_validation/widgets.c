/* Included by tests/editor_validation_test.c. Not compiled alone: the functions are
 * static and use the helpers in that file.
 */
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

    /* Hearts: the same through the finish dialog, capped at MAX_HEARTS
     * (0 is allowed: it means the default)... */
    es.level.initial_hearts = 2;
    config_frame(&es, NULL, 1, CFG_X + 80, CFG_HEARTS_Y + 4);
    if (expect_int("hearts active", es.ui.active_id, 9006) != 0) goto done;
    strcpy(es.ui.edit_buf, "7");
    es.ui.edit_cursor = 1;
    editor_test_set_finish_field_choice(1);
    editor_handle_event(&es, &event);
    if (expect_int("applied hearts clamp", es.level.initial_hearts, 3) != 0) goto done;

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
/*
 * A pasted copy lands where dragging the original 24 px right and down
 * would put it: one shared helper (editor_move_placement) moves both, so a
 * bird's height or a saw's y can no longer follow a drag but not a paste.
 */
static int paste_moves_like_a_drag(void)
{
    static const EntityType types[] = { ENT_BIRD, ENT_CIRCULAR_SAW, ENT_BRIDGE, ENT_COIN };
    EditorState es = {0};
    int failed = 1;

    editor_level_init_defaults(&es.level);
    es.undo = undo_create();
    if (!es.undo) return 1;
    es.level.bird_count = 1;
    es.level.birds[0] = (BirdPlacement){200.0f, 100.0f, 30.0f, 150.0f, 250.0f, 0};
    es.level.circular_saw_count = 1;
    es.level.circular_saws[0] = (CircularSawPlacement){300.0f, 120.0f, 280.0f, 340.0f, 1};
    es.level.bridge_count = 1;
    es.level.bridges[0] = (BridgePlacement){500.0f, 150.0f, 3};
    es.level.coin_count = 1;
    es.level.coins[0] = (CoinPlacement){100.0f, 100.0f};
    if (level_is_valid("paste-like-drag base", &es.level) != 0) goto done;

    for (size_t t = 0; t < sizeof(types) / sizeof(types[0]); t++) {
        EntityType type = types[t];
        PlacementData original = editor_snapshot_entity(&es.level, type, 0);
        PlacementData dragged = editor_move_placement(type, &original, 24.0f, 24.0f);
        PlacementData pasted;
        editor_clamp_placement(&es.level, type, &dragged);
        es.selection.type = type;
        es.selection.index = 0;
        editor_copy_selected(&es);
        editor_paste_clipboard(&es);
        if (expect_int("one copy", editor_entity_count(&es.level, type), 2) != 0) goto done;
        pasted = editor_snapshot_entity(&es.level, type, 1);
        if (expect_int(editor_entity_type_name(type),
                       memcmp(&pasted, &dragged, sizeof(pasted)) == 0, 1) != 0)
            goto done;
    }
    if (expect_float_value("bird height moved", es.level.birds[1].base_y, 124.0f) != 0)
        goto done;
    failed = 0;
done:
    undo_destroy(es.undo);
    return failed;
}

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
