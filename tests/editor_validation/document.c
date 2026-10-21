/* Included by tests/editor_validation_test.c. Not compiled alone: the functions are
 * static and use the helpers in that file.
 */
/*
 * editor_document_hash lists LevelDef fields by hand; a forgotten field
 * would make edits to it invisible to dirty tracking (no "*", no save
 * prompt).  Entity coverage iterates the palette table, so a new entity
 * type is checked automatically; each element's *content* must matter,
 * not just the array count.
 */
static int expect_hash_changes(const char *name, uint64_t before,
                               const LevelDef *level)
{
    if (editor_document_hash(level) == before) {
        fprintf(stderr, "editor_validation_test: document hash ignores %s\n", name);
        return 1;
    }
    return 0;
}

/*
 * entity_meta.c's table is filled with designated initializers, so a type
 * left out of it compiles (its row is all zeros) unless it is the last one.
 * Check every row here: names, a capacity, a preview, and storage that
 * points at that type's own array (adding one entity changes only its own
 * count).
 */
static int entity_table_has_a_row_for_every_type(void)
{
    static LevelDef level;
    PlacementData data;
    EntityTextures textures;
    Texture2D fake_texture;

    memset(&data, 0, sizeof(data));
    for (size_t i = 0; i < sizeof(textures) / sizeof(Texture2D *); i++)
        ((Texture2D **)&textures)[i] = &fake_texture;

    for (int t = 0; t < ENT_COUNT; t++) {
        EntityType type = (EntityType)t;
        const EditorEntityPreview *preview = editor_entity_preview(type);

        if (strcmp(editor_entity_type_name(type), "Unknown") == 0 ||
            strcmp(editor_entity_palette_name(type), "Unknown") == 0 ||
            editor_entity_capacity(type) <= 0 || !preview) {
            fprintf(stderr, "editor_validation_test: entity type %d has no table row\n", t);
            return 1;
        }
        if (preview->texture != EDITOR_NO_TEXTURE &&
            editor_entity_texture(&textures, type) != &fake_texture) {
            fprintf(stderr, "editor_validation_test: %s preview texture is not an EntityTextures member\n",
                    editor_entity_type_name(type));
            return 1;
        }
        if (editor_entity_type_is_singleton(type)) continue;

        memset(&level, 0, sizeof(level));
        if (editor_entity_insert(&level, type, 0, &data) != 0) return 1;
        for (int other = 0; other < ENT_COUNT; other++) {
            int expected = (other == t) ? 1 : 0;
            if (editor_entity_type_is_singleton((EntityType)other)) continue;
            if (editor_entity_count(&level, (EntityType)other) != expected) {
                fprintf(stderr, "editor_validation_test: inserting a %s changed the %s count\n",
                        editor_entity_type_name(type),
                        editor_entity_type_name((EntityType)other));
                return 1;
            }
        }
    }
    if (editor_entity_preview(ENT_COUNT) != NULL ||
        editor_entity_texture(&textures, ENT_COUNT) != NULL) return 1;
    return 0;
}

static int document_hash_covers_every_entity_and_config_field(void)
{
    LevelDef level;
    uint64_t before;
    PlacementData first, second;

    memset(&first, 0x11, sizeof(first));
    memset(&second, 0x22, sizeof(second));

    for (int p = 0; p < editor_entity_palette_entry_count(); p++) {
        EntityType type = editor_entity_palette_entry_type(p);
        const char *name = editor_entity_type_name(type);

        editor_level_init_defaults(&level);
        before = editor_document_hash(&level);
        if (editor_entity_type_is_singleton(type)) {
            if (editor_entity_write(&level, type, 0, &first) != 0) return 1;
        } else if (editor_entity_insert(&level, type, 0, &first) != 0) {
            return 1;
        }
        if (expect_hash_changes(name, before, &level) != 0) return 1;

        before = editor_document_hash(&level);
        if (editor_entity_write(&level, type, 0, &second) != 0 ||
            expect_hash_changes(name, before, &level) != 0) return 1;
    }

    /* Level-wide settings: take the hash, change one field, compare. */
    editor_level_init_defaults(&level);
    before = editor_document_hash(&level); level.name[0] = 'Z';
    if (expect_hash_changes("name", before, &level)) return 1;
    before = editor_document_hash(&level); strcpy(level.description, "d");
    if (expect_hash_changes("description", before, &level)) return 1;
    before = editor_document_hash(&level); strcpy(level.generated_by, "g");
    if (expect_hash_changes("generated_by", before, &level)) return 1;
    before = editor_document_hash(&level); level.screen_count++;
    if (expect_hash_changes("screen_count", before, &level)) return 1;
    before = editor_document_hash(&level); strcpy(level.next_phase, "levels/x.toml");
    if (expect_hash_changes("next_phase", before, &level)) return 1;
    before = editor_document_hash(&level); level.background_layer_count = 1;
    if (expect_hash_changes("background_layer_count", before, &level)) return 1;
    before = editor_document_hash(&level); level.background_layers[0].path[0] = 'b';
    if (expect_hash_changes("background_layers.path", before, &level)) return 1;
    before = editor_document_hash(&level); level.background_layers[0].speed = 0.5f;
    if (expect_hash_changes("background_layers.speed", before, &level)) return 1;
    before = editor_document_hash(&level); level.foreground_layer_count = 1;
    if (expect_hash_changes("foreground_layer_count", before, &level)) return 1;
    before = editor_document_hash(&level); level.foreground_layers[0].path[0] = 'f';
    if (expect_hash_changes("foreground_layers.path", before, &level)) return 1;
    before = editor_document_hash(&level); level.foreground_layers[0].speed = 0.5f;
    if (expect_hash_changes("foreground_layers.speed", before, &level)) return 1;
    before = editor_document_hash(&level); level.fog_layer_count = 1;
    if (expect_hash_changes("fog_layer_count", before, &level)) return 1;
    before = editor_document_hash(&level); level.fog_layers[0].path[0] = 'o';
    if (expect_hash_changes("fog_layers.path", before, &level)) return 1;
    before = editor_document_hash(&level); level.fog_layers[0].speed = 0.5f;
    if (expect_hash_changes("fog_layers.speed", before, &level)) return 1;
    before = editor_document_hash(&level); level.player_start_x += 1.0f;
    if (expect_hash_changes("player_start_x", before, &level)) return 1;
    before = editor_document_hash(&level); level.player_start_y += 1.0f;
    if (expect_hash_changes("player_start_y", before, &level)) return 1;
    before = editor_document_hash(&level); strcpy(level.music_path, "m");
    if (expect_hash_changes("music_path", before, &level)) return 1;
    before = editor_document_hash(&level); level.music_volume++;
    if (expect_hash_changes("music_volume", before, &level)) return 1;
    before = editor_document_hash(&level); level.floor_tile_path[0] = 'X';
    if (expect_hash_changes("floor_tile_path", before, &level)) return 1;
    before = editor_document_hash(&level); level.initial_hearts++;
    if (expect_hash_changes("initial_hearts", before, &level)) return 1;
    before = editor_document_hash(&level); level.initial_lives++;
    if (expect_hash_changes("initial_lives", before, &level)) return 1;
    before = editor_document_hash(&level); level.score_per_life++;
    if (expect_hash_changes("score_per_life", before, &level)) return 1;
    before = editor_document_hash(&level); level.coin_score++;
    if (expect_hash_changes("coin_score", before, &level)) return 1;

    /* physics is a block of floats: change each one in turn. */
    for (size_t i = 0; i < sizeof(level.physics) / sizeof(float); i++) {
        float value;
        char label[32];
        before = editor_document_hash(&level);
        memcpy(&value, (char *)&level.physics + i * sizeof(float), sizeof(value));
        value += 1.0f;
        memcpy((char *)&level.physics + i * sizeof(float), &value, sizeof(value));
        snprintf(label, sizeof(label), "physics float %zu", i);
        if (expect_hash_changes(label, before, &level) != 0) return 1;
    }
    return 0;
}

/* Feed `output` to file_dialog_read_path as if a picker had printed it. */
static int read_picker_output(const char *output, char *buf, int buf_size)
{
    FILE *fp = tmpfile();
    int result;
    if (!fp) return -100;
    fputs(output, fp);
    rewind(fp);
    result = file_dialog_read_path(fp, buf, buf_size);
    fclose(fp);
    return result;
}

static int dialog_quoting_and_picked_paths_stay_literal(void)
{
    char buf[64];

    /* A picked name containing a line break is refused, never truncated.
     * "\r\n" is a line ending only on Windows; elsewhere the '\r' belongs
     * to the name ("a.toml\r" is a different file from "a.toml"). */
    if (expect_int("plain path", read_picker_output("/tmp/a.toml\n", buf, sizeof(buf)),
                   FILE_DIALOG_SELECTED) != 0 ||
        expect_string("plain path text", buf, "/tmp/a.toml") != 0 ||
#ifdef _WIN32
        expect_int("crlf path", read_picker_output("C:\\a.toml\r\n", buf, sizeof(buf)),
                   FILE_DIALOG_SELECTED) != 0 ||
        expect_string("crlf path text", buf, "C:\\a.toml") != 0 ||
#else
        expect_int("crlf path", read_picker_output("/tmp/a.toml\r\n", buf, sizeof(buf)),
                   FILE_DIALOG_INVALID_PATH) != 0 ||
#endif
        expect_int("trailing carriage return without newline",
                   read_picker_output("/tmp/a.toml\r", buf, sizeof(buf)),
                   FILE_DIALOG_INVALID_PATH) != 0 ||
        expect_int("newline in name",
                   read_picker_output("/tmp/evil\nreal.toml\n", buf, sizeof(buf)),
                   FILE_DIALOG_INVALID_PATH) != 0 ||
        expect_int("carriage return in name",
                   read_picker_output("/tmp/evil\rreal.toml\n", buf, sizeof(buf)),
                   FILE_DIALOG_INVALID_PATH) != 0 ||
        expect_int("no output", read_picker_output("", buf, sizeof(buf)),
                   FILE_DIALOG_CANCELLED) != 0) return 1;

    {
        EditorState es = {0};
        file_dialog_test_set_open_result(FILE_DIALOG_INVALID_PATH, NULL);
        editor_open_level_file(&es);
        if (expect_string("open line break status", es.status_message,
                          "Open failed: file names with line breaks are not supported") != 0)
            return 1;
    }
    return 0;
}

static int property_command_undo_redo(void)
{
    EditorState es;
    Command cmd;

    memset(&es, 0, sizeof(es));
    editor_level_init_defaults(&es.level);
    es.level.coin_count = 1;
    es.level.coins[0].x = 24.0f;
    es.level.coins[0].y = 48.0f;
    es.selection.type = ENT_COIN;
    es.selection.index = 0;
    es.undo = undo_create();
    editor_set_document_save_point(&es);

    editor_begin_change_tracking(&es, EDITOR_CHANGE_ENTITY);
    editor_capture_change_before(&es, -1);
    es.level.coins[0].x = 96.0f;
    editor_commit_change(&es);

    if (expect_float_value("property forward", es.level.coins[0].x, 96.0f) != 0)
        goto fail;
    if (expect_int("property dirty", es.modified, 1) != 0) goto fail;
    if (expect_int("property undo available", es.undo->top, 1) != 0) goto fail;

    if (!undo_pop(es.undo, &cmd)) goto fail;
    editor_apply_undo_command(&es, &cmd, 1);
    editor_refresh_dirty(&es);
    if (expect_float_value("property undo", es.level.coins[0].x, 24.0f) != 0)
        goto fail;
    if (expect_int("property undo clean", es.modified, 0) != 0) goto fail;

    if (!redo_pop(es.undo, &cmd)) goto fail;
    editor_apply_undo_command(&es, &cmd, 0);
    editor_refresh_dirty(&es);
    if (expect_float_value("property redo", es.level.coins[0].x, 96.0f) != 0)
        goto fail;
    if (expect_int("property redo dirty", es.modified, 1) != 0) goto fail;

    undo_destroy(es.undo);
    return 0;

fail:
    undo_destroy(es.undo);
    return 1;
}

static int config_command_preserves_entity_edit(void)
{
    EditorState es;
    Command cmd;

    memset(&es, 0, sizeof(es));
    editor_level_init_defaults(&es.level);
    es.level.coin_count = 1;
    es.level.coins[0].x = 32.0f;
    es.selection.type = ENT_COIN;
    es.selection.index = 0;
    es.undo = undo_create();
    editor_set_document_save_point(&es);

    editor_begin_change_tracking(&es, EDITOR_CHANGE_ENTITY);
    editor_capture_change_before(&es, -1);
    es.level.coins[0].x = 128.0f;
    editor_commit_change(&es);

    editor_begin_change_tracking(&es, EDITOR_CHANGE_CONFIG);
    editor_capture_change_before(&es, -1);
    es.level.music_volume = 77;
    memset(es.level.description,'x',sizeof(es.level.description)-1);
    es.level.description[sizeof(es.level.description)-1]='\0';
    editor_commit_change(&es);

    if (!undo_pop(es.undo, &cmd)) goto fail;
    editor_apply_undo_command(&es, &cmd, 1);
    editor_refresh_dirty(&es);
    if (expect_float_value("config undo keeps entity", es.level.coins[0].x,
                           128.0f) != 0)
        goto fail;
    if (expect_int("config undo restores config", es.level.music_volume, 0) != 0)
        goto fail;
    if (expect_string("long metadata undo", es.level.description, "") != 0) goto fail;

    if (!redo_pop(es.undo, &cmd)) goto fail;
    editor_apply_undo_command(&es, &cmd, 0);
    editor_refresh_dirty(&es);
    if (expect_int("config redo", es.level.music_volume, 77) != 0) goto fail;
    if (expect_int("long metadata redo length", (int)strlen(es.level.description), LEVEL_DESCRIPTION_CAPACITY-1) != 0) goto fail;

    undo_destroy(es.undo);
    return 0;

fail:
    undo_destroy(es.undo);
    return 1;
}

static int last_star_text_property_undo_redo(void)
{
    EditorState es;
    Command cmd;

    memset(&es, 0, sizeof(es));
    editor_level_init_defaults(&es.level);
    strncpy(es.level.next_phase, "levels/old.toml",
            sizeof(es.level.next_phase) - 1);
    es.selection.type = ENT_LAST_STAR;
    es.selection.index = 0;
    es.undo = undo_create();
    editor_set_document_save_point(&es);

    editor_begin_change_tracking(&es, EDITOR_CHANGE_ENTITY);
    editor_capture_change_before(&es, EDITOR_LAST_STAR_NEXT_PHASE_WIDGET);
    strncpy(es.level.next_phase, "levels/new.toml",
            sizeof(es.level.next_phase) - 1);
    editor_commit_change(&es);
    if (expect_int("last star text command", es.undo->top, 1) != 0) goto fail;

    if (!undo_pop(es.undo, &cmd)) goto fail;
    editor_apply_undo_command(&es, &cmd, 1);
    if (expect_string("last star text undo", es.level.next_phase,
                      "levels/old.toml") != 0)
        goto fail;
    if (expect_int("last star text undo clean", es.modified, 0) != 0) goto fail;

    if (!redo_pop(es.undo, &cmd)) goto fail;
    editor_apply_undo_command(&es, &cmd, 0);
    if (expect_string("last star text redo", es.level.next_phase,
                      "levels/new.toml") != 0)
        goto fail;
    if (expect_int("last star text redo dirty", es.modified, 1) != 0) goto fail;

    /* The text lives on the heap; when it cannot be recorded, the edit is
     * taken back rather than left without an undo step. */
    editor_begin_change_tracking(&es, EDITOR_CHANGE_ENTITY);
    editor_capture_change_before(&es, EDITOR_LAST_STAR_NEXT_PHASE_WIDGET);
    strncpy(es.level.next_phase, "levels/other.toml", sizeof(es.level.next_phase) - 1);
    undo_test_limit_allocations(0);
    editor_commit_change(&es);
    undo_test_limit_allocations(-1);
    if (expect_string("unrecorded text taken back", es.level.next_phase,
                      "levels/new.toml") != 0 ||
        expect_int("still one step", es.undo->top, 1) != 0 ||
        expect_prefix("cancel reported", es.status_message,
                      "Edit cancelled: cannot allocate undo history") != 0)
        goto fail;

    /* An entry without text costs no allocation and stores none. */
    if (expect_int("text off the entry", (int)(sizeof(UndoEntry) < 256), 1) != 0) goto fail;

    undo_destroy(es.undo);
    return 0;

fail:
    undo_destroy(es.undo);
    return 1;
}

static int dirty_save_point_tracks_undo_redo(void)
{
    EditorState es;
    Command cmd;

    memset(&es, 0, sizeof(es));
    editor_level_init_defaults(&es.level);
    es.level.coin_count = 1;
    es.level.coins[0].x = 16.0f;
    es.selection.type = ENT_COIN;
    es.selection.index = 0;
    es.undo = undo_create();
    editor_set_document_save_point(&es);

    editor_begin_change_tracking(&es, EDITOR_CHANGE_ENTITY);
    editor_capture_change_before(&es, -1);
    es.level.coins[0].x = 64.0f;
    editor_commit_change(&es);
    if (expect_int("save point edit dirty", es.modified, 1) != 0) goto fail;

    if (!undo_pop(es.undo, &cmd)) goto fail;
    editor_apply_undo_command(&es, &cmd, 1);
    editor_refresh_dirty(&es);
    if (expect_int("save point undo clean", es.modified, 0) != 0) goto fail;

    if (!redo_pop(es.undo, &cmd)) goto fail;
    editor_apply_undo_command(&es, &cmd, 0);
    editor_refresh_dirty(&es);
    if (expect_int("save point redo dirty", es.modified, 1) != 0) goto fail;

    undo_destroy(es.undo);
    return 0;

fail:
    undo_destroy(es.undo);
    return 1;
}

static int selection_structural_mutations_are_safe(void)
{
    EditorState es;
    Command cmd;

    memset(&es, 0, sizeof(es));
    editor_level_init_defaults(&es.level);
    es.undo = undo_create();
    if (!es.undo) return 1;

    es.level.coin_count = 2;
    es.level.coins[0].x = 16.0f;
    es.level.coins[0].y = 16.0f;
    es.level.coins[1].x = 64.0f;
    es.level.coins[1].y = 16.0f;
    es.selection.type = ENT_COIN;
    es.selection.index = 1;
    es.tool = TOOL_DELETE;

    tools_mouse_down(&es, 16.0f, 16.0f);
    if (expect_int("delete shifts selected index", es.selection.index, 0) != 0)
        goto fail;
    if (expect_float_value("delete keeps selected entity", es.level.coins[0].x,
                           64.0f) != 0)
        goto fail;

    if (!undo_pop(es.undo, &cmd)) goto fail;
    editor_apply_undo_command(&es, &cmd, 1);
    if (expect_int("undo delete shifts selection back", es.selection.index, 1) != 0)
        goto fail;
    if (!editor_selection_is_valid(&es)) goto fail;

    if (!redo_pop(es.undo, &cmd)) goto fail;
    editor_apply_undo_command(&es, &cmd, 0);
    if (expect_int("redo delete shifts selection", es.selection.index, 0) != 0)
        goto fail;
    if (!editor_selection_is_valid(&es)) goto fail;

    undo_clear(es.undo);
    es.level.coin_count = 0;
    es.selection.index = -1;
    es.tool = TOOL_PLACE;
    es.palette_type = ENT_COIN;
    tools_mouse_down(&es, 120.0f, 48.0f);
    if (expect_int("place selects new entity", es.selection.index, 0) != 0)
        goto fail;
    if (!editor_selection_is_valid(&es)) goto fail;

    if (!undo_pop(es.undo, &cmd)) goto fail;
    editor_apply_undo_command(&es, &cmd, 1);
    if (expect_int("undo selected place clears selection",
                   es.selection.index, -1) != 0)
        goto fail;
    if (expect_int("undo selected place count", es.level.coin_count, 0) != 0)
        goto fail;

    if (!redo_pop(es.undo, &cmd)) goto fail;
    editor_apply_undo_command(&es, &cmd, 0);
    if (expect_int("redo place selects valid entity", es.selection.index, 0) != 0)
        goto fail;

    editor_copy_selected(&es);
    editor_paste_clipboard(&es);
    if (expect_int("paste count", es.level.coin_count, 2) != 0) goto fail;
    if (!editor_selection_is_valid(&es)) goto fail;
    if (!undo_pop(es.undo, &cmd)) goto fail;
    editor_apply_undo_command(&es, &cmd, 1);
    if (expect_int("undo paste count", es.level.coin_count, 1) != 0) goto fail;
    if (expect_int("undo paste selection safe", es.selection.index, -1) != 0)
        goto fail;

    es.selection.type = ENT_COIN;
    es.selection.index = 99;
    editor_selection_reconcile(&es);
    if (expect_int("invalid selection reconciled", es.selection.index, -1) != 0)
        goto fail;

    undo_destroy(es.undo);
    return 0;

fail:
    undo_destroy(es.undo);
    return 1;
}

static int checkpoint_editor_mutations_are_reversible(void)
{
    EditorState es = {0};
    Command cmd;

    editor_level_init_defaults(&es.level);
    es.undo = undo_create();
    es.tool = TOOL_PLACE;
    es.palette_type = ENT_CHECKPOINT;
    tools_mouse_down(&es, 120.0f, 180.0f);
    if (expect_int("checkpoint place count", es.level.checkpoint_count, 1) != 0)
        goto fail;
    if (expect_float_value("checkpoint place y", es.level.checkpoints[0].y, 180.0f) != 0)
        goto fail;

    es.tool = TOOL_SELECT;
    tools_mouse_down(&es, 120.0f, 180.0f);
    if (expect_int("checkpoint select", es.selection.index, 0) != 0) goto fail;
    tools_mouse_drag(&es, 160.0f, 200.0f);
    tools_mouse_up(&es, 160.0f, 200.0f);
    if (expect_float_value("checkpoint drag", es.level.checkpoints[0].x, 160.0f) != 0)
        goto fail;

    editor_copy_selected(&es);
    editor_paste_clipboard(&es);
    if (expect_int("checkpoint paste count", es.level.checkpoint_count, 2) != 0)
        goto fail;
    if (!undo_pop(es.undo, &cmd)) goto fail;
    editor_apply_undo_command(&es, &cmd, 1);
    if (expect_int("checkpoint undo paste", es.level.checkpoint_count, 1) != 0)
        goto fail;
    if (!redo_pop(es.undo, &cmd)) goto fail;
    editor_apply_undo_command(&es, &cmd, 0);
    if (expect_int("checkpoint redo paste", es.level.checkpoint_count, 2) != 0)
        goto fail;

    es.selection.type = ENT_CHECKPOINT;
    es.selection.index = 1;
    tools_delete_selected(&es);
    if (expect_int("checkpoint delete count", es.level.checkpoint_count, 1) != 0)
        goto fail;

    undo_destroy(es.undo);
    return 0;
fail:
    undo_destroy(es.undo);
    return 1;
}

/* Three valid rails; a spike block rides rail 2, a float platform rail 1. */
static void fill_rail_level(LevelDef *level)
{
    editor_level_init_defaults(level);
    level->rail_count = 3;
    for (int i = 0; i < 3; i++)
        level->rails[i] = (RailPlacement){RAIL_LAYOUT_RECT, 32 + i * 128, 32, 4, 4, 0};
    level->spike_block_count = 1;
    level->spike_blocks[0] = (SpikeBlockPlacement){2, 1.5f, 3.0f};
    level->float_platform_count = 1;
    level->float_platforms[0] = (FloatPlatformPlacement){
        FLOAT_PLATFORM_RAIL, 0.0f, 0.0f, 3, 1, 2.0f, 1.0f};
}

static int level_is_valid(const char *name, const LevelDef *level)
{
    char error[128];
    if (level_validate_runtime(level, error, sizeof(error)) != 0) {
        fprintf(stderr, "editor_validation_test: %s invalid: %s\n", name, error);
        return 1;
    }
    return 0;
}

/*
 * A Static or Crumble float platform keeps its old rail_index, and deleting
 * rails renumbers only the references that are in use.  Switching such a
 * platform back to Rail must not leave it on a rail that no longer exists
 * (the level would stop validating), and must say which rail it rides.
 */
static int float_platform_rail_switch_rechecks_its_rail(void)
{
    EditorState es = {0};

    fill_rail_level(&es.level);
    es.undo = undo_create();
    if (!es.undo) return 1;

    /* Rail 1 -> Static, then delete rails 0 and 1 (the spike block rides
     * rail 2, which becomes rail 0).  The platform still says rail 1. */
    if (editor_set_float_platform_mode(&es, 0, FLOAT_PLATFORM_STATIC) != 0) goto fail;
    es.level.float_platforms[0].x = 400.0f;
    es.level.float_platforms[0].y = 100.0f;
    es.selection.type = ENT_RAIL;
    es.selection.index = 0;
    tools_delete_selected(&es);
    es.selection.index = 0;
    tools_delete_selected(&es);
    if (expect_int("two rails deleted", es.level.rail_count, 1) != 0 ||
        expect_int("static platform kept its number",
                   es.level.float_platforms[0].rail_index, 1) != 0) goto fail;

    /* Back to Rail: the missing rail is replaced by the nearest one. */
    if (expect_int("switch to rail", editor_set_float_platform_mode(&es, 0,
                   FLOAT_PLATFORM_RAIL), 0) != 0 ||
        expect_int("re-picked rail", es.level.float_platforms[0].rail_index, 0) != 0 ||
        expect_prefix("re-pick explained", es.status_message,
                      "Float Platform rides rail 0 (its rail 1 no longer exists)") != 0 ||
        level_is_valid("after switch to rail", &es.level) != 0) goto fail;

    /* A number that still names a rail is kept, and announced. */
    if (editor_set_float_platform_mode(&es, 0, FLOAT_PLATFORM_CRUMBLE) != 0 ||
        editor_set_float_platform_mode(&es, 0, FLOAT_PLATFORM_RAIL) != 0 ||
        expect_prefix("kept rail announced", es.status_message,
                      "Float Platform rides rail 0; edit rail_index") != 0) goto fail;

    /* Without any rail the switch is refused and nothing changes. */
    if (editor_set_float_platform_mode(&es, 0, FLOAT_PLATFORM_STATIC) != 0) goto fail;
    es.level.spike_block_count = 0;
    es.level.rail_count = 0;
    if (expect_int("no rail refused", editor_set_float_platform_mode(&es, 0,
                   FLOAT_PLATFORM_RAIL), -1) != 0 ||
        expect_int("mode unchanged", (int)es.level.float_platforms[0].mode,
                   (int)FLOAT_PLATFORM_STATIC) != 0 ||
        expect_prefix("no rail explained", es.status_message,
                      "Cannot switch Float Platform to Rail: place a rail first") != 0)
        goto fail;
    undo_destroy(es.undo);
    return 0;
fail:
    undo_destroy(es.undo);
    return 1;
}

/*
 * The derived positions and path rules live once (level.h, level_validate.h)
 * and the game, the validator and the editor all call them.  Pin their
 * values, and check the editor's hit test uses the same platform top.
 */
static int shared_level_rules_have_one_answer(void)
{
    LevelDef level;
    EditorRect r;

    editor_level_init_defaults(&level);
    level.platform_count = 1;
    level.platforms[0] = (PlatformPlacement){100.0f, 2, 1, ""};
    if (expect_float_value("2-tile platform top", level_platform_top_y(2),
                           (float)(FLOOR_Y - 2 * TILE_SIZE + 16)) != 0 ||
        expect_float_value("default axe y", level_axe_trap_y(&(AxeTrapPlacement){0}),
                           level_platform_top_y(3)) != 0 ||
        expect_float_value("custom axe y",
                           level_axe_trap_y(&(AxeTrapPlacement){.y = 50.0f}), 50.0f) != 0 ||
        expect_float_value("default saw y",
                           level_circular_saw_y(&(CircularSawPlacement){0}),
                           level_platform_top_y(2) - SAW_DISPLAY_H) != 0 ||
        !editor_entity_bounds(&level, ENT_PLATFORM, 0, &r) ||
        expect_float_value("hit test platform top", r.y, level_platform_top_y(2)) != 0)
        return 1;
    if (expect_int("parent segment", level_path_has_parent_segment("assets/../x.png"), 1) != 0 ||
        expect_int("leading parent", level_path_has_parent_segment("../x.png"), 1) != 0 ||
        expect_int("dots in a name", level_path_has_parent_segment("a/..b/c..png"), 0) != 0 ||
        expect_int("control byte", level_path_has_control_char("a\nb"), 1) != 0 ||
        expect_int("plain path", level_path_has_control_char("assets/a.png"), 0) != 0)
        return 1;
    return 0;
}

static int expect_location(const char *name, const LevelIssueLocation *where,
                           const char *path, int index, const char *field)
{
    if (strcmp(where->path, path) != 0 || where->index != index ||
        strcmp(where->field, field) != 0) {
        fprintf(stderr, "editor_validation_test: %s got {%s, %d, %s} expected {%s, %d, %s}\n",
                name, where->path, where->index, where->field, path, index, field);
        return 1;
    }
    return 0;
}

/*
 * Validation messages carry a structured location (spec N-001) so the
 * editor can take the designer to the problem.  The validator promises
 * that every message starts with the TOML path of the bad value; these
 * cases pin that promise for each family of checks, including the ones
 * that used to print "spiders[].frame_index" without the element.
 */
static int validation_errors_report_where_they_are(void)
{
    LevelDef base, level;
    LevelIssueLocation where;
    char err[256];

    if (level_issue_location_parse("coins[3].x is 9.00 (expected 0..1)", &where) != 1 ||
        expect_location("parse element field", &where, "coins", 3, "x") != 0 ||
        level_issue_location_parse("screen_count is 0 (expected 1..99)", &where) != 1 ||
        expect_location("parse key", &where, "screen_count", -1, "") != 0 ||
        level_issue_location_parse("rails[2] has negative origin", &where) != 1 ||
        expect_location("parse element", &where, "rails", 2, "") != 0 ||
        expect_int("not a path", level_issue_location_parse("LevelDef is NULL", &where), 0) != 0 ||
        expect_int("half a path", level_issue_location_parse("coins[x].y bad", &where), 0) != 0 ||
        expect_location("cleared on failure", &where, "", -1, "") != 0)
        return 1;

    editor_level_init_defaults(&base);
    base.coin_count = 2;
    base.coins[0] = (CoinPlacement){100.0f, 100.0f};
    base.coins[1] = (CoinPlacement){140.0f, 100.0f};
    base.spider_count = 1;
    base.spiders[0] = (SpiderPlacement){300.0f, 50.0f, 250.0f, 350.0f, 0};
    base.axe_trap_count = 1;
    base.axe_traps[0] = (AxeTrapPlacement){.pillar_x = 500.0f, .y = 0.0f, .mode = AXE_MODE_PENDULUM};
    base.checkpoint_count = 1;
    base.checkpoints[0] = (CheckpointPlacement){600.0f, 100.0f};
    if (level_is_valid("location base", &base) != 0 ||
        expect_int("valid has no location",
                   level_validate_runtime_at(&base, err, sizeof(err), &where), 0) != 0 ||
        expect_location("valid location", &where, "", -1, "") != 0) return 1;

    level = base;
    level.coins[1].x = 99999.0f;
    if (level_validate_runtime_at(&level, err, sizeof(err), &where) == 0 ||
        expect_location("coin x", &where, "coins", 1, "x") != 0) return 1;
    level = base;
    level.spiders[0].frame_index = 99;
    if (level_validate_runtime_at(&level, err, sizeof(err), &where) == 0 ||
        expect_location("spider frame", &where, "spiders", 0, "frame_index") != 0) return 1;
    level = base;
    level.spiders[0].vx = 1.0e9f;
    if (level_validate_runtime_at(&level, err, sizeof(err), &where) == 0 ||
        expect_location("spider vx", &where, "spiders", 0, "vx") != 0) return 1;
    level = base;
    level.axe_traps[0].y = 5000.0f;
    if (level_validate_runtime_at(&level, err, sizeof(err), &where) == 0 ||
        expect_location("axe y", &where, "axe_traps", 0, "y") != 0) return 1;
    level = base;
    level.checkpoints[0].x = 1.0f;          /* behind the player start */
    if (level_validate_runtime_at(&level, err, sizeof(err), &where) == 0 ||
        expect_location("checkpoint x", &where, "checkpoints", 0, "x") != 0) return 1;
    level = base;
    level.screen_count = 500;
    if (level_validate_runtime_at(&level, err, sizeof(err), &where) == 0 ||
        expect_location("screens", &where, "screen_count", -1, "") != 0) return 1;
    level = base;
    level.physics.air_friction = NAN;
    if (level_validate_runtime_at(&level, err, sizeof(err), &where) == 0 ||
        expect_location("physics", &where, "physics", -1, "air_friction") != 0) return 1;
    level = base;
    level.player_start_x = 99999.0f;
    if (level_validate_runtime_at(&level, err, sizeof(err), &where) == 0 ||
        expect_location("player start", &where, "player_start", -1, "x") != 0) return 1;

    /* The editor's report keeps the location next to each message, and
     * maps TOML names back to its entity types. */
    {
        EditorValidationReport report;
        level = base;
        level.coins[1].x = 99999.0f;
        level.name[0] = '\0';
        (void)editor_validate_level(&level, &report);
        if (expect_int("report messages", report.message_count >= 2, 1) != 0 ||
            expect_location("report error", &report.locations[0], "coins", 1, "x") != 0 ||
            expect_location("report warning", &report.locations[report.message_count - 1],
                            "name", -1, "") != 0 ||
            expect_int("coins type", editor_entity_type_for_toml("coins"), ENT_COIN) != 0 ||
            expect_int("checkpoint type", editor_entity_type_for_toml("checkpoints"),
                       ENT_CHECKPOINT) != 0 ||
            expect_int("spawn type", editor_entity_type_for_toml("player_start"),
                       ENT_PLAYER_SPAWN) != 0 ||
            expect_int("config key", editor_entity_type_for_toml("screen_count"), ENT_COUNT) != 0)
            return 1;
    }
    return 0;
}

/* Every error level_validate_runtime_each reported, in order. */
typedef struct {
    int count;
    char first[256];
    LevelIssueLocation where[16];
} CollectedIssues;

static void collect_issue(void *context, const char *message,
                          const LevelIssueLocation *where)
{
    CollectedIssues *issues = context;
    if (issues->count == 0) snprintf(issues->first, sizeof(issues->first), "%s", message);
    if (issues->count < 16) issues->where[issues->count] = *where;
    issues->count++;
}

/*
 * The editor lists every runtime error at once, while the game keeps its
 * single-error API: the first error collected is word for word the one
 * level_validate_runtime reports.  Checks that need an earlier value to be
 * good (t_offset needs a valid rail_index) are skipped instead of reading
 * past an array, and a bad array count stops everything.
 */
static int validation_reports_every_runtime_error(void)
{
    LevelDef level;
    CollectedIssues issues;
    EditorValidationReport report;
    char err[256];

    editor_level_init_defaults(&level);
    level.coin_count = 2;
    level.coins[0] = (CoinPlacement){100.0f, 100.0f};
    level.coins[1] = (CoinPlacement){99999.0f, 100.0f};      /* 1: coins[1].x  */
    level.spider_count = 1;
    level.spiders[0] = (SpiderPlacement){300.0f, 0.0f, 250.0f, 350.0f, 0};  /* vx 0 */
    level.axe_trap_count = 1;
    level.axe_traps[0] = (AxeTrapPlacement){.pillar_x = 500.0f, .mode = (AxeTrapMode)7};
    level.spike_block_count = 1;                               /* no rails at all */
    level.spike_blocks[0] = (SpikeBlockPlacement){5, 1.0f, 3.0f};
    level.physics.air_friction = NAN;

    memset(&issues, 0, sizeof(issues));
    if (expect_int("errors found",
                   level_validate_runtime_each(&level, collect_issue, &issues), 5) != 0 ||
        expect_int("errors reported", issues.count, 5) != 0 ||
        level_validate_runtime(&level, err, sizeof(err)) == 0 ||
        expect_int("first matches the game", strcmp(issues.first, err), 0) != 0 ||
        expect_location("physics first", &issues.where[0], "physics", -1, "air_friction") != 0 ||
        expect_location("spider vx", &issues.where[1], "spiders", 0, "vx") != 0 ||
        expect_location("coin x", &issues.where[2], "coins", 1, "x") != 0 ||
        expect_location("rider rail", &issues.where[3], "spike_blocks", 0, "rail_index") != 0 ||
        expect_location("axe mode", &issues.where[4], "axe_traps", 0, "mode") != 0)
        return 1;

    /* The editor's report lists them all, each with its location. */
    (void)editor_validate_level(&level, &report);
    if (expect_int("report errors", report.error_count, 5) != 0 ||
        expect_location("report first", &report.locations[0], "physics", -1, "air_friction") != 0 ||
        expect_location("report last", &report.locations[4], "axe_traps", 0, "mode") != 0 ||
        expect_int("nothing hidden", editor_validation_hidden_count(&report), 0) != 0)
        return 1;

    /* Every coin, star, climbable, platform, bridge and pad left of the
     * world (or worse): more errors than the report keeps.  It keeps the
     * first EDITOR_VALIDATION_MAX_MESSAGES, and the counts keep them all. */
    editor_level_init_defaults(&level);
    level.coin_count = MAX_COINS;
    for (int i = 0; i < MAX_COINS; i++) level.coins[i] = (CoinPlacement){-5.0f, 100.0f};
    level.star_yellow_count = level.star_green_count = level.star_red_count = 16;
    for (int i = 0; i < 16; i++) {
        level.star_yellows[i] = (StarYellowPlacement){-5.0f, 100.0f};
        level.star_greens[i] = (StarGreenPlacement){-5.0f, 100.0f};
        level.star_reds[i] = (StarRedPlacement){-5.0f, 100.0f};
    }
    level.vine_count = MAX_VINES;
    for (int i = 0; i < MAX_VINES; i++) level.vines[i] = (VinePlacement){-5.0f, 100.0f, 3, 0};
    level.ladder_count = level.rope_count = 16;
    for (int i = 0; i < 16; i++) {
        level.ladders[i] = (LadderPlacement){-5.0f, 100.0f, 3};
        level.ropes[i] = (RopePlacement){-5.0f, 100.0f, 3};
    }
    level.platform_count = MAX_PLATFORMS;
    for (int i = 0; i < MAX_PLATFORMS; i++)
        level.platforms[i] = (PlatformPlacement){.x = -5.0f, .tile_height = 1, .tile_width = 1};
    level.bridge_count = MAX_BRIDGES;
    for (int i = 0; i < MAX_BRIDGES; i++) level.bridges[i] = (BridgePlacement){-5.0f, 100.0f, 3};
    level.bouncepad_small_count = MAX_BOUNCEPADS_SMALL;
    level.bouncepad_medium_count = MAX_BOUNCEPADS_MEDIUM;
    level.bouncepad_high_count = MAX_BOUNCEPADS_HIGH;
    for (int i = 0; i < 16; i++) {
        level.bouncepads_small[i] = (BouncepadPlacement){.x = -5.0f};
        level.bouncepads_medium[i] = (BouncepadPlacement){.x = -5.0f};
        level.bouncepads_high[i] = (BouncepadPlacement){.x = -5.0f};
    }
    (void)editor_validate_level(&level, &report);
    if (expect_int("more than kept", report.error_count > EDITOR_VALIDATION_MAX_MESSAGES, 1) != 0 ||
        expect_int("listed", report.message_count, EDITOR_VALIDATION_MAX_MESSAGES) != 0 ||
        expect_int("hidden", editor_validation_hidden_count(&report),
                   report.error_count - EDITOR_VALIDATION_MAX_MESSAGES) != 0)
        return 1;

    /* A bad count is reported alone: the arrays cannot be walked. */
    level.coin_count = MAX_COINS + 1;
    memset(&issues, 0, sizeof(issues));
    if (expect_int("count stops", level_validate_runtime_each(&level, collect_issue, &issues), 1) != 0 ||
        expect_location("count location", &issues.where[0], "coin_count", -1, "") != 0)
        return 1;

    /* A valid level reports nothing. */
    editor_level_init_defaults(&level);
    memset(&issues, 0, sizeof(issues));
    if (expect_int("valid", level_validate_runtime_each(&level, collect_issue, &issues), 0) != 0 ||
        expect_int("valid calls", issues.count, 0) != 0)
        return 1;
    return 0;
}
