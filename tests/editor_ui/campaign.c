/* Included by tests/editor_ui_test.c. Not compiled alone: the functions are
 * static and use the helpers in that file.
 */
/* A scratch game folder: levels/a, b, c chained and listed in order, and
 * levels/d not listed (see enter_ui_campaign). */
#define UI_CAMPAIGN_ROOT TEST_OUT "ui-campaign-root"
static int write_ui_campaign_level(const char *path, const char *name, const char *next)
{
    char text[512];
    snprintf(text, sizeof(text),
             "format_version = 1\nname = \"%s\"\nscreen_count = 4\n\n"
             "[last_star]\nx = 300.0\ny = 200.0\n%s%s%s", name,
             next[0] ? "next_phase = \"" : "", next, next[0] ? "\"\n" : "");
    return write_text_file(path, text);
}

/*
 * Write the scratch campaign afresh, so each test starts from the same
 * files whatever the one before it saved, and make it the working folder
 * (the view names levels relative to it, as the game does).  The folder
 * the test came from goes in cwd, to return to.  Returns 0, or -1 with the
 * working folder unchanged.
 */
static int enter_ui_campaign(char *cwd, size_t cwd_size)
{
    test_make_folder(UI_CAMPAIGN_ROOT);
    test_make_folder(UI_CAMPAIGN_ROOT "/levels");
    test_make_folder(UI_CAMPAIGN_ROOT "/levels/campaigns");
    if (write_ui_campaign_level(UI_CAMPAIGN_ROOT "/levels/a.toml", "Alpha", "levels/b.toml") ||
        write_ui_campaign_level(UI_CAMPAIGN_ROOT "/levels/b.toml", "Bravo", "levels/c.toml") ||
        write_ui_campaign_level(UI_CAMPAIGN_ROOT "/levels/c.toml", "Charlie", "") ||
        write_ui_campaign_level(UI_CAMPAIGN_ROOT "/levels/d.toml", "Delta", "") ||
        write_text_file(UI_CAMPAIGN_ROOT "/levels/campaigns/main.toml",
                        "format_version = 1\nlevels = [\"levels/a.toml\", "
                        "\"levels/b.toml\", \"levels/c.toml\"]\n"))
        return -1;
    if (test_working_folder(cwd, cwd_size) != 0) return -1;
    return test_change_folder(UI_CAMPAIGN_ROOT);
}

/* The view's rows as their files' letters, in order: "abc" for a.toml,
 * b.toml and c.toml ("" while the view is closed). */
static const char *campaign_rows(const EditorState *es)
{
    static char letters[16];
    const CampaignCatalog *view = editor_campaign_entries(es);
    size_t count = 0;

    for (; view && count < view->count && count < sizeof(letters) - 1; count++)
        letters[count] = view->levels[count].path[sizeof("levels/") - 1];
    letters[count] = '\0';
    return letters;
}

/* Drop `count` paths on the window and run the frame that sees them. */
static void drop_frame(EditorState *es, const char *const *paths, unsigned int count)
{
    editor_test_drop_files(paths, count);
    ui_frame(es, NEUTRAL_X, NEUTRAL_Y);
}

/*
 * A level file dropped on the window opens like a recent file: it asks
 * about unsaved changes, closes the Campaign view first (whose unsaved
 * edits need a second drop to discard), and anything but exactly one
 * .toml file is turned away with a status message.
 */
static int dropped_level_opens_like_a_recent_file(void)
{
    int failed = 0;
    EditorState es;
    char cwd[4096];
    int moved = 0;
    static char long_path[EDITOR_DROP_TEST_LONG + 1];
    const char *one[] = {"levels/a.toml"};
    /* No file of this name exists in any letter case: Windows and macOS
     * would open levels/b.toml for "levels/B.TOML". */
    const char *upper[] = {"levels/missing.TOML"};
    const char *two[] = {"levels/a.toml", "levels/b.toml"};
    const char *art[] = {"levels/a.png"};
    const char *delta[] = {"levels/d.toml"};
    const char *too_long[] = {long_path};
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(enter_ui_campaign(cwd, sizeof(cwd)) == 0);
    moved = 1;

    drop_frame(&es, one, 1);
    CHECK(strcmp(es.file_path, "levels/a.toml") == 0);
    CHECK(strcmp(es.level.name, "Alpha") == 0);
    CHECK(strstr(es.status_message, "Loaded levels/a.toml") != NULL);

    /* The extension is matched in any case: this one reached the loader,
     * which then could not find it. */
    drop_frame(&es, upper, 1);
    CHECK(strcmp(es.file_path, "levels/a.toml") == 0);
    CHECK(strstr(es.status_message, "Load failed") != NULL);

    /* Two files, or one that is not a level, change nothing. */
    drop_frame(&es, two, 2);
    CHECK(strcmp(es.status_message, "Drop one .toml level to open it") == 0);
    drop_frame(&es, art, 1);
    CHECK(strcmp(es.status_message, "Drop one .toml level to open it") == 0);
    CHECK(strcmp(es.file_path, "levels/a.toml") == 0);

    /* A path as long as raylib's buffer may have been cut short. */
    memset(long_path, 'x', sizeof(long_path) - 1);
    memcpy(long_path + sizeof(long_path) - 1 - 5, ".toml", 5);
    drop_frame(&es, too_long, 1);
    CHECK(strstr(es.status_message, "too long") != NULL);
    CHECK(strcmp(es.file_path, "levels/a.toml") == 0);

    /* Unsaved changes ask first: Cancel keeps them, Discard opens. */
    es.level.screen_count = 5;
    editor_refresh_dirty(&es);
    CHECK(es.modified);
    editor_test_set_discard_choice(0);
    drop_frame(&es, delta, 1);
    CHECK(strcmp(es.file_path, "levels/a.toml") == 0 && es.level.screen_count == 5);
    CHECK(strstr(es.status_message, "cancelled") != NULL);
    editor_test_set_discard_choice(1);
    drop_frame(&es, delta, 1);
    CHECK(strcmp(es.file_path, "levels/d.toml") == 0 && !es.modified);
    CHECK(strcmp(es.level.name, "Delta") == 0);

    /* The Campaign view closes for the dropped level... */
    CHECK(editor_campaign_open(&es, NULL) == 0 && es.campaign != NULL);
    drop_frame(&es, one, 1);
    CHECK(es.campaign == NULL && strcmp(es.file_path, "levels/a.toml") == 0);
    /* ...but with unsaved campaign edits only on the second drop. */
    CHECK(editor_campaign_open(&es, NULL) == 0);
    CHECK(editor_campaign_move(&es, 2, -1) == 0);
    drop_frame(&es, delta, 1);
    CHECK(es.campaign != NULL && strcmp(es.file_path, "levels/a.toml") == 0);
    CHECK(strstr(es.status_message, "drop the level again") != NULL);
    drop_frame(&es, delta, 1);
    CHECK(es.campaign == NULL && strcmp(es.file_path, "levels/d.toml") == 0);

    /* A playtest owns the editor until it ends. */
    es.playing = 1;
    editor_test_drop_files(one, 1);
    editor_handle_dropped_files(&es);
    es.playing = 0;
    CHECK(strcmp(es.file_path, "levels/d.toml") == 0);
    CHECK(strstr(es.status_message, "Playtest running") != NULL);
done:
    editor_test_drop_files(NULL, 0);
    editor_test_set_discard_choice(-1);
    if (moved && test_change_folder(cwd) != 0) failed = 1;
    close_editor(&es);
    return failed;
}

/*
 * A shipped level still names its old WAV loop: its bytes key players'
 * ghosts and Continue points.  The editor opens it with the QOA file the
 * game plays and without unsaved changes, so nothing asks to save it and
 * the file only changes (to the .qoa name) when someone edits and saves.
 */
static int shipped_wav_loop_opens_unmodified(void)
{
    int failed = 0;
    EditorState es;
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(editor_load_level(&es, "levels/01_lugio_01.toml") == 0);
    CHECK(strcmp(es.level.music_path, "assets/sounds/levels/lava.qoa") == 0);
    CHECK(!es.modified);
    editor_refresh_dirty(&es);
    CHECK(!es.modified);
done:
    close_editor(&es);
    return failed;
}

/* A second level dropped while the unsaved-changes prompt is up, then
 * Discard chosen in that prompt (Left from Save, Enter). */
static void drop_bravo_during_the_prompt(void)
{
    const char *bravo[] = {"levels/b.toml"};
    editor_test_drop_files(bravo, 1);
    push_key(KEY_LEFT, 0);
    push_key(KEY_ENTER, 0);
    dialog_choice_test_set_poll_hook(NULL);
}

/*
 * raylib lends its own list of dropped paths, and the next drop frees and
 * replaces it.  The unsaved-changes prompt polls the window every frame,
 * so a drop can land while it is up: the level dropped first still opens,
 * the list is not released twice, and the later drop opens next frame.
 */
static int drop_during_the_prompt_waits_its_turn(void)
{
    int failed = 0;
    EditorState es;
    char cwd[4096];
    int moved = 0;
    int misuses = editor_test_drop_misuses();
    const char *delta[] = {"levels/d.toml"};
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(enter_ui_campaign(cwd, sizeof(cwd)) == 0);
    moved = 1;
    CHECK(editor_load_level(&es, "levels/a.toml") == 0);

    es.level.screen_count = 5;
    editor_refresh_dirty(&es);
    CHECK(es.modified);
    editor_test_set_discard_choice(-1);   /* the real prompt answers */
    dialog_choice_test_set_poll_hook(drop_bravo_during_the_prompt);
    drop_frame(&es, delta, 1);
    CHECK(strcmp(es.file_path, "levels/d.toml") == 0 && !es.modified);
    CHECK(editor_test_drop_misuses() == misuses);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(strcmp(es.file_path, "levels/b.toml") == 0);
    CHECK(editor_test_drop_misuses() == misuses);
done:
    dialog_choice_test_set_poll_hook(NULL);
    editor_test_drop_files(NULL, 0);
    input_clear();
    if (moved && test_change_folder(cwd) != 0) failed = 1;
    close_editor(&es);
    return failed;
}

/* Click button `index` (a CampaignButton) of the Campaign view's bottom row. */
static void click_campaign_button(EditorState *es, int index)
{
    click_frame(es, CAMPAIGN_VIEW_X + index * CAMPAIGN_BUTTON_STEP + 10,
                CAMPAIGN_BUTTONS_Y + 10);
}

/*
 * The Campaign view as a designer uses it: Ctrl+M covers the canvas (a
 * click there places nothing), a row click and Up reorder, typing in a
 * name field renames the level, Link in order and Save write the files
 * the game's own loader then reads, and Esc returns to the level.
 */
static int campaign_view_reorders_renames_and_saves(void)
{
    int failed = 0;
    EditorState es;
    char cwd[4096];
    int moved = 0;
    CampaignCatalog reloaded = {0};
    const CampaignCatalog *view;
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(enter_ui_campaign(cwd, sizeof(cwd)) == 0);
    moved = 1;

    es.tool = TOOL_PLACE;
    es.palette_type = ENT_COIN;
    key_frame(&es, KEY_M, INPUT_CTRL);
    CHECK(es.campaign != NULL);
    view = editor_campaign_entries(&es);
    CHECK(view && view->count == 3);
    click_frame(&es, 400, 500);                      /* the canvas sleeps */
    CHECK(es.level.coin_count == 0);
    key_frame(&es, KEY_DELETE, 0);                   /* level keys too */
    CHECK(strstr(es.status_message, "Campaign view") != NULL);

    /* Select row 3 (c) and move it up. */
    click_frame(&es, CAMPAIGN_VIEW_X + 20, CAMPAIGN_ROWS_Y + 2 * CAMPAIGN_ROW_H + 8);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UP);
    CHECK(strcmp(view->levels[1].path, "levels/c.toml") == 0);
    CHECK(editor_campaign_problem(&es)[0] != '\0' || !view->levels[0].available);

    /* Rename the first level: the field holds its name, typing appends. */
    click_frame(&es, CAMPAIGN_NAME_X + 10, CAMPAIGN_ROWS_Y + 8);
    CHECK(es.ui.active_id == CAMPAIGN_NAME_FIELD_ID);
    push_text("!");
    key_frame(&es, KEY_ENTER, 0);
    CHECK(strcmp(view->levels[0].level.name, "Alpha!") == 0);
    CHECK(strcmp(view->levels[0].display_name, "Alpha!") == 0);

    /* Link in order, then Save. */
    click_campaign_button(&es, CAMPAIGN_BUTTON_LINK);
    CHECK(editor_campaign_problem(&es)[0] == '\0');
    click_campaign_button(&es, CAMPAIGN_BUTTON_SAVE);
    CHECK(strstr(es.status_message, "Campaign saved") != NULL);
    CHECK(campaign_catalog_load(CAMPAIGN_MANIFEST_PATH, &reloaded) == 0);
    CHECK(reloaded.count == 3 && strcmp(reloaded.levels[1].path, "levels/c.toml") == 0);
    CHECK(strcmp(reloaded.levels[0].display_name, "Alpha!") == 0);
    CHECK(reloaded.levels[0].available && reloaded.levels[1].available &&
          reloaded.levels[2].available);

    /* Ctrl+S saves the campaign even while a name is being typed (the
     * name is applied first), and never saves the level instead. */
    click_frame(&es, CAMPAIGN_NAME_X + 10, CAMPAIGN_ROWS_Y + CAMPAIGN_ROW_H + 8);
    CHECK(es.ui.active_id == CAMPAIGN_NAME_FIELD_ID + 1);
    push_text("?");
    key_frame(&es, KEY_S, INPUT_CTRL);
    CHECK(es.ui.active_id == 0);
    CHECK(strstr(es.status_message, "Campaign saved") != NULL);
    CHECK(campaign_catalog_load(CAMPAIGN_MANIFEST_PATH, &reloaded) == 0);
    CHECK(strcmp(reloaded.levels[1].display_name, "Charlie?") == 0);
    /* Esc first cancels a name being typed, then closes. */
    click_frame(&es, CAMPAIGN_NAME_X + 10, CAMPAIGN_ROWS_Y + 8);
    CHECK(es.ui.active_id == CAMPAIGN_NAME_FIELD_ID);
    key_frame(&es, KEY_ESCAPE, 0);
    CHECK(es.ui.active_id == 0 && es.campaign != NULL);

    /* A name typed but not applied is unsaved work: quitting only warns. */
    click_frame(&es, CAMPAIGN_NAME_X + 10, CAMPAIGN_ROWS_Y + 8);
    push_text("#");
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(editor_campaign_unsaved(&es) == 1);
    push_event(INPUT_QUIT, 0, 0, 0, 0);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(es.running && es.campaign != NULL);
    CHECK(strstr(es.status_message, "Unsaved campaign changes") != NULL);
    key_frame(&es, KEY_ESCAPE, 0);                   /* drop the typing */
    CHECK(editor_campaign_unsaved(&es) == 0);

    /* Revert (asked twice, like Close) puts back what is on disk. */
    click_frame(&es, CAMPAIGN_VIEW_X + 20, CAMPAIGN_ROWS_Y + 2 * CAMPAIGN_ROW_H + 8);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UP);  /* b above c... */
    CHECK(strcmp(view->levels[1].path, "levels/b.toml") == 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_REVERT);
    CHECK(strstr(es.status_message, "Revert again") != NULL);
    view = editor_campaign_entries(&es);
    CHECK(strcmp(view->levels[1].path, "levels/b.toml") == 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_REVERT);
    view = editor_campaign_entries(&es);
    CHECK(view && strcmp(view->levels[1].path, "levels/c.toml") == 0);
    CHECK(editor_campaign_unsaved(&es) == 0);
    CHECK(strstr(es.status_message, "reverted") != NULL);

    /* Esc goes back to the level. */
    key_frame(&es, KEY_ESCAPE, 0);
    CHECK(es.campaign == NULL);
    click_frame(&es, 400, 500);
    CHECK(es.level.coin_count == 1);
done:
    campaign_catalog_cleanup(&reloaded);
    if (moved && test_change_folder(cwd) != 0) failed = 1;
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/* Click row `row` of the Campaign view on its file name, which selects it. */
static void click_campaign_row(EditorState *es, int row)
{
    click_frame(es, CAMPAIGN_VIEW_X + 20, CAMPAIGN_ROWS_Y + row * CAMPAIGN_ROW_H + 8);
}

/* Click the name field of row `row`, which starts typing in it. */
static void click_campaign_name(EditorState *es, int row)
{
    click_frame(es, CAMPAIGN_NAME_X + 10, CAMPAIGN_ROWS_Y + row * CAMPAIGN_ROW_H + 8);
}

/*
 * The Campaign view has a history of its own: every edit made there (move,
 * remove, add, relink) is one step that Ctrl+Z takes back and Ctrl+Y or
 * Ctrl+Shift+Z makes again, and a new edit forgets what was undone.
 */
static int campaign_undo_and_redo_replay_each_edit(void)
{
    int failed = 0;
    EditorState es;
    char cwd[4096];
    int moved = 0;
    const CampaignCatalog *view;
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(enter_ui_campaign(cwd, sizeof(cwd)) == 0);
    moved = 1;
    key_frame(&es, KEY_M, INPUT_CTRL);
    view = editor_campaign_entries(&es);
    CHECK(view && strcmp(campaign_rows(&es), "abc") == 0);

    /* Nothing done yet: the keys say so and change nothing. */
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(strstr(es.status_message, "nothing to undo") != NULL);
    key_frame(&es, KEY_Y, INPUT_CTRL);
    CHECK(strstr(es.status_message, "nothing to redo") != NULL);
    CHECK(strcmp(campaign_rows(&es), "abc") == 0);

    /* Move: select c and press Up; each of the three keys does its part. */
    click_campaign_row(&es, 2);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UP);
    CHECK(strcmp(campaign_rows(&es), "acb") == 0);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(strcmp(campaign_rows(&es), "abc") == 0);
    CHECK(strstr(es.status_message, "Undid: moved levels/c.toml") != NULL);
    key_frame(&es, KEY_Y, INPUT_CTRL);
    CHECK(strcmp(campaign_rows(&es), "acb") == 0);
    CHECK(strstr(es.status_message, "Redid: moved levels/c.toml") != NULL);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    key_frame(&es, KEY_Z, INPUT_CTRL | INPUT_SHIFT);
    CHECK(strcmp(campaign_rows(&es), "acb") == 0);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(strcmp(campaign_rows(&es), "abc") == 0);
    CHECK(editor_campaign_undo_steps(&es) == 0 && editor_campaign_redo_steps(&es) == 1);

    /* Remove b: undo reads its file again and puts it back where it was. */
    click_campaign_row(&es, 1);
    click_campaign_button(&es, CAMPAIGN_BUTTON_REMOVE);
    CHECK(strcmp(campaign_rows(&es), "ac") == 0);
    CHECK(editor_campaign_redo_steps(&es) == 0);      /* a new edit: no redo */
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(strcmp(campaign_rows(&es), "abc") == 0);
    CHECK(view->levels[1].loaded && strcmp(view->levels[1].level.name, "Bravo") == 0);
    CHECK(strcmp(view->levels[1].level.next_phase, "levels/c.toml") == 0);
    CHECK(view->levels[0].available && view->levels[1].available && view->levels[2].available);
    key_frame(&es, KEY_Y, INPUT_CTRL);
    CHECK(strcmp(campaign_rows(&es), "ac") == 0);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(strcmp(campaign_rows(&es), "abc") == 0);

    /* Add d (the picker's answer, given straight to the view). */
    CHECK(editor_campaign_add(&es, "levels/d.toml") == 0);
    CHECK(strcmp(campaign_rows(&es), "abcd") == 0);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(strcmp(campaign_rows(&es), "abc") == 0);
    CHECK(editor_campaign_unsaved(&es) == 0);
    key_frame(&es, KEY_Y, INPUT_CTRL);
    CHECK(strcmp(campaign_rows(&es), "abcd") == 0);
    CHECK(view->levels[3].loaded && strcmp(view->levels[3].level.name, "Delta") == 0);

    /* Link in order must remember the old links to be undone.  Without
     * the memory for them it changes none, and records nothing. */
    editor_campaign_test_fail_history_allocation(1);
    click_campaign_button(&es, CAMPAIGN_BUTTON_LINK);
    editor_campaign_test_fail_history_allocation(0);
    CHECK(strstr(es.status_message, "Not linked: out of memory") != NULL);
    CHECK(view->levels[2].level.next_phase[0] == '\0');
    CHECK(editor_campaign_undo_steps(&es) == 1);
    /* With it, c is linked to d, and that is one step too. */
    click_campaign_button(&es, CAMPAIGN_BUTTON_LINK);
    CHECK(strcmp(view->levels[2].level.next_phase, "levels/d.toml") == 0);
    CHECK(editor_campaign_undo_steps(&es) == 2);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(view->levels[2].level.next_phase[0] == '\0');
    CHECK(!view->levels[2].available);
    key_frame(&es, KEY_Y, INPUT_CTRL);
    CHECK(strcmp(view->levels[2].level.next_phase, "levels/d.toml") == 0);
    CHECK(view->levels[2].available);

    /* A new edit after an undo forgets the undone step. */
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(editor_campaign_redo_steps(&es) == 1);
    click_campaign_row(&es, 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_DOWN);
    CHECK(strcmp(campaign_rows(&es), "bacd") == 0);
    CHECK(editor_campaign_redo_steps(&es) == 0);
    key_frame(&es, KEY_Y, INPUT_CTRL);
    CHECK(strstr(es.status_message, "nothing to redo") != NULL);
    CHECK(strcmp(campaign_rows(&es), "bacd") == 0);

    /* All the way back is the campaign the files hold. */
    key_frame(&es, KEY_Z, INPUT_CTRL);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(strcmp(campaign_rows(&es), "abc") == 0);
    CHECK(editor_campaign_undo_steps(&es) == 0);
    CHECK(editor_campaign_unsaved(&es) == 0);
done:
    editor_campaign_test_fail_history_allocation(0);
    if (moved && test_change_folder(cwd) != 0) failed = 1;
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/*
 * A rename is one step for each name applied, however many keys it took
 * and however it was applied (Enter, a click elsewhere, or the undo key
 * itself).  Typing alone is not a step.
 */
static int campaign_rename_is_one_undo_step(void)
{
    int failed = 0;
    EditorState es;
    char cwd[4096];
    int moved = 0;
    const CampaignCatalog *view;
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(enter_ui_campaign(cwd, sizeof(cwd)) == 0);
    moved = 1;
    key_frame(&es, KEY_M, INPUT_CTRL);
    view = editor_campaign_entries(&es);
    CHECK(view && view->count == 3);

    /* Three keystrokes in three frames, then Enter. */
    click_campaign_name(&es, 0);
    CHECK(es.ui.active_id == CAMPAIGN_NAME_FIELD_ID);
    push_text("1");
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    push_text("2");
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(editor_campaign_undo_steps(&es) == 0);
    push_text("3");
    key_frame(&es, KEY_ENTER, 0);
    CHECK(strcmp(view->levels[0].level.name, "Alpha123") == 0);
    CHECK(editor_campaign_undo_steps(&es) == 1);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(strcmp(view->levels[0].level.name, "Alpha") == 0);
    CHECK(strcmp(view->levels[0].display_name, "Alpha") == 0);
    CHECK(editor_campaign_unsaved(&es) == 0);
    key_frame(&es, KEY_Y, INPUT_CTRL);
    CHECK(strcmp(view->levels[0].level.name, "Alpha123") == 0);
    CHECK(strcmp(view->levels[0].display_name, "Alpha123") == 0);
    CHECK(editor_campaign_unsaved(&es) == 1);

    /* Enter on a name left as it was is no edit, so no step. */
    click_campaign_name(&es, 2);
    key_frame(&es, KEY_ENTER, 0);
    CHECK(es.ui.active_id == 0 && editor_campaign_undo_steps(&es) == 1);

    /* A click in another row's field applies the name being typed. */
    click_campaign_name(&es, 1);
    push_text("?");
    click_campaign_name(&es, 2);
    CHECK(es.ui.active_id == CAMPAIGN_NAME_FIELD_ID + 2);
    CHECK(strcmp(view->levels[1].level.name, "Bravo?") == 0);
    CHECK(editor_campaign_undo_steps(&es) == 2);

    /* Ctrl+Z while typing applies the typed name as a step and takes that
     * step back at once: the name is as before, and Ctrl+Y brings the
     * typed one. */
    push_text("#");
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.ui.active_id == 0);
    CHECK(strcmp(view->levels[2].level.name, "Charlie") == 0);
    CHECK(editor_campaign_undo_steps(&es) == 2 && editor_campaign_redo_steps(&es) == 1);
    key_frame(&es, KEY_Y, INPUT_CTRL);
    CHECK(strcmp(view->levels[2].level.name, "Charlie#") == 0);

    /* Each name comes back in turn, and none of this was a level edit. */
    key_frame(&es, KEY_Z, INPUT_CTRL);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(strcmp(view->levels[1].level.name, "Bravo") == 0);
    CHECK(strcmp(view->levels[0].level.name, "Alpha123") == 0);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(strcmp(view->levels[0].level.name, "Alpha") == 0);
    CHECK(editor_campaign_unsaved(&es) == 0);
    CHECK(es.undo->top == 0 && es.undo->redo_top == 0 && !es.modified);
done:
    if (moved && test_change_folder(cwd) != 0) failed = 1;
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/*
 * The history holds the latest CAMPAIGN_HISTORY_MAX steps: one edit more
 * drops the oldest, which can then no longer be undone.
 */
static int campaign_history_keeps_the_latest_steps(void)
{
    int failed = 0;
    EditorState es;
    char cwd[4096];
    int moved = 0;
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(enter_ui_campaign(cwd, sizeof(cwd)) == 0);
    moved = 1;
    key_frame(&es, KEY_M, INPUT_CTRL);
    CHECK(es.campaign != NULL);

    /* One move more than fits: b and c change places each time. */
    for (int i = 0; i < CAMPAIGN_HISTORY_MAX + 1; i++)
        CHECK(editor_campaign_move(&es, 1, +1) == 0);
    CHECK(strcmp(campaign_rows(&es), "acb") == 0);   /* an odd number of swaps */
    CHECK(editor_campaign_undo_steps(&es) == CAMPAIGN_HISTORY_MAX);

    /* Every step kept can be undone; the first move is beyond reach. */
    for (int i = 0; i < CAMPAIGN_HISTORY_MAX; i++)
        CHECK(editor_campaign_undo(&es) == 1);
    CHECK(editor_campaign_undo(&es) == 0);
    CHECK(strstr(es.status_message, "nothing to undo") != NULL);
    CHECK(strcmp(campaign_rows(&es), "acb") == 0);
    CHECK(editor_campaign_unsaved(&es) == 1);
    CHECK(editor_campaign_redo_steps(&es) == CAMPAIGN_HISTORY_MAX);
    for (int i = 0; i < CAMPAIGN_HISTORY_MAX; i++)
        CHECK(editor_campaign_redo(&es) == 1);
    CHECK(editor_campaign_redo(&es) == 0);
    CHECK(strcmp(campaign_rows(&es), "acb") == 0);
done:
    if (moved && test_change_folder(cwd) != 0) failed = 1;
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/*
 * A row that left the list comes back by reading its level file again.  A
 * name changed and not yet saved is in no file, so the step keeps it; and
 * while the file cannot be read, the step is refused and the list stays as
 * it is.
 */
static int campaign_undo_reads_the_level_file_again(void)
{
    int failed = 0;
    EditorState es;
    char cwd[4096];
    int moved = 0;
    const CampaignCatalog *view;
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(enter_ui_campaign(cwd, sizeof(cwd)) == 0);
    moved = 1;
    key_frame(&es, KEY_M, INPUT_CTRL);
    view = editor_campaign_entries(&es);
    CHECK(view && view->count == 3);

    /* Rename b without saving, then remove it. */
    click_campaign_name(&es, 1);
    push_text("2");
    key_frame(&es, KEY_ENTER, 0);
    CHECK(strcmp(view->levels[1].level.name, "Bravo2") == 0);
    click_campaign_row(&es, 1);
    click_campaign_button(&es, CAMPAIGN_BUTTON_REMOVE);
    CHECK(strcmp(campaign_rows(&es), "ac") == 0);

    /* Its file disappears: undo says so and changes nothing. */
    CHECK(remove("levels/b.toml") == 0);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(strstr(es.status_message, "Cannot undo: levels/b.toml no longer loads") != NULL);
    CHECK(strcmp(campaign_rows(&es), "ac") == 0);
    CHECK(editor_campaign_undo_steps(&es) == 2 && editor_campaign_redo_steps(&es) == 0);
    /* A file that is there but broken is refused the same way.  A second
     * refusal asks whether to skip the step; "Keep the step" (the
     * dialog's default, and the answer these frames give) changes nothing. */
    CHECK(write_text_file("levels/b.toml", "format_version = \n") == 0);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(strstr(es.status_message, "no longer loads") != NULL);
    CHECK(strcmp(campaign_rows(&es), "ac") == 0);
    CHECK(editor_campaign_undo_steps(&es) == 2 && editor_campaign_redo_steps(&es) == 0);

    /* The file is back: the same undo now works, and the row has the name
     * it showed, which the file never held. */
    CHECK(write_ui_campaign_level("levels/b.toml", "Bravo", "levels/c.toml") == 0);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(strcmp(campaign_rows(&es), "abc") == 0);
    CHECK(view->levels[1].loaded && strcmp(view->levels[1].level.name, "Bravo2") == 0);
    CHECK(editor_campaign_unsaved(&es) == 1);
    key_frame(&es, KEY_Z, INPUT_CTRL);               /* ...and then the rename */
    CHECK(strcmp(view->levels[1].level.name, "Bravo") == 0);
    CHECK(editor_campaign_unsaved(&es) == 0);

    /* Redoing an Add reads the file too. */
    CHECK(editor_campaign_add(&es, "levels/d.toml") == 0);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(remove("levels/d.toml") == 0);
    key_frame(&es, KEY_Y, INPUT_CTRL);
    CHECK(strstr(es.status_message, "Cannot redo: levels/d.toml no longer loads") != NULL);
    CHECK(strcmp(campaign_rows(&es), "abc") == 0);
    CHECK(editor_campaign_redo_steps(&es) == 1);
done:
    if (moved && test_change_folder(cwd) != 0) failed = 1;
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/* A status no command writes, to tell "the click did nothing" from a
 * command that ran and reported. */
#define UNTOUCHED_STATUS "status left alone"

/*
 * The Undo and Redo buttons do what Ctrl+Z and Ctrl+Y do, for every kind of
 * step, and say which step it was.  With nothing to undo or redo a button
 * is drawn disabled and a click on it does nothing at all.
 */
static int campaign_undo_and_redo_buttons_replay_each_edit(void)
{
    int failed = 0;
    EditorState es;
    char cwd[4096];
    int moved = 0;
    const CampaignCatalog *view;
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(enter_ui_campaign(cwd, sizeof(cwd)) == 0);
    moved = 1;
    key_frame(&es, KEY_M, INPUT_CTRL);
    view = editor_campaign_entries(&es);
    CHECK(view && strcmp(campaign_rows(&es), "abc") == 0);

    /* Nothing done yet: both buttons are disabled.  The keys would say
     * "nothing to undo"; a disabled button says nothing. */
    editor_set_status(&es, UNTOUCHED_STATUS);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    click_campaign_button(&es, CAMPAIGN_BUTTON_REDO);
    CHECK(strcmp(es.status_message, UNTOUCHED_STATUS) == 0);
    CHECK(strcmp(campaign_rows(&es), "abc") == 0);
    CHECK(editor_campaign_undo_steps(&es) == 0 && editor_campaign_redo_steps(&es) == 0);
    CHECK(editor_campaign_unsaved(&es) == 0);

    /* Move: c goes up, Undo brings it down, Redo takes it up again. */
    click_campaign_row(&es, 2);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UP);
    CHECK(strcmp(campaign_rows(&es), "acb") == 0);
    editor_set_status(&es, UNTOUCHED_STATUS);
    click_campaign_button(&es, CAMPAIGN_BUTTON_REDO);     /* still disabled */
    CHECK(strcmp(es.status_message, UNTOUCHED_STATUS) == 0);
    CHECK(strcmp(campaign_rows(&es), "acb") == 0);
    CHECK(editor_campaign_undo_steps(&es) == 1 && editor_campaign_redo_steps(&es) == 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(strcmp(campaign_rows(&es), "abc") == 0);
    CHECK(strcmp(es.status_message, "Undid: moved levels/c.toml") == 0);
    CHECK(editor_campaign_undo_steps(&es) == 0 && editor_campaign_redo_steps(&es) == 1);
    editor_set_status(&es, UNTOUCHED_STATUS);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);     /* disabled again */
    CHECK(strcmp(es.status_message, UNTOUCHED_STATUS) == 0);
    CHECK(strcmp(campaign_rows(&es), "abc") == 0);
    CHECK(editor_campaign_undo_steps(&es) == 0 && editor_campaign_redo_steps(&es) == 1);
    click_campaign_button(&es, CAMPAIGN_BUTTON_REDO);
    CHECK(strcmp(campaign_rows(&es), "acb") == 0);
    CHECK(strcmp(es.status_message, "Redid: moved levels/c.toml") == 0);
    /* The buttons and the keys walk the same history. */
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(strcmp(campaign_rows(&es), "abc") == 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_REDO);
    CHECK(strcmp(campaign_rows(&es), "acb") == 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(strcmp(campaign_rows(&es), "abc") == 0);

    /* Remove b. */
    click_campaign_row(&es, 1);
    click_campaign_button(&es, CAMPAIGN_BUTTON_REMOVE);
    CHECK(strcmp(campaign_rows(&es), "ac") == 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(strcmp(campaign_rows(&es), "abc") == 0);
    CHECK(strcmp(es.status_message, "Undid: removed levels/b.toml") == 0);
    CHECK(view->levels[1].loaded && strcmp(view->levels[1].level.name, "Bravo") == 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_REDO);
    CHECK(strcmp(campaign_rows(&es), "ac") == 0);
    CHECK(strcmp(es.status_message, "Redid: removed levels/b.toml") == 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(editor_campaign_unsaved(&es) == 0);

    /* Add d (the picker's answer, given straight to the view). */
    CHECK(editor_campaign_add(&es, "levels/d.toml") == 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(strcmp(campaign_rows(&es), "abc") == 0);
    CHECK(strcmp(es.status_message, "Undid: added levels/d.toml") == 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_REDO);
    CHECK(strcmp(campaign_rows(&es), "abcd") == 0);
    CHECK(strcmp(es.status_message, "Redid: added levels/d.toml") == 0);

    /* Link in order points c at d. */
    click_campaign_button(&es, CAMPAIGN_BUTTON_LINK);
    CHECK(strcmp(view->levels[2].level.next_phase, "levels/d.toml") == 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(view->levels[2].level.next_phase[0] == '\0');
    CHECK(strcmp(es.status_message, "Undid: linked the levels in order") == 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_REDO);
    CHECK(strcmp(view->levels[2].level.next_phase, "levels/d.toml") == 0);
    CHECK(strcmp(es.status_message, "Redid: linked the levels in order") == 0);

    /* Rename a. */
    click_campaign_name(&es, 0);
    push_text("!");
    key_frame(&es, KEY_ENTER, 0);
    CHECK(strcmp(view->levels[0].level.name, "Alpha!") == 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(strcmp(view->levels[0].level.name, "Alpha") == 0);
    CHECK(strcmp(es.status_message, "Undid: renamed levels/a.toml") == 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_REDO);
    CHECK(strcmp(view->levels[0].level.name, "Alpha!") == 0);
    CHECK(strcmp(es.status_message, "Redid: renamed levels/a.toml") == 0);

    /* A click on Undo while a name is being typed applies the name as a
     * step and takes that step back, as Ctrl+Z does; Redo returns it. */
    click_campaign_name(&es, 1);
    push_text("#");
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(es.ui.active_id == CAMPAIGN_NAME_FIELD_ID + 1);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(es.ui.active_id == 0);
    CHECK(strcmp(view->levels[1].level.name, "Bravo") == 0);
    CHECK(editor_campaign_redo_steps(&es) == 1);
    click_campaign_button(&es, CAMPAIGN_BUTTON_REDO);
    CHECK(strcmp(view->levels[1].level.name, "Bravo#") == 0);

    /* The same click with an empty history: a typed name is work for Undo,
     * so the button is not disabled while one is being typed. */
    click_campaign_button(&es, CAMPAIGN_BUTTON_REVERT);
    click_campaign_button(&es, CAMPAIGN_BUTTON_REVERT);
    view = editor_campaign_entries(&es);
    CHECK(view && editor_campaign_undo_steps(&es) == 0);
    click_campaign_name(&es, 0);
    push_text("?");
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(es.ui.active_id == 0 && strcmp(view->levels[0].level.name, "Alpha") == 0);
    CHECK(editor_campaign_undo_steps(&es) == 0 && editor_campaign_redo_steps(&es) == 1);
    click_campaign_button(&es, CAMPAIGN_BUTTON_REDO);
    CHECK(strcmp(view->levels[0].level.name, "Alpha?") == 0);

    /* None of this touched the level's own history. */
    CHECK(es.undo->top == 0 && es.undo->redo_top == 0 && !es.modified);
done:
    if (moved && test_change_folder(cwd) != 0) failed = 1;
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/*
 * An Undo refused because the removed level's file no longer loads can be
 * skipped, but only by answering "Skip this step" to a question the second
 * refusal asks: the dialog's default ("Keep the step", what Enter chooses)
 * and Cancel leave the history alone.  The skipped step is named, the list
 * stays as it is, and
 * the steps before it then undo and redo what they did to the other rows:
 * their row numbers no longer count the level that is gone for good.
 */
static int campaign_refused_undo_can_be_skipped(void)
{
    int failed = 0;
    EditorState es;
    char cwd[4096];
    int moved = 0;
    const CampaignCatalog *view;
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(enter_ui_campaign(cwd, sizeof(cwd)) == 0);
    moved = 1;
    key_frame(&es, KEY_M, INPUT_CTRL);
    view = editor_campaign_entries(&es);
    CHECK(view && strcmp(campaign_rows(&es), "abc") == 0);

    /* Seven steps.  c is the level that will be lost: two of the steps
     * act on it alone, the others on rows above and below it. */
    CHECK(editor_campaign_add(&es, "levels/d.toml") == 0);      /* a b c d */
    click_campaign_row(&es, 2);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UP);             /* a c b d */
    click_campaign_name(&es, 1);
    push_text("2");
    key_frame(&es, KEY_ENTER, 0);                               /* Charlie2 */
    click_campaign_name(&es, 3);
    push_text("2");
    key_frame(&es, KEY_ENTER, 0);                               /* Delta2 */
    click_campaign_row(&es, 3);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UP);             /* a c d b */
    click_campaign_button(&es, CAMPAIGN_BUTTON_LINK);           /* a>c>d>b */
    click_campaign_row(&es, 1);
    click_campaign_button(&es, CAMPAIGN_BUTTON_REMOVE);         /* a d b */
    CHECK(strcmp(campaign_rows(&es), "adb") == 0);
    CHECK(editor_campaign_undo_steps(&es) == 7);
    CHECK(strcmp(view->levels[0].level.next_phase, "levels/c.toml") == 0);
    CHECK(editor_campaign_undo_blocked(&es) == 0);

    /* The file goes.  The first Undo only says so, however it is given. */
    CHECK(remove("levels/c.toml") == 0);
    skip_step_answer = EDITOR_SKIP_STEP_SKIP;   /* not asked yet: must not matter */
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(strstr(es.status_message, "Cannot undo: levels/c.toml no longer loads") != NULL);
    CHECK(strstr(es.status_message, "Undo again to skip this step") != NULL);
    CHECK(editor_campaign_undo_blocked(&es) == 1);
    CHECK(strcmp(campaign_rows(&es), "adb") == 0 && editor_campaign_undo_steps(&es) == 7);

    /* The second one asks.  The default answer, "Keep the step" (Enter
     * in the dialog), changes nothing, by button or by key, however often;
     * neither does Cancel, or closing the dialog. */
    skip_step_answer = EDITOR_SKIP_STEP_KEEP;
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(strstr(es.status_message, "Undo step kept: levels/c.toml") != NULL);
    CHECK(editor_campaign_undo_blocked(&es) == 1);
    CHECK(strcmp(campaign_rows(&es), "adb") == 0 && editor_campaign_undo_steps(&es) == 7);
    editor_set_status(&es, UNTOUCHED_STATUS);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(strstr(es.status_message, "Undo step kept: levels/c.toml") != NULL);
    skip_step_answer = EDITOR_SKIP_STEP_CANCEL;
    editor_set_status(&es, UNTOUCHED_STATUS);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(strstr(es.status_message, "Undo step kept: levels/c.toml") != NULL);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(editor_campaign_undo_blocked(&es) == 1);
    CHECK(strcmp(campaign_rows(&es), "adb") == 0);
    CHECK(editor_campaign_undo_steps(&es) == 7 && editor_campaign_redo_steps(&es) == 0);
    CHECK(strcmp(view->levels[1].level.name, "Delta2") == 0);

    /* "Skip this step" drops the Remove, and with it the two steps that
     * only changed c: its move up and its rename.  The status names what
     * was skipped; the list on screen is as it was. */
    skip_step_answer = EDITOR_SKIP_STEP_SKIP;
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(strcmp(es.status_message, "Skipped: removed levels/c.toml (it stays out of the "
                 "campaign), and 2 older steps that changed nothing else") == 0);
    CHECK(editor_campaign_undo_blocked(&es) == 0);
    CHECK(strcmp(campaign_rows(&es), "adb") == 0);
    CHECK(editor_campaign_undo_steps(&es) == 4 && editor_campaign_redo_steps(&es) == 0);
    CHECK(strcmp(view->levels[0].level.next_phase, "levels/c.toml") == 0);
    CHECK(strcmp(view->levels[1].level.name, "Delta2") == 0);

    /* The relink, now of three rows: each gets back the link it had. */
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(strcmp(es.status_message, "Undid: linked the levels in order") == 0);
    CHECK(strcmp(campaign_rows(&es), "adb") == 0);
    CHECK(strcmp(view->levels[0].level.next_phase, "levels/b.toml") == 0);
    CHECK(view->levels[1].level.next_phase[0] == '\0');
    CHECK(strcmp(view->levels[2].level.next_phase, "levels/c.toml") == 0);
    /* Redo writes the links the relink wrote then: a pointed at c, and
     * that is what the list showed before this Undo, c gone or not. */
    click_campaign_button(&es, CAMPAIGN_BUTTON_REDO);
    CHECK(strcmp(view->levels[0].level.next_phase, "levels/c.toml") == 0);
    CHECK(strcmp(view->levels[1].level.next_phase, "levels/b.toml") == 0);
    CHECK(view->levels[2].level.next_phase[0] == '\0');
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);

    /* d's move up: it was row 4 going to row 3, and is now row 3 to 2. */
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(strcmp(es.status_message, "Undid: moved levels/d.toml") == 0);
    CHECK(strcmp(campaign_rows(&es), "abd") == 0);
    /* d's rename, one row higher than when it was typed. */
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(strcmp(es.status_message, "Undid: renamed levels/d.toml") == 0);
    CHECK(strcmp(view->levels[2].level.name, "Delta") == 0);
    CHECK(strcmp(view->levels[0].level.name, "Alpha") == 0 &&
          strcmp(view->levels[1].level.name, "Bravo") == 0);
    /* d's Add: all the way back is the campaign as it was, without c. */
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(strcmp(es.status_message, "Undid: added levels/d.toml") == 0);
    CHECK(strcmp(campaign_rows(&es), "ab") == 0);
    CHECK(editor_campaign_undo_steps(&es) == 0 && editor_campaign_redo_steps(&es) == 4);
    CHECK(editor_campaign_unsaved(&es) == 1);        /* the file lists c */

    /* And forwards again, to exactly the list that was on screen. */
    for (int i = 0; i < 4; i++) click_campaign_button(&es, CAMPAIGN_BUTTON_REDO);
    CHECK(strcmp(campaign_rows(&es), "adb") == 0);
    CHECK(strcmp(view->levels[1].level.name, "Delta2") == 0);
    CHECK(strcmp(view->levels[0].level.next_phase, "levels/c.toml") == 0);
    CHECK(strcmp(view->levels[1].level.next_phase, "levels/b.toml") == 0);
    CHECK(view->levels[2].level.next_phase[0] == '\0');
    CHECK(editor_campaign_redo_steps(&es) == 0);

    /* A level that was added and removed inside the history leaves it
     * altogether, and an older Remove puts its row back where it belongs.
     * Start again from the files. */
    CHECK(write_ui_campaign_level("levels/c.toml", "Charlie", "") == 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_REVERT);
    click_campaign_button(&es, CAMPAIGN_BUTTON_REVERT);
    view = editor_campaign_entries(&es);
    CHECK(view && strcmp(campaign_rows(&es), "abc") == 0);
    CHECK(editor_campaign_move(&es, 1, +1) == 0);               /* a c b */
    CHECK(editor_campaign_add(&es, "levels/d.toml") == 0);      /* a c b d */
    CHECK(editor_campaign_remove(&es, 0) == 0);                 /* c b d */
    CHECK(editor_campaign_move(&es, 2, -1) == 0);               /* c d b */
    CHECK(editor_campaign_remove(&es, 1) == 0);                 /* c b */
    CHECK(remove("levels/d.toml") == 0);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(editor_campaign_undo_blocked(&es) == 1);
    skip_step_answer = EDITOR_SKIP_STEP_SKIP;
    key_frame(&es, KEY_Z, INPUT_CTRL);               /* the key asks as well */
    CHECK(strstr(es.status_message, "Skipped: removed levels/d.toml") != NULL);
    CHECK(strstr(es.status_message, "2 older steps") != NULL);
    CHECK(strcmp(campaign_rows(&es), "cb") == 0 && editor_campaign_undo_steps(&es) == 2);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(strcmp(es.status_message, "Undid: removed levels/a.toml") == 0);
    CHECK(strcmp(campaign_rows(&es), "acb") == 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(strcmp(campaign_rows(&es), "abc") == 0);
    CHECK(editor_campaign_unsaved(&es) == 0);        /* d was never saved */

    /* Skipping is offered for a step refused twice in a row only: once
     * the file is back, the very same Undo works and nothing is asked. */
    CHECK(editor_campaign_remove(&es, 2) == 0);                 /* a b */
    CHECK(editor_campaign_remove(&es, 0) == 0);                 /* b */
    CHECK(remove("levels/a.toml") == 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(editor_campaign_undo_blocked(&es) == 1);
    CHECK(write_ui_campaign_level("levels/a.toml", "Alpha", "levels/b.toml") == 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);           /* answer: skip */
    CHECK(strcmp(es.status_message, "Undid: removed levels/a.toml") == 0);
    CHECK(strcmp(campaign_rows(&es), "ab") == 0);
    CHECK(editor_campaign_undo_blocked(&es) == 0);
    /* A new edit also ends the blocked state: the newest step is then
     * another one, and that one has not been refused. */
    CHECK(editor_campaign_remove(&es, 0) == 0);                 /* b */
    CHECK(remove("levels/a.toml") == 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(editor_campaign_undo_blocked(&es) == 1);
    CHECK(editor_campaign_add(&es, "levels/d.toml") == -1);     /* d is gone */
    CHECK(editor_campaign_undo_blocked(&es) == 1);
    CHECK(editor_campaign_move(&es, 0, +1) == -1);              /* one row */
    CHECK(editor_campaign_undo_blocked(&es) == 1);
    click_campaign_name(&es, 0);
    push_text("!");
    key_frame(&es, KEY_ENTER, 0);
    CHECK(editor_campaign_undo_blocked(&es) == 0);
    /* The lost row was above the one an older Remove took out, so that
     * Remove now puts c back one row higher. */
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);           /* the rename */
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);           /* refused */
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);           /* skipped */
    CHECK(strcmp(es.status_message,
                 "Skipped: removed levels/a.toml (it stays out of the campaign)") == 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UNDO);
    CHECK(strcmp(es.status_message, "Undid: removed levels/c.toml") == 0);
    CHECK(strcmp(campaign_rows(&es), "bc") == 0);
    CHECK(editor_campaign_undo_steps(&es) == 0);
done:
    if (moved && test_change_folder(cwd) != 0) failed = 1;
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/* The scratch campaign's level files: what enter_ui_campaign writes. */
static const struct {
    const char *path, *name, *next;
} ui_campaign_files[] = {
    {"levels/a.toml", "Alpha",   "levels/b.toml"},
    {"levels/b.toml", "Bravo",   "levels/c.toml"},
    {"levels/c.toml", "Charlie", ""},
    {"levels/d.toml", "Delta",   ""},
};
#define UI_CAMPAIGN_FILES ((int)(sizeof(ui_campaign_files) / sizeof(ui_campaign_files[0])))

/* The view's list at one moment, to compare with another moment. */
typedef struct {
    int count;
    struct {
        char file;                          /* 'a' for levels/a.toml */
        char name[LEVEL_NAME_CAPACITY];
        char next[sizeof(((LevelDef *)0)->next_phase)];
    } rows[UI_CAMPAIGN_FILES];
} CampaignSnapshot;

static void take_campaign_snapshot(const EditorState *es, CampaignSnapshot *shot)
{
    const CampaignCatalog *view = editor_campaign_entries(es);

    memset(shot, 0, sizeof(*shot));
    for (size_t i = 0; view && i < view->count && i < (size_t)UI_CAMPAIGN_FILES; i++) {
        shot->rows[i].file = view->levels[i].path[sizeof("levels/") - 1];
        memcpy(shot->rows[i].name, view->levels[i].level.name, sizeof(shot->rows[i].name));
        memcpy(shot->rows[i].next, view->levels[i].level.next_phase,
               sizeof(shot->rows[i].next));
        shot->count++;
    }
}

/* The same list with the row of level `file` taken out (if it has one). */
static void snapshot_without(const CampaignSnapshot *shot, char file, CampaignSnapshot *out)
{
    memset(out, 0, sizeof(*out));
    for (int i = 0; i < shot->count; i++)
        if (shot->rows[i].file != file) out->rows[out->count++] = shot->rows[i];
}

static int same_snapshot(const CampaignSnapshot *a, const CampaignSnapshot *b)
{
    if (a->count != b->count) return 0;
    for (int i = 0; i < a->count; i++)
        if (a->rows[i].file != b->rows[i].file ||
            strcmp(a->rows[i].name, b->rows[i].name) != 0 ||
            strcmp(a->rows[i].next, b->rows[i].next) != 0) return 0;
    return 1;
}

/* Does the list have a row for level `file`? */
static int snapshot_lists(const CampaignSnapshot *shot, char file)
{
    for (int i = 0; i < shot->count; i++)
        if (shot->rows[i].file == file) return 1;
    return 0;
}

/*
 * The next number below `limit` from a small generator (the constants are
 * Numerical Recipes'; its high bits are the usable ones).  Unlike rand(), it
 * gives the same numbers on every system, so a failing trial fails everywhere.
 */
static int next_random(uint32_t *seed, int limit)
{
    *seed = *seed * 1664525u + 1013904223u;
    return (int)((*seed >> 16) % (uint32_t)limit);
}

/*
 * Skipping a refused Undo rewrites the older steps, so check the rule they
 * must obey against many histories rather than a few chosen ones.
 *
 * Each trial makes a run of edits picked by a small fixed generator (the
 * same on every machine and every run), then removes a level, loses its
 * file and skips the refused Undo.  From there, every Undo must show the
 * list that was on screen one edit earlier, minus the lost level's row
 * (from the edit that brought the level in, if the history holds it); an
 * edit that only changed that row must be gone from the history; and
 * redoing everything must end on exactly the list the skip left.
 */
static int campaign_skipped_step_leaves_a_consistent_history(void)
{
    enum { TRIALS = 120, MAX_EDITS = 10 };
    int failed = 0;
    EditorState es;
    char cwd[4096];
    int moved = 0;
    int trial = 0;
    uint32_t seed = 20261010u;
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(enter_ui_campaign(cwd, sizeof(cwd)) == 0);
    moved = 1;

    for (trial = 0; trial < TRIALS; trial++) {
        CampaignSnapshot seen[MAX_EDITS + 1];     /* the list after each edit */
        CampaignSnapshot expect[MAX_EDITS + 1];   /* ...as Undo must show it  */
        CampaignSnapshot now, left;
        const CampaignCatalog *view;
        int edits = 0;         /* how many edits made a step */
        int expected = 0;
        int came_in = 0;       /* the edit that added the lost level, or 0 */
        int wanted;
        int lost_row;
        char lost;
        char lost_path[sizeof("levels/a.toml")];

        /* Every trial starts from the same files and a fresh view. */
        for (int f = 0; f < UI_CAMPAIGN_FILES; f++)
            CHECK(write_ui_campaign_level(ui_campaign_files[f].path, ui_campaign_files[f].name,
                                          ui_campaign_files[f].next) == 0);
        (void)editor_campaign_close(&es, 1);
        CHECK(editor_campaign_open(&es, NULL) == 0);
        view = editor_campaign_entries(&es);
        take_campaign_snapshot(&es, &seen[0]);

        /* Some picks are refused (a move off the end, a level already
         * listed), hence the tries.  The list keeps at least two rows, so
         * that one is left after the Remove that ends the run. */
        wanted = 2 + next_random(&seed, MAX_EDITS - 1);
        for (int tries = 0; edits < wanted && tries < 60; tries++) {
            int steps = editor_campaign_undo_steps(&es);
            int count = (int)view->count;
            int row = next_random(&seed, count);
            int file = next_random(&seed, UI_CAMPAIGN_FILES);

            switch (next_random(&seed, 6)) {
            case 0: (void)editor_campaign_move(&es, row, -1); break;
            case 1: (void)editor_campaign_move(&es, row, +1); break;
            case 2: if (count > 2) (void)editor_campaign_remove(&es, row); break;
            case 3: (void)editor_campaign_add(&es, ui_campaign_files[file].path); break;
            case 4: (void)editor_campaign_link_in_order(&es); break;
            case 5:                 /* a rename, typed as a designer types it */
                click_campaign_name(&es, row);
                push_text("x");
                key_frame(&es, KEY_ENTER, 0);
                break;
            }
            /* An edit that was refused, or changed nothing, is no step. */
            if (editor_campaign_undo_steps(&es) == steps) continue;
            CHECK(editor_campaign_undo_steps(&es) == steps + 1);
            take_campaign_snapshot(&es, &seen[++edits]);
        }
        CHECK(edits >= 1 && view->count >= 2);

        /* Remove one level and lose its file. */
        lost_row = next_random(&seed, (int)view->count);
        lost = view->levels[lost_row].path[sizeof("levels/") - 1];
        snprintf(lost_path, sizeof(lost_path), "levels/%c.toml", lost);
        CHECK(editor_campaign_remove(&es, lost_row) == 0);
        take_campaign_snapshot(&es, &left);
        CHECK(remove(lost_path) == 0);
        CHECK(editor_campaign_undo(&es) == -1);
        take_campaign_snapshot(&es, &now);
        CHECK(same_snapshot(&now, &left) && editor_campaign_undo_blocked(&es) == 1);

        /* Skip it: the list does not change. */
        editor_test_set_skip_step_choice(EDITOR_SKIP_STEP_SKIP);
        CHECK(editor_campaign_undo(&es) == 2);
        take_campaign_snapshot(&es, &now);
        CHECK(same_snapshot(&now, &left) && editor_campaign_undo_blocked(&es) == 0);

        /* What each Undo must show.  The lost level's row is taken out of
         * every list since the edit that last brought the level in (edit
         * 0, the list as opened, if no edit did).  Two neighbouring lists
         * that are then the same mean an edit of that row alone, which
         * the skip must have dropped. */
        for (int e = 1; e <= edits; e++)
            if (!snapshot_lists(&seen[e - 1], lost) && snapshot_lists(&seen[e], lost))
                came_in = e;
        for (int e = 0; e <= edits; e++) {
            CampaignSnapshot shown;
            if (e >= came_in) snapshot_without(&seen[e], lost, &shown);
            else shown = seen[e];
            if (expected > 0 && same_snapshot(&shown, &expect[expected - 1])) continue;
            expect[expected++] = shown;
        }
        CHECK(same_snapshot(&expect[expected - 1], &left));
        CHECK(editor_campaign_undo_steps(&es) == expected - 1);

        /* The file is back (an older step may read it), then every step
         * backwards and forwards. */
        for (int f = 0; f < UI_CAMPAIGN_FILES; f++)
            if (ui_campaign_files[f].path[sizeof("levels/") - 1] == lost)
                CHECK(write_ui_campaign_level(ui_campaign_files[f].path, ui_campaign_files[f].name,
                                              ui_campaign_files[f].next) == 0);
        for (int e = expected - 2; e >= 0; e--) {
            CHECK(editor_campaign_undo(&es) == 1);
            take_campaign_snapshot(&es, &now);
            CHECK(same_snapshot(&now, &expect[e]));
        }
        CHECK(editor_campaign_undo(&es) == 0);
        for (int e = 1; e < expected; e++) {
            CHECK(editor_campaign_redo(&es) == 1);
            take_campaign_snapshot(&es, &now);
            CHECK(same_snapshot(&now, &expect[e]));
        }
        CHECK(editor_campaign_redo(&es) == 0);
    }
done:
    /* The generator is the same everywhere, so the trial number is enough
     * to run that very history again. */
    if (failed) fprintf(stderr, "editor_ui_test: (in skipped-step history %d)\n", trial);
    if (moved && test_change_folder(cwd) != 0) failed = 1;
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/*
 * The unsaved mark compares the view with the files, so it follows the
 * history: undoing back to what was saved is "nothing unsaved", Save makes
 * the list as it stands the saved one, and Revert and reopening the view
 * start a new history.
 */
static int campaign_unsaved_mark_follows_the_history(void)
{
    int failed = 0;
    EditorState es;
    char cwd[4096];
    int moved = 0;
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(enter_ui_campaign(cwd, sizeof(cwd)) == 0);
    moved = 1;
    key_frame(&es, KEY_M, INPUT_CTRL);
    CHECK(es.campaign != NULL && editor_campaign_unsaved(&es) == 0);

    /* Edit, undo, redo. */
    click_campaign_row(&es, 2);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UP);    /* a c b */
    CHECK(editor_campaign_unsaved(&es) == 1);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(editor_campaign_unsaved(&es) == 0);
    key_frame(&es, KEY_Y, INPUT_CTRL);
    CHECK(editor_campaign_unsaved(&es) == 1);

    /* Save: the list as it stands, relinked, is now the saved one. */
    click_campaign_button(&es, CAMPAIGN_BUTTON_LINK);
    click_campaign_button(&es, CAMPAIGN_BUTTON_SAVE);
    CHECK(strstr(es.status_message, "Campaign saved") != NULL);
    CHECK(editor_campaign_unsaved(&es) == 0);
    CHECK(editor_campaign_undo_steps(&es) == 2);     /* Save keeps the history */

    /* Undoing past the Save differs from the files again, step by step,
     * and redoing arrives back at the saved campaign. */
    key_frame(&es, KEY_Z, INPUT_CTRL);               /* the links */
    CHECK(strcmp(campaign_rows(&es), "acb") == 0 && editor_campaign_unsaved(&es) == 1);
    key_frame(&es, KEY_Z, INPUT_CTRL);               /* the move */
    CHECK(strcmp(campaign_rows(&es), "abc") == 0 && editor_campaign_unsaved(&es) == 1);
    key_frame(&es, KEY_Y, INPUT_CTRL);
    CHECK(editor_campaign_unsaved(&es) == 1);
    key_frame(&es, KEY_Y, INPUT_CTRL);
    CHECK(strcmp(campaign_rows(&es), "acb") == 0 && editor_campaign_unsaved(&es) == 0);

    /* Revert (asked twice) reads the files again and clears the history. */
    click_campaign_row(&es, 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_DOWN);  /* c a b */
    CHECK(editor_campaign_undo_steps(&es) == 3);
    click_campaign_button(&es, CAMPAIGN_BUTTON_REVERT);
    CHECK(editor_campaign_undo_steps(&es) == 3);     /* only warned so far */
    click_campaign_button(&es, CAMPAIGN_BUTTON_REVERT);
    CHECK(strcmp(campaign_rows(&es), "acb") == 0 && editor_campaign_unsaved(&es) == 0);
    CHECK(editor_campaign_undo_steps(&es) == 0 && editor_campaign_redo_steps(&es) == 0);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(strstr(es.status_message, "nothing to undo") != NULL);
    CHECK(strcmp(campaign_rows(&es), "acb") == 0);

    /* So does closing the view and opening it again. */
    click_campaign_row(&es, 0);
    click_campaign_button(&es, CAMPAIGN_BUTTON_DOWN);  /* c a b */
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(editor_campaign_redo_steps(&es) == 1 && editor_campaign_unsaved(&es) == 0);
    key_frame(&es, KEY_ESCAPE, 0);
    CHECK(es.campaign == NULL);
    key_frame(&es, KEY_M, INPUT_CTRL);
    CHECK(es.campaign != NULL);
    CHECK(editor_campaign_undo_steps(&es) == 0 && editor_campaign_redo_steps(&es) == 0);
    key_frame(&es, KEY_Y, INPUT_CTRL);
    CHECK(strcmp(campaign_rows(&es), "acb") == 0);
done:
    if (moved && test_change_folder(cwd) != 0) failed = 1;
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/*
 * While the Campaign view is open the undo keys belong to it: the level's
 * own undo stack and the level are left alone, whether or not the view has
 * a step to replay.  With the view closed the keys undo the level again.
 */
static int campaign_undo_keys_leave_the_level_history_alone(void)
{
    static const int mods[] = { INPUT_CTRL, INPUT_CTRL | INPUT_SHIFT };
    int failed = 0;
    EditorState es;
    char cwd[4096];
    int moved = 0;
    uint64_t level;
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(enter_ui_campaign(cwd, sizeof(cwd)) == 0);
    moved = 1;

    /* One step in the level's history: a coin. */
    es.tool = TOOL_PLACE;
    es.palette_type = ENT_COIN;
    click_frame(&es, 400, 500);
    CHECK(es.level.coin_count == 1 && es.undo->top == 1 && es.undo->redo_top == 0);
    level = doc_hash(&es);

    /* An empty campaign history must not let the keys fall through. */
    key_frame(&es, KEY_M, INPUT_CTRL);
    CHECK(es.campaign != NULL);
    for (size_t i = 0; i < sizeof(mods) / sizeof(mods[0]); i++) {
        key_frame(&es, KEY_Z, mods[i]);
        CHECK(es.undo->top == 1 && es.undo->redo_top == 0 && doc_hash(&es) == level);
    }
    key_frame(&es, KEY_Y, INPUT_CTRL);
    CHECK(es.undo->top == 1 && es.undo->redo_top == 0 && doc_hash(&es) == level);

    /* Nor do campaign steps: a move and a rename, undone and redone. */
    click_campaign_row(&es, 2);
    click_campaign_button(&es, CAMPAIGN_BUTTON_UP);
    click_campaign_name(&es, 0);
    push_text("!");
    key_frame(&es, KEY_ENTER, 0);
    CHECK(editor_campaign_undo_steps(&es) == 2);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(strcmp(campaign_rows(&es), "abc") == 0);
    CHECK(es.undo->top == 1 && es.undo->redo_top == 0 && doc_hash(&es) == level);
    key_frame(&es, KEY_Y, INPUT_CTRL);
    key_frame(&es, KEY_Z, INPUT_CTRL | INPUT_SHIFT);
    CHECK(strcmp(campaign_rows(&es), "acb") == 0);
    CHECK(es.undo->top == 1 && es.undo->redo_top == 0 && doc_hash(&es) == level);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(editor_campaign_unsaved(&es) == 0);

    /* Back in the level, the same keys undo and redo the coin. */
    key_frame(&es, KEY_ESCAPE, 0);
    CHECK(es.campaign == NULL);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coin_count == 0 && es.undo->top == 0 && es.undo->redo_top == 1);
    key_frame(&es, KEY_Y, INPUT_CTRL);
    CHECK(es.level.coin_count == 1 && doc_hash(&es) == level);
done:
    if (moved && test_change_folder(cwd) != 0) failed = 1;
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}
