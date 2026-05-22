#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#include "editor/editor.h"
#include "editor/editor_files.h"
#include "editor/editor_session.h"
#include "editor/editor_validation.h"
#include "editor/undo.h"

#define EDITOR_WORKFLOW_LEVEL_PATH "out/test_editor_workflow_level.toml"
#define EDITOR_WORKFLOW_RECENT_PATH "out/editor_recent.txt"

static int expect_int(const char *name, int actual, int expected)
{
    if (actual != expected) {
        fprintf(stderr, "editor_validation_test: %s got %d expected %d\n",
                name, actual, expected);
        return 1;
    }
    return 0;
}

static int expect_string(const char *name, const char *actual,
                         const char *expected)
{
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "editor_validation_test: %s got '%s' expected '%s'\n",
                name, actual, expected);
        return 1;
    }
    return 0;
}

static int expect_prefix(const char *name, const char *actual,
                         const char *expected)
{
    size_t len = strlen(expected);

    if (strncmp(actual, expected, len) != 0) {
        fprintf(stderr, "editor_validation_test: %s got '%s' expected prefix '%s'\n",
                name, actual, expected);
        return 1;
    }
    return 0;
}

static void ensure_out_dir(void)
{
#ifdef _WIN32
    _mkdir("out");
#else
    mkdir("out", 0755);
#endif
}

static void fill_valid_minimal(LevelDef *def)
{
    level_def_init_defaults(def);
    strncpy(def->name, "Validation Fixture", sizeof(def->name) - 1);
    def->screen_count = 1;
    strncpy(def->floor_tile_path, "assets/sprites/levels/grass_tileset.png",
            sizeof(def->floor_tile_path) - 1);
    def->last_star.x = 100.0f;
    def->last_star.y = 100.0f;
}

static int rejects_bad_runtime_link(void)
{
    LevelDef def;
    EditorValidationReport report;

    fill_valid_minimal(&def);
    def.spike_block_count = 1;
    def.spike_blocks[0].rail_index = 0;

    if (expect_int("bad link result", editor_validate_level(&def, &report), -1) != 0)
        return 1;
    if (expect_int("bad link errors", report.error_count, 1) != 0) return 1;

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
    if (report.error_count < 1) {
        fprintf(stderr, "editor_validation_test: unsafe floor path should report error\n");
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
    if (report.error_count < 1) {
        fprintf(stderr, "editor_validation_test: unsafe platform path should report error\n");
        return 1;
    }

    fill_valid_minimal(&def);
    strncpy(def.next_phase, "../levels/evil.toml", sizeof(def.next_phase) - 1);
    if (expect_int("unsafe next phase result",
                   editor_validate_level(&def, &report), -1) != 0)
        return 1;
    if (report.error_count < 1) {
        fprintf(stderr, "editor_validation_test: unsafe next phase should report error\n");
        return 1;
    }

    return 0;
}

static int save_and_load_resets_editor_session(void)
{
    EditorState es;
    Command cmd;
    int result = 1;

    ensure_out_dir();
    remove(EDITOR_WORKFLOW_LEVEL_PATH);
    memset(&es, 0, sizeof(es));
    memset(&cmd, 0, sizeof(cmd));

    es.undo = undo_create();
    if (!es.undo) {
        fprintf(stderr, "editor_validation_test: undo_create failed\n");
        return 1;
    }

    editor_level_init_defaults(&es.level);
    strncpy(es.level.name, "Workflow Fixture", sizeof(es.level.name) - 1);
    es.level.coin_count = 1;
    es.level.coins[0].x = 64.0f;
    es.level.coins[0].y = 96.0f;
    strncpy(es.file_path, EDITOR_WORKFLOW_LEVEL_PATH,
            sizeof(es.file_path) - 1);
    es.modified = 1;
    es.selection.type = ENT_COIN;
    es.selection.index = 0;

    cmd.type = CMD_PLACE;
    cmd.entity_type = ENT_COIN;
    cmd.entity_index = 0;
    cmd.after.coin = es.level.coins[0];
    undo_push(es.undo, cmd);

    if (editor_save_current_level(&es) != 0) goto cleanup;
    if (expect_int("saved modified", es.modified, 0) != 0) goto cleanup;
    if (expect_prefix("save status", es.status_message, "Saved ") != 0)
        goto cleanup;
    if (expect_int("saved file exists",
                   editor_file_exists(EDITOR_WORKFLOW_LEVEL_PATH), 1) != 0)
        goto cleanup;

    editor_level_init_defaults(&es.level);
    es.modified = 1;
    es.selection.index = 7;
    undo_push(es.undo, cmd);
    if (expect_int("dirty undo count", es.undo->top, 2) != 0) goto cleanup;

    if (editor_load_level(&es, EDITOR_WORKFLOW_LEVEL_PATH) != 0) goto cleanup;
    if (expect_string("loaded file path", es.file_path,
                      EDITOR_WORKFLOW_LEVEL_PATH) != 0) goto cleanup;
    if (expect_string("loaded level name", es.level.name,
                      "Workflow Fixture") != 0) goto cleanup;
    if (expect_int("loaded coin count", es.level.coin_count, 1) != 0)
        goto cleanup;
    if (expect_int("loaded modified", es.modified, 0) != 0) goto cleanup;
    if (expect_int("loaded selection", es.selection.index, -1) != 0)
        goto cleanup;
    if (expect_int("loaded undo count", es.undo->top, 0) != 0)
        goto cleanup;
    if (expect_int("recent count after load", es.recent_file_count, 1) != 0)
        goto cleanup;
    if (expect_string("recent first after load", es.recent_files[0],
                      EDITOR_WORKFLOW_LEVEL_PATH) != 0) goto cleanup;

    result = 0;

cleanup:
    undo_destroy(es.undo);
    remove(EDITOR_WORKFLOW_LEVEL_PATH);
    return result;
}

static int invalid_autosave_does_not_consume_autosave_interval(void)
{
    EditorState es;

    ensure_out_dir();
    memset(&es, 0, sizeof(es));
    editor_level_init_defaults(&es.level);
    strncpy(es.autosave_path, "out/autosave/test_editor_autosave.toml",
            sizeof(es.autosave_path) - 1);
    remove(es.autosave_path);

    es.modified = 1;
    es.last_autosave_ms = SDL_GetTicks() - 30001u;
    es.level.coin_count = MAX_COINS + 1;

    editor_maybe_autosave(&es);
    if (expect_int("invalid autosave not written",
                   editor_file_exists(es.autosave_path), 0) != 0)
        return 1;

    es.level.coin_count = 0;
    editor_maybe_autosave(&es);
    if (expect_int("fixed autosave written immediately",
                   editor_file_exists(es.autosave_path), 1) != 0)
        return 1;

    remove(es.autosave_path);
    return 0;
}

static int loads_recent_files_with_trim_and_limit(void)
{
    EditorState es;
    FILE *fp;

    ensure_out_dir();
    fp = fopen(EDITOR_WORKFLOW_RECENT_PATH, "w");
    if (!fp) {
        fprintf(stderr, "editor_validation_test: cannot write recent file\n");
        return 1;
    }
    fprintf(fp, "levels/a.toml\n\nlevels/b.toml\r\nlevels/c.toml\n");
    fprintf(fp, "levels/d.toml\nlevels/e.toml\nlevels/f.toml\n");
    fclose(fp);

    memset(&es, 0, sizeof(es));
    editor_load_recent_files(&es);

    if (expect_int("recent count", es.recent_file_count, 5) != 0) return 1;
    if (expect_string("recent 0", es.recent_files[0], "levels/a.toml") != 0)
        return 1;
    if (expect_string("recent 1", es.recent_files[1], "levels/b.toml") != 0)
        return 1;
    if (expect_string("recent 4", es.recent_files[4], "levels/e.toml") != 0)
        return 1;

    return 0;
}

int main(void)
{
    if (accepts_valid_level() != 0) return 1;
    if (rejects_bad_count_and_path() != 0) return 1;
    if (rejects_missing_phase_and_layer_paths() != 0) return 1;
    if (rejects_bad_runtime_link() != 0) return 1;
    if (warns_without_blocking() != 0) return 1;
    if (rejects_unsafe_asset_paths() != 0) return 1;
    if (save_and_load_resets_editor_session() != 0) return 1;
    if (invalid_autosave_does_not_consume_autosave_interval() != 0) return 1;
    if (loads_recent_files_with_trim_and_limit() != 0) return 1;

    puts("editor_validation_test: ok");
    return 0;
}
