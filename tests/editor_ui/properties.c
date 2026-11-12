/* Included by tests/editor_ui_test.c. Not compiled alone: the functions are
 * static and use the helpers in that file.
 */
/* ------------------------------------------------------------------ */
/* Properties and level config                                         */
/* ------------------------------------------------------------------ */

/*
 * Select the first entity of every type in a real level, draw its
 * properties, and click every row of the panel. Whatever a click does —
 * start a text edit, open a dropdown, flip a toggle — must leave the level
 * valid, and any change must be exactly one undo step.
 */
static int probe_properties_of_every_type(const char *level_path, int seen[ENT_COUNT],
                                          int *changes)
{
    int failed = 0;
    EditorState es;
    CHECK(open_editor(&es, level_path) == 0);
    for (int type = 0; type < ENT_COUNT; type++) {
        if (editor_entity_count(&es.level, (EntityType)type) == 0) continue;
        es.selection.type = (EntityType)type;
        es.selection.index = 0;
        es.tool = TOOL_SELECT;
        ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
        CHECK(es.selection.index == 0);
        if (seen[type]) continue;   /* already probed in an earlier level */
        for (int y = props_top(&es) + 36; y < EDITOR_H - STATUS_H; y += 24) {
            uint64_t before = doc_hash(&es);
            int undo_before = es.undo->top;
            click_frame(&es, PANEL_FIELD_X, y);
            CHECK(level_is_valid(&es));
            if (settle_probe(&es, PANEL_FIELD_X, y, before, undo_before, changes) != 0 ||
                !level_is_valid(&es)) {
                fprintf(stderr, "editor_ui_test: %s probe of %s at y=%d broke undo\n",
                        level_path, editor_entity_type_name((EntityType)type), y);
                CHECK(0);
            }
            es.selection.type = (EntityType)type;
            es.selection.index = 0;
        }
        seen[type] = 1;
    }
done:
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

static int properties_panel_handles_every_entity_type(void)
{
    int failed = 0, changes = 0;
    int seen[ENT_COUNT] = {0};
    CHECK(probe_properties_of_every_type("levels/02_lugio_02.toml", seen, &changes) == 0);
    CHECK(probe_properties_of_every_type("levels/00_sandbox_01.toml", seen, &changes) == 0);
    int types = 0;
    for (int i = 0; i < ENT_COUNT; i++) types += seen[i];
    CHECK(types >= 25);
    /* Toggles and dropdown picks really edit the document. */
    CHECK(changes >= 3);
done:
    return failed;
}

/* Type `text` into the field of row `row` of a several-entity panel and
 * press Enter. Row 0 sits under the title bar and the one-line note. */
static void type_into_shared_row(EditorState *es, int row, const char *text)
{
    int y = props_top(es) + 24 + 8 + 20 + row * 24 + 6;
    click_frame(es, PANEL_FIELD_X, y);
    snprintf(es->ui.edit_buf, sizeof(es->ui.edit_buf), "%s", text);
    es->ui.edit_cursor = (int)strlen(text);
    push_key(KEY_ENTER, 0);
    ui_frame(es, PANEL_FIELD_X, y);
}

/*
 * With several entities of one type selected, the Properties panel shows
 * that type's fields, and a value typed into one is given to every
 * selected entity as a single undo step. A value that would break one of
 * them is refused for all, the first one included. Entities of different
 * types still show the summary and edit nothing.
 */
static int a_field_edits_every_selected_entity_of_one_type(void)
{
    int failed = 0;
    EditorState es;
    PlacementData data;
    CHECK(open_editor(&es, NULL) == 0);
    es.level.screen_count = 4;
    es.tool = TOOL_SELECT;

    /* Three coins at different heights. */
    for (int i = 0; i < 3; i++) {
        memset(&data, 0, sizeof(data));
        data.coin.x = 100.0f + 40.0f * i;
        data.coin.y = 100.0f + 10.0f * i;
        CHECK(editor_add_placement(&es, ENT_COIN, &data, "place") == 0);
    }
    Selection coins[3] = { {ENT_COIN, 0}, {ENT_COIN, 1}, {ENT_COIN, 2} };
    CHECK(editor_select_items(&es, coins, 3) == 3);
    CHECK(editor_selection_count(&es) == 3);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);

    int steps = es.undo->top;
    type_into_shared_row(&es, 1, "64");                 /* row 1 is y */
    CHECK(es.level.coins[0].y == 64.0f && es.level.coins[1].y == 64.0f &&
          es.level.coins[2].y == 64.0f);
    CHECK(es.level.coins[0].x == 100.0f && es.level.coins[2].x == 180.0f);   /* x untouched */
    CHECK(es.undo->top == steps + 1);                   /* one step for all three */
    CHECK(strstr(es.status_message, "set on 3") != NULL);
    CHECK(editor_selection_count(&es) == 3);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coins[0].y == 100.0f && es.level.coins[1].y == 110.0f &&
          es.level.coins[2].y == 120.0f);
    key_frame(&es, KEY_Z, INPUT_CTRL | INPUT_SHIFT);
    CHECK(es.level.coins[1].y == 64.0f && es.level.coins[2].y == 64.0f);

    /* Typing the value they already share changes nothing and records nothing. */
    CHECK(editor_select_items(&es, coins, 3) == 3);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    steps = es.undo->top;
    type_into_shared_row(&es, 1, "64");
    CHECK(es.undo->top == steps);

    /* Two spiders with patrols apart. Moving both right ends to 250 would
     * put the second one's right end left of its left end (400): refused
     * for both, the first one's own edit taken back too. */
    memset(&data, 0, sizeof(data));
    data.spider.x = 150.0f; data.spider.vx = 40.0f;
    data.spider.patrol_x0 = 100.0f; data.spider.patrol_x1 = 300.0f;
    CHECK(editor_add_placement(&es, ENT_SPIDER, &data, "place") == 0);
    data.spider.x = 450.0f;
    data.spider.patrol_x0 = 400.0f; data.spider.patrol_x1 = 600.0f;
    CHECK(editor_add_placement(&es, ENT_SPIDER, &data, "place") == 0);
    CHECK(level_is_valid(&es));
    Selection spiders[2] = { {ENT_SPIDER, 0}, {ENT_SPIDER, 1} };
    CHECK(editor_select_items(&es, spiders, 2) == 2);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    steps = es.undo->top;
    type_into_shared_row(&es, 3, "250");                /* row 3 is patrol_x1 */
    CHECK(es.level.spiders[0].patrol_x1 == 300.0f && es.level.spiders[1].patrol_x1 == 600.0f);
    CHECK(es.undo->top == steps && level_is_valid(&es));
    CHECK(strstr(es.status_message, "Not applied to all 2") != NULL);

    /* A value both can take is applied to both. */
    CHECK(editor_select_items(&es, spiders, 2) == 2);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    type_into_shared_row(&es, 1, "55");                 /* row 1 is vx */
    CHECK(es.level.spiders[0].vx == 55.0f && es.level.spiders[1].vx == 55.0f);
    CHECK(es.undo->top == steps + 1 && level_is_valid(&es));

    /* No memory for the step that would hold both: the edit is taken back
     * whole, the primary's own value included, and the bar says why. */
    CHECK(editor_select_items(&es, spiders, 2) == 2);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    steps = es.undo->top;
    undo_test_limit_allocations(0);
    type_into_shared_row(&es, 1, "66");
    undo_test_limit_allocations(-1);
    CHECK(es.level.spiders[0].vx == 55.0f && es.level.spiders[1].vx == 55.0f);
    CHECK(es.undo->top == steps);
    CHECK(strcmp(es.status_message, "Edit cancelled: cannot allocate undo history") == 0);

    /* A validation message about "patrol" opens the field that usually
     * fixes it, the right end; nothing is selected-dependent until the
     * panel draws, and an empty selection draws no fields at all. */
    properties_focus_entity_field(&es, ENT_SPIDER, 0, "patrol");
    CHECK(es.panel_open == 1);
    editor_select_none(&es);
    properties_render(&es, 0, 400);

    /* Different types selected: the summary, and no field under the click. */
    Selection mixed[2] = { {ENT_COIN, 0}, {ENT_SPIDER, 0} };
    CHECK(editor_select_items(&es, mixed, 2) == 2);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    steps = es.undo->top;
    uint64_t hash = doc_hash(&es);
    type_into_shared_row(&es, 0, "7");
    CHECK(doc_hash(&es) == hash && es.undo->top == steps);
done:
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

#ifndef _WIN32
/* A playtest launcher that never starts anything (F5 below). */
static int refuse_launch(const char *const *argv)
{
    (void)argv;
    return -1;
}
#endif

/* Three coins at heights 100, 110 and 120, all selected, and one frame
 * drawn so the panel shows their shared rows. */
static int select_three_coins_at_different_heights(EditorState *es)
{
    static const Selection coins[3] = { {ENT_COIN, 0}, {ENT_COIN, 1}, {ENT_COIN, 2} };
    for (int i = 0; i < 3; i++) es->level.coins[i].y = 100.0f + 10.0f * i;
    if (editor_select_items(es, coins, 3) != 3) return -1;
    ui_frame(es, NEUTRAL_X, NEUTRAL_Y);
    return 0;
}

/* Click the field of shared row `row` and type `text`, without Enter. */
static void start_typing_into_shared_row(EditorState *es, int row, const char *text)
{
    int y = props_top(es) + 24 + 8 + 20 + row * 24 + 6;
    click_frame(es, PANEL_FIELD_X, y);
    snprintf(es->ui.edit_buf, sizeof(es->ui.edit_buf), "%s", text);
    es->ui.edit_cursor = (int)strlen(text);
}

/* All three coins at height y, and one undo step since `steps`. */
static int three_coins_at(const EditorState *es, float y, int steps)
{
    return es->level.coins[0].y == y && es->level.coins[1].y == y &&
           es->level.coins[2].y == y && es->undo->top == steps + 1;
}

/*
 * A value typed into a shared row reaches every selected entity however
 * the field is left, not only with Enter: Tab, a click on the canvas or on
 * another field, Ctrl+S and F5 all finish the edit first, and each is one
 * undo step for all of them.
 */
static int a_shared_field_reaches_every_entity_however_it_is_left(void)
{
    int failed = 0;
    EditorState es;
    PlacementData data;
    int steps;
    CHECK(open_editor(&es, NULL) == 0);
    es.level.screen_count = 4;
    es.tool = TOOL_SELECT;
    for (int i = 0; i < 3; i++) {
        memset(&data, 0, sizeof(data));
        data.coin.x = 100.0f + 40.0f * i;
        data.coin.y = 100.0f;
        CHECK(editor_add_placement(&es, ENT_COIN, &data, "place") == 0);
    }

    /* A click on an empty part of the canvas. */
    CHECK(select_three_coins_at_different_heights(&es) == 0);
    steps = es.undo->top;
    start_typing_into_shared_row(&es, 1, "65");
    click_frame(&es, CANVAS_W / 2, TOOLBAR_H + 40);
    CHECK(three_coins_at(&es, 65.0f, steps));
    /* One step: one undo puts all three back. */
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coins[0].y == 100.0f && es.level.coins[1].y == 110.0f &&
          es.level.coins[2].y == 120.0f);

    /* A click on another field of the same panel. */
    CHECK(select_three_coins_at_different_heights(&es) == 0);
    steps = es.undo->top;
    start_typing_into_shared_row(&es, 1, "66");
    click_frame(&es, PANEL_FIELD_X, props_top(&es) + 24 + 8 + 20 + 6);   /* row 0 */
    CHECK(three_coins_at(&es, 66.0f, steps));
    CHECK(es.level.coins[0].x == 100.0f && es.level.coins[2].x == 180.0f);
    key_frame(&es, KEY_ESCAPE, 0);

    /* Ctrl+S (the Save As picker answers Cancel). */
    CHECK(select_three_coins_at_different_heights(&es) == 0);
    steps = es.undo->top;
    start_typing_into_shared_row(&es, 1, "67");
    key_frame(&es, KEY_S, INPUT_CTRL);
    CHECK(three_coins_at(&es, 67.0f, steps));

#ifndef _WIN32
    /* F5 (the launcher starts nothing). */
    editor_test_set_play_launcher(refuse_launch);
    CHECK(select_three_coins_at_different_heights(&es) == 0);
    steps = es.undo->top;
    start_typing_into_shared_row(&es, 1, "68");
    key_frame(&es, KEY_F5, 0);
    CHECK(three_coins_at(&es, 68.0f, steps));
    CHECK(!es.playing);
#endif

    /* Undo straight from the field: the edit lands for all three first,
     * and that step is the one undone. */
    CHECK(select_three_coins_at_different_heights(&es) == 0);
    steps = es.undo->top;
    start_typing_into_shared_row(&es, 1, "69");
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.undo->top == steps);
    CHECK(es.level.coins[0].y == 100.0f && es.level.coins[1].y == 110.0f &&
          es.level.coins[2].y == 120.0f);
    key_frame(&es, KEY_Z, INPUT_CTRL | INPUT_SHIFT);
    CHECK(es.level.coins[0].y == 69.0f && es.level.coins[1].y == 69.0f &&
          es.level.coins[2].y == 69.0f);

    /* Tab (last: the field it moves to may scroll the panel). */
    CHECK(select_three_coins_at_different_heights(&es) == 0);
    steps = es.undo->top;
    start_typing_into_shared_row(&es, 1, "64");
    key_frame(&es, KEY_TAB, 0);
    CHECK(three_coins_at(&es, 64.0f, steps));
done:
#ifndef _WIN32
    editor_test_set_play_launcher(NULL);
#endif
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

static int level_config_sections_resize_the_panel(void)
{
    int failed = 0, changes = 0;
    EditorState es;
    CHECK(open_editor(&es, "levels/00_sandbox_01.toml") == 0);
    CHECK(es.config_open == 1);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);

    /* Validation messages and recent files take rows of their own. */
    int base = editor_config_total_height(&es);
    es.recent_file_count = 2;
    CHECK(editor_config_total_height(&es) > base);
    es.recent_file_count = 0;
    int messages = es.validation_report.message_count;
    es.validation_report.message_count = messages + 3;
    CHECK(editor_config_total_height(&es) > base);
    es.validation_report.message_count = messages;
    CHECK(editor_config_total_height(&es) == base);

    /* Click down the visible config panel, scrolling it as we go. Section
     * headers open (the panel grows); fields edit or open lists. Every
     * change is one undo step and the level stays valid. */
    int grew = 0, max_height = base;
    for (int pass = 0; pass < 3; pass++) {
        for (int y = TOOLBAR_H + 30; y < TOOLBAR_H + (EDITOR_H - STATUS_H - TOOLBAR_H) / 2; y += 11) {
            for (int column = 0; column < 2; column++) {
                int x = column ? PANEL_FIELD_X : PANEL_LABEL_X;
                uint64_t before = doc_hash(&es);
                int undo_before = es.undo->top;
                int height = editor_config_total_height(&es);
                click_frame(&es, x, y);
                if (editor_config_total_height(&es) > height) grew = 1;
                if (editor_config_total_height(&es) > max_height)
                    max_height = editor_config_total_height(&es);
                CHECK(es.config_open == 1 || y < TOOLBAR_H + 28);
                CHECK(settle_probe(&es, x, y, before, undo_before, &changes) == 0);
                if (!es.config_open) click_frame(&es, PANEL_LABEL_X, TOOLBAR_H + 8);
            }
        }
        /* Scroll the config panel down for the next pass. */
        push_wheel(PANEL_LABEL_X, TOOLBAR_H + 100, -6.0f, 0);
        ui_frame(&es, PANEL_LABEL_X, TOOLBAR_H + 100);
    }
    CHECK(grew && max_height > base);
    CHECK(level_is_valid(&es));
    /* Scrolling and section toggles are view state, not document edits. */
    push_wheel(PANEL_LABEL_X, TOOLBAR_H + 100, 50.0f, 0);
    ui_frame(&es, PANEL_LABEL_X, TOOLBAR_H + 100);
    /* A trackpad's small fractions add up to a scroll instead of each
     * being cut to nothing: ten 0.1-notch swipes scroll one notch. */
    {
        /* The top edge of one field (found by its id), before and after. */
        int top_y = -1, scrolled_y = -1, field_id = 0;
        for (int y = TOOLBAR_H + 150; y < TOOLBAR_H + 300 && top_y < 0; y += 2) {
            click_frame(&es, PANEL_FIELD_X, y);
            if (es.ui.active_id) {
                top_y = y;
                field_id = es.ui.active_id;
            }
            key_frame(&es, KEY_ESCAPE, 0);
        }
        CHECK(top_y > 0);
        for (int i = 0; i < 10; i++) push_wheel(PANEL_LABEL_X, TOOLBAR_H + 100, -0.1f, 0);
        ui_frame(&es, PANEL_LABEL_X, TOOLBAR_H + 100);
        for (int y = top_y - 40; y <= top_y && scrolled_y < 0; y += 2) {
            click_frame(&es, PANEL_FIELD_X, y);
            if (es.ui.active_id == field_id) scrolled_y = y;
            key_frame(&es, KEY_ESCAPE, 0);
        }
        CHECK(scrolled_y > 0 && scrolled_y < top_y);
        push_wheel(PANEL_LABEL_X, TOOLBAR_H + 100, 50.0f, 0);
        ui_frame(&es, PANEL_LABEL_X, TOOLBAR_H + 100);
    }
    /* The header folds the whole panel to one row. */
    click_frame(&es, PANEL_LABEL_X, TOOLBAR_H + 8);
    CHECK(es.config_open == 0);
    click_frame(&es, PANEL_LABEL_X, TOOLBAR_H + 8);
    CHECK(es.config_open == 1);
done:
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

