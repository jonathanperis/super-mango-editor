/* Included by tests/editor_validation_test.c. Not compiled alone: the functions are
 * static and use the helpers in that file.
 */
/*
 * A rail copied together with the spike block riding it pastes as a new
 * rail with the copy riding the NEW rail; deleting the pair is one undo
 * step even though the rail must go after its rider.
 */
static int group_copy_and_delete_keep_riders_with_their_rail(void)
{
    EditorState es = {0};
    LevelDef original;
    Command cmd;
    int group;

    editor_level_init_defaults(&es.level);
    es.level.rail_count = 2;
    es.level.rails[0] = (RailPlacement){RAIL_LAYOUT_RECT, 32, 32, 4, 4, 0};
    es.level.rails[1] = (RailPlacement){RAIL_LAYOUT_RECT, 400, 32, 4, 4, 0};
    es.level.spike_block_count = 1;
    es.level.spike_blocks[0] = (SpikeBlockPlacement){1, 1.0f, 3.0f};
    es.undo = undo_create();
    if (!es.undo || level_is_valid("group rail base", &es.level) != 0) goto fail;
    original = es.level;
    editor_set_document_save_point(&es);

    (void)editor_select_items(&es, (Selection[]){{ENT_SPIKE_BLOCK, 0}, {ENT_RAIL, 1}}, 2);
    editor_copy_selected(&es);
    editor_paste_clipboard(&es);
    if (expect_int("pasted rail", es.level.rail_count, 3) != 0 ||
        expect_int("pasted rider", es.level.spike_block_count, 2) != 0 ||
        expect_int("copy rides the copied rail", es.level.spike_blocks[1].rail_index, 2) != 0 ||
        expect_int("original keeps its rail", es.level.spike_blocks[0].rail_index, 1) != 0 ||
        expect_int("both copies selected", editor_selection_count(&es), 2) != 0 ||
        level_is_valid("after group paste", &es.level) != 0) goto fail;

    /* Undo the paste as one step (the editor pops a whole group). */
    group = undo_top_group(es.undo);
    while (undo_top_group(es.undo) == group && group != 0 && undo_pop(es.undo, &cmd))
        editor_apply_undo_command(&es, &cmd, 1);
    /* (The hash covers what is in use; removed slots may keep old bytes.) */
    if (expect_int("paste undone",
                   editor_document_hash(&es.level) == editor_document_hash(&original), 1) != 0)
        goto fail;

    /* Delete the rail together with its rider: allowed, and one step. */
    (void)editor_select_items(&es, (Selection[]){{ENT_RAIL, 1}, {ENT_SPIKE_BLOCK, 0}}, 2);
    tools_delete_selected(&es);
    if (expect_int("pair deleted rails", es.level.rail_count, 1) != 0 ||
        expect_int("pair deleted riders", es.level.spike_block_count, 0) != 0) goto fail;
    group = undo_top_group(es.undo);
    while (undo_top_group(es.undo) == group && group != 0 && undo_pop(es.undo, &cmd))
        editor_apply_undo_command(&es, &cmd, 1);
    if (expect_int("delete undone exactly",
                   editor_document_hash(&es.level) == editor_document_hash(&original), 1) != 0)
        goto fail;

    /* A group paste or duplicate that cannot complete adds nothing at all,
     * and is refused before anything is recorded: the undone delete above
     * can still be redone. */
    es.level.coin_count = MAX_COINS - 1;
    for (int i = 0; i < MAX_COINS - 1; i++) es.level.coins[i] = (CoinPlacement){10.0f, 10.0f};
    (void)editor_select_items(&es, (Selection[]){{ENT_COIN, 0}, {ENT_COIN, 1}}, 2);
    editor_copy_selected(&es);
    {
        int top = es.undo->top;
        int redo = es.undo->redo_top;
        if (expect_int("a delete to redo", redo > 0, 1) != 0) goto fail;
        editor_paste_clipboard(&es);
        if (expect_int("all or nothing", es.level.coin_count, MAX_COINS - 1) != 0 ||
            expect_int("no history left", es.undo->top, top) != 0 ||
            expect_int("redo kept after paste", es.undo->redo_top, redo) != 0 ||
            expect_prefix("paste refusal", es.status_message, "Cannot paste Coin: limit of") != 0)
            goto fail;
        editor_duplicate_selection(&es);
        if (expect_int("duplicate all or nothing", es.level.coin_count, MAX_COINS - 1) != 0 ||
            expect_int("no duplicate history", es.undo->top, top) != 0 ||
            expect_int("redo kept after duplicate", es.undo->redo_top, redo) != 0)
            goto fail;
    }
    undo_destroy(es.undo);
    return 0;
fail:
    undo_destroy(es.undo);
    return 1;
}

static int rail_deletion_keeps_references_valid(void)
{
    EditorState es = {0};
    LevelDef original;
    Command cmd;

    fill_rail_level(&es.level);
    original = es.level;
    es.undo = undo_create();
    if (!es.undo) return 1;
    editor_set_document_save_point(&es);

    /* A referenced rail is refused with an explanation. */
    es.selection.type = ENT_RAIL;
    es.selection.index = 1;
    tools_delete_selected(&es);
    if (expect_int("referenced rail kept", es.level.rail_count, 3) != 0 ||
        expect_prefix("referenced rail status", es.status_message,
                      "Rail 1 is used by 0 spike blocks and 1 float platform") != 0 ||
        expect_int("refusal records no history", es.undo->top, 0) != 0) goto fail;

    /* An unused rail is removed and later references shift down. */
    es.selection.index = 0;
    tools_delete_selected(&es);
    if (expect_int("unused rail removed", es.level.rail_count, 2) != 0 ||
        expect_int("spike block follows rail", es.level.spike_blocks[0].rail_index, 1) != 0 ||
        expect_int("float platform follows rail",
                   es.level.float_platforms[0].rail_index, 0) != 0 ||
        level_is_valid("after rail delete", &es.level) != 0) goto fail;

    /* Undo restores the exact document, so the save point is clean again. */
    if (!undo_pop(es.undo, &cmd)) goto fail;
    editor_apply_undo_command(&es, &cmd, 1);
    if (expect_int("undo rail count", es.level.rail_count, 3) != 0 ||
        expect_int("undo restores bytes",
                   memcmp(&es.level, &original, sizeof(original)) == 0, 1) != 0 ||
        expect_int("undo rail clean", es.modified, 0) != 0) goto fail;

    if (!redo_pop(es.undo, &cmd)) goto fail;
    editor_apply_undo_command(&es, &cmd, 0);
    if (expect_int("redo spike index", es.level.spike_blocks[0].rail_index, 1) != 0 ||
        level_is_valid("after rail redo", &es.level) != 0) goto fail;

    /* A copy made before the delete still pastes onto the rail it rode:
     * undo the delete, copy the spike block (rail 2), redo the delete
     * (that rail is now index 1), then paste. */
    if (!undo_pop(es.undo, &cmd)) goto fail;
    editor_apply_undo_command(&es, &cmd, 1);
    es.selection.type = ENT_SPIKE_BLOCK;
    es.selection.index = 0;
    editor_copy_selected(&es);
    if (!redo_pop(es.undo, &cmd)) goto fail;
    editor_apply_undo_command(&es, &cmd, 0);
    editor_paste_clipboard(&es);
    if (expect_int("pasted copy added", es.level.spike_block_count, 2) != 0 ||
        expect_int("pasted copy follows its rail",
                   es.level.spike_blocks[1].rail_index, 1) != 0 ||
        level_is_valid("after rail-rider paste", &es.level) != 0) goto fail;

    undo_destroy(es.undo);
    return 0;
fail:
    undo_destroy(es.undo);
    return 1;
}

/* Press on (x, y), move to (x + dx, y + dy), release. */
static void drag_by(EditorState *es, float x, float y, float dx, float dy)
{
    tools_mouse_down(es, x, y);
    tools_mouse_drag(es, x + dx, y + dy);
    tools_mouse_up(es, x + dx, y + dy);
}

static int undo_last(EditorState *es)
{
    Command cmd;
    if (!undo_pop(es->undo, &cmd)) return 1;
    editor_apply_undo_command(es, &cmd, 1);
    return 0;
}

static int drag_round_trips_and_follows_grab_point(void)
{
    EditorState es = {0};
    EditorRect r;
    InputEvent event;

    editor_level_init_defaults(&es.level);
    es.undo = undo_create();
    if (!es.undo) return 1;
    es.camera.zoom = 2.0f;
    es.tool = TOOL_SELECT;
    es.level.floor_gap_count = 3;
    es.level.floor_gaps[0] = 64;
    es.level.floor_gaps[1] = 96;
    es.level.floor_gaps[2] = 320;
    es.level.blue_flame_count = 1;
    es.level.blue_flames[0].x = 64.0f;
    es.level.axe_trap_count = 1;
    es.level.axe_traps[0] = (AxeTrapPlacement){200.0f, 0.0f, AXE_MODE_PENDULUM};
    es.level.coin_count = 1;
    es.level.coins[0] = (CoinPlacement){100.0f, 100.0f};
    es.level.spider_count = 1;
    es.level.spiders[0] = (SpiderPlacement){100.0f, 50.0f, 50.0f, 150.0f, 0};
    editor_set_document_save_point(&es);

    /* A flame is drawn centred in its gap; dragging must move the gap by
     * exactly the cursor distance, and undo must return to the save point. */
    if (!editor_entity_bounds(&es.level, ENT_BLUE_FLAME, 0, &r)) goto fail;
    drag_by(&es, r.x + 10.0f, r.y + 10.0f, 32.0f, 0.0f);
    if (expect_float_value("flame drag exact", es.level.blue_flames[0].x, 96.0f) != 0 ||
        undo_last(&es) != 0 ||
        expect_float_value("flame undo", es.level.blue_flames[0].x, 64.0f) != 0 ||
        expect_int("flame undo clean", es.modified, 0) != 0) goto fail;

    /* A flame erupts from a gap, so a drag between gaps lands on the
     * nearest one: 64 + 150 = 214 is 106 px from 320 and 118 px from 96;
     * 64 + 100 = 164 is closer to 96. */
    drag_by(&es, r.x + 10.0f, r.y + 10.0f, 150.0f, 0.0f);
    if (expect_float_value("flame drag snaps to nearest gap",
                           es.level.blue_flames[0].x, 320.0f) != 0 ||
        undo_last(&es) != 0) goto fail;
    drag_by(&es, r.x + 10.0f, r.y + 10.0f, 100.0f, 0.0f);
    if (expect_float_value("flame drag snaps back to closer gap",
                           es.level.blue_flames[0].x, 96.0f) != 0 ||
        undo_last(&es) != 0 ||
        expect_int("flame snap undo clean", es.modified, 0) != 0) goto fail;

    /* Axe y = 0 means "default height"; a horizontal move keeps the 0. */
    if (!editor_entity_bounds(&es.level, ENT_AXE_TRAP, 0, &r)) goto fail;
    drag_by(&es, r.x + 5.0f, r.y + 5.0f, 40.0f, 0.0f);
    if (expect_float_value("axe drag x", es.level.axe_traps[0].pillar_x, 240.0f) != 0 ||
        expect_float_value("axe default y kept", es.level.axe_traps[0].y, 0.0f) != 0 ||
        undo_last(&es) != 0 ||
        expect_int("axe undo clean", es.modified, 0) != 0) goto fail;
    drag_by(&es, r.x + 5.0f, r.y + 5.0f, 0.0f, 10.0f);
    if (expect_float_value("axe vertical drag", es.level.axe_traps[0].y,
                           level_axe_trap_y(&(AxeTrapPlacement){0}) + 10.0f) != 0 ||
        undo_last(&es) != 0 || expect_int("axe y undo clean", es.modified, 0) != 0)
        goto fail;
    /* Moving it up to exactly y = 0 must not read back as "default". */
    drag_by(&es, r.x + 5.0f, r.y + 5.0f, 0.0f,
            -level_axe_trap_y(&(AxeTrapPlacement){0}));
    if (expect_int("axe at top keeps a custom y", es.level.axe_traps[0].y > 0.0f, 1) != 0 ||
        undo_last(&es) != 0) goto fail;

    /* A click, or a wobble under the threshold, changes nothing. */
    tools_mouse_down(&es, 104.0f, 104.0f);
    tools_mouse_drag(&es, 104.5f, 104.0f);
    tools_mouse_up(&es, 104.5f, 104.0f);
    if (expect_int("click records nothing", es.undo->top, 0) != 0 ||
        expect_float_value("click keeps coin", es.level.coins[0].x, 100.0f) != 0 ||
        expect_int("click selects coin", es.selection.type, ENT_COIN) != 0) goto fail;

    /* The grabbed point follows the cursor: no jump to the corner. */
    drag_by(&es, 104.0f, 104.0f, 10.0f, 0.0f);
    if (expect_float_value("grab offset x", es.level.coins[0].x, 110.0f) != 0 ||
        expect_float_value("grab offset y", es.level.coins[0].y, 100.0f) != 0 ||
        undo_last(&es) != 0) goto fail;

    /* Spiders take their patrol range along and stay inside the world. */
    drag_by(&es, 110.0f, 245.0f, 40.0f, 0.0f);
    if (expect_float_value("spider x", es.level.spiders[0].x, 140.0f) != 0 ||
        expect_float_value("spider x0", es.level.spiders[0].patrol_x0, 90.0f) != 0 ||
        expect_float_value("spider x1", es.level.spiders[0].patrol_x1, 190.0f) != 0)
        goto fail;
    drag_by(&es, 150.0f, 245.0f, 5000.0f, 0.0f);
    if (expect_float_value("spider range clamped", es.level.spiders[0].patrol_x1,
                           editor_world_width(&es.level)) != 0 ||
        level_is_valid("after far spider drag", &es.level) != 0) goto fail;
    undo_clear(es.undo);
    editor_set_document_save_point(&es);

    /* Undo is ignored mid-drag; Esc cancels the move without history. */
    tools_mouse_down(&es, 104.0f, 104.0f);
    tools_mouse_drag(&es, 140.0f, 104.0f);
    memset(&event, 0, sizeof(event));
    event.type = INPUT_KEY_DOWN;
    event.key = KEY_Z;
    event.mods = INPUT_CTRL;
    editor_handle_event(&es, &event);
    if (expect_float_value("undo blocked during drag", es.level.coins[0].x, 136.0f) != 0 ||
        expect_prefix("drag blocks keys", es.status_message, "Release the mouse") != 0)
        goto fail;
    event.key = KEY_ESCAPE;
    event.mods = 0;
    editor_handle_event(&es, &event);
    if (expect_float_value("esc restores", es.level.coins[0].x, 100.0f) != 0 ||
        expect_int("esc ends drag", es.dragging, 0) != 0 ||
        expect_int("esc records nothing", es.undo->top, 0) != 0) goto fail;

    /* A level broken while the button is held puts the entity back. */
    tools_mouse_down(&es, 104.0f, 104.0f);
    tools_mouse_drag(&es, 140.0f, 104.0f);
    es.level.music_volume = 999;
    tools_mouse_up(&es, 140.0f, 104.0f);
    es.level.music_volume = 0;
    if (expect_float_value("invalid release restores", es.level.coins[0].x, 100.0f) != 0 ||
        expect_int("invalid release records nothing", es.undo->top, 0) != 0) goto fail;

    undo_destroy(es.undo);
    return 0;
fail:
    undo_destroy(es.undo);
    return 1;
}

/*
 * Every editor mutation must leave a level that passes validation: for each
 * palette type, place at the world corners, paste repeatedly and drag far
 * outside the world.  Refusals are fine; an invalid level is not.
 */
static int editor_mutations_keep_level_valid(void)
{
    char name[96];

    for (int p = 0; p < editor_entity_palette_entry_count(); p++) {
        EntityType type = editor_entity_palette_entry_type(p);
        EditorState es = {0};
        float world_w;
        /* x = -1 stands for "half a pixel before the world's right edge". */
        const float points[3][2] = {
            {0.5f, 0.5f}, {200.0f, 150.0f}, {-1.0f, (float)GAME_H - 0.5f}
        };

        editor_level_init_defaults(&es.level);
        es.level.rail_count = 1;  /* lets spike blocks attach */
        es.level.rails[0] = (RailPlacement){RAIL_LAYOUT_RECT, 32, 32, 4, 4, 0};
        /* Lets flames erupt; a gap at each end so every corner has one near. */
        es.level.floor_gap_count = 2;
        es.level.floor_gaps[0] = 0;
        es.level.floor_gaps[1] = (int)editor_world_width(&es.level) - FLOOR_GAP_W;
        es.undo = undo_create();
        if (!es.undo) return 1;
        es.camera.zoom = 1.0f;
        es.palette_type = type;
        world_w = editor_world_width(&es.level);

        for (int i = 0; i < 3; i++) {
            float x = points[i][0] < 0.0f ? world_w - 0.5f : points[i][0];
            es.tool = TOOL_PLACE;
            tools_mouse_down(&es, x, points[i][1]);
            snprintf(name, sizeof(name), "place %s at %d",
                     editor_entity_type_name(type), i);
            if (level_is_valid(name, &es.level) != 0) goto fail;
        }
        if (es.selection.index < 0) {
            fprintf(stderr, "editor_validation_test: %s was never placed (%s)\n",
                    editor_entity_type_name(type), es.status_message);
            goto fail;
        }

        editor_copy_selected(&es);
        for (int i = 0; i < 4; i++) {
            editor_paste_clipboard(&es);
            snprintf(name, sizeof(name), "paste %s #%d",
                     editor_entity_type_name(type), i);
            if (level_is_valid(name, &es.level) != 0) goto fail;
        }

        {
            EditorRect r;
            if (editor_entity_bounds(&es.level, es.selection.type,
                                     es.selection.index, &r)) {
                es.tool = TOOL_SELECT;
                drag_by(&es, r.x + 1.0f, r.y + 1.0f, 9000.0f, 9000.0f);
                drag_by(&es, r.x + 1.0f, r.y + 1.0f, -9000.0f, -9000.0f);
                snprintf(name, sizeof(name), "drag %s",
                         editor_entity_type_name(type));
                if (level_is_valid(name, &es.level) != 0) goto fail;
            }
        }
        undo_destroy(es.undo);
        continue;
fail:
        undo_destroy(es.undo);
        return 1;
    }
    return 0;
}

static int refused_mutations_explain_why(void)
{
    EditorState es = {0};

    editor_level_init_defaults(&es.level);
    es.undo = undo_create();
    if (!es.undo) return 1;

    /* Rail riders need a rail. */
    es.tool = TOOL_PLACE;
    es.palette_type = ENT_SPIKE_BLOCK;
    tools_mouse_down(&es, 100.0f, 100.0f);
    if (expect_int("no rail no spike block", es.level.spike_block_count, 0) != 0 ||
        expect_string("no rail status", es.status_message,
                      "Cannot place Spike Block: place a rail first") != 0) goto fail;

    /* Flames erupt from a floor gap; with none there is nowhere to go. */
    es.palette_type = ENT_FIRE_FLAME;
    tools_mouse_down(&es, 100.0f, 100.0f);
    if (expect_int("no gap no flame", es.level.fire_flame_count, 0) != 0 ||
        expect_string("no gap status", es.status_message,
                      "Cannot place Fire Flame: place a floor gap first") != 0) goto fail;
    /* With gaps, a click lands the flame on the gap under the cursor
     * (the click is the flame's centre, like the ghost preview). */
    es.level.floor_gap_count = 2;
    es.level.floor_gaps[0] = 64;
    es.level.floor_gaps[1] = 96;
    tools_mouse_down(&es, 96.0f + FLOOR_GAP_W / 2.0f + 5.0f, 100.0f);
    if (expect_int("flame on gap placed", es.level.fire_flame_count, 1) != 0 ||
        expect_float_value("flame on clicked gap", es.level.fire_flames[0].x, 96.0f) != 0)
        goto fail;
    /* Deleting the gap a flame stands on is refused with the rule. */
    es.tool = TOOL_SELECT;
    es.selection.type = ENT_FLOOR_GAP;
    es.selection.index = 1;
    tools_delete_selected(&es);
    if (expect_int("gap under flame kept", es.level.floor_gap_count, 2) != 0 ||
        expect_prefix("gap under flame status", es.status_message,
                      "Cannot delete Floor Gap: fire_flames[0].x") != 0) goto fail;
    es.level.fire_flame_count = 0;
    es.level.floor_gap_count = 0;
    undo_clear(es.undo);
    es.tool = TOOL_PLACE;

    /* A full array is reported instead of silently ignored. */
    es.palette_type = ENT_COIN;
    es.level.coin_count = MAX_COINS;
    for (int i = 0; i < MAX_COINS; i++)
        es.level.coins[i] = (CoinPlacement){10.0f, 10.0f};
    tools_mouse_down(&es, 100.0f, 100.0f);
    if (expect_int("full coins", es.level.coin_count, MAX_COINS) != 0 ||
        expect_prefix("full coin status", es.status_message,
                      "Cannot place Coin: limit of") != 0) goto fail;
    es.selection.type = ENT_COIN;
    es.selection.index = 0;
    editor_copy_selected(&es);
    editor_paste_clipboard(&es);
    if (expect_prefix("full paste status", es.status_message,
                      "Cannot paste Coin: limit of") != 0) goto fail;
    es.level.coin_count = 0;

    /* Rules involving other entities are checked after clamping. */
    es.palette_type = ENT_CHECKPOINT;
    tools_mouse_down(&es, 20.0f, 100.0f);
    if (expect_int("checkpoint behind start refused", es.level.checkpoint_count, 0) != 0 ||
        expect_prefix("checkpoint status", es.status_message,
                      "Cannot place Checkpoint here:") != 0) goto fail;

    /* A clipboard from another level cannot ride a rail this level lacks:
     * the copy remembers its rail's shape, and no rail here matches it. */
    es.clipboard_count = 1;
    memset(&es.clipboard[0], 0, sizeof(es.clipboard[0]));
    es.clipboard[0].type = ENT_SPIKE_BLOCK;
    es.clipboard[0].data.spike_block = (SpikeBlockPlacement){2, 0.0f, 3.0f};
    es.clipboard[0].rail_index = -1;     /* copied in another document */
    es.clipboard[0].rail_item = -1;
    es.clipboard[0].has_rail = 1;
    es.clipboard[0].rail = (RailPlacement){RAIL_LAYOUT_HORIZ, 200, 80, 6, 1, 1};
    es.level.rail_count = 1;
    es.level.rails[0] = (RailPlacement){RAIL_LAYOUT_RECT, 32, 32, 4, 4, 0};
    editor_paste_clipboard(&es);
    if (expect_int("missing rail paste refused", es.level.spike_block_count, 0) != 0 ||
        expect_string("missing rail status", es.status_message,
                      "Paste blocked: the copied Spike Block's rail is not in this level") != 0)
        goto fail;

    /* The same copy re-attaches by rail shape, whatever index it had: the
     * stored index 2 becomes 0, where the matching rail lives here. */
    es.clipboard[0].rail = es.level.rails[0];
    es.clipboard[0].data.spike_block = (SpikeBlockPlacement){2, 11.5f, 3.0f};
    /* A t_offset past the end of a loop also wraps onto the rail. */
    editor_paste_clipboard(&es);
    if (expect_int("wrapped paste count", es.level.spike_block_count, 1) != 0 ||
        expect_int("pasted rider re-attached by shape",
                   es.level.spike_blocks[0].rail_index, 0) != 0 ||
        level_is_valid("wrapped spike block", &es.level) != 0) goto fail;

    undo_destroy(es.undo);
    return 0;
fail:
    undo_destroy(es.undo);
    return 1;
}

static int camera_scrolls_vertically_and_stays_clamped(void)
{
    EditorState es = {0};
    InputEvent event;
    float wx, wy;
    int canvas_bottom = TOOLBAR_H + CANVAS_H - 1;

    editor_level_init_defaults(&es.level);
    es.undo = undo_create();
    if (!es.undo) return 1;
    es.camera.zoom = 2.0f;

    /* Ctrl+wheel near the bottom zooms to 3x around the cursor, so the
     * floor that was under the cursor stays on screen. */
    memset(&event, 0, sizeof(event));
    event.type = INPUT_WHEEL;
    event.x = 200;
    event.y = canvas_bottom;
    event.wheel = 1.0f;
    event.mods = INPUT_CTRL;
    canvas_screen_to_world(&es, event.x, event.y, &wx, &wy);
    editor_handle_event(&es, &event);
    {
        float after_x, after_y;
        canvas_screen_to_world(&es, event.x, event.y, &after_x, &after_y);
        if (expect_float_value("zoom preset", es.camera.zoom, 3.0f) != 0 ||
            expect_float_value("zoom keeps cursor x", after_x, wx) != 0 ||
            expect_int("zoom scrolls down", es.camera.y > 0.0f, 1) != 0) goto fail;
    }
    {
        float floor_screen = ((float)FLOOR_Y - es.camera.y) * es.camera.zoom + TOOLBAR_H;
        if (expect_int("floor visible at 3x", floor_screen >= TOOLBAR_H &&
                       floor_screen <= TOOLBAR_H + CANVAS_H, 1) != 0) goto fail;
    }

    /* Shift+wheel pans vertically and clamps at the world's bottom edge. */
    event.mods = INPUT_SHIFT;
    event.wheel = -50.0f;
    editor_handle_event(&es, &event);
    if (expect_float_value("vertical clamp",
                           es.camera.y, (float)GAME_H - CANVAS_H / es.camera.zoom) != 0)
        goto fail;

    /* macOS reports Shift+wheel as horizontal scroll; it still pans up/down. */
    event.wheel = 0.0f;
    event.wheel_x = 50.0f;
    editor_handle_event(&es, &event);
    if (expect_float_value("shift horizontal wheel pans vertically", es.camera.y, 0.0f) != 0)
        goto fail;
    event.wheel = -50.0f;
    event.wheel_x = 0.0f;
    editor_handle_event(&es, &event);

    /* Clicks use the vertical offset too. */
    es.tool = TOOL_PLACE;
    es.palette_type = ENT_COIN;
    event.type = INPUT_MOUSE_DOWN;
    event.button = MOUSE_BUTTON_LEFT;
    event.mods = 0;
    event.x = 90;
    event.y = TOOLBAR_H + 30;
    editor_handle_event(&es, &event);
    if (expect_int("coin placed", es.level.coin_count, 1) != 0 ||
        expect_float_value("click world y", es.level.coins[0].y,
                           es.camera.y + 30.0f / es.camera.zoom) != 0) goto fail;

    /* Right-click deletes, but not during a left-button drag: deleting
     * would shift the array under the drag and overwrite another entity. */
    es.tool = TOOL_SELECT;
    tools_mouse_down(&es, es.level.coins[0].x + 4.0f, es.level.coins[0].y + 4.0f);
    tools_mouse_drag(&es, es.level.coins[0].x + 40.0f, es.level.coins[0].y + 4.0f);
    event.button = MOUSE_BUTTON_RIGHT;
    editor_handle_event(&es, &event);
    if (expect_int("right-click ignored during drag", es.level.coin_count, 1) != 0)
        goto fail;
    tools_mouse_up(&es, es.level.coins[0].x, es.level.coins[0].y);
    event.type = INPUT_MOUSE_UP;
    editor_handle_event(&es, &event);

    /* Zooming out or shrinking the level re-clamps the camera. */
    es.camera.x = 1000.0f;
    canvas_set_zoom(&es, 1.0f, 0, TOOLBAR_H);
    if (expect_float_value("1x has no vertical scroll", es.camera.y, 0.0f) != 0) goto fail;
    es.level.screen_count = 3;
    canvas_clamp_camera(&es);
    if (expect_float_value("screen shrink clamps x", es.camera.x,
                           3.0f * GAME_W - CANVAS_W) != 0) goto fail;

    undo_destroy(es.undo);
    return 0;
fail:
    undo_destroy(es.undo);
    return 1;
}
