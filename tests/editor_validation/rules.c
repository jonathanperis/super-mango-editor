/* Included by tests/editor_validation_test.c. Not compiled alone: the functions are
 * static and use the helpers in that file.
 */
static int rejects_bad_runtime_link(void)
{
    LevelDef def;
    EditorValidationReport report;

    fill_valid_minimal(&def);
    def.spike_block_count = 1;
    def.spike_blocks[0].rail_index = 0;

    if (expect_int("bad link result", editor_validate_level(&def, &report), -1) != 0)
        return 1;
    /* The block names a rail that does not exist, and its speed was left
     * at 0: both are reported, the rail first. */
    if (expect_int("bad link errors", report.error_count, 2) != 0) return 1;
    if (expect_location("bad link first", &report.locations[0],
                        "spike_blocks", 0, "rail_index") != 0) return 1;

    return 0;
}

static int accepts_valid_level(void)
{
    LevelDef def;
    EditorValidationReport report;

    fill_valid_minimal(&def);

    if (expect_int("valid result", editor_validate_level(&def, &report), 0) != 0)
        return 1;
    if (expect_int("valid errors", report.error_count, 0) != 0) return 1;
    if (expect_int("valid warnings", report.warning_count, 0) != 0) return 1;

    return 0;
}

static int rejects_bad_count_and_path(void)
{
    LevelDef def;
    EditorValidationReport report;

    fill_valid_minimal(&def);
    def.coin_count = MAX_COINS + 1;
    strncpy(def.music_path, "assets/sounds/levels/missing.wav",
            sizeof(def.music_path) - 1);

    if (expect_int("invalid result", editor_validate_level(&def, &report), -1) != 0)
        return 1;
    if (expect_int("invalid errors", report.error_count, 2) != 0) return 1;

    return 0;
}

static int rejects_missing_phase_and_layer_paths(void)
{
    LevelDef def;
    EditorValidationReport report;

    fill_valid_minimal(&def);
    strncpy(def.next_phase, "levels/missing_next.toml", sizeof(def.next_phase) - 1);
    def.background_layer_count = 1;
    strncpy(def.background_layers[0].path,
            "assets/sprites/backgrounds/missing.png",
            sizeof(def.background_layers[0].path) - 1);
    def.foreground_layer_count = 1;
    strncpy(def.foreground_layers[0].path,
            "assets/sprites/foregrounds/missing.png",
            sizeof(def.foreground_layers[0].path) - 1);
    def.fog_layer_count = 1;
    strncpy(def.fog_layers[0].path,
            "assets/sprites/foregrounds/missing_fog.png",
            sizeof(def.fog_layers[0].path) - 1);

    if (expect_int("missing paths result", editor_validate_level(&def, &report), -1) != 0)
        return 1;
    if (expect_int("missing paths errors", report.error_count, 4) != 0) return 1;

    return 0;
}

static int warns_without_blocking(void)
{
    LevelDef def;
    EditorValidationReport report;

    fill_valid_minimal(&def);
    def.name[0] = '\0';

    if (expect_int("warning result", editor_validate_level(&def, &report), 0) != 0)
        return 1;
    if (expect_int("warning errors", report.error_count, 0) != 0) return 1;
    if (expect_int("warning count", report.warning_count, 1) != 0) return 1;

    return 0;
}

/*
 * The editor lists every error the game's rules find and adds only what
 * the game cannot check: that files exist.  It never says the same thing
 * twice, and accepts what the game accepts, such as screen_count 0 (the
 * game's default of four screens).
 */
static int editor_checks_do_not_repeat_runtime_errors(void)
{
    LevelDef def;
    EditorValidationReport report;

    fill_valid_minimal(&def);
    def.screen_count = 0;
    if (expect_int("screen_count 0 accepted", editor_validate_level(&def, &report), 0) != 0 ||
        expect_int("screen_count 0 errors", report.error_count, 0) != 0)
        return 1;

    def.screen_count = -1;
    if (expect_int("negative screens refused", editor_validate_level(&def, &report), -1) != 0 ||
        expect_int("negative screens once", report.error_count, 1) != 0 ||
        expect_prefix("negative screens message", report.messages[0], "screen_count") != 0)
        return 1;

    /* A path in the wrong folder is the game's error; "missing" would
     * be a second message about the same field. */
    fill_valid_minimal(&def);
    snprintf(def.music_path, sizeof(def.music_path), "%s", "music/missing.wav");
    if (expect_int("wrong folder refused", editor_validate_level(&def, &report), -1) != 0 ||
        expect_int("wrong folder once", report.error_count, 1) != 0 ||
        expect_int("game's wording", strstr(report.messages[0], "must match") != NULL, 1) != 0)
        return 1;

    /* A path the game accepts but whose file is missing: the editor's own. */
    snprintf(def.music_path, sizeof(def.music_path), "%s", "assets/sounds/missing.wav");
    if (expect_int("missing refused", editor_validate_level(&def, &report), -1) != 0 ||
        expect_int("missing once", report.error_count, 1) != 0 ||
        expect_prefix("missing message", report.messages[0],
                      "music_path missing: assets/sounds/missing.wav") != 0)
        return 1;
    return 0;
}

static int rejects_unsafe_asset_paths(void)
{
    LevelDef def;
    EditorValidationReport report;

    fill_valid_minimal(&def);
    strncpy(def.floor_tile_path, "/tmp/grass_tileset.png",
            sizeof(def.floor_tile_path) - 1);
    if (expect_int("unsafe floor path result",
                   editor_validate_level(&def, &report), -1) != 0)
        return 1;
    if (report.error_count != 1) {
        fprintf(stderr, "editor_validation_test: unsafe floor path should report exactly one error\n");
        return 1;
    }

    fill_valid_minimal(&def);
    def.platform_count = 1;
    def.platforms[0].x = 64.0f;
    def.platforms[0].tile_height = 1;
    def.platforms[0].tile_width = 1;
    strncpy(def.platforms[0].tile_path, "../secret.png",
            sizeof(def.platforms[0].tile_path) - 1);
    if (expect_int("unsafe platform path result",
                   editor_validate_level(&def, &report), -1) != 0)
        return 1;
    if (report.error_count != 1) {
        fprintf(stderr, "editor_validation_test: unsafe platform path should report exactly one error\n");
        return 1;
    }

    fill_valid_minimal(&def);
    strncpy(def.next_phase, "../levels/evil.toml", sizeof(def.next_phase) - 1);
    if (expect_int("unsafe next phase result",
                   editor_validate_level(&def, &report), -1) != 0)
        return 1;
    if (report.error_count != 1) {
        fprintf(stderr, "editor_validation_test: unsafe next phase should report exactly one error\n");
        return 1;
    }

    return 0;
}

