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

