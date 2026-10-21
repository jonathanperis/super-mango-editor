/* Included by tests/editor_ui_test.c. Not compiled alone: the functions are
 * static and use the helpers in that file.
 */
/* ------------------------------------------------------------------ */
/* Keyboard editing of the selection                                   */
/* ------------------------------------------------------------------ */

/* Place a coin at screen (sx, sy) with the Place tool, then select it. */
static int place_and_select_coin(EditorState *es, int sx, int sy)
{
    int before = es->level.coin_count;
    es->palette_type = ENT_COIN;
    es->tool = TOOL_PLACE;
    ui_frame(es, sx, sy);
    click_frame(es, sx, sy);
    es->tool = TOOL_SELECT;
    click_frame(es, sx, sy);
    return es->level.coin_count == before + 1 && es->selection.type == ENT_COIN ? 0 : -1;
}

/*
 * The mouse cursor follows what is under it: the I-beam on a field, the
 * four-way arrows while a coin is dragged, the arrow elsewhere, and the
 * system cursor is touched only when that changes.  (A scrollbar drag's
 * up-down arrows are checked on a bare UIState below.)
 */
static int mouse_cursor_follows_fields_and_drags(void)
{
    int failed = 0;
    EditorState es;
    const int sx = 300, sy = 300;
    CHECK(open_editor(&es, NULL) == 0);

    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(es.ui.cursor_shown == MOUSE_CURSOR_DEFAULT);
    int changes = es.ui.cursor_changes;
    ui_frame(&es, CANVAS_W + 90, CFG_SCREENS_Y);
    CHECK(es.ui.cursor_shown == MOUSE_CURSOR_IBEAM);
    CHECK(es.ui.cursor_changes == changes + 1);
    ui_frame(&es, CANVAS_W + 92, CFG_SCREENS_Y + 2);   /* still on it */
    CHECK(es.ui.cursor_changes == changes + 1);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(es.ui.cursor_shown == MOUSE_CURSOR_DEFAULT && es.ui.cursor_changes == changes + 2);

    CHECK(place_and_select_coin(&es, sx, sy) == 0);
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, 0, sx, sy);
    push_event(INPUT_MOUSE_MOVE, 0, 0, sx + 30, sy + 10);
    ui_frame(&es, sx + 30, sy + 10);
    CHECK(es.dragging && es.ui.cursor_shown == MOUSE_CURSOR_RESIZE_ALL);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, 0, sx + 30, sy + 10);
    ui_frame(&es, sx + 30, sy + 10);
    CHECK(!es.dragging && es.ui.cursor_shown == MOUSE_CURSOR_DEFAULT);

    /* A dragged scrollbar thumb shows the up-down arrows. */
    UIState ui;
    UIScroll scroll = {0};
    ui_init(&ui, NULL);
    ui_begin_frame(&ui);
    ui.mouse_x = 2; ui.mouse_y = 10; ui.mouse_clicked = 1; ui.mouse_down = 1;
    (void)ui_scrollbar_input(&ui, &scroll, 0, 0, 5, 100, 100, 400);
    CHECK(ui.scroll_drag == &scroll);
    ui_apply_cursor(&ui);
    CHECK(ui.cursor_shown == MOUSE_CURSOR_RESIZE_NS);
    ui_begin_frame(&ui);
    ui.mouse_down = 0;
    (void)ui_scrollbar_input(&ui, &scroll, 0, 0, 5, 100, 100, 400);
    ui_apply_cursor(&ui);
    CHECK(ui.cursor_shown == MOUSE_CURSOR_DEFAULT && ui.cursor_changes == 2);
done:
    close_editor(&es);
    return failed;
}

/*
 * Arrow keys nudge the selection (1 px, Shift = 16 px).  A quick run of
 * nudges is one undo step; a pause starts a new one.  Backspace deletes
 * the selection like Delete (Mac laptops have no Delete key).
 */
static int arrow_keys_nudge_and_backspace_deletes(void)
{
    int failed = 0;
    EditorState es;
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(place_and_select_coin(&es, 300, 300) == 0);
    const int coin = es.selection.index;
    const float x0 = es.level.coins[coin].x, y0 = es.level.coins[coin].y;
    const int undo_top = es.undo->top;

    key_frame(&es, KEY_RIGHT, 0);
    key_frame(&es, KEY_RIGHT, 0);
    key_frame(&es, KEY_RIGHT, 0);
    key_frame(&es, KEY_DOWN, INPUT_SHIFT);
    CHECK(es.level.coins[coin].x == x0 + 3.0f && es.level.coins[coin].y == y0 + 16.0f);
    CHECK(es.undo->top == undo_top + 1);          /* one step for the run */
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coins[coin].x == x0 && es.level.coins[coin].y == y0);
    key_frame(&es, KEY_Y, INPUT_CTRL);
    CHECK(es.level.coins[coin].x == x0 + 3.0f && es.level.coins[coin].y == y0 + 16.0f);

    /* After a pause, the next nudge is a step of its own. */
    es.nudge_ms -= 2 * NUDGE_COALESCE_MS;
    key_frame(&es, KEY_LEFT, INPUT_SHIFT);
    key_frame(&es, KEY_UP, 0);
    CHECK(es.level.coins[coin].x == x0 + 3.0f - 16.0f && es.level.coins[coin].y == y0 + 15.0f);
    CHECK(es.undo->top == undo_top + 2);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coins[coin].x == x0 + 3.0f && es.level.coins[coin].y == y0 + 16.0f);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coins[coin].x == x0 && es.level.coins[coin].y == y0);

    /* A nudge never leaves the world: at the left edge nothing moves. */
    es.level.coins[coin].x = 0.0f;
    key_frame(&es, KEY_LEFT, 0);
    CHECK(es.level.coins[coin].x == 0.0f && strstr(es.status_message, "Nothing moved"));
    es.level.coins[coin].x = x0;

    /* Backspace deletes the selection; undo brings it back. */
    const int coins = es.level.coin_count;
    key_frame(&es, KEY_BACKSPACE, 0);
    CHECK(es.level.coin_count == coins - 1 && es.selection.index < 0);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coin_count == coins);

    /* After a field commit with Enter the panel has the focus: a Backspace
     * still meant for the field deletes nothing.  Delete still does, and a
     * click on the canvas gives Backspace back. */
    CHECK(place_and_select_coin(&es, 360, 300) == 0);
    click_frame(&es, CANVAS_W + 60, CFG_NAME_Y);
    CHECK(es.ui.active_id != 0);
    key_frame(&es, KEY_ENTER, 0);
    CHECK(es.ui.active_id == 0 && es.selection.index >= 0);
    const int before_backspace = es.level.coin_count;
    key_frame(&es, KEY_BACKSPACE, 0);
    CHECK(es.level.coin_count == before_backspace && es.selection.index >= 0);
    CHECK(strstr(es.status_message, "Backspace deletes on the canvas") != NULL);
    key_frame(&es, KEY_DELETE, 0);
    CHECK(es.level.coin_count == before_backspace - 1);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(place_and_select_coin(&es, 420, 300) == 0);
    key_frame(&es, KEY_BACKSPACE, 0);
    CHECK(es.level.coin_count == before_backspace);
done:
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/*
 * Ctrl+D copies the selection one paste offset along and selects the copy,
 * so repeating it lays out a row.  The clipboard is not touched, and the
 * one-per-level entities refuse.
 */
static int ctrl_d_duplicates_with_a_stepping_offset(void)
{
    int failed = 0;
    EditorState es;
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(place_and_select_coin(&es, 300, 300) == 0);
    const float x0 = es.level.coins[0].x, y0 = es.level.coins[0].y;
    const int undo_top = es.undo->top;

    key_frame(&es, KEY_D, INPUT_CTRL);
    CHECK(es.level.coin_count == 2 && es.selection.index == 1);
    CHECK(es.level.coins[1].x == x0 + 24.0f && es.level.coins[1].y == y0 + 24.0f);
    key_frame(&es, KEY_D, INPUT_CTRL);
    CHECK(es.level.coin_count == 3 && es.selection.index == 2);
    CHECK(es.level.coins[2].x == x0 + 48.0f && es.level.coins[2].y == y0 + 48.0f);
    CHECK(es.undo->top == undo_top + 2 && es.clipboard_count == 0 && level_is_valid(&es));
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coin_count == 2);

    /* The player spawn is unique: nothing is added. */
    es.selection.type = ENT_PLAYER_SPAWN;
    es.selection.index = 0;
    key_frame(&es, KEY_D, INPUT_CTRL);
    CHECK(strstr(es.status_message, "cannot be duplicated") != NULL);
    CHECK(es.undo->top == undo_top + 1);
done:
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/* The screen pixel showing world point (wx, wy): canvas_screen_to_world
 * backwards. */
static void world_to_screen(const EditorState *es, float wx, float wy, float *sx, float *sy)
{
    float zoom = es->camera.zoom > 0.0f ? es->camera.zoom : 1.0f;
    *sx = (wx - es->camera.x) * zoom;
    *sy = (wy - es->camera.y) * zoom + (float)TOOLBAR_H;
}

/*
 * Snap to grid (key S) applies to placing as well as dragging; Shift
 * inverts it for one click or drag.  The status bar shows whether it is on.
 */
static int snap_toggle_applies_to_placing_and_dragging(void)
{
    int failed = 0;
    EditorState es;
    float wx, wy;
    CHECK(open_editor(&es, NULL) == 0);
    /* The status-bar labels around the indicator do not overlap. */
    CHECK(8 + ui_text_width(&es.ui, "Mouse: (1600, 300)") < 150);
    CHECK(150 + ui_text_width(&es.ui, "Snap: off") < 228);
    CHECK(228 + ui_text_width(&es.ui, "Tool: Delete") < STATUS_SUMMARY_X);
    CHECK(228 + ui_text_width(&es.ui, "Place: Bouncepad Medium") < STATUS_SUMMARY_X);
    /* The longest summary (the report keeps 256 messages) ends before the
     * entity count. */
    CHECK(STATUS_SUMMARY_X + ui_text_width(&es.ui, "Validation: 256 errors, 256 warnings") <
          STATUS_ENTITIES_X);
    {
        EditorValidationReport report = {0};
        report.error_count = 1;
        CHECK(strcmp(editor_validation_summary(&report), "Validation: 1 error") == 0);
        report.warning_count = 2;
        CHECK(strcmp(editor_validation_summary(&report), "Validation: 1 error, 2 warnings") == 0);
        report.error_count = 0;
        CHECK(strcmp(editor_validation_summary(&report), "Validation: 2 warnings") == 0);
    }

    CHECK(es.snap_to_grid == 0);
    key_frame(&es, KEY_S, 0);
    CHECK(es.snap_to_grid == 1 && strstr(es.status_message, "Snap to grid on"));

    /* A placement lands on the grid cell's corner... */
    es.palette_type = ENT_COIN;
    es.tool = TOOL_PLACE;
    const int sx = 317, sy = 251;
    canvas_screen_to_world(&es, sx, sy, &wx, &wy);
    CHECK(fmodf(wx, (float)TILE_SIZE) != 0.0f);
    ui_frame(&es, sx, sy);
    click_frame(&es, sx, sy);
    CHECK(es.level.coin_count == 1);
    CHECK(es.level.coins[0].x == floorf(wx / TILE_SIZE) * TILE_SIZE);
    CHECK(es.level.coins[0].y == floorf(wy / TILE_SIZE) * TILE_SIZE);
    /* ...unless Shift is held, which places freely. */
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, INPUT_SHIFT, sx, sy);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, INPUT_SHIFT, sx, sy);
    ui_frame(&es, sx, sy);
    CHECK(es.level.coin_count == 2 && es.level.coins[1].x == wx);

    /* A drag of the free coin snaps its corner onto the grid. */
    es.tool = TOOL_SELECT;
    int cx = sx + 2, cy = sy + 2;
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, 0, cx, cy);
    push_event(INPUT_MOUSE_MOVE, 0, 0, cx + 70, cy + 9);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, 0, cx + 70, cy + 9);
    ui_frame(&es, cx + 70, cy + 9);
    CHECK(es.selection.type == ENT_COIN);
    {
        const CoinPlacement *moved = &es.level.coins[es.selection.index];
        CHECK(fmodf(moved->x, (float)TILE_SIZE) == 0.0f);
        CHECK(fmodf(moved->y, (float)TILE_SIZE) == 0.0f);
    }

    /* Select: Shift pressed once the drag is under way moves freely, read
     * at each motion event; the position at release is the one kept. */
    {
        const int index = es.selection.index;
        float gx, gy, before_x = es.level.coins[index].x;
        world_to_screen(&es, es.level.coins[index].x + 4.0f,
                        es.level.coins[index].y + 4.0f, &gx, &gy);
        push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, 0, (int)gx, (int)gy);
        push_event(INPUT_MOUSE_MOVE, 0, INPUT_SHIFT, (int)gx + 31, (int)gy + 5);
        push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, INPUT_SHIFT, (int)gx + 31, (int)gy + 5);
        ui_frame(&es, (int)gx + 31, (int)gy + 5);
        CHECK(es.selection.index == index && es.level.coins[index].x != before_x);
        CHECK(fmodf(es.level.coins[index].x, (float)TILE_SIZE) != 0.0f);
        /* Shift at the press does not drag at all: it takes the coin out
         * of the selection. */
        before_x = es.level.coins[index].x;
        world_to_screen(&es, es.level.coins[index].x + 4.0f,
                        es.level.coins[index].y + 4.0f, &gx, &gy);
        push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, INPUT_SHIFT, (int)gx, (int)gy);
        push_event(INPUT_MOUSE_MOVE, 0, INPUT_SHIFT, (int)gx + 60, (int)gy);
        push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, INPUT_SHIFT, (int)gx + 60, (int)gy);
        ui_frame(&es, (int)gx + 60, (int)gy);
        CHECK(es.level.coins[index].x == before_x && editor_selection_count(&es) == 0);
    }

    key_frame(&es, KEY_S, 0);
    CHECK(es.snap_to_grid == 0);
done:
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/*
 * Overlapping entities: a click picks the topmost; Alt+click, or a second
 * click on the very same spot, steps to the next one underneath and wraps.
 */
static int alt_click_cycles_through_overlapping_entities(void)
{
    int failed = 0;
    EditorState es;
    const int sx = 300, sy = 300;
    CHECK(open_editor(&es, NULL) == 0);
    es.tool = TOOL_PLACE;
    es.palette_type = ENT_COIN;
    click_frame(&es, sx, sy);
    es.palette_type = ENT_STAR_YELLOW;            /* drawn above coins */
    click_frame(&es, sx, sy);
    CHECK(es.level.coin_count == 1 && es.level.star_yellow_count == 1);
    es.tool = TOOL_SELECT;

    click_frame(&es, sx + 2, sy + 2);
    CHECK(es.selection.type == ENT_STAR_YELLOW);
    CHECK(strstr(es.status_message, "1 of 2") != NULL);
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, INPUT_ALT, sx + 5, sy + 5);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, INPUT_ALT, sx + 5, sy + 5);
    ui_frame(&es, sx + 5, sy + 5);
    CHECK(es.selection.type == ENT_COIN && strstr(es.status_message, "2 of 2"));
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, INPUT_ALT, sx + 5, sy + 5);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, INPUT_ALT, sx + 5, sy + 5);
    ui_frame(&es, sx + 5, sy + 5);
    CHECK(es.selection.type == ENT_STAR_YELLOW);   /* wrapped to the top */

    /* A second plain click on the same spot steps down too... */
    click_frame(&es, sx + 5, sy + 5);
    CHECK(es.selection.type == ENT_COIN);
    /* ...but a click somewhere else starts again from the top. */
    click_frame(&es, sx + 3, sy + 1);
    CHECK(es.selection.type == ENT_STAR_YELLOW);
    /* Nothing changed in the document: picking is not an edit. */
    CHECK(es.undo->top == 2);
done:
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/*
 * Clickable validation messages (spec N-001): clicking a message selects
 * the entity it is about and pans the canvas to it, or focuses the Level
 * Config field.  The status-bar summary goes to the first message.
 */
static int validation_messages_take_you_to_the_problem(void)
{
    int failed = 0;
    EditorState es;
    /* First message row: below the Level Config title and its summary. */
    const int row_x = CANVAS_W + 60, row_y = TOOLBAR_H + 28 + 8 + 20 + 9;
    CHECK(open_editor(&es, NULL) == 0);
    es.level.screen_count = 20;
    es.level.coin_count = 3;
    for (int i = 0; i < 3; i++)
        es.level.coins[i] = (CoinPlacement){200.0f + 40.0f * (float)i, 100.0f};
    es.level.checkpoint_count = 1;
    es.level.checkpoints[0] = (CheckpointPlacement){6000.0f, 100.0f};
    CHECK(level_is_valid(&es));

    /* A checkpoint far off to the right goes out of the world. */
    es.level.screen_count = 4;
    es.camera.x = 0.0f;
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(es.validation_report.error_count > 0);
    CHECK(strcmp(es.validation_report.locations[0].path, "checkpoints") == 0);
    click_frame(&es, row_x, row_y);
    CHECK(es.selection.type == ENT_CHECKPOINT && es.selection.index == 0);
    CHECK(es.tool == TOOL_SELECT && es.camera.x > 0.0f);
    CHECK(strstr(es.status_message, "Checkpoint 0") != NULL);
    /* ...and its x field takes the caret (properties.c numbers entity
     * fields type * 100 + field + 1; x is the checkpoint's field 0). */
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(es.ui.active_id == (int)ENT_CHECKPOINT * 100 + 1);
    key_frame(&es, KEY_ESCAPE, 0);
    es.level.checkpoints[0].x = 900.0f;

    /* A bad config value focuses its field. */
    es.level.coin_score = -5;
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(strcmp(es.validation_report.locations[0].path, "coin_score") == 0);
    click_frame(&es, row_x, row_y);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(es.ui.active_id == 9012);
    key_frame(&es, KEY_ESCAPE, 0);
    es.level.coin_score = 10;

    /* The status-bar summary jumps to the first message's entity. */
    es.level.coins[2].y = 9999.0f;
    es.selection.index = -1;
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    click_frame(&es, STATUS_SUMMARY_X + 5, EDITOR_H - STATUS_H / 2);
    CHECK(es.selection.type == ENT_COIN && es.selection.index == 2);
    /* The next frame draws the coin's fields with y holding the caret. */
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(es.ui.active_id == (int)ENT_COIN * 100 + 2);
    key_frame(&es, KEY_ESCAPE, 0);
    es.level.coins[2].y = 100.0f;

    /* An enemy's bad vx focuses its vx field (spider field 1). */
    es.level.spider_count = 1;
    es.level.spiders[0] = (SpiderPlacement){300.0f, 0.0f, 250.0f, 350.0f, 0};
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(strcmp(es.validation_report.locations[0].field, "vx") == 0);
    CHECK(editor_focus_validation_issue(&es, 0) == 1);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(es.selection.type == ENT_SPIDER && es.ui.active_id == (int)ENT_SPIDER * 100 + 2);
    key_frame(&es, KEY_ESCAPE, 0);
    es.level.spiders[0].vx = 50.0f;

    /* A dropdown takes the focus by opening: the axe's mode (field 1)... */
    es.level.axe_trap_count = 1;
    es.level.axe_traps[0] = (AxeTrapPlacement){.pillar_x = 500.0f, .mode = (AxeTrapMode)9};
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(editor_focus_validation_issue(&es, 0) == 1);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(es.ui.dropdown_open_id == (int)ENT_AXE_TRAP * 100 + 2);
    click_frame(&es, NEUTRAL_X, NEUTRAL_Y);          /* closes the list */
    es.level.axe_trap_count = 0;

    /* ...and a Level Config list row: layer 1's asset dropdown unfolds
     * its group and opens, its speed field takes the caret. */
    g_plx_open = 0;
    es.level.background_layer_count = 2;
    snprintf(es.level.background_layers[0].path, sizeof(es.level.background_layers[0].path),
             "assets/sprites/backgrounds/sky_blue.png");
    snprintf(es.level.background_layers[1].path, sizeof(es.level.background_layers[1].path),
             "assets/sprites/backgrounds/sky.txt");
    es.level.background_layers[1].speed = 0.5f;
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(strcmp(es.validation_report.locations[0].path, "background_layers") == 0);
    CHECK(editor_focus_validation_issue(&es, 0) == 1);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(g_plx_open == 1 && es.ui.dropdown_open_id == 9201);
    click_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    {
        LevelIssueLocation speed = {"background_layers", 1, "speed"};
        CHECK(properties_focus_config(&es, &speed) == 1);
        ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
        CHECK(es.ui.active_id == 9101);
        key_frame(&es, KEY_ESCAPE, 0);
    }
done:
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/*
 * Every validation row can be reached: more rows than fit scroll with the
 * wheel over them, each one is clickable, and the row clicked last is
 * what Ctrl+C copies (until a click elsewhere).
 */
static int validation_rows_scroll_and_copy(void)
{
    int failed = 0;
    EditorState es;
    const int row_x = CANVAS_W + 60, row_y = TOOLBAR_H + 28 + 8 + 20 + 9;
    char first[EDITOR_VALIDATION_MESSAGE_LEN];
    CHECK(open_editor(&es, NULL) == 0);

    /* Twelve coins below the world: twelve errors, eight rows shown. */
    es.level.coin_count = 12;
    for (int i = 0; i < 12; i++)
        es.level.coins[i] = (CoinPlacement){100.0f + 30.0f * (float)i, 9999.0f};
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(es.validation_report.message_count == 12);
    CHECK(es.message_scroll.offset == 0);
    snprintf(first, sizeof(first), "%s", es.validation_report.messages[0]);

    /* The first row takes you to coin 0, and Ctrl+C copies the row. */
    click_frame(&es, row_x, row_y);
    CHECK(es.selection.type == ENT_COIN && es.selection.index == 0);
    CHECK(es.focus_area == EDITOR_FOCUS_MESSAGES);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    key_frame(&es, KEY_C, INPUT_CTRL);
    CHECK(strcmp(editor_test_clipboard(), first) == 0);
    CHECK(strncmp(es.status_message, "Copied: coins[0]", 16) == 0);
    /* Text selected in the field the row opened is the newer choice:
     * Ctrl+C now copies that, as Ctrl+X would cut it. */
    CHECK(es.ui.active_id != 0);
    key_frame(&es, KEY_A, INPUT_CTRL);
    key_frame(&es, KEY_C, INPUT_CTRL);
    CHECK(es.ui.edit_buf[0] != '\0');
    CHECK(strcmp(editor_test_clipboard(), es.ui.edit_buf) == 0);
    key_frame(&es, KEY_ESCAPE, 0);

    /* The wheel over the rows scrolls them, not the panel; the first row
     * shown is now message 4, and clicking it goes to coin 4. */
    push_wheel(row_x, row_y, -4.0f, 0);
    ui_frame(&es, row_x, row_y);
    CHECK(es.message_scroll.offset == 4);
    click_frame(&es, row_x, row_y);
    CHECK(es.selection.type == ENT_COIN && es.selection.index == 4);
    key_frame(&es, KEY_ESCAPE, 0);
    /* It stops at the last full window: rows 5-12. */
    push_wheel(row_x, row_y, -20.0f, 0);
    ui_frame(&es, row_x, row_y);
    CHECK(es.message_scroll.offset == 12 - EDITOR_VALIDATION_VISIBLE_ROWS);
    click_frame(&es, row_x, row_y + 18 * (EDITOR_VALIDATION_VISIBLE_ROWS - 1));
    CHECK(es.selection.type == ENT_COIN && es.selection.index == 11);
    key_frame(&es, KEY_ESCAPE, 0);

    /* A click on the canvas takes the focus away: Ctrl+C copies entities
     * again, not the row. */
    editor_test_set_clipboard("");
    click_frame(&es, NEUTRAL_X, TOOLBAR_H + 200);
    CHECK(es.focus_area == EDITOR_FOCUS_CANVAS);
    key_frame(&es, KEY_C, INPUT_CTRL);
    CHECK(editor_test_clipboard()[0] == '\0');

    /* Folded, Level Config draws no rows, so the wheel over that spot must
     * not be taken for the hidden list: the palette is there now. */
    CHECK(properties_message_list_contains(row_x, row_y));
    es.config_open = 0;
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(!properties_message_list_contains(row_x, row_y));
done:
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/* Write `text` to `path`; 0 on success. */
static int write_text_file(const char *path, const char *text)
{
    FILE *file = fopen(path, "wb");
    if (!file) return -1;
    if (fputs(text, file) == EOF) { fclose(file); return -1; }
    return fclose(file) == 0 ? 0 : -1;
}

/*
 * A file that will not open says why, in the Level Config panel: the TOML
 * syntax error with its line, or every runtime rule the file breaks with
 * the line of each value.  The document stays as it was, and clicking the
 * heading hides the list.
 */
static int files_that_will_not_open_list_why(void)
{
    int failed = 0;
    EditorState es;
    const char *broken = TEST_OUT "ui_broken_syntax.toml";
    const char *invalid = TEST_OUT "ui_invalid_rules.toml";
    /* The list starts the Level Config content, above the summary. */
    const int heading_x = CANVAS_W + 60, heading_y = TOOLBAR_H + 28 + 8 + 9;
    CHECK(open_editor(&es, NULL) == 0);
    uint64_t before = doc_hash(&es);

    /* Line 3 is not "key = value". */
    CHECK(write_text_file(broken,
                          "format_version = 1\n"
                          "name = \"Broken\"\n"
                          "screen_count 4\n") == 0);
    CHECK(editor_load_level(&es, broken) != 0);
    CHECK(doc_hash(&es) == before);
    CHECK(es.load_report.count == 1);
    CHECK(strncmp(es.load_report.messages[0], "line 3: TOML syntax", 19) == 0);
    CHECK(strstr(es.status_message, "line 3") != NULL);

    /* Well-formed TOML that breaks two runtime rules: both are listed,
     * each with the line of its value. */
    CHECK(write_text_file(invalid,
                          "format_version = 1\n"
                          "name = \"Invalid\"\n"
                          "screen_count = 4\n"
                          "\n"
                          "[[coins]]\n"
                          "x = 99999.0\n"
                          "y = 100.0\n"
                          "\n"
                          "[[spiders]]\n"
                          "x = 300.0\n"
                          "vx = 0.0\n"
                          "patrol_x0 = 250.0\n"
                          "patrol_x1 = 350.0\n"
                          "frame_index = 0\n") == 0);
    CHECK(editor_load_level(&es, invalid) != 0);
    CHECK(es.load_report.count == 2);
    CHECK(strncmp(es.load_report.messages[0], "line 11: spiders[0].vx", 22) == 0);
    CHECK(strncmp(es.load_report.messages[1], "line 6: coins[0].x", 18) == 0);
    CHECK(strstr(es.load_report.file, "ui_invalid_rules.toml") != NULL);

    /* The panel shows them; a row can be selected and copied... */
    CHECK(es.config_open == 1);
    int with_list = editor_config_total_height(&es);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    click_frame(&es, heading_x, heading_y + 18);
    CHECK(es.focus_area == EDITOR_FOCUS_MESSAGES && es.load_report.count == 2);
    key_frame(&es, KEY_C, INPUT_CTRL);
    CHECK(strncmp(editor_test_clipboard(), "line 11: spiders[0].vx", 22) == 0);
    /* ...and clicking the heading hides them. */
    click_frame(&es, heading_x, heading_y);
    CHECK(es.load_report.count == 0);
    CHECK(editor_config_total_height(&es) < with_list);
    CHECK(doc_hash(&es) == before);
done:
    clear_dialog_seams();
    close_editor(&es);
    (void)remove(broken);
    (void)remove(invalid);
    return failed;
}
