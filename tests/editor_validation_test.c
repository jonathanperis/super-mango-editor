#include <stdio.h>
#include <string.h>

#include "editor/editor_validation.h"

static int expect_int(const char *name, int actual, int expected)
{
    if (actual != expected) {
        fprintf(stderr, "editor_validation_test: %s got %d expected %d\n",
                name, actual, expected);
        return 1;
    }
    return 0;
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

int main(void)
{
    if (accepts_valid_level() != 0) return 1;
    if (rejects_bad_count_and_path() != 0) return 1;
    if (rejects_missing_phase_and_layer_paths() != 0) return 1;
    if (rejects_bad_runtime_link() != 0) return 1;
    if (warns_without_blocking() != 0) return 1;

    puts("editor_validation_test: ok");
    return 0;
}
