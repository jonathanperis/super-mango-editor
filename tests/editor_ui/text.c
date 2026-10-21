/* Included by tests/editor_ui_test.c. Not compiled alone: the functions are
 * static and use the helpers in that file.
 */
/* ------------------------------------------------------------------ */
/* Text editing: caret keys and Tab                                    */
/* ------------------------------------------------------------------ */


static void text_frame(EditorState *es, const char *text)
{
    push_text(text);
    ui_frame(es, NEUTRAL_X, NEUTRAL_Y);
}

/*
 * Fields used to append and backspace at the end only.  The caret now
 * moves with Left/Right/Home/End, typing inserts at it, Backspace and
 * Delete remove the whole UTF-8 character on either side, and Tab /
 * Shift+Tab commit the field and move to the next / previous one.
 */
static int text_fields_move_the_caret_and_tab_between_fields(void)
{
    int failed = 0;
    EditorState es;
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(strcmp(es.level.name, "Untitled") == 0);

    click_frame(&es, CANVAS_W + 60, CFG_NAME_Y);
    CHECK(es.ui.active_id == 9000);
    key_frame(&es, KEY_HOME, 0);
    text_frame(&es, "\xc3\xa9");                 /* é, two bytes */
    CHECK(strcmp(es.ui.edit_buf, "\xc3\xa9Untitled") == 0 && es.ui.edit_cursor == 2);
    key_frame(&es, KEY_RIGHT, 0);                 /* past the U */
    key_frame(&es, KEY_DELETE, 0);                /* the n goes */
    key_frame(&es, KEY_END, 0);
    text_frame(&es, "!");
    key_frame(&es, KEY_LEFT, 0);
    key_frame(&es, KEY_LEFT, 0);
    key_frame(&es, KEY_BACKSPACE, 0);             /* the e before d */
    CHECK(strcmp(es.ui.edit_buf, "\xc3\xa9Utitld!") == 0);
    /* Left and Backspace step over the whole two-byte character. */
    key_frame(&es, KEY_HOME, 0);
    key_frame(&es, KEY_RIGHT, 0);
    CHECK(es.ui.edit_cursor == 2);
    key_frame(&es, KEY_BACKSPACE, 0);
    CHECK(strcmp(es.ui.edit_buf, "Utitld!") == 0 && es.ui.edit_cursor == 0);
    /* Keys and text in one frame keep their order: x, Right, y. */
    {
        InputEvent event = {0};
        event.type = INPUT_TEXT;
        event.text[0] = 'x';
        input_push(&event);
        push_key(KEY_RIGHT, 0);
        event.text[0] = 'y';
        input_push(&event);
        ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    }
    CHECK(strcmp(es.ui.edit_buf, "xUytitld!") == 0);
    key_frame(&es, KEY_ENTER, 0);
    CHECK(es.ui.active_id == 0 && strcmp(es.level.name, "xUytitld!") == 0);

    /* Tab commits the typed value as one undo step and moves on. */
    int undo_top = es.undo->top;
    click_frame(&es, CANVAS_W + 90, CFG_SCREENS_Y);
    CHECK(es.ui.active_id == 9011);
    key_frame(&es, KEY_END, 0);
    key_frame(&es, KEY_BACKSPACE, 0);
    text_frame(&es, "5");
    key_frame(&es, KEY_TAB, 0);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(es.level.screen_count == 5 && es.undo->top == undo_top + 1);
    CHECK(es.ui.active_id != 0 && es.ui.active_id != 9011);
    /* Shift+Tab comes back, with nothing to commit. */
    key_frame(&es, KEY_TAB, INPUT_SHIFT);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(es.ui.active_id == 9011 && es.undo->top == undo_top + 1);
    /* An invalid value keeps the focus where it is. */
    key_frame(&es, KEY_END, 0);
    key_frame(&es, KEY_BACKSPACE, 0);
    text_frame(&es, "-");
    key_frame(&es, KEY_TAB, 0);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(es.ui.active_id == 9011 && es.level.screen_count == 5);
    key_frame(&es, KEY_ESCAPE, 0);
    CHECK(es.ui.active_id == 0);

    /* Dropdowns are in the Tab order: Tab from the axe's y field opens its
     * mode list, Down + Enter picks Spin as one undo step, and Tab from an
     * open list closes it and moves on. */
    es.level.axe_trap_count = 1;
    es.level.axe_traps[0] = (AxeTrapPlacement){.pillar_x = 500.0f, .mode = AXE_MODE_PENDULUM};
    editor_select_only(&es, ENT_AXE_TRAP, 0);
    es.panel_open = 1;
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    ui_focus_field(&es.ui, (int)ENT_AXE_TRAP * 100 + 4);          /* y */
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(es.ui.active_id == (int)ENT_AXE_TRAP * 100 + 4);
    undo_top = es.undo->top;
    key_frame(&es, KEY_TAB, 0);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(es.ui.active_id == 0 && es.ui.dropdown_open_id == (int)ENT_AXE_TRAP * 100 + 2);
    key_frame(&es, KEY_DOWN, 0);
    key_frame(&es, KEY_DOWN, 0);                                  /* stops at Spin */
    CHECK(es.ui.dropdown_highlight == 1 && es.level.axe_traps[0].mode == AXE_MODE_PENDULUM);
    key_frame(&es, KEY_ENTER, 0);
    CHECK(es.ui.dropdown_open_id == 0 && es.level.axe_traps[0].mode == AXE_MODE_SPIN);
    CHECK(es.undo->top == undo_top + 1);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.axe_traps[0].mode == AXE_MODE_PENDULUM);
    /* Shift+Tab from the reopened list goes back to y; Esc just closes. */
    ui_focus_field(&es.ui, (int)ENT_AXE_TRAP * 100 + 2);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(es.ui.dropdown_open_id == (int)ENT_AXE_TRAP * 100 + 2);
    key_frame(&es, KEY_TAB, INPUT_SHIFT);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(es.ui.dropdown_open_id == 0 && es.ui.active_id == (int)ENT_AXE_TRAP * 100 + 4);
    key_frame(&es, KEY_ESCAPE, 0);
    ui_focus_field(&es.ui, (int)ENT_AXE_TRAP * 100 + 2);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    key_frame(&es, KEY_ESCAPE, 0);
    CHECK(es.ui.dropdown_open_id == 0 && es.level.axe_traps[0].mode == AXE_MODE_PENDULUM);
done:
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/*
 * Text selection: Shift+arrows, Ctrl+A and a mouse drag select; Ctrl+Left /
 * Right jump a word (Shift: selecting it); Ctrl+C / X / V and typing act on
 * the selection; all in whole UTF-8 characters.
 */
/*
 * Up / Down step the active number field (Shift: ten steps) inside its
 * limits, as a staged edit that Return commits in one undo step.  Text
 * fields ignore them.
 */
static int up_and_down_step_number_fields(void)
{
    int failed = 0;
    EditorState es;
    CHECK(open_editor(&es, NULL) == 0);
    int undo_top = es.undo->top;
    CHECK(es.level.screen_count == 4);

    click_frame(&es, CANVAS_W + 90, CFG_SCREENS_Y);
    CHECK(es.ui.active_id == 9011);
    key_frame(&es, KEY_UP, 0);
    CHECK(strcmp(es.ui.edit_buf, "5") == 0);
    key_frame(&es, KEY_UP, INPUT_SHIFT);
    CHECK(strcmp(es.ui.edit_buf, "15") == 0);
    CHECK(es.level.screen_count == 4);            /* staged, not stored */
    for (int i = 0; i < 10; i++) key_frame(&es, KEY_UP, INPUT_SHIFT);
    CHECK(strcmp(es.ui.edit_buf, "99") == 0);     /* the field's maximum */
    key_frame(&es, KEY_DOWN, INPUT_SHIFT);
    CHECK(strcmp(es.ui.edit_buf, "89") == 0);
    key_frame(&es, KEY_ENTER, 0);
    CHECK(es.ui.active_id == 0 && es.level.screen_count == 89);
    CHECK(es.undo->top == undo_top + 1);

    /* Down stops at the minimum; a half-typed value steps from the
     * stored one. */
    click_frame(&es, CANVAS_W + 90, CFG_SCREENS_Y);
    key_frame(&es, KEY_END, 0);
    key_frame(&es, KEY_BACKSPACE, 0);
    key_frame(&es, KEY_BACKSPACE, 0);
    text_frame(&es, "-");
    key_frame(&es, KEY_DOWN, 0);
    CHECK(strcmp(es.ui.edit_buf, "88") == 0);
    for (int i = 0; i < 10; i++) key_frame(&es, KEY_DOWN, INPUT_SHIFT);
    CHECK(strcmp(es.ui.edit_buf, "1") == 0);
    key_frame(&es, KEY_ESCAPE, 0);
    CHECK(es.level.screen_count == 89);

    /* A text field keeps its text and caret. */
    click_frame(&es, CANVAS_W + 60, CFG_NAME_Y);
    CHECK(es.ui.active_id == 9000);
    key_frame(&es, KEY_HOME, 0);
    key_frame(&es, KEY_UP, 0);
    key_frame(&es, KEY_DOWN, 0);
    CHECK(strcmp(es.ui.edit_buf, "Untitled") == 0 && es.ui.edit_cursor == 0);
    key_frame(&es, KEY_ESCAPE, 0);
done:
    close_editor(&es);
    return failed;
}

/*
 * A float field shows the shortest text that reads back as its value: a
 * parallax speed of 0.08 is "0.08", not the "0.0799999982" of %.9g, and a
 * value that needs every digit keeps them.
 */
static int float_fields_show_the_shortest_text(void)
{
    int failed = 0;
    UIState ui;
    float values[] = {0.08f, 0.15f, 0.38f, 100.0f, -2.5f, 1.0f / 3.0f, 16777217.0f, 1e20f};
    const char *texts[] = {"0.08", "0.15", "0.38", "100", "-2.5", "0.33333334", "16777216", "1.00000002e+20"};
    ui_init(&ui, NULL);
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
        ui_begin_frame(&ui);
        ui.mouse_x = ui.mouse_y = 5;
        ui.mouse_clicked = 1;
        (void)ui_float_field(&ui, 1, 0, 0, 100, &values[i]);
        CHECK(ui.active_id == 1 && strcmp(ui.edit_buf, texts[i]) == 0);
        ui_cancel_active_edit(&ui);
    }
done:
    ui_cleanup(&ui);
    return failed;
}

/* Float fields step by 1 from 10 up and by 0.1 below, without float
 * error showing, step over 0 when 0 is refused, and keep to the limits;
 * an int field with a step uses it. */
static int number_field_steps_suit_the_field(void)
{
    int failed = 0;
    UIState ui;
    float small = 0.2f, big = 12.5f, speed = 0.1f;
    int snapped = 32;
    ui_init(&ui, NULL);

#define ACTIVATE(call) do { ui_begin_frame(&ui); ui.mouse_x = ui.mouse_y = 5; \
        ui.mouse_clicked = 1; (void)(call); } while (0)
    ACTIVATE(ui_float_field_limited(&ui, 1, 0, 0, 100, &small, -1.0f, 1.0f));
    CHECK(ui.active_id == 1);
    ui_edit_key(&ui, UI_KEY_UP, 0);
    CHECK(strcmp(ui.edit_buf, "0.3") == 0);
    ui_edit_key(&ui, UI_KEY_UP, 0);
    ui_edit_key(&ui, UI_KEY_UP, 0);
    CHECK(strcmp(ui.edit_buf, "0.5") == 0);
    ui_edit_key(&ui, UI_KEY_UP, 1);
    CHECK(strcmp(ui.edit_buf, "1") == 0);         /* the limit */
    CHECK(ui_apply_active_edit(&ui) == 2 && small == 1.0f);

    ACTIVATE(ui_float_field(&ui, 2, 0, 0, 100, &big));
    ui_edit_key(&ui, UI_KEY_UP, 0);
    CHECK(strcmp(ui.edit_buf, "13.5") == 0);
    ui_edit_key(&ui, UI_KEY_DOWN, 1);
    CHECK(strcmp(ui.edit_buf, "3.5") == 0);
    ui_edit_key(&ui, UI_KEY_DOWN, 0);
    CHECK(strcmp(ui.edit_buf, "3.4") == 0);
    ui_cancel_active_edit(&ui);

    ACTIVATE(ui_float_field_nonzero(&ui, 3, 0, 0, 100, &speed, 5.0f));
    ui_edit_key(&ui, UI_KEY_DOWN, 0);
    CHECK(strcmp(ui.edit_buf, "-0.1") == 0);      /* 0 is skipped */
    ui_cancel_active_edit(&ui);

    ACTIVATE(ui_int_field_limited(&ui, 4, 0, 0, 100, &snapped, 0, 64, 16));
    ui_edit_key(&ui, UI_KEY_UP, 0);
    CHECK(strcmp(ui.edit_buf, "48") == 0);
    ui_edit_key(&ui, UI_KEY_UP, 1);
    CHECK(strcmp(ui.edit_buf, "64") == 0);
    ui_edit_key(&ui, UI_KEY_DOWN, 1);
    CHECK(strcmp(ui.edit_buf, "0") == 0);
#undef ACTIVATE
done:
    ui_cleanup(&ui);
    return failed;
}

/* Does the side panel, drawn alone, use the text colour anywhere in the
 * rectangle?  (Memory readback is bottom-origin BGRA, see toolbar_pixel.) */
static int panel_has_text_in(EditorState *es, int x, int y, int w, int h)
{
    Color ink = ui_color(&es->ui, UI_COLOR_TEXT);
    int found = 0;
    es->ui.mouse_x = NEUTRAL_X;
    es->ui.mouse_y = NEUTRAL_Y;
    BeginDrawing();
    ClearBackground(BLACK);
    editor_render_side_panels(es);
    rlDrawRenderBatchActive();
    Image shot = LoadImageFromScreen();
    EndDrawing();
    if (!IsImageValid(shot)) return -1;
    for (int py = y; py < y + h && !found; py++) {
        for (int px = x; px < x + w && !found; px++) {
#ifdef MANGO_MEMORY_TESTS
            Color c = GetImageColor(shot, px, shot.height - 1 - py);
            unsigned char red = c.r;
            c.r = c.b;
            c.b = red;
#else
            Color c = GetImageColor(shot, px, py);
#endif
            found = same_color(c, ink);
        }
    }
    UnloadImage(shot);
    return found;
}

/*
 * A field shows its text on its own row: a line break is a pilcrow rather
 * than a second line drawn over the field below, and text wider than the
 * field stops at "..." inside it.  Shift+Enter adds a line break to the
 * description, the one field that keeps them; elsewhere it is dropped.
 */
static int text_fields_keep_line_breaks_on_one_row(void)
{
    int failed = 0;
    EditorState es;
    const int desc_y = CFG_NAME_Y - 4 + 24, field_x = CANVAS_W + 55;
    CHECK(open_editor(&es, NULL) == 0);
    snprintf(es.level.description, sizeof(es.level.description),
             "Line one of a description far too long for its field\nLine two");
    /* Under the field (the gap above By) and right of it: no text. */
    CHECK(panel_has_text_in(&es, field_x, desc_y + 3, 300, 12) == 1);
    CHECK(panel_has_text_in(&es, field_x, desc_y + 20, 310, 4) == 0);
    CHECK(panel_has_text_in(&es, field_x + 310, desc_y, PANEL_W - 55 - 310, 20) == 0);

    snprintf(es.level.description, sizeof(es.level.description), "ab");
    click_frame(&es, field_x + 100, desc_y + 4);
    CHECK(es.ui.active_id == 9001 && es.ui.edit_multiline);
    key_frame(&es, KEY_END, 0);
    key_frame(&es, KEY_LEFT, 0);
    key_frame(&es, KEY_ENTER, INPUT_SHIFT);
    CHECK(es.ui.active_id == 9001);
    text_frame(&es, "c");
    key_frame(&es, KEY_ENTER, 0);
    CHECK(es.ui.active_id == 0 && strcmp(es.level.description, "a\ncb") == 0);

    click_frame(&es, field_x + 100, CFG_NAME_Y);
    CHECK(es.ui.active_id == 9000 && !es.ui.edit_multiline);
    key_frame(&es, KEY_END, 0);
    key_frame(&es, KEY_ENTER, INPUT_SHIFT);
    CHECK(es.ui.active_id == 0 && strcmp(es.level.name, "Untitled") == 0);
done:
    close_editor(&es);
    return failed;
}

static int text_fields_select_copy_cut_and_paste(void)
{
    int failed = 0;
    EditorState es;
    const int field_left = CANVAS_W + 55, field_right = CANVAS_W + 55 + 310;
    CHECK(open_editor(&es, NULL) == 0);
    editor_test_set_clipboard("");

    click_frame(&es, CANVAS_W + 60, CFG_NAME_Y);
    CHECK(es.ui.active_id == 9000 && strcmp(es.ui.edit_buf, "Untitled") == 0);
    /* Nothing selected: Ctrl+C copies nothing and says how to select. */
    key_frame(&es, KEY_C, INPUT_CTRL);
    CHECK(editor_test_clipboard()[0] == '\0' && strstr(es.status_message, "Nothing selected"));
    /* Ctrl+A, Ctrl+C: the whole field. */
    key_frame(&es, KEY_A, INPUT_CTRL);
    key_frame(&es, KEY_C, INPUT_CTRL);
    CHECK(strcmp(editor_test_clipboard(), "Untitled") == 0);
    /* Shift+Left twice selects "ed"; Ctrl+X cuts it. */
    key_frame(&es, KEY_END, 0);
    key_frame(&es, KEY_LEFT, INPUT_SHIFT);
    key_frame(&es, KEY_LEFT, INPUT_SHIFT);
    key_frame(&es, KEY_X, INPUT_CTRL);
    CHECK(strcmp(editor_test_clipboard(), "ed") == 0 && strcmp(es.ui.edit_buf, "Untitl") == 0);
    /* Ctrl+Left jumps to the start of the word; Ctrl+V pastes there. */
    key_frame(&es, KEY_LEFT, INPUT_CTRL);
    CHECK(es.ui.edit_cursor == 0);
    key_frame(&es, KEY_V, INPUT_CTRL);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(strcmp(es.ui.edit_buf, "edUntitl") == 0);
    /* Typing replaces a selection; word jumps step over whole UTF-8
     * characters, so Ctrl+Shift+Right selects "w\xc3\xb6rld" whole. */
    key_frame(&es, KEY_A, INPUT_CTRL);
    text_frame(&es, "H\xc3\xa9 llo w\xc3\xb6rld");
    CHECK(strcmp(es.ui.edit_buf, "H\xc3\xa9 llo w\xc3\xb6rld") == 0);
    key_frame(&es, KEY_LEFT, INPUT_CTRL);
    CHECK(es.ui.edit_cursor == (int)strlen("H\xc3\xa9 llo "));
    key_frame(&es, KEY_RIGHT, INPUT_CTRL | INPUT_SHIFT);
    key_frame(&es, KEY_C, INPUT_CTRL);
    CHECK(strcmp(editor_test_clipboard(), "w\xc3\xb6rld") == 0);
    /* Shift+Left from inside a two-byte character's neighbour takes it
     * whole: select "\xc3\xa9" and delete it. */
    key_frame(&es, KEY_HOME, 0);
    key_frame(&es, KEY_RIGHT, 0);
    key_frame(&es, KEY_RIGHT, INPUT_SHIFT);
    key_frame(&es, KEY_DELETE, 0);
    CHECK(strcmp(es.ui.edit_buf, "H llo w\xc3\xb6rld") == 0);
    /* Left without Shift ends a selection at its start. */
    key_frame(&es, KEY_END, INPUT_SHIFT);
    key_frame(&es, KEY_LEFT, 0);
    CHECK(es.ui.edit_cursor == 1 && es.ui.edit_anchor < 0);

    /* A press past the end of the text and a drag to the field's left
     * edge select everything; Backspace deletes the selection. */
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, 0, field_right - 10, CFG_NAME_Y);
    ui_frame(&es, field_right - 10, CFG_NAME_Y);
    CHECK(es.ui.active_id == 9000 && es.ui.edit_cursor == (int)strlen(es.ui.edit_buf));
    ui_frame(&es, field_left + 1, CFG_NAME_Y);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, 0, field_left + 1, CFG_NAME_Y);
    ui_frame(&es, field_left + 1, CFG_NAME_Y);
    CHECK(es.ui.edit_cursor == 0 && es.ui.edit_anchor == (int)strlen(es.ui.edit_buf));
    key_frame(&es, KEY_BACKSPACE, 0);
    CHECK(es.ui.edit_buf[0] == '\0');
    key_frame(&es, KEY_ESCAPE, 0);
    CHECK(strcmp(es.level.name, "Untitled") == 0);
done:
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

