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

