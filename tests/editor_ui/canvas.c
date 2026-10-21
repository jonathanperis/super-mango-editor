/* Included by tests/editor_ui_test.c. Not compiled alone: the functions are
 * static and use the helpers in that file.
 */
/* ------------------------------------------------------------------ */
/* Canvas                                                              */
/* ------------------------------------------------------------------ */

/* Undo and Redo say what they did in the status bar, and bring the entity
 * they changed back into view when the canvas has scrolled away from it. */
static int history_reports_and_reveals_the_step(void)
{
    int failed = 0;
    EditorState es;
    EditorRect r;
    CHECK(open_editor(&es, NULL) == 0);
    es.level.screen_count = 8;
    es.palette_type = ENT_COIN;
    key_frame(&es, KEY_TWO, 0);
    ui_frame(&es, 300, 300);
    click_frame(&es, 300, 300);
    CHECK(es.level.coin_count == 1);
    CHECK(editor_entity_bounds(&es.level, ENT_COIN, 0, &r) && canvas_shows(&es, &r));

    es.camera.x = 2000.0f;
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(!canvas_shows(&es, &r));
    key_frame(&es, KEY_Z, INPUT_CTRL);
    /* The coin is gone, so there is nothing to scroll to. */
    CHECK(es.level.coin_count == 0 && es.camera.x == 2000.0f);
    CHECK(strcmp(es.status_message, "Undid: placed Coin") == 0);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(strcmp(es.status_message, "Nothing to undo") == 0);
    key_frame(&es, KEY_Z, INPUT_CTRL | INPUT_SHIFT);
    CHECK(es.level.coin_count == 1);
    CHECK(strcmp(es.status_message, "Redid: placed Coin") == 0);
    CHECK(editor_entity_bounds(&es.level, ENT_COIN, 0, &r) && canvas_shows(&es, &r));
    /* Already in view: a step leaves the scroll alone. */
    float shown_x = es.camera.x;
    key_frame(&es, KEY_Z, INPUT_CTRL);
    key_frame(&es, KEY_Z, INPUT_CTRL | INPUT_SHIFT);
    CHECK(es.camera.x == shown_x);
    key_frame(&es, KEY_Z, INPUT_CTRL | INPUT_SHIFT);
    CHECK(strcmp(es.status_message, "Nothing to redo") == 0);
done:
    close_editor(&es);
    return failed;
}

/* A paste that includes the one Last Star moves it and places the rest as
 * one step. Undoing that step reports a change, not "placed", and shows
 * the star, the one entity the undo leaves in the level. */
static int mixed_step_reports_change_and_shows_survivor(void)
{
    int failed = 0;
    EditorState es;
    EditorRect r;
    CHECK(open_editor(&es, NULL) == 0);
    es.level.screen_count = 8;
    es.level.coin_count = 1;
    es.level.coins[0].x = 100.0f;
    es.level.coins[0].y = 200.0f;
    es.level.last_star = (LastStarPlacement){3000.0f, 200.0f};
    CHECK(undo_group_begin(es.undo, 2) != 0);
    Command place = {0};
    place.type = CMD_PLACE;
    place.entity_type = ENT_COIN;
    place.entity_index = 0;
    place.after.coin = es.level.coins[0];
    CHECK(undo_push(es.undo, &place));
    Command move = {0};
    move.type = CMD_MOVE;
    move.entity_type = ENT_LAST_STAR;
    move.before.last_star = (LastStarPlacement){2900.0f, 200.0f};
    move.after.last_star = es.level.last_star;
    CHECK(undo_push(es.undo, &move));
    undo_group_end(es.undo);
    es.camera.x = 0.0f;
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coin_count == 0 && es.level.last_star.x == 2900.0f);
    CHECK(strcmp(es.status_message, "Undid: changed 2 entities") == 0);
    CHECK(editor_entity_bounds(&es.level, ENT_LAST_STAR, 0, &r) && canvas_shows(&es, &r));
done:
    close_editor(&es);
    return failed;
}

/* The overview strip under the canvas spans the whole level: a click
 * centres the view there, a drag slides it, and the release lets go. */
static int minimap_jumps_and_drags_the_view(void)
{
    int failed = 0;
    EditorState es;
    CHECK(open_editor(&es, NULL) == 0);
    es.level.screen_count = 8;
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    uint64_t before = doc_hash(&es);
    float world_w = editor_world_width(&es.level);
    CHECK(editor_minimap_world_x(&es, EDITOR_MINIMAP_X) == 0.0f);
    CHECK(editor_minimap_world_x(&es, EDITOR_MINIMAP_X + EDITOR_MINIMAP_WIDTH) == world_w);
    CHECK(editor_minimap_contains(EDITOR_MINIMAP_X, EDITOR_MINIMAP_Y + EDITOR_MINIMAP_HEIGHT - 1) &&
          !editor_minimap_contains(EDITOR_MINIMAP_X, EDITOR_H - STATUS_H + 8));

    const int y = EDITOR_MINIMAP_Y + 3, mid = EDITOR_MINIMAP_X + EDITOR_MINIMAP_WIDTH / 2;
    float view_w = (float)CANVAS_W / es.camera.zoom;
    CHECK(es.camera.x == 0.0f);
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, 0, mid, y);
    ui_frame(&es, mid, y);
    CHECK(fabsf(es.camera.x + view_w / 2.0f - editor_minimap_world_x(&es, mid)) < 0.5f);
    float clicked_x = es.camera.x;
    push_event(INPUT_MOUSE_MOVE, 0, 0, mid + 100, y);
    ui_frame(&es, mid + 100, y);
    CHECK(es.minimap_dragging && es.camera.x > clicked_x);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, 0, mid + 100, y);
    ui_frame(&es, mid + 100, y);
    CHECK(!es.minimap_dragging);
    /* Released, the pointer passing over the strip moves nothing. */
    float dropped_x = es.camera.x;
    ui_frame(&es, EDITOR_MINIMAP_X, y);
    CHECK(es.camera.x == dropped_x);
    CHECK(doc_hash(&es) == before && es.level.coin_count == 0);
    /* A click pressed and released within one frame still jumps. */
    es.camera.x = 0.0f;
    click_frame(&es, mid, y);
    CHECK(es.camera.x > 0.0f && !es.minimap_dragging);
    /* A hand-edited level can hold any number: the strip clips it as a
     * float, before it becomes a pixel, and draws nothing outside itself. */
    int l, t, w, h;
    CHECK(editor_minimap_strip_rect(world_w, 0.0f, 0.0f, 1e30f, 1e30f, &l, &t, &w, &h) &&
          l == EDITOR_MINIMAP_X && t == EDITOR_MINIMAP_Y &&
          w == EDITOR_MINIMAP_WIDTH && h == EDITOR_MINIMAP_HEIGHT);
    CHECK(!editor_minimap_strip_rect(world_w, 1e30f, 1e30f, 16.0f, 16.0f, &l, &t, &w, &h));
    CHECK(!editor_minimap_strip_rect(world_w, -1e30f, -1e30f, 16.0f, 16.0f, &l, &t, &w, &h));
    CHECK(editor_minimap_strip_rect(world_w, -1e30f, -1e30f, INFINITY, INFINITY, &l, &t, &w, &h) &&
          l == EDITOR_MINIMAP_X && w == EDITOR_MINIMAP_WIDTH);
    CHECK(!editor_minimap_strip_rect(world_w, NAN, NAN, 16.0f, 16.0f, &l, &t, &w, &h));
    /* Inside the level, a small entity is still a pixel where it stands. */
    CHECK(editor_minimap_strip_rect(world_w, world_w / 2.0f, 0.0f, 1.0f, 1.0f, &l, &t, &w, &h) &&
          l == EDITOR_MINIMAP_X + EDITOR_MINIMAP_WIDTH / 2 && w == 1 && h == 1);
done:
    close_editor(&es);
    return failed;
}

/* F1 shows the shortcut list over the editor; while it is up, keys and
 * clicks change nothing beneath it, and F1, Esc or a click close it. The
 * toolbar's tooltips come from the same table. */
static int f1_sheet_lists_the_shortcuts(void)
{
    int failed = 0;
    EditorState es;
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(strcmp(editor_shortcut_tooltip(EDITOR_SHORTCUT_SAVE), "Save (Ctrl+S)") == 0);
    CHECK(strcmp(editor_shortcut_tooltip(EDITOR_SHORTCUT_PLAYTEST), "Playtest (F5; Shift+F5 from here)") == 0);
    CHECK(editor_shortcut_tooltip(EDITOR_SHORTCUT_UNDO) == NULL);
    CHECK(ui_text_width(&es.ui, "Undo") > 0);  /* the font measures, so the widths below mean something */
    for (int i = 0; i < EDITOR_SHORTCUT_COUNT; i++) {
        const EditorShortcut *s = editor_shortcut((EditorShortcutId)i);
        CHECK(s && s->keys && s->keys[0] && s->action && s->action[0]);
        /* Each column fits the 640 px panel (keys 190 px, then the action). */
        if (ui_text_width(&es.ui, s->keys) > 180 || ui_text_width(&es.ui, s->action) > 426)
            fprintf(stderr, "shortcut row too wide: %s / %s (%d px)\n", s->keys, s->action,
                    ui_text_width(&es.ui, s->action));
        CHECK(ui_text_width(&es.ui, s->keys) <= 180 && ui_text_width(&es.ui, s->action) <= 426);
    }

    key_frame(&es, KEY_F1, 0);
    CHECK(es.shortcut_sheet_open);
    /* Keys under the sheet do nothing: 2 would pick the Place tool. */
    key_frame(&es, KEY_TWO, 0);
    CHECK(es.tool == TOOL_SELECT && es.shortcut_sheet_open);
    key_frame(&es, KEY_ESCAPE, 0);
    CHECK(!es.shortcut_sheet_open);
    key_frame(&es, KEY_F1, 0);
    key_frame(&es, KEY_F1, 0);
    CHECK(!es.shortcut_sheet_open);
    /* A click closes it without placing, selecting or pressing anything. */
    key_frame(&es, KEY_TWO, 0);
    es.palette_type = ENT_COIN;
    key_frame(&es, KEY_F1, 0);
    click_frame(&es, 300, 300);
    CHECK(!es.shortcut_sheet_open && es.level.coin_count == 0 && !es.mouse_down);
    click_frame(&es, 300, 300);
    CHECK(es.level.coin_count == 1);
    /* Once closed, F1 and Esc are ordinary keys again: Esc leaves Place. */
    key_frame(&es, KEY_ESCAPE, 0);
    CHECK(es.tool == TOOL_SELECT);
done:
    close_editor(&es);
    return failed;
}

static int canvas_place_select_drag_delete_and_undo(void)
{
    int failed = 0;
    EditorState es;
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(es.level.coin_count == 0 && es.camera.zoom == 2.0f);

    /* Place a coin with the Place tool (key 2). */
    es.palette_type = ENT_COIN;
    key_frame(&es, KEY_TWO, 0);
    CHECK(es.tool == TOOL_PLACE);
    const int sx = 300, sy = 300;
    ui_frame(&es, sx, sy);
    click_frame(&es, sx, sy);
    CHECK(es.level.coin_count == 1 && es.undo->top == 1 && es.modified);
    /* A click outside the canvas never places. */
    click_frame(&es, CANVAS_W + 10, EDITOR_H - STATUS_H - 10);
    CHECK(es.level.coin_count == 1);

    /* Select it where it was placed (key 1 = Select tool). */
    key_frame(&es, KEY_ONE, 0);
    click_frame(&es, sx, sy);
    CHECK(es.selection.type == ENT_COIN && es.selection.index == 0);
    const float x0 = es.level.coins[0].x, y0 = es.level.coins[0].y;

    /* Drag it: 60 x 20 screen pixels at zoom 2 is 30 x 10 world pixels, and
     * the point that was grabbed stays under the cursor. */
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, 0, sx, sy);
    push_event(INPUT_MOUSE_MOVE, 0, 0, sx + 30, sy + 10);
    push_event(INPUT_MOUSE_MOVE, 0, 0, sx + 60, sy + 20);
    ui_frame(&es, sx + 60, sy + 20);
    /* Mid-drag, history keys are refused with an explanation. */
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(strstr(es.status_message, "Release the mouse") != NULL);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, 0, sx + 60, sy + 20);
    ui_frame(&es, sx + 60, sy + 20);
    CHECK(es.level.coins[0].x == x0 + 30.0f && es.level.coins[0].y == y0 + 10.0f);
    CHECK(es.undo->top == 2);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coins[0].x == x0 && es.level.coins[0].y == y0);
    key_frame(&es, KEY_Z, INPUT_CTRL | INPUT_SHIFT);   /* Ctrl+Shift+Z redoes */
    CHECK(es.level.coins[0].x == x0 + 30.0f);
    key_frame(&es, KEY_Z, INPUT_CTRL);

    /* Esc during a drag puts the coin back and records nothing. */
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, 0, sx, sy);
    push_event(INPUT_MOUSE_MOVE, 0, 0, sx + 80, sy);
    ui_frame(&es, sx + 80, sy);
    key_frame(&es, KEY_ESCAPE, 0);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, 0, sx + 80, sy);
    ui_frame(&es, sx + 80, sy);
    CHECK(es.level.coins[0].x == x0 && es.undo->top == 1);

    /* Copy and paste makes a second coin; undo takes it away again. */
    click_frame(&es, sx, sy);
    key_frame(&es, KEY_C, INPUT_CTRL);
    key_frame(&es, KEY_V, INPUT_CTRL);
    CHECK(es.level.coin_count == 2 && level_is_valid(&es));
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coin_count == 1);

    /* Three ways to delete, each undoable: Delete key, right-click, and
     * the Delete tool (key 3). */
    click_frame(&es, sx, sy);
    CHECK(es.selection.index == 0);
    key_frame(&es, KEY_DELETE, 0);
    CHECK(es.level.coin_count == 0 && es.selection.index < 0);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coin_count == 1);
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_RIGHT, 0, sx, sy);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_RIGHT, 0, sx, sy);
    ui_frame(&es, sx, sy);
    CHECK(es.level.coin_count == 0);
    key_frame(&es, KEY_Y, INPUT_CTRL);   /* redo of nothing: still deleted */
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coin_count == 1);
    key_frame(&es, KEY_THREE, 0);
    CHECK(es.tool == TOOL_DELETE);
    click_frame(&es, sx, sy);
    CHECK(es.level.coin_count == 0);
    key_frame(&es, KEY_ESCAPE, 0);
    CHECK(es.tool == TOOL_SELECT);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coin_count == 1);

    /* Esc with a selection clears it. */
    click_frame(&es, sx, sy);
    CHECK(es.selection.index == 0);
    key_frame(&es, KEY_ESCAPE, 0);
    CHECK(es.selection.index < 0);

    /* The wheel pans; Ctrl+wheel steps through the 1/2/3/5 zoom presets
     * around the cursor; Shift+wheel pans vertically when the world is
     * taller than the view. The camera never leaves the world. */
    push_wheel(400, 300, -2.0f, 0);
    ui_frame(&es, 400, 300);
    CHECK(es.camera.x == 2 * 48.0f / 2.0f);
    push_wheel(400, 300, 100.0f, 0);
    ui_frame(&es, 400, 300);
    CHECK(es.camera.x == 0.0f);
    /* Zoom stops at 5x and at a quarter: it used to wrap from 5x to 1x. */
    static const float up[] = {3.0f, 5.0f, 5.0f};
    for (int i = 0; i < 3; i++) {
        push_wheel(400, 300, 1.0f, INPUT_CTRL);
        ui_frame(&es, 400, 300);
        CHECK(es.camera.zoom == up[i]);
    }
    /* Down through every preset to a quarter, where it stops; then back
     * up to 1x. */
    static const float down[] = {3.0f, 2.0f, 1.0f, 0.5f, 0.25f, 0.25f};
    for (int i = 0; i < 6; i++) {
        push_wheel(400, 300, -1.0f, INPUT_CTRL);
        ui_frame(&es, 400, 300);
        CHECK(es.camera.zoom == down[i]);
    }
    push_wheel(400, 300, 1.0f, INPUT_CTRL);
    push_wheel(400, 300, 1.0f, INPUT_CTRL);
    ui_frame(&es, 400, 300);
    CHECK(es.camera.zoom == 1.0f);
    /* A trackpad pinch sends fractions: four quarter notches make one step. */
    for (int i = 0; i < 3; i++) push_wheel(400, 300, 0.25f, INPUT_CTRL);
    ui_frame(&es, 400, 300);
    CHECK(es.camera.zoom == 1.0f);
    push_wheel(400, 300, 0.25f, INPUT_CTRL);
    ui_frame(&es, 400, 300);
    CHECK(es.camera.zoom == 2.0f);
    push_wheel(400, 300, -1.0f, INPUT_CTRL);
    ui_frame(&es, 400, 300);
    CHECK(es.camera.zoom == 1.0f);
    push_wheel(400, 300, 1.0f, INPUT_CTRL);
    push_wheel(400, 300, 1.0f, INPUT_CTRL);
    push_wheel(400, 300, 1.0f, INPUT_CTRL);
    ui_frame(&es, 400, 300);
    CHECK(es.camera.zoom == 5.0f);
    push_wheel(400, 300, -10.0f, INPUT_SHIFT);
    ui_frame(&es, 400, 300);
    CHECK(es.camera.y > 0.0f && es.camera.y <= GAME_H - CANVAS_H / 5.0f + 0.01f);
    ui_frame(&es, 400, 300);   /* draw the grid at 5x */
    for (int i = 0; i < 3; i++) push_wheel(400, 300, -1.0f, INPUT_CTRL);
    ui_frame(&es, 400, 300);
    CHECK(es.camera.zoom == 1.0f && es.camera.y == 0.0f);

    /* Typing into the selected coin's first field (x) and pressing Enter
     * moves it; undo restores it. The field is found by probing. */
    push_wheel(400, 300, 1.0f, INPUT_CTRL);
    ui_frame(&es, 400, 300);
    CHECK(es.camera.zoom == 2.0f);
    push_wheel(400, 300, 100.0f, 0);   /* pan back to the left edge */
    ui_frame(&es, 400, 300);
    CHECK(es.camera.x == 0.0f && es.camera.y == 0.0f);
    click_frame(&es, sx, sy);
    CHECK(es.selection.type == ENT_COIN && es.selection.index == 0);
    int field_y = -1;
    for (int y = props_top(&es) + 30; y < EDITOR_H - STATUS_H && field_y < 0; y += 6) {
        click_frame(&es, PANEL_FIELD_X, y);
        if (es.ui.active_id) field_y = y;
    }
    CHECK(field_y > 0);
    for (int i = 0; i < 16; i++) key_frame(&es, KEY_BACKSPACE, 0);
    push_text("123");
    ui_frame(&es, PANEL_FIELD_X, field_y);
    int undo_top = es.undo->top;
    key_frame(&es, KEY_ENTER, 0);
    CHECK(es.ui.active_id == 0);
    CHECK(es.level.coins[0].x == 123.0f || es.level.coins[0].y == 123.0f);
    CHECK(es.undo->top == undo_top + 1);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coins[0].x == x0 && es.level.coins[0].y == y0);

    /* The properties header folds the panel; the canvas keeps the selection. */
    const int header_y = props_top(&es);
    for (int y = header_y; y < header_y + 24; y += 6) {
        click_frame(&es, PANEL_LABEL_X, y);
        if (!es.panel_open) break;
    }
    CHECK(es.panel_open == 0 && es.selection.index == 0);
    for (int y = props_top(&es); y < props_top(&es) + 24; y += 4) {
        click_frame(&es, PANEL_LABEL_X, y);
        if (es.panel_open) break;
    }
    CHECK(es.panel_open == 1);

    /* Two full-size frames through editor_run_frame itself. */
    editor_run_frame(&es);
    editor_run_frame(&es);
    CHECK(es.running && es.validation_cache_valid);
done:
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

