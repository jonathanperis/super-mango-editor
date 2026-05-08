#include <errno.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#include "editor/serializer.h"
#include "levels/level.h"

static int fail(const char *msg)
{
    fprintf(stderr, "level_serializer_test: %s\n", msg);
    return 1;
}

static int ensure_out_dir(void)
{
#ifdef _WIN32
    if (_mkdir("out") != 0 && errno != EEXIST)
        return fail("could not create out directory");
#else
    if (mkdir("out", 0755) != 0 && errno != EEXIST)
        return fail("could not create out directory");
#endif
    return 0;
}

static int write_too_many_coins_fixture(const char *path)
{
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;

    fprintf(fp, "name = \"Too Many Coins\"\n");
    fprintf(fp, "screen_count = 1\n");
    fprintf(fp, "player_start_x = 0.0\n");
    fprintf(fp, "player_start_y = 0.0\n");
    fprintf(fp, "music_path = \"\"\n");
    fprintf(fp, "music_volume = 0\n");
    fprintf(fp, "floor_tile_path = \"\"\n");
    fprintf(fp, "initial_hearts = 3\n");
    fprintf(fp, "initial_lives = 3\n");
    fprintf(fp, "score_per_life = 1000\n");
    fprintf(fp, "coin_score = 100\n\n");

    for (int i = 0; i < MAX_COINS + 1; i++) {
        fprintf(fp, "[[coins]]\n");
        fprintf(fp, "x = %d.0\n", i);
        fprintf(fp, "y = 0.0\n\n");
    }

    fclose(fp);
    return 0;
}

static int load_all_repo_levels(void)
{
    const char *levels[] = {
        "levels/00_sandbox_01.toml",
        "levels/01_lugio_01.toml",
        "levels/02_lugio_02.toml",
    };

    for (int i = 0; i < (int)(sizeof(levels) / sizeof(levels[0])); i++) {
        LevelDef def;
        if (level_load_toml(levels[i], &def) != 0) {
            fprintf(stderr, "failed to load %s\n", levels[i]);
            return 1;
        }
        if (def.name[0] == '\0') return fail("level name should not be empty");
        if (def.screen_count <= 0) return fail("screen_count should be positive");
    }

    return 0;
}

static int roundtrip_sandbox(void)
{
    const char *path = "out/test_roundtrip_level.toml";
    LevelDef before;
    LevelDef after;

    if (level_load_toml("levels/00_sandbox_01.toml", &before) != 0)
        return fail("could not load sandbox for roundtrip");

    if (level_save_toml(&before, path) != 0)
        return fail("could not save roundtrip fixture");

    if (level_load_toml(path, &after) != 0)
        return fail("could not reload roundtrip fixture");

    if (strcmp(before.name, after.name) != 0)
        return fail("roundtrip changed level name");
    if (before.coin_count != after.coin_count)
        return fail("roundtrip changed coin count");
    if (before.platform_count != after.platform_count)
        return fail("roundtrip changed platform count");
    if (before.background_layer_count != after.background_layer_count)
        return fail("roundtrip changed background layer count");

    remove(path);
    return 0;
}

static int escaped_strings_roundtrip(void)
{
    const char *path = "out/test_escaped_strings.toml";
    LevelDef before;
    LevelDef after;

    memset(&before, 0, sizeof(before));
    before.screen_count = 1;
    strncpy(before.name, "Quote \"Mango\"", sizeof(before.name) - 1);
    strncpy(before.description, "Line one\\path\nLine \"two\"\tTabbed",
            sizeof(before.description) - 1);
    strncpy(before.generated_by, "Bosser \\ QA", sizeof(before.generated_by) - 1);
    strncpy(before.music_path, "assets/sounds/screens/confirm_ui.wav",
            sizeof(before.music_path) - 1);
    strncpy(before.floor_tile_path, "assets/sprites/levels/grass_tileset.png",
            sizeof(before.floor_tile_path) - 1);

    if (level_save_toml(&before, path) != 0)
        return fail("could not save escaped string fixture");

    if (level_load_toml(path, &after) != 0)
        return fail("could not reload escaped string fixture");

    if (strcmp(before.name, after.name) != 0)
        return fail("escaped roundtrip changed name");
    if (strcmp(before.description, after.description) != 0)
        return fail("escaped roundtrip changed description");
    if (strcmp(before.generated_by, after.generated_by) != 0)
        return fail("escaped roundtrip changed generated_by");
    if (strcmp(before.music_path, after.music_path) != 0)
        return fail("escaped roundtrip changed music_path");
    if (strcmp(before.floor_tile_path, after.floor_tile_path) != 0)
        return fail("escaped roundtrip changed floor_tile_path");

    remove(path);
    return 0;
}

static int rejects_oversized_arrays(void)
{
    const char *path = "out/test_too_many_coins.toml";
    LevelDef def;

    if (write_too_many_coins_fixture(path) != 0)
        return fail("could not write oversized fixture");

    if (level_load_toml(path, &def) == 0)
        return fail("oversized coins array should fail");

    remove(path);
    return 0;
}

int main(void)
{
    if (ensure_out_dir() != 0) return 1;
    if (load_all_repo_levels() != 0) return 1;
    if (roundtrip_sandbox() != 0) return 1;
    if (escaped_strings_roundtrip() != 0) return 1;
    if (rejects_oversized_arrays() != 0) return 1;

    puts("level_serializer_test: ok");
    return 0;
}
