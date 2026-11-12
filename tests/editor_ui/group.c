/* Included by tests/editor_ui_test.c. Not compiled alone: the functions are
 * static and use the helpers in that file.
 */
/*
 * Multi-select: a box on empty canvas selects what it touches, Shift+click
 * adds or removes one entity, and move (drag or arrows), delete, copy /
 * paste and duplicate act on the whole group as ONE undo step each.
 */
static int box_and_shift_select_act_on_the_group(void)
{
    int failed = 0;
    EditorState es;
    float x[3], y[3];
    CHECK(open_editor(&es, NULL) == 0);
    es.tool = TOOL_PLACE;
    es.palette_type = ENT_COIN;
    for (int i = 0; i < 3; i++) click_frame(&es, 200 + 60 * i, 300);
    CHECK(es.level.coin_count == 3);
    es.tool = TOOL_SELECT;
    for (int i = 0; i < 3; i++) { x[i] = es.level.coins[i].x; y[i] = es.level.coins[i].y; }

    /* Box from empty canvas over all three coins. */
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, 0, 150, 250);
    push_event(INPUT_MOUSE_MOVE, 0, 0, 250, 300);
    push_event(INPUT_MOUSE_MOVE, 0, 0, 350, 350);
    ui_frame(&es, 350, 350);
    CHECK(es.box_selecting);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, 0, 350, 350);
    ui_frame(&es, 350, 350);
    CHECK(!es.box_selecting && editor_selection_count(&es) == 3);
    for (int i = 0; i < 3; i++) CHECK(editor_is_selected(&es, ENT_COIN, i));
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);           /* "3 selected" panel */

    /* Dragging one member moves all three: one undo step. */
    int undo_top = es.undo->top;
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, 0, 262, 302);
    push_event(INPUT_MOUSE_MOVE, 0, 0, 282, 302);
    push_event(INPUT_MOUSE_MOVE, 0, 0, 302, 302);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, 0, 302, 302);
    ui_frame(&es, 302, 302);
    for (int i = 0; i < 3; i++) CHECK(es.level.coins[i].x == x[i] + 20.0f);
    CHECK(es.undo->top == undo_top + 1 && editor_selection_count(&es) == 3);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    for (int i = 0; i < 3; i++) CHECK(es.level.coins[i].x == x[i]);
    CHECK(es.undo->top == undo_top);
    key_frame(&es, KEY_Y, INPUT_CTRL);
    for (int i = 0; i < 3; i++) CHECK(es.level.coins[i].x == x[i] + 20.0f);
    key_frame(&es, KEY_Z, INPUT_CTRL);

    /* The arrows nudge the group. */
    key_frame(&es, KEY_DOWN, 0);
    for (int i = 0; i < 3; i++) CHECK(es.level.coins[i].y == y[i] + 1.0f);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    for (int i = 0; i < 3; i++) CHECK(es.level.coins[i].y == y[i]);

    /* Shift+click takes one out, and puts it back. */
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, INPUT_SHIFT, 262, 302);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, INPUT_SHIFT, 262, 302);
    ui_frame(&es, 262, 302);
    CHECK(editor_selection_count(&es) == 2 && !editor_is_selected(&es, ENT_COIN, 1));
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, INPUT_SHIFT, 262, 302);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, INPUT_SHIFT, 262, 302);
    ui_frame(&es, 262, 302);
    CHECK(editor_selection_count(&es) == 3);

    /* Copy and paste the group: three copies, selected, one undo step. */
    undo_top = es.undo->top;
    key_frame(&es, KEY_C, INPUT_CTRL);
    CHECK(es.clipboard_count == 3);
    key_frame(&es, KEY_V, INPUT_CTRL);
    CHECK(es.level.coin_count == 6 && editor_selection_count(&es) == 3);
    for (int i = 3; i < 6; i++) CHECK(editor_is_selected(&es, ENT_COIN, i));
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coin_count == 3 && es.undo->top == undo_top);
    /* Redo brings the copies back selected together, as the paste left them. */
    key_frame(&es, KEY_Y, INPUT_CTRL);
    CHECK(es.level.coin_count == 6 && editor_selection_count(&es) == 3);
    for (int i = 3; i < 6; i++) CHECK(editor_is_selected(&es, ENT_COIN, i));
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coin_count == 3 && es.undo->top == undo_top);

    /* Duplicate the group, then undo it in one step. */
    CHECK(editor_select_items(&es, (Selection[]){{ENT_COIN, 0}, {ENT_COIN, 1}, {ENT_COIN, 2}}, 3) == 3);
    key_frame(&es, KEY_D, INPUT_CTRL);
    CHECK(es.level.coin_count == 6 && editor_selection_count(&es) == 3);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coin_count == 3);

    /* Delete removes the whole group; one Ctrl+Z brings all three back. */
    CHECK(editor_select_items(&es, (Selection[]){{ENT_COIN, 0}, {ENT_COIN, 1}, {ENT_COIN, 2}}, 3) == 3);
    key_frame(&es, KEY_DELETE, 0);
    CHECK(es.level.coin_count == 0 && editor_selection_count(&es) == 0);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coin_count == 3);
    for (int i = 0; i < 3; i++) CHECK(es.level.coins[i].x == x[i] && es.level.coins[i].y == y[i]);

    /* The Delete tool, or a right-click, on a member deletes the whole
     * selection as one step; on anything else just that entity. */
    CHECK(editor_select_items(&es, (Selection[]){{ENT_COIN, 0}, {ENT_COIN, 1}}, 2) == 2);
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_RIGHT, 0, 322, 302);   /* coin 2: not selected */
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_RIGHT, 0, 322, 302);
    ui_frame(&es, 322, 302);
    CHECK(es.level.coin_count == 2 && editor_selection_count(&es) == 2);
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_RIGHT, 0, 262, 302);   /* coin 1: a member */
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_RIGHT, 0, 262, 302);
    ui_frame(&es, 262, 302);
    CHECK(es.level.coin_count == 0 && editor_selection_count(&es) == 0);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coin_count == 2);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coin_count == 3);
    CHECK(editor_select_items(&es, (Selection[]){{ENT_COIN, 1}, {ENT_COIN, 2}}, 2) == 2);
    es.tool = TOOL_DELETE;
    click_frame(&es, 322, 302);
    CHECK(es.level.coin_count == 1 && es.level.coins[0].x == x[0]);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coin_count == 3);
    es.tool = TOOL_SELECT;

    /* A plain click on one member (no drag) selects just it. */
    CHECK(editor_select_items(&es, (Selection[]){{ENT_COIN, 0}, {ENT_COIN, 1}, {ENT_COIN, 2}}, 3) == 3);
    click_frame(&es, 322, 302);
    CHECK(editor_selection_count(&es) == 1 && es.selection.index == 2);
    /* Shift+box adds to the selection; Esc clears it. */
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, INPUT_SHIFT, 150, 250);
    push_event(INPUT_MOUSE_MOVE, 0, INPUT_SHIFT, 230, 350);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, INPUT_SHIFT, 230, 350);
    ui_frame(&es, 230, 350);
    CHECK(editor_selection_count(&es) == 2 && editor_is_selected(&es, ENT_COIN, 0));
    key_frame(&es, KEY_ESCAPE, 0);
    CHECK(editor_selection_count(&es) == 0);
    CHECK(level_is_valid(&es));
done:
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}


/*
 * A selection holds at most EDITOR_MAX_SELECTION entities.  Shift+click on
 * one more, on the canvas or in the Entities list, adds nothing and says
 * why, in the words a box selection that reaches the limit uses.
 */
static int shift_click_on_a_full_selection_says_so(void)
{
    int failed = 0;
    EditorState es;
    PlacementData pd;
    EditorRect spawn;
    static Selection coins[EDITOR_MAX_SELECTION];
    float fx, fy;
    int sx, sy;
    CHECK(open_editor(&es, NULL) == 0);
    es.level.screen_count = 4;
    es.tool = TOOL_SELECT;

    /* A full selection of coins in a row above the player spawn. */
    for (int i = 0; i < EDITOR_MAX_SELECTION; i++) {
        memset(&pd, 0, sizeof(pd));
        pd.coin.x = 40.0f + 12.0f * (float)i;
        pd.coin.y = 40.0f;
        CHECK(add_entity(&es, ENT_COIN, &pd) == i);
        coins[i].type = ENT_COIN;
        coins[i].index = i;
    }
    CHECK(editor_select_items(&es, coins, EDITOR_MAX_SELECTION) == EDITOR_MAX_SELECTION);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);

    /* Shift+click on the player spawn, on the canvas. */
    CHECK(editor_entity_bounds(&es.level, ENT_PLAYER_SPAWN, 0, &spawn));
    world_to_screen(&es, spawn.x + spawn.w / 2.0f, spawn.y + spawn.h / 2.0f, &fx, &fy);
    sx = (int)roundf(fx);
    sy = (int)roundf(fy);
    es.status_message[0] = '\0';
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, INPUT_SHIFT, sx, sy);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, INPUT_SHIFT, sx, sy);
    ui_frame(&es, sx, sy);
    CHECK(editor_selection_count(&es) == EDITOR_MAX_SELECTION);
    CHECK(!editor_is_selected(&es, ENT_PLAYER_SPAWN, 0));
    CHECK(strstr(es.status_message, "a selection holds at most 64") != NULL);

    /* The same in the Entities list (searched, so the row is in view). */
    list_search(&es, "spawn");
    key_frame(&es, KEY_ENTER, 0);               /* keep the search, leave the field */
    CHECK(es.entity_list_open);
    sy = list_row_y(&es, ENT_PLAYER_SPAWN, 0);
    CHECK(sy > 0);
    es.status_message[0] = '\0';
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, INPUT_SHIFT, LIST_X, sy);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, INPUT_SHIFT, LIST_X, sy);
    ui_frame(&es, LIST_X, sy);
    CHECK(editor_selection_count(&es) == EDITOR_MAX_SELECTION);
    CHECK(!editor_is_selected(&es, ENT_PLAYER_SPAWN, 0));
    CHECK(strstr(es.status_message, "a selection holds at most 64") != NULL);

    /* Taking one out still works, and then there is room again. */
    list_search(&es, "coin");
    key_frame(&es, KEY_ENTER, 0);
    sy = list_row_y(&es, ENT_COIN, 0);
    CHECK(sy > 0);
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, INPUT_SHIFT, LIST_X, sy);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, INPUT_SHIFT, LIST_X, sy);
    ui_frame(&es, LIST_X, sy);
    CHECK(editor_selection_count(&es) == EDITOR_MAX_SELECTION - 1);
    list_search(&es, "spawn");
    key_frame(&es, KEY_ENTER, 0);
    sy = list_row_y(&es, ENT_PLAYER_SPAWN, 0);
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, INPUT_SHIFT, LIST_X, sy);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, INPUT_SHIFT, LIST_X, sy);
    ui_frame(&es, LIST_X, sy);
    CHECK(editor_selection_count(&es) == EDITOR_MAX_SELECTION);
    CHECK(editor_is_selected(&es, ENT_PLAYER_SPAWN, 0));
done:
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}
