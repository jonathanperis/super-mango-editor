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
    LevelDef def = {0};
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
    def.physics.walk_max_speed = -1.0f;
    def.physics.run_max_speed = -1.0f;
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

int main(void)
{
    if (exports_minimal_level_files() != 0) return 1;

    puts("exporter_test: ok");
    return 0;
}
