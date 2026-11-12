/* Included by tests/editor_ui_test.c. Not compiled alone: the functions are
 * static and use the helpers in that file.
 */
/* ------------------------------------------------------------------ */
/* Patrol ranges and resize handles on the canvas                      */
/* ------------------------------------------------------------------ */

/* Add one entity at the end of its array and return its index (-1 when
 * the array is full). */
static int add_entity(EditorState *es, EntityType type, const PlacementData *pd)
{
    int index = editor_entity_count(&es->level, type);
    return editor_entity_insert(&es->level, type, index, pd) == 0 ? index : -1;
}

/* The handle of the given kind on entity (type, index), as a screen pixel.
 * Returns 0 when the entity has no such handle. */
static int handle_screen_point(const EditorState *es, EntityType type, int index,
                               EditorHandleKind kind, int *sx, int *sy)
{
    EditorHandle handles[EDITOR_HANDLE_MAX];
    int count = editor_entity_handles(&es->level, type, index, handles);
    for (int i = 0; i < count; i++) {
        float fx, fy;
        if (handles[i].kind != kind) continue;
        world_to_screen(es, handles[i].x, handles[i].y, &fx, &fy);
        *sx = (int)roundf(fx);
        *sy = (int)roundf(fy);
        return 1;
    }
    return 0;
}

/* Press on (sx, sy), move by (dx, dy) screen pixels and run the frame that
 * sees both; the button stays down. */
static void press_and_move(EditorState *es, int sx, int sy, int dx, int dy)
{
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, 0, sx, sy);
    push_event(INPUT_MOUSE_MOVE, 0, 0, sx + dx, sy + dy);
    ui_frame(es, sx + dx, sy + dy);
}

static void release_at(EditorState *es, int sx, int sy)
{
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, 0, sx, sy);
    ui_frame(es, sx, sy);
}

/* A whole drag of a handle: press, move, release. */
static void drag_handle(EditorState *es, int sx, int sy, int dx, int dy)
{
    press_and_move(es, sx, sy, dx, dy);
    release_at(es, sx + dx, sy + dy);
}

/*
 * A patroller's range is the stretch its sprite covers: patrol_x0 to
 * patrol_x1 for an enemy (the validator's meaning), and one sprite width
 * further for a saw, whose x itself stops at patrol_x1.  The canvas draws
 * the ranges at every zoom and scroll, selected or not.
 */
static int patrol_ranges_show_where_patrollers_turn(void)
{
    int failed = 0;
    EditorState es;
    PlacementData pd;
    EditorPatrolRange range;
    EditorRect bounds;
    CHECK(open_editor(&es, NULL) == 0);

    memset(&pd, 0, sizeof(pd));
    pd.spider.x = 200.0f;
    pd.spider.vx = 40.0f;
    pd.spider.patrol_x0 = 150.0f;
    pd.spider.patrol_x1 = 400.0f;
    const int spider = add_entity(&es, ENT_SPIDER, &pd);
    CHECK(spider == 0 && level_is_valid(&es));

    CHECK(editor_patrol_range(&es.level, ENT_SPIDER, spider, &range) == 1);
    CHECK(range.x0 == 150.0f && range.x1 == 400.0f);
    CHECK(editor_entity_bounds(&es.level, ENT_SPIDER, spider, &bounds) == 1);
    CHECK(range.y > bounds.y && range.y < bounds.y + bounds.h);

    memset(&pd, 0, sizeof(pd));
    pd.circular_saw.x = 500.0f;
    pd.circular_saw.patrol_x0 = 480.0f;
    pd.circular_saw.patrol_x1 = 640.0f;
    pd.circular_saw.direction = 1;
    const int saw = add_entity(&es, ENT_CIRCULAR_SAW, &pd);
    CHECK(saw == 0);
    CHECK(editor_patrol_range(&es.level, ENT_CIRCULAR_SAW, saw, &range) == 1);
    CHECK(range.x0 == 480.0f && range.x1 == 640.0f + (float)SAW_DISPLAY_W);

    /* Things that do not patrol, and entities that do not exist, have none. */
    CHECK(editor_patrol_range(&es.level, ENT_PLAYER_SPAWN, 0, &range) == 0);
    CHECK(editor_patrol_range(&es.level, ENT_SPIDER, 5, &range) == 0);

    /* Drawing them changes nothing, whatever is selected and wherever the
     * view is (ranges out of view are skipped). */
    const uint64_t before = doc_hash(&es);
    for (int zoom = 0; zoom < CANVAS_ZOOM_COUNT; zoom++) {
        canvas_set_zoom(&es, canvas_zoom_preset(zoom), CANVAS_W / 2, TOOLBAR_H + CANVAS_H / 2);
        es.camera.x = 0.0f;
        ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
        editor_select_only(&es, ENT_SPIDER, spider);
        ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
        es.camera.x = 1200.0f;      /* both ranges are off to the left */
        ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
        editor_select_none(&es);
    }
    CHECK(doc_hash(&es) == before && es.undo->top == 0);
done:
    close_editor(&es);
    return failed;
}

/*
 * The ends of a selected patroller's range are handles: a drag moves
 * patrol_x0 or patrol_x1 inside the validator's limits, shows the value,
 * and is one undo step on release.  Esc puts the value back.  Keys that
 * would change the document wait until the button is released.
 */
static int patrol_handles_move_the_range_ends(void)
{
    int failed = 0;
    EditorState es;
    PlacementData pd;
    int sx, sy;
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(es.camera.zoom == 2.0f);

    memset(&pd, 0, sizeof(pd));
    pd.spider.x = 200.0f;
    pd.spider.vx = 40.0f;
    pd.spider.patrol_x0 = 150.0f;
    pd.spider.patrol_x1 = 400.0f;
    CHECK(add_entity(&es, ENT_SPIDER, &pd) == 0 && level_is_valid(&es));
    const SpiderPlacement *spider = &es.level.spiders[0];
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);

    /* No handles until exactly this entity is selected with Select. */
    CHECK(!editor_handles_shown(&es));
    editor_select_only(&es, ENT_SPIDER, 0);
    CHECK(editor_handles_shown(&es));
    es.tool = TOOL_PLACE;
    CHECK(!editor_handles_shown(&es));
    es.tool = TOOL_SELECT;

    /* Drag the right end 60 canvas px (30 world px at 2x) to the right. */
    CHECK(handle_screen_point(&es, ENT_SPIDER, 0, EDITOR_HANDLE_PATROL_END, &sx, &sy));
    const int undo_top = es.undo->top;
    press_and_move(&es, sx, sy, 60, 0);
    CHECK(es.resizing && es.dragging);
    CHECK(spider->patrol_x1 == 430.0f && spider->patrol_x0 == 150.0f && spider->x == 200.0f);
    CHECK(strstr(es.status_message, "patrol: 150 to 430") != NULL);
    CHECK(es.ui.cursor_shown == MOUSE_CURSOR_RESIZE_EW);
    CHECK(es.undo->top == undo_top);              /* nothing recorded yet */
    /* Undo, Delete and the F1 list wait for the release. */
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(spider->patrol_x1 == 430.0f && strstr(es.status_message, "finish the resize"));
    key_frame(&es, KEY_DELETE, 0);
    CHECK(es.level.spider_count == 1);
    key_frame(&es, KEY_F1, 0);
    CHECK(!es.shortcut_sheet_open);
    release_at(&es, sx + 60, sy);
    CHECK(!es.resizing && !es.dragging && !es.mouse_down);
    CHECK(es.undo->top == undo_top + 1 && es.modified && level_is_valid(&es));
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(spider->patrol_x1 == 400.0f && es.undo->top == undo_top);
    key_frame(&es, KEY_Y, INPUT_CTRL);
    CHECK(spider->patrol_x1 == 430.0f);
    key_frame(&es, KEY_Z, INPUT_CTRL);

    /* Esc during the drag cancels it: the value returns, nothing is
     * recorded, and the release that follows does nothing. */
    CHECK(handle_screen_point(&es, ENT_SPIDER, 0, EDITOR_HANDLE_PATROL_START, &sx, &sy));
    press_and_move(&es, sx, sy, -40, 0);
    CHECK(spider->patrol_x0 == 130.0f);
    key_frame(&es, KEY_ESCAPE, 0);
    CHECK(!es.resizing && !es.dragging && spider->patrol_x0 == 150.0f);
    CHECK(strstr(es.status_message, "Resize cancelled") != NULL);
    release_at(&es, sx - 40, sy);
    CHECK(spider->patrol_x0 == 150.0f && es.undo->top == undo_top);
    CHECK(es.selection.type == ENT_SPIDER && es.selection.index == 0);

    /* A click on a handle that does not move changes nothing. */
    const uint64_t before = doc_hash(&es);
    click_frame(&es, sx, sy);
    CHECK(doc_hash(&es) == before && es.undo->top == undo_top && !es.resizing);
    CHECK(es.selection.type == ENT_SPIDER && es.selection.index == 0);

    /* The left end cannot pass the spider itself, and the right end stays
     * a sprite width past it: the level never becomes invalid. */
    drag_handle(&es, sx, sy, 400, 0);
    CHECK(spider->patrol_x0 == 200.0f && level_is_valid(&es));
    CHECK(handle_screen_point(&es, ENT_SPIDER, 0, EDITOR_HANDLE_PATROL_END, &sx, &sy));
    drag_handle(&es, sx, sy, -600, 0);
    CHECK(spider->patrol_x1 == 200.0f + (float)SPIDER_FRAME_W && level_is_valid(&es));
    CHECK(es.undo->top == undo_top + 2);
    /* Neither end leaves the world. */
    CHECK(handle_screen_point(&es, ENT_SPIDER, 0, EDITOR_HANDLE_PATROL_START, &sx, &sy));
    drag_handle(&es, sx, sy, -2000, 0);
    CHECK(spider->patrol_x0 == 0.0f && level_is_valid(&es));

    /* With snap on (key S) an end lands on the 48 px grid; Shift, read at
     * each motion, turns that off for the drag. */
    key_frame(&es, KEY_S, 0);
    CHECK(es.snap_to_grid == 1);
    CHECK(handle_screen_point(&es, ENT_SPIDER, 0, EDITOR_HANDLE_PATROL_END, &sx, &sy));
    press_and_move(&es, sx, sy, 70, 0);           /* 264 + 35 = 299 -> 288 */
    CHECK(spider->patrol_x1 == 288.0f);
    push_event(INPUT_MOUSE_MOVE, 0, INPUT_SHIFT, sx + 70, sy);
    ui_frame(&es, sx + 70, sy);
    CHECK(spider->patrol_x1 == 299.0f);
    release_at(&es, sx + 70, sy);
    key_frame(&es, KEY_S, 0);
    CHECK(level_is_valid(&es));

    /* Handles keep their size on screen: at 1/2x a press 5 canvas px from
     * the centre (10 world px) still takes the handle, 12 px does not. */
    canvas_set_zoom(&es, 0.5f, 0, TOOLBAR_H);
    es.camera.x = 0.0f;
    {
        EditorHandle handles[EDITOR_HANDLE_MAX];
        CHECK(editor_entity_handles(&es.level, ENT_SPIDER, 0, handles) == 2);
        CHECK(handles[1].kind == EDITOR_HANDLE_PATROL_END);
        CHECK(editor_handle_at(&es, handles[1].x + 10.0f, handles[1].y) == EDITOR_HANDLE_PATROL_END);
        CHECK(editor_handle_at(&es, handles[1].x + 24.0f, handles[1].y) == EDITOR_HANDLE_NONE);
        canvas_set_zoom(&es, 5.0f, 0, TOOLBAR_H);
        CHECK(editor_handle_at(&es, handles[1].x + 1.0f, handles[1].y) == EDITOR_HANDLE_PATROL_END);
        CHECK(editor_handle_at(&es, handles[1].x + 2.0f, handles[1].y) == EDITOR_HANDLE_NONE);
    }
done:
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/*
 * Handles only take a plain press that lands on them.  A press anywhere
 * else selects and drags as before, Shift+click on a handle still works on
 * the selection, and a multi-selection has no handles at all.
 */
static int handles_leave_other_clicks_alone(void)
{
    int failed = 0;
    EditorState es;
    PlacementData pd;
    int sx, sy;
    float fx, fy;
    CHECK(open_editor(&es, NULL) == 0);

    memset(&pd, 0, sizeof(pd));
    pd.spike_row.x = 96.0f;
    pd.spike_row.count = 3;
    CHECK(add_entity(&es, ENT_SPIKE_ROW, &pd) == 0);
    /* A coin lying right under the spike row's handle, and one far away. */
    EditorHandle handles[EDITOR_HANDLE_MAX];
    CHECK(editor_entity_handles(&es.level, ENT_SPIKE_ROW, 0, handles) == 1);
    memset(&pd, 0, sizeof(pd));
    pd.coin.x = handles[0].x - 8.0f;
    pd.coin.y = handles[0].y - 8.0f;
    CHECK(add_entity(&es, ENT_COIN, &pd) == 0);
    pd.coin.x = 300.0f;
    pd.coin.y = 100.0f;
    CHECK(add_entity(&es, ENT_COIN, &pd) == 1);
    CHECK(level_is_valid(&es));
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);

    editor_select_only(&es, ENT_SPIKE_ROW, 0);
    CHECK(handle_screen_point(&es, ENT_SPIKE_ROW, 0, EDITOR_HANDLE_WIDTH, &sx, &sy));
    const int undo_top = es.undo->top;

    /* A click on the far coin selects it: the handle is not under it. */
    world_to_screen(&es, 308.0f, 108.0f, &fx, &fy);
    click_frame(&es, (int)fx, (int)fy);
    CHECK(es.selection.type == ENT_COIN && es.selection.index == 1 && !es.resizing);
    /* The spike row is no longer selected, so its handle is gone: a press
     * there now picks the coin lying under it. */
    click_frame(&es, sx, sy);
    CHECK(es.selection.type == ENT_COIN && es.selection.index == 0 && !es.resizing);

    /* Shift+click on the handle adds the coin under it to the selection
     * instead of starting a resize. */
    editor_select_only(&es, ENT_SPIKE_ROW, 0);
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, INPUT_SHIFT, sx, sy);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, INPUT_SHIFT, sx, sy);
    ui_frame(&es, sx, sy);
    CHECK(!es.resizing && editor_selection_count(&es) == 2);
    CHECK(editor_is_selected(&es, ENT_COIN, 0) && editor_is_selected(&es, ENT_SPIKE_ROW, 0));
    /* Two entities selected: no handles, so a plain press there does not
     * resize either. */
    CHECK(!editor_handles_shown(&es));
    CHECK(editor_handle_at(&es, handles[0].x, handles[0].y) == EDITOR_HANDLE_NONE);
    CHECK(es.level.spike_rows[0].count == 3 && es.undo->top == undo_top);
done:
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/*
 * The growing edge of anything made of tiles is a handle: dragging it
 * changes the count by whole tiles, stops at the last count the validator
 * accepts, and is one undo step.  A rail's corner sizes both ways.
 */
static int size_handles_change_tile_counts(void)
{
    int failed = 0;
    EditorState es;
    PlacementData pd;
    int sx, sy;
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(es.camera.zoom == 2.0f);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);

    /* ---- A spike row: 16 px tiles to the right ---- */
    memset(&pd, 0, sizeof(pd));
    pd.spike_row.x = 96.0f;
    pd.spike_row.count = 3;
    CHECK(add_entity(&es, ENT_SPIKE_ROW, &pd) == 0 && level_is_valid(&es));
    editor_select_only(&es, ENT_SPIKE_ROW, 0);
    CHECK(handle_screen_point(&es, ENT_SPIKE_ROW, 0, EDITOR_HANDLE_WIDTH, &sx, &sy));
    int undo_top = es.undo->top;
    /* 64 canvas px = 32 world px = two more tiles. */
    press_and_move(&es, sx, sy, 64, 0);
    CHECK(es.resizing && es.level.spike_rows[0].count == 5);
    CHECK(strstr(es.status_message, "Spike Row: 5 tiles wide") != NULL);
    /* Less than half a tile further rounds back to the same count. */
    push_event(INPUT_MOUSE_MOVE, 0, 0, sx + 76, sy);
    ui_frame(&es, sx + 76, sy);
    CHECK(es.level.spike_rows[0].count == 5 && es.undo->top == undo_top);
    release_at(&es, sx + 76, sy);
    CHECK(es.undo->top == undo_top + 1 && es.level.spike_rows[0].x == 96.0f);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.spike_rows[0].count == 3);
    key_frame(&es, KEY_Y, INPUT_CTRL);
    CHECK(es.level.spike_rows[0].count == 5 && strstr(es.status_message, "Redid: changed Spike Row"));
    /* Far to the right it stops at the validator's most; far to the left
     * at one tile.  The level is valid at every stop. */
    CHECK(handle_screen_point(&es, ENT_SPIKE_ROW, 0, EDITOR_HANDLE_WIDTH, &sx, &sy));
    drag_handle(&es, sx, sy, 700, 0);
    CHECK(es.level.spike_rows[0].count == 16 && level_is_valid(&es));
    {
        /* One more would be refused: the handle stopped exactly at the limit. */
        es.level.spike_rows[0].count = 17;
        CHECK(!level_is_valid(&es));
        es.level.spike_rows[0].count = 16;
    }
    /* The handle is now off the right of the view; bring it back. */
    es.camera.x = 96.0f;
    CHECK(handle_screen_point(&es, ENT_SPIKE_ROW, 0, EDITOR_HANDLE_WIDTH, &sx, &sy));
    drag_handle(&es, sx, sy, -800, 0);
    CHECK(es.level.spike_rows[0].count == 1 && level_is_valid(&es));
    es.camera.x = 0.0f;

    /* ---- A vine: grows downward, and stops at the bottom of the level ---- */
    memset(&pd, 0, sizeof(pd));
    pd.vine.x = 240.0f;
    pd.vine.y = 40.0f;
    pd.vine.tile_count = 2;
    CHECK(add_entity(&es, ENT_VINE, &pd) == 0 && level_is_valid(&es));
    editor_select_only(&es, ENT_VINE, 0);
    CHECK(handle_screen_point(&es, ENT_VINE, 0, EDITOR_HANDLE_HEIGHT, &sx, &sy));
    undo_top = es.undo->top;
    press_and_move(&es, sx, sy, 0, 2 * VINE_STEP * 2);   /* two tiles at 2x */
    CHECK(es.level.vines[0].tile_count == 4);
    CHECK(es.ui.cursor_shown == MOUSE_CURSOR_RESIZE_NS);
    CHECK(strstr(es.status_message, "Vine: 4 tiles tall") != NULL);
    release_at(&es, sx, sy + 2 * VINE_STEP * 2);
    CHECK(es.undo->top == undo_top + 1);
    CHECK(handle_screen_point(&es, ENT_VINE, 0, EDITOR_HANDLE_HEIGHT, &sx, &sy));
    drag_handle(&es, sx, sy, 0, 900);
    CHECK(level_is_valid(&es) && es.level.vines[0].tile_count > 4);
    {
        const int most = es.level.vines[0].tile_count;
        es.level.vines[0].tile_count = most + 1;
        CHECK(!level_is_valid(&es));
        es.level.vines[0].tile_count = most;
    }
    CHECK(es.level.vines[0].y == 40.0f);

    /* ---- A ground pillar: width to the right, height upward ---- */
    memset(&pd, 0, sizeof(pd));
    pd.platform.x = 288.0f;
    pd.platform.tile_height = 2;
    pd.platform.tile_width = 1;
    CHECK(add_entity(&es, ENT_PLATFORM, &pd) == 0 && level_is_valid(&es));
    editor_select_only(&es, ENT_PLATFORM, 0);
    {
        EditorHandle handles[EDITOR_HANDLE_MAX];
        CHECK(editor_entity_handles(&es.level, ENT_PLATFORM, 0, handles) == 2);
    }
    undo_top = es.undo->top;
    CHECK(handle_screen_point(&es, ENT_PLATFORM, 0, EDITOR_HANDLE_WIDTH, &sx, &sy));
    drag_handle(&es, sx, sy, TILE_SIZE * 2, 0);          /* one tile at 2x */
    CHECK(es.level.platforms[0].tile_width == 2 && es.level.platforms[0].tile_height == 2);
    CHECK(handle_screen_point(&es, ENT_PLATFORM, 0, EDITOR_HANDLE_HEIGHT, &sx, &sy));
    drag_handle(&es, sx, sy, 0, -TILE_SIZE * 2);         /* up one tile */
    CHECK(es.level.platforms[0].tile_height == 3 && es.level.platforms[0].tile_width == 2);
    CHECK(strstr(es.status_message, "Platform: 2 x 3 tiles") != NULL);
    CHECK(es.undo->top == undo_top + 2 && level_is_valid(&es));
    CHECK(es.level.platforms[0].x == 288.0f);
    /* Dragged above the top of the level, it stops at the tallest pillar
     * the validator allows. */
    CHECK(handle_screen_point(&es, ENT_PLATFORM, 0, EDITOR_HANDLE_HEIGHT, &sx, &sy));
    drag_handle(&es, sx, sy, 0, -900);
    CHECK(level_is_valid(&es) && es.level.platforms[0].tile_height >= 3);

    /* ---- A rail loop with a spike block on it: the corner sizes w and h ---- */
    memset(&pd, 0, sizeof(pd));
    pd.rail.layout = RAIL_LAYOUT_RECT;
    pd.rail.x = 32;
    pd.rail.y = 32;
    pd.rail.w = 4;
    pd.rail.h = 3;
    CHECK(add_entity(&es, ENT_RAIL, &pd) == 0);
    memset(&pd, 0, sizeof(pd));
    pd.spike_block.rail_index = 0;
    pd.spike_block.t_offset = 1.0f;
    pd.spike_block.speed = 3.0f;
    CHECK(add_entity(&es, ENT_SPIKE_BLOCK, &pd) == 0 && level_is_valid(&es));
    editor_select_only(&es, ENT_RAIL, 0);
    CHECK(handle_screen_point(&es, ENT_RAIL, 0, EDITOR_HANDLE_CORNER, &sx, &sy));
    undo_top = es.undo->top;
    press_and_move(&es, sx, sy, 2 * RAIL_TILE_W * 2, RAIL_TILE_H * 2);
    CHECK(es.level.rails[0].w == 6 && es.level.rails[0].h == 4);
    CHECK(es.ui.cursor_shown == MOUSE_CURSOR_RESIZE_NWSE);
    CHECK(strstr(es.status_message, "Rail: 6 x 4 tiles") != NULL);
    release_at(&es, sx + 2 * RAIL_TILE_W * 2, sy + RAIL_TILE_H * 2);
    CHECK(es.undo->top == undo_top + 1 && level_is_valid(&es));
    CHECK(es.level.rails[0].x == 32 && es.level.rails[0].y == 32);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.rails[0].w == 4 && es.level.rails[0].h == 3 && es.undo->top == undo_top);
    /* Shrunk as far as it goes, it is still a loop the validator accepts. */
    CHECK(handle_screen_point(&es, ENT_RAIL, 0, EDITOR_HANDLE_CORNER, &sx, &sy));
    drag_handle(&es, sx, sy, -400, -400);
    CHECK(es.level.rails[0].w == 2 && es.level.rails[0].h == 2 && level_is_valid(&es));

    /* Coins, stars and the like have nothing to resize. */
    {
        EditorHandle handles[EDITOR_HANDLE_MAX];
        CHECK(editor_entity_handles(&es.level, ENT_PLAYER_SPAWN, 0, handles) == 0);
    }
done:
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/*
 * A drag far past a size limit asks level validation about a handful of
 * counts, not one per tile: the search halves the gap between the nearest
 * accepted and the nearest refused count.  It stops where the old
 * one-tile-at-a-time search did, at the largest count the level accepts
 * (one more is refused), and a drag back in stops at one tile.
 */
static int size_handle_search_is_bounded(void)
{
    int failed = 0;
    EditorState es;
    PlacementData pd;
    int sx, sy, before;
    CHECK(open_editor(&es, NULL) == 0);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);

    memset(&pd, 0, sizeof(pd));
    pd.spike_row.x = 96.0f;
    pd.spike_row.count = 3;
    CHECK(add_entity(&es, ENT_SPIKE_ROW, &pd) == 0 && level_is_valid(&es));
    editor_select_only(&es, ENT_SPIKE_ROW, 0);
    CHECK(handle_screen_point(&es, ENT_SPIKE_ROW, 0, EDITOR_HANDLE_WIDTH, &sx, &sy));
    /* 3000 canvas px at 2x zoom asks for the 128-tile maximum. */
    before = editor_resize_test_validations();
    press_and_move(&es, sx, sy, 3000, 0);
    CHECK(es.resizing);
    CHECK(editor_resize_test_validations() - before <= 9);   /* 1 + log2(128) */
    {
        const int most = es.level.spike_rows[0].count;
        CHECK(most > 3 && level_is_valid(&es));
        es.level.spike_rows[0].count = most + 1;
        CHECK(!level_is_valid(&es));
        es.level.spike_rows[0].count = most;
    }
    /* Back far to the left: one tile, as cheaply. */
    before = editor_resize_test_validations();
    push_event(INPUT_MOUSE_MOVE, 0, 0, sx - 3000, sy);
    ui_frame(&es, sx - 3000, sy);
    CHECK(es.level.spike_rows[0].count == 1);
    CHECK(editor_resize_test_validations() - before <= 9);
    release_at(&es, sx - 3000, sy);
    CHECK(level_is_valid(&es));

    /* A vine dragged far down stops at the bottom of the level. */
    memset(&pd, 0, sizeof(pd));
    pd.vine.x = 240.0f;
    pd.vine.y = 40.0f;
    pd.vine.tile_count = 2;
    CHECK(add_entity(&es, ENT_VINE, &pd) == 0 && level_is_valid(&es));
    editor_select_only(&es, ENT_VINE, 0);
    CHECK(handle_screen_point(&es, ENT_VINE, 0, EDITOR_HANDLE_HEIGHT, &sx, &sy));
    before = editor_resize_test_validations();
    drag_handle(&es, sx, sy, 0, 3000);
    CHECK(editor_resize_test_validations() - before <= 10);  /* the release checks once more */
    {
        const int most = es.level.vines[0].tile_count;
        CHECK(most > 2 && level_is_valid(&es));
        es.level.vines[0].tile_count = most + 1;
        CHECK(!level_is_valid(&es));
        es.level.vines[0].tile_count = most;
    }
done:
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}
