#include <stdio.h>
#include <string.h>

#include "editor/exporter.h"

static int expect_file_contains(const char *path, const char *needle)
{
    char buf[8192];
    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "exporter_test: could not open %s\n", path);
        return 1;
    }

    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    buf[n] = '\0';

    if (!strstr(buf, needle)) {
        fprintf(stderr, "exporter_test: %s missing '%s'\n", path, needle);
        return 1;
    }

    return 0;
}

static int exports_minimal_level_files(void)
{
    LevelDef def;
    level_def_init_defaults(&def);
    strncpy(def.name, "Exporter Fixture", sizeof(def.name) - 1);
    def.screen_count = 2;
    def.player_start_x = 12.0f;
    def.player_start_y = 34.0f;
    strncpy(def.music_path, "assets/sounds/levels/water.wav", sizeof(def.music_path) - 1);
    def.music_volume = 7;
    strncpy(def.floor_tile_path, "assets/sprites/levels/grass_tileset.png", sizeof(def.floor_tile_path) - 1);
    def.initial_hearts = 3;
    def.initial_lives = 5;
    def.score_per_life = 1000;
    def.coin_score = 100;
    def.physics.cam_lookahead_max = 50.0f;
    def.coin_count = 1;
    def.coins[0].x = 42.0f;
    def.coins[0].y = 84.0f;

    if (level_export_c(&def, "test_export_level", "out") != 0) {
        fprintf(stderr, "exporter_test: export failed\n");
        return 1;
    }

    if (expect_file_contains("out/test_export_level.h", "extern const LevelDef test_export_level_def;") != 0)
        return 1;
    if (expect_file_contains("out/test_export_level.c", "const LevelDef test_export_level_def = {") != 0)
        return 1;
    if (expect_file_contains("out/test_export_level.c", ".name = \"Exporter Fixture\",") != 0)
        return 1;
    if (expect_file_contains("out/test_export_level.c", ".screen_count = 2,") != 0)
        return 1;
    if (expect_file_contains("out/test_export_level.c", ".floor_tile_path = \"assets/sprites/levels/grass_tileset.png\",") != 0)
        return 1;
    if (expect_file_contains("out/test_export_level.c", ".coin_count = 1,") != 0)
        return 1;
    if (expect_file_contains("out/test_export_level.c", ".cam_lookahead_max = 50.00f,") != 0)
        return 1;

    remove("out/test_export_level.h");
    remove("out/test_export_level.c");
    return 0;
}

static int exports_escaped_c_strings(void)
{
    LevelDef def;
    level_def_init_defaults(&def);
    strncpy(def.name, "Quote \"Slash\\", sizeof(def.name) - 1);
    strncpy(def.description, "Line 1\nLine \"2\" \\ tail", sizeof(def.description) - 1);
    strncpy(def.generated_by, "tool \"name\" \001A \177f \200Z " "?" "?/\\\"", sizeof(def.generated_by) - 1);
    strncpy(def.next_phase, "levels/next\"phase.toml", sizeof(def.next_phase) - 1);
    strncpy(def.music_path, "assets/sounds/levels/water\"x.wav", sizeof(def.music_path) - 1);
    strncpy(def.floor_tile_path, "assets/sprites/levels/grass\\tile.png", sizeof(def.floor_tile_path) - 1);
    def.background_layer_count = 1;
    strncpy(def.background_layers[0].path, "assets/sprites/backgrounds/sky\"one.png", sizeof(def.background_layers[0].path) - 1);
    def.background_layers[0].speed = 0.25f;
    def.foreground_layer_count = 1;
    strncpy(def.foreground_layers[0].path, "assets/sprites/foregrounds/fog\\one.png", sizeof(def.foreground_layers[0].path) - 1);
    def.foreground_layers[0].speed = 0.75f;
    def.fog_layer_count = 1;
    strncpy(def.fog_layers[0].path, "assets/sprites/foregrounds/fog\nline.png", sizeof(def.fog_layers[0].path) - 1);
    def.fog_layers[0].speed = 0.50f;

    if (level_export_c(&def, "test_export_escape", "out") != 0) {
        fprintf(stderr, "exporter_test: escaped export failed\n");
        return 1;
    }

    if (expect_file_contains("out/test_export_escape.c", ".name = \"Quote \\\"Slash\\\\\",") != 0)
        return 1;
    if (expect_file_contains("out/test_export_escape.c", ".description = \"Line 1\\nLine \\\"2\\\" \\\\ tail\",") != 0)
        return 1;
    if (expect_file_contains("out/test_export_escape.c", ".generated_by = \"tool \\\"name\\\" \\001A \\177f \\200Z \\?\\?/\\\\\\\"\",") != 0)
        return 1;
    if (expect_file_contains("out/test_export_escape.c", ".next_phase = \"levels/next\\\"phase.toml\",") != 0)
        return 1;
    if (expect_file_contains("out/test_export_escape.c", "{ \"assets/sprites/backgrounds/sky\\\"one.png\", 0.25f },") != 0)
        return 1;
    if (expect_file_contains("out/test_export_escape.c", "{ \"assets/sprites/foregrounds/fog\\\\one.png\", 0.75f },") != 0)
        return 1;
    if (expect_file_contains("out/test_export_escape.c", "{ \"assets/sprites/foregrounds/fog\\nline.png\", 0.50f },") != 0)
        return 1;
    if (expect_file_contains("out/test_export_escape.c", ".music_path   = \"assets/sounds/levels/water\\\"x.wav\",") != 0)
        return 1;
    if (expect_file_contains("out/test_export_escape.c", ".floor_tile_path = \"assets/sprites/levels/grass\\\\tile.png\",") != 0)
        return 1;

    remove("out/test_export_escape.h");
    remove("out/test_export_escape.c");
    return 0;
}

static int rejects_invalid_export_identifier(void)
{
    LevelDef def;
    level_def_init_defaults(&def);

    const char *invalid_names[] = {
        "bad-name",
        "1_bad",
        "",
        "bad/name",
        "bad.name",
        "bad name",
        "bad\"name",
    };

    for (size_t i = 0; i < sizeof(invalid_names) / sizeof(invalid_names[0]); i++) {
        if (level_export_c(&def, invalid_names[i], "out") == 0) {
            fprintf(stderr, "exporter_test: accepted invalid C identifier '%s'\n", invalid_names[i]);
            return 1;
        }
    }

    if (level_export_c(&def, NULL, "out") == 0) {
        fprintf(stderr, "exporter_test: accepted null C identifier\n");
        return 1;
    }

    if (level_export_c(&def, "test_export_level", NULL) == 0) {
        fprintf(stderr, "exporter_test: accepted null export directory\n");
        remove("out/test_export_level.h");
        remove("out/test_export_level.c");
        return 1;
    }

    if (level_export_c(NULL, "test_export_level", "out") == 0) {
        fprintf(stderr, "exporter_test: accepted null level definition\n");
        remove("out/test_export_level.h");
        remove("out/test_export_level.c");
        return 1;
    }

    return 0;
}

static int rejects_export_paths_that_do_not_fit(void)
{
    LevelDef def;
    level_def_init_defaults(&def);

    char long_name[600];
    long_name[0] = 'l';
    for (size_t i = 1; i < sizeof(long_name) - 1; i++) {
        long_name[i] = 'a';
    }
    long_name[sizeof(long_name) - 1] = '\0';

    if (level_export_c(&def, long_name, "out") == 0) {
        fprintf(stderr, "exporter_test: accepted export path that exceeds buffer\n");
        return 1;
    }

    return 0;
}

int main(void)
{
    if (exports_minimal_level_files() != 0) return 1;
    if (exports_escaped_c_strings() != 0) return 1;
    if (rejects_invalid_export_identifier() != 0) return 1;
    if (rejects_export_paths_that_do_not_fit() != 0) return 1;

    puts("exporter_test: ok");
    return 0;
}
