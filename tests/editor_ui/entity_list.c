/* Included by tests/editor_ui_test.c. Not compiled alone: the functions are
 * static and use the helpers in that file.
 */
/* ------------------------------------------------------------------ */
/* The Entities list and the layer rows of Level Config                */
/* ------------------------------------------------------------------ */

/* A point inside the Entities list's rows, clear of the scrollbar. */
#define LIST_X (CANVAS_W + 120)

/* The window y of the middle of the row that shows entity (type, index),
 * or -1 when the list does not show it. */
static int list_row_y(const EditorState *es, EntityType type, int index)
{
    EditorSideLayout layout;
    int row = editor_entity_list_row_of(es, type, index);
    editor_side_panel_layout(es, &layout);
    if (!layout.list_open || row < 0) return -1;
    return layout.list_y + ENTITY_LIST_ROWS_TOP + row * ENTITY_LIST_ROW_H
         - es->entity_list_scroll.offset + ENTITY_LIST_ROW_H / 2;
}

/* Replace the search text: Ctrl+F puts the caret in the field, Ctrl+A
 * and Backspace clear what is there, and the typing goes in its place. */
static void list_search(EditorState *es, const char *text)
{
    key_frame(es, KEY_F, INPUT_CTRL);
    push_key(KEY_A, INPUT_CTRL);
    push_key(KEY_BACKSPACE, 0);
    push_text(text);
    ui_frame(es, NEUTRAL_X, NEUTRAL_Y);
}

/* Every entity of the level, counted the way the list counts them. */
static int level_entity_total(const EditorState *es)
{
    int total = 0;
    for (int t = 0; t < ENT_COUNT; t++) total += editor_entity_count(&es->level, (EntityType)t);
    return total;
}

/* How many types have at least one entity: one heading row each. */
static int level_type_total(const EditorState *es)
{
    int types = 0;
    for (int t = 0; t < ENT_COUNT; t++)
        if (editor_entity_count(&es->level, (EntityType)t) > 0) types++;
    return types;
}

/*
 * Ctrl+F opens the Entities list in the right column with the caret in its
 * search field.  The rows follow the level (every entity, under its type),
 * the search narrows them as it is typed, a click selects and centres, and
 * Shift+click adds to the selection.  Esc closes the list again.
 */
static int entity_list_finds_and_selects_entities(void)
{
    int failed = 0;
    EditorState es;
    EditorSideLayout layout;
    char text[64];
    CHECK(open_editor(&es, "levels/00_sandbox_01.toml") == 0);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(es.level.coin_count >= 6 && level_is_valid(&es));

    /* Closed, the column is as it always was. */
    editor_side_panel_layout(&es, &layout);
    CHECK(!layout.list_open && layout.config_h > 0 && layout.palette_h > 0);

    /* The shortcut is in the F1 list, and the toolbar's tooltip names it. */
    CHECK(strcmp(editor_shortcut(EDITOR_SHORTCUT_ENTITY_LIST)->keys, "Ctrl+F") == 0);
    CHECK(strcmp(editor_shortcut_tooltip(EDITOR_SHORTCUT_ENTITY_LIST), "Entities list (Ctrl+F)") == 0);

    key_frame(&es, KEY_F, INPUT_CTRL);
    CHECK(es.entity_list_open && es.ui.active_id == ENTITY_LIST_SEARCH_ID);
    editor_side_panel_layout(&es, &layout);
    CHECK(layout.list_open && layout.list_y == TOOLBAR_H && layout.config_h == 0);
    CHECK(layout.list_h == EDITOR_H - STATUS_H - TOOLBAR_H);   /* nothing selected */

    /* Every entity has a row, and every type that has entities a heading. */
    CHECK(editor_entity_list_row_count(&es) == level_entity_total(&es) + level_type_total(&es));
    editor_entity_list_row_text(&es.level, ENT_COIN, 2, text, sizeof(text));
    {
        char expected[64];
        snprintf(expected, sizeof(expected), "Coin #2 (%d, %d)",
                 (int)roundf(es.level.coins[2].x), (int)roundf(es.level.coins[2].y));
        CHECK(strcmp(text, expected) == 0);
    }

    /* Typing narrows the rows at once, whatever the case. */
    push_text("COIN");
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(strcmp(editor_entity_list_filter(&es), "COIN") == 0);
    CHECK(editor_entity_list_row_count(&es) == 1 + es.level.coin_count);
    CHECK(editor_entity_list_row_of(&es, ENT_COIN, 0) == 1);
    CHECK(editor_entity_list_row_of(&es, ENT_PLAYER_SPAWN, 0) == -1);

    /* A click on a row selects that entity, with the Select tool, and
     * brings it to the middle of the canvas.  The search text stays. */
    const int last = es.level.coin_count - 1;
    es.camera.x = 0.0f;
    const int undo_top = es.undo->top;
    const uint64_t before = doc_hash(&es);
    int y = list_row_y(&es, ENT_COIN, last);
    CHECK(y > 0);
    click_frame(&es, LIST_X, y);
    CHECK(es.selection.type == ENT_COIN && es.selection.index == last);
    CHECK(editor_selection_count(&es) == 1 && es.tool == TOOL_SELECT);
    CHECK(es.ui.active_id == 0 && strcmp(es.entity_list_filter, "COIN") == 0);
    {
        EditorRect r;
        CHECK(editor_entity_bounds(&es.level, ENT_COIN, last, &r) && canvas_shows(&es, &r));
        /* Centred, unless the level's edge stops the view first. */
        float zoom = es.camera.zoom;
        float centre = es.camera.x + (float)CANVAS_W / (2.0f * zoom);
        float world_w = editor_world_width(&es.level);
        CHECK(fabsf(centre - (r.x + r.w / 2.0f)) < 1.0f ||
              es.camera.x == 0.0f || es.camera.x == world_w - (float)CANVAS_W / zoom);
    }
    /* The properties of the selection take their place under the list. */
    editor_side_panel_layout(&es, &layout);
    CHECK(layout.list_open && layout.props_h > 0 && layout.props_y == layout.list_y + layout.list_h);

    /* Shift+click adds another row's entity, and again takes it out. */
    y = list_row_y(&es, ENT_COIN, 1);
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, INPUT_SHIFT, LIST_X, y);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, INPUT_SHIFT, LIST_X, y);
    ui_frame(&es, LIST_X, y);
    CHECK(editor_selection_count(&es) == 2);
    CHECK(editor_is_selected(&es, ENT_COIN, last) && editor_is_selected(&es, ENT_COIN, 1));
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, INPUT_SHIFT, LIST_X, y);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, INPUT_SHIFT, LIST_X, y);
    ui_frame(&es, LIST_X, y);
    CHECK(editor_selection_count(&es) == 1 && editor_is_selected(&es, ENT_COIN, last));
    /* Selecting is not an edit. */
    CHECK(doc_hash(&es) == before && es.undo->top == undo_top);

    /* A number searches the coordinates: the rows are the entities whose
     * x or y is that number. */
    {
        char number[16];
        int matches = 0;
        int x = (int)roundf(es.level.coins[2].x);
        snprintf(number, sizeof(number), "%d", x);
        list_search(&es, number);
        CHECK(editor_entity_list_row_of(&es, ENT_COIN, 2) > 0);
        for (int t = 0; t < ENT_COUNT; t++)
            for (int i = 0; i < editor_entity_count(&es.level, (EntityType)t); i++)
                matches += editor_entity_list_matches(&es.level, (EntityType)t, i, number);
        CHECK(matches >= 1 && matches < level_entity_total(&es));
        /* A name and a number together: both must fit. */
        snprintf(text, sizeof(text), "coin %d", x);
        CHECK(editor_entity_list_matches(&es.level, ENT_COIN, 2, text));
        snprintf(text, sizeof(text), "spider %d", x);
        CHECK(!editor_entity_list_matches(&es.level, ENT_COIN, 2, text));
        snprintf(text, sizeof(text), "(%d, %d)", x, (int)roundf(es.level.coins[2].y));
        CHECK(editor_entity_list_matches(&es.level, ENT_COIN, 2, text));
        CHECK(!editor_entity_list_matches(&es.level, ENT_COIN, 2, "-99999"));
        /* Part of a name is enough; a word no type has lists nothing. */
        CHECK(editor_entity_list_matches(&es.level, ENT_JUMPING_SPIDER, 0, "ing sp") ||
              es.level.jumping_spider_count == 0);
        list_search(&es, "zzz");
        CHECK(editor_entity_list_row_count(&es) == 0);
    }

    /* Esc first gives up the typing, then closes the list. */
    key_frame(&es, KEY_ESCAPE, 0);
    CHECK(es.entity_list_open && es.ui.active_id == 0);
    key_frame(&es, KEY_ESCAPE, 0);
    CHECK(!es.entity_list_open);
    editor_side_panel_layout(&es, &layout);
    CHECK(!layout.list_open && layout.config_h > 0);
    CHECK(es.selection.index >= 0);             /* the selection outlives it */

    /* The Place tool needs the palette: choosing it closes the list, and
     * opening the list from Place goes back to Select. */
    key_frame(&es, KEY_F, INPUT_CTRL);
    key_frame(&es, KEY_ESCAPE, 0);              /* leave the search field */
    CHECK(es.entity_list_open);
    key_frame(&es, KEY_TWO, 0);
    CHECK(es.tool == TOOL_PLACE && !es.entity_list_open);
    key_frame(&es, KEY_F, INPUT_CTRL);
    CHECK(es.tool == TOOL_SELECT && es.entity_list_open);
done:
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/*
 * The rows are read from the level each frame, so they stay right after a
 * delete, an undo, a paste and another level being loaded.  The wheel
 * scrolls the rows.
 */
static int entity_list_follows_the_level(void)
{
    int failed = 0;
    EditorState es;
    char first[64], text[64];
    CHECK(open_editor(&es, "levels/00_sandbox_01.toml") == 0);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);

    list_search(&es, "coin");
    key_frame(&es, KEY_ENTER, 0);
    CHECK(es.ui.active_id == 0 && strcmp(es.entity_list_filter, "coin") == 0);
    const int coins = es.level.coin_count;
    CHECK(coins >= 3 && editor_entity_list_row_count(&es) == 1 + coins);
    editor_entity_list_row_text(&es.level, ENT_COIN, 1, first, sizeof(first));

    /* Delete coin 0 through its row: the old coin 1 is now "Coin #0". */
    click_frame(&es, LIST_X, list_row_y(&es, ENT_COIN, 0));
    CHECK(es.selection.type == ENT_COIN && es.selection.index == 0);
    key_frame(&es, KEY_DELETE, 0);
    CHECK(es.level.coin_count == coins - 1);
    CHECK(editor_entity_list_row_count(&es) == coins);
    CHECK(editor_entity_list_row_of(&es, ENT_COIN, coins - 1) == -1);
    editor_entity_list_row_text(&es.level, ENT_COIN, 0, text, sizeof(text));
    CHECK(strcmp(text + strlen("Coin #0"), first + strlen("Coin #1")) == 0);
    /* Undo puts it back, and the rows with it. */
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coin_count == coins && editor_entity_list_row_count(&es) == 1 + coins);
    editor_entity_list_row_text(&es.level, ENT_COIN, 1, text, sizeof(text));
    CHECK(strcmp(text, first) == 0);

    /* Copy and paste a coin: one more row, and a click on it selects the
     * copy, not a stale index. */
    click_frame(&es, LIST_X, list_row_y(&es, ENT_COIN, 1));
    key_frame(&es, KEY_C, INPUT_CTRL);
    key_frame(&es, KEY_V, INPUT_CTRL);
    CHECK(es.level.coin_count == coins + 1);
    CHECK(editor_entity_list_row_of(&es, ENT_COIN, coins) == 1 + coins);
    click_frame(&es, LIST_X, list_row_y(&es, ENT_COIN, 0));
    CHECK(es.selection.type == ENT_COIN && es.selection.index == 0);
    {
        int y = list_row_y(&es, ENT_COIN, coins);
        EditorSideLayout layout;
        editor_side_panel_layout(&es, &layout);
        /* Only rows inside the list take clicks; this one is in view. */
        CHECK(y > layout.list_y + ENTITY_LIST_ROWS_TOP && y < layout.list_y + layout.list_h);
        click_frame(&es, LIST_X, y);
        CHECK(es.selection.type == ENT_COIN && es.selection.index == coins);
    }

    /* With no search text the list is taller than its panel: the wheel
     * scrolls it a row per notch and stops at the ends. */
    list_search(&es, "");
    key_frame(&es, KEY_ENTER, 0);
    {
        EditorSideLayout layout;
        editor_side_panel_layout(&es, &layout);
        int rows = editor_entity_list_row_count(&es);
        int view = layout.list_h - ENTITY_LIST_ROWS_TOP;
        CHECK(rows * ENTITY_LIST_ROW_H > view);
        CHECK(es.entity_list_scroll.offset == 0);
        push_wheel(LIST_X, layout.list_y + 100, -3.0f, 0);
        ui_frame(&es, LIST_X, layout.list_y + 100);
        CHECK(es.entity_list_scroll.offset == 3 * ENTITY_LIST_ROW_H);
        /* The canvas did not take the wheel. */
        const float camera_x = es.camera.x;
        push_wheel(LIST_X, layout.list_y + 100, -1000.0f, 0);
        ui_frame(&es, LIST_X, layout.list_y + 100);
        CHECK(es.entity_list_scroll.offset == rows * ENTITY_LIST_ROW_H - view);
        CHECK(es.camera.x == camera_x);
        /* The last entity's row is now in view and takes a click. */
        int y = list_row_y(&es, ENT_PLAYER_SPAWN, 0);
        CHECK(y > layout.list_y + ENTITY_LIST_ROWS_TOP);
        click_frame(&es, LIST_X, y);
        CHECK(es.selection.type == ENT_PLAYER_SPAWN && es.selection.index == 0);
    }

    /* Another level: the rows are that level's, and the scroll position is
     * pulled back inside the new, shorter or longer, list. */
    editor_test_set_discard_choice(1);
    CHECK(editor_load_level(&es, "levels/labs/06_camera.toml") == 0);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    {
        EditorSideLayout layout;
        editor_side_panel_layout(&es, &layout);
        int rows = editor_entity_list_row_count(&es);
        int view = layout.list_h - ENTITY_LIST_ROWS_TOP;
        int most = rows * ENTITY_LIST_ROW_H > view ? rows * ENTITY_LIST_ROW_H - view : 0;
        CHECK(es.entity_list_open);
        CHECK(rows == level_entity_total(&es) + level_type_total(&es));
        CHECK(es.entity_list_scroll.offset <= most);
        /* No row names an entity the new level does not have. */
        for (int t = 0; t < ENT_COUNT; t++) {
            int n = editor_entity_count(&es.level, (EntityType)t);
            CHECK(editor_entity_list_row_of(&es, (EntityType)t, n) == -1);
        }
    }
    CHECK(level_is_valid(&es));
done:
    clear_dialog_seams();
    editor_test_set_discard_choice(-1);
    close_editor(&es);
    return failed;
}

/* One frame of the Level Config panel alone, drawn full height so nothing
 * is scrolled: a press at (mx, my) when click is set. */
static void config_only_frame(EditorState *es, int click, int mx, int my)
{
    arm_dialog_seams();
    ui_begin_frame(&es->ui);
    if (click) (void)ui_press(&es->ui);
    es->mouse_x = es->ui.mouse_x = mx;
    es->mouse_y = es->ui.mouse_y = my;
    BeginDrawing();
    BeginTextureMode(tiny_target);
    level_config_render(es, TOOLBAR_H, EDITOR_H - TOOLBAR_H, EDITOR_H - TOOLBAR_H);
    EndTextureMode();
    EndDrawing();
}

/*
 * Each row of a layer list has its own move-up, move-down and remove
 * buttons.  A click is one undoable Level Config step, the canvas preview
 * follows the new order, and the first row cannot go up nor the last down.
 */
static int layer_rows_move_and_remove_as_one_step(void)
{
    int failed = 0;
    EditorState es;
    static const char *const sky = "assets/sprites/backgrounds/sky_blue.png";
    static const char *const clouds = "assets/sprites/backgrounds/clouds_bg.png";
    static const char *const mountains = "assets/sprites/backgrounds/glacial_mountains.png";
    const int saved_plx_open = g_plx_open;
    CHECK(open_editor(&es, NULL) == 0);

    /* Three background layers, back to front: sky, clouds, mountains. */
    snprintf(es.level.background_layers[0].path, sizeof(es.level.background_layers[0].path), "%s", sky);
    snprintf(es.level.background_layers[1].path, sizeof(es.level.background_layers[1].path), "%s", clouds);
    snprintf(es.level.background_layers[2].path, sizeof(es.level.background_layers[2].path), "%s", mountains);
    es.level.background_layers[0].speed = 0.0f;
    es.level.background_layers[1].speed = 0.25f;
    es.level.background_layers[2].speed = 0.5f;
    es.level.background_layer_count = 3;
    editor_sync_config_resources(&es);
    editor_set_document_save_point(&es);
    CHECK(level_is_valid(&es));
    CHECK(strcmp(es.preview_sky_path, sky) == 0 && strcmp(es.preview_layer_paths[1], clouds) == 0);

    /* Draw the panel alone and unscrolled; no recent files or messages
     * above the layer rows, so they start where the widget tests expect. */
    es.recent_file_count = 0;
    g_plx_open = 1;
    g_fg_open = g_fog_open = g_phys_open = 0;
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(es.validation_report.message_count == 0 && es.load_report.count == 0);
    const int row0 = TOOLBAR_H + 368 + 5;        /* a point inside row 0 */
    const int up = CANVAS_W + CONFIG_LAYER_UP_X + CONFIG_LAYER_BUTTON_W / 2;
    const int down = CANVAS_W + CONFIG_LAYER_DOWN_X + CONFIG_LAYER_BUTTON_W / 2;
    const int remove = CANVAS_W + CONFIG_LAYER_REMOVE_X + CONFIG_LAYER_BUTTON_W / 2;
    /* Check the rows are where this test clicks: row 1's dropdown opens. */
    config_only_frame(&es, 1, CANVAS_W + 40, row0 + 20);
    CHECK(es.ui.dropdown_open_id == 9200 + 1);
    es.ui.dropdown_open_id = 0;
    const int undo_top = es.undo->top;
    const uint64_t original = doc_hash(&es);

    /* The first row cannot move up, the last cannot move down. */
    config_only_frame(&es, 1, up, row0);
    config_only_frame(&es, 1, down, row0 + 40);
    CHECK(doc_hash(&es) == original && es.undo->top == undo_top);

    /* Row 0 down: clouds are now drawn first, and the preview follows. */
    config_only_frame(&es, 1, down, row0);
    CHECK(strcmp(es.level.background_layers[0].path, clouds) == 0);
    CHECK(strcmp(es.level.background_layers[1].path, sky) == 0);
    CHECK(es.level.background_layers[0].speed == 0.25f && es.level.background_layers[1].speed == 0.0f);
    CHECK(strcmp(es.level.background_layers[2].path, mountains) == 0);
    CHECK(es.level.background_layer_count == 3 && es.undo->top == undo_top + 1 && es.modified);
    CHECK(strcmp(es.preview_sky_path, clouds) == 0 && strcmp(es.preview_layer_paths[1], sky) == 0);
    CHECK(es.textures.sky != NULL && es.preview_layers[1] != NULL);
    CHECK(level_is_valid(&es));
    /* One undo step: Ctrl+Z restores the exact document and its preview. */
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(doc_hash(&es) == original && es.undo->top == undo_top && !es.modified);
    CHECK(strcmp(es.preview_sky_path, sky) == 0 && strcmp(es.preview_layer_paths[1], clouds) == 0);
    CHECK(strstr(es.status_message, "Undid: changed level settings") != NULL);
    key_frame(&es, KEY_Y, INPUT_CTRL);
    CHECK(strcmp(es.level.background_layers[0].path, clouds) == 0);
    key_frame(&es, KEY_Z, INPUT_CTRL);

    /* Row 2 up: mountains go behind the clouds. */
    config_only_frame(&es, 1, up, row0 + 40);
    CHECK(strcmp(es.level.background_layers[1].path, mountains) == 0);
    CHECK(strcmp(es.level.background_layers[2].path, clouds) == 0);
    CHECK(strcmp(es.preview_layer_paths[2], clouds) == 0 && es.undo->top == undo_top + 1);

    /* Remove row 0 (the sky): the two rows below move up, as one step. */
    config_only_frame(&es, 1, remove, row0);
    CHECK(es.level.background_layer_count == 2);
    CHECK(strcmp(es.level.background_layers[0].path, mountains) == 0);
    CHECK(strcmp(es.level.background_layers[1].path, clouds) == 0);
    CHECK(strcmp(es.preview_sky_path, mountains) == 0 && es.undo->top == undo_top + 2);
    CHECK(level_is_valid(&es));
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.background_layer_count == 3);
    CHECK(strcmp(es.level.background_layers[0].path, sky) == 0);
    CHECK(strcmp(es.level.background_layers[1].path, mountains) == 0);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(doc_hash(&es) == original && es.undo->top == undo_top);

    /* The fog list has the same buttons (its rows use their own array). */
    snprintf(es.level.fog_layers[0].path, sizeof(es.level.fog_layers[0].path),
             "assets/sprites/foregrounds/fog_1.png");
    snprintf(es.level.fog_layers[1].path, sizeof(es.level.fog_layers[1].path),
             "assets/sprites/foregrounds/fog_2.png");
    es.level.fog_layers[0].speed = 0.5f;
    es.level.fog_layers[1].speed = 0.75f;
    es.level.fog_layer_count = 2;
    es.level.background_layer_count = 0;
    g_plx_open = 0;
    g_fog_open = 1;
    editor_sync_config_resources(&es);
    {
        /* Closed lists are one header each: find fog row 0 by its dropdown. */
        int fog_row = -1;
        for (int y = TOOLBAR_H + 360; y < EDITOR_H - 20 && fog_row < 0; y += 4) {
            config_only_frame(&es, 1, CANVAS_W + 60, y);
            if (es.ui.dropdown_open_id == 9600) fog_row = y;
            es.ui.dropdown_open_id = 0;
            g_plx_open = 0;      /* a probe may have clicked a header */
            g_phys_open = 0;
            g_fg_open = 0;
            g_fog_open = 1;
        }
        CHECK(fog_row > 0);
        const int steps = es.undo->top;
        config_only_frame(&es, 1, down, fog_row);
        CHECK(es.level.fog_layers[0].speed == 0.75f && es.level.fog_layers[1].speed == 0.5f);
        CHECK(es.undo->top == steps + 1);
        config_only_frame(&es, 1, remove, fog_row);
        CHECK(es.level.fog_layer_count == 1 && es.level.fog_layers[0].speed == 0.5f);
        CHECK(es.undo->top == steps + 2);
    }
done:
    g_plx_open = saved_plx_open;
    g_fg_open = g_fog_open = g_phys_open = 0;
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}
