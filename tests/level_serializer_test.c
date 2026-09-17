#include <errno.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#include "shared/serializer.h"
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

static int expect_int_value(const char *name, int actual, int expected)
{
    if (actual != expected) {
        fprintf(stderr, "level_serializer_test: %s got %d expected %d\n",
                name, actual, expected);
        return 1;
    }
    return 0;
}

static int expect_float_value(const char *name, float actual, float expected)
{
    float diff = actual - expected;
    if (diff < 0.0f) diff = -diff;
    if (diff > 0.001f) {
        fprintf(stderr, "level_serializer_test: %s got %.3f expected %.3f\n",
                name, actual, expected);
        return 1;
    }
    return 0;
}

static int expect_str_value(const char *name, const char *actual,
                            const char *expected)
{
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "level_serializer_test: %s got '%s' expected '%s'\n",
                name, actual, expected);
        return 1;
    }
    return 0;
}

static void fill_rich_roundtrip_fixture(LevelDef *def)
{
    level_def_init_defaults(def);

    strncpy(def->name, "Rich Serializer Fixture", sizeof(def->name) - 1);
    strncpy(def->description, "Roundtrip every major LevelDef field.",
            sizeof(def->description) - 1);
    strncpy(def->generated_by, "serializer-test", sizeof(def->generated_by) - 1);
    def->screen_count = 2;

    def->checkpoint_count = 2;
    def->checkpoints[0].x = 240.0f;
    def->checkpoints[0].y = 104.0f;
    def->checkpoints[1].x = 560.0f;
    def->checkpoints[1].y = 196.0f;

    def->floor_gaps[0] = 320;
    def->floor_gaps[1] = 704;
    def->floor_gap_count = 2;

    def->rail_count = 2;
    def->rails[0].layout = RAIL_LAYOUT_RECT;
    def->rails[0].x = 240;
    def->rails[0].y = 96;
    def->rails[0].w = 4;
    def->rails[0].h = 3;
    def->rails[0].end_cap = 0;
    def->rails[1].layout = RAIL_LAYOUT_HORIZ;
    def->rails[1].x = 560;
    def->rails[1].y = 144;
    def->rails[1].w = 5;
    def->rails[1].h = 0;
    def->rails[1].end_cap = 1;

    def->platform_count = 1;
    def->platforms[0].x = 180.25f;
    def->platforms[0].tile_height = 3;
    def->platforms[0].tile_width = 2;
    strncpy(def->platforms[0].tile_path,
            "assets/sprites/levels/grass_tileset.png",
            sizeof(def->platforms[0].tile_path) - 1);

    def->coin_count = 1;
    def->coins[0].x = 64.5f;
    def->coins[0].y = 128.25f;
    def->star_yellow_count = 1;
    def->star_yellows[0].x = 90.0f;
    def->star_yellows[0].y = 120.0f;
    def->star_green_count = 1;
    def->star_greens[0].x = 120.0f;
    def->star_greens[0].y = 130.0f;
    def->star_red_count = 1;
    def->star_reds[0].x = 150.0f;
    def->star_reds[0].y = 140.0f;
    def->last_star.x = 760.0f;
    def->last_star.y = 160.0f;
    strncpy(def->next_phase, "levels/02_lugio_02.toml",
            sizeof(def->next_phase) - 1);

    def->spider_count = 1;
    def->spiders[0].x = 210.0f;
    def->spiders[0].vx = -24.5f;
    def->spiders[0].patrol_x0 = 190.0f;
    def->spiders[0].patrol_x1 = 260.0f;
    def->spiders[0].frame_index = 2;
    def->jumping_spider_count = 1;
    def->jumping_spiders[0].x = 300.0f;
    def->jumping_spiders[0].vx = 32.0f;
    def->jumping_spiders[0].patrol_x0 = 280.0f;
    def->jumping_spiders[0].patrol_x1 = 360.0f;
    def->bird_count = 1;
    def->birds[0].x = 420.0f;
    def->birds[0].base_y = 72.0f;
    def->birds[0].vx = 38.0f;
    def->birds[0].patrol_x0 = 390.0f;
    def->birds[0].patrol_x1 = 510.0f;
    def->birds[0].frame_index = 1;
    def->faster_bird_count = 1;
    def->faster_birds[0].x = 520.0f;
    def->faster_birds[0].base_y = 82.0f;
    def->faster_birds[0].vx = -62.0f;
    def->faster_birds[0].patrol_x0 = 480.0f;
    def->faster_birds[0].patrol_x1 = 620.0f;
    def->faster_birds[0].frame_index = FBIRD_FRAMES - 1;
    def->fish_count = 1;
    def->fish[0].x = 340.0f;
    def->fish[0].vx = 18.0f;
    def->fish[0].patrol_x0 = 320.0f;
    def->fish[0].patrol_x1 = 380.0f;
    def->faster_fish_count = 1;
    def->faster_fish[0].x = 440.0f;
    def->faster_fish[0].vx = -28.0f;
    def->faster_fish[0].patrol_x0 = 400.0f;
    def->faster_fish[0].patrol_x1 = 500.0f;

    def->axe_trap_count = 1;
    def->axe_traps[0].pillar_x = 288.0f;
    def->axe_traps[0].y = 112.0f;
    def->axe_traps[0].mode = AXE_MODE_SPIN;
    def->circular_saw_count = 1;
    def->circular_saws[0].x = 500.0f;
    def->circular_saws[0].y = 184.0f;
    def->circular_saws[0].patrol_x0 = 460.0f;
    def->circular_saws[0].patrol_x1 = 560.0f;
    def->circular_saws[0].direction = -1;
    def->spike_row_count = 1;
    def->spike_rows[0].x = 600.0f;
    def->spike_rows[0].count = 4;
    def->spike_platform_count = 1;
    def->spike_platforms[0].x = 660.0f;
    def->spike_platforms[0].y = 190.0f;
    def->spike_platforms[0].tile_count = 3;
    def->spike_block_count = 1;
    def->spike_blocks[0].rail_index = 0;
    def->spike_blocks[0].t_offset = 1.5f;
    def->spike_blocks[0].speed = 2.25f;
    def->blue_flame_count = 1;
    def->blue_flames[0].x = 320.0f;
    def->fire_flame_count = 1;
    def->fire_flames[0].x = 704.0f;

    def->float_platform_count = 1;
    def->float_platforms[0].mode = FLOAT_PLATFORM_RAIL;
    def->float_platforms[0].x = 560.0f;
    def->float_platforms[0].y = 144.0f;
    def->float_platforms[0].tile_count = 4;
    def->float_platforms[0].rail_index = 1;
    def->float_platforms[0].t_offset = 0.75f;
    def->float_platforms[0].speed = 1.5f;
    def->bridge_count = 1;
    def->bridges[0].x = 220.0f;
    def->bridges[0].y = 192.0f;
    def->bridges[0].brick_count = 5;
    def->bouncepad_small_count = 1;
    def->bouncepads_small[0].x = 260.0f;
    def->bouncepads_small[0].launch_vy = -320.0f;
    def->bouncepads_small[0].pad_type = BOUNCEPAD_GREEN;
    def->bouncepad_medium_count = 1;
    def->bouncepads_medium[0].x = 300.0f;
    def->bouncepads_medium[0].launch_vy = -420.0f;
    def->bouncepads_medium[0].pad_type = BOUNCEPAD_WOOD;
    def->bouncepad_high_count = 1;
    def->bouncepads_high[0].x = 340.0f;
    def->bouncepads_high[0].launch_vy = -520.0f;
    def->bouncepads_high[0].pad_type = BOUNCEPAD_RED;
    def->vine_count = 1;
    def->vines[0].x = 180.0f;
    def->vines[0].y = 96.0f;
    def->vines[0].tile_count = 3;
    def->vines[0].vine_type = 1;
    def->ladder_count = 1;
    def->ladders[0].x = 400.0f;
    def->ladders[0].y = 120.0f;
    def->ladders[0].tile_count = 4;
    def->rope_count = 1;
    def->ropes[0].x = 480.0f;
    def->ropes[0].y = 80.0f;
    def->ropes[0].tile_count = 5;

    def->background_layer_count = 1;
    strncpy(def->background_layers[0].path,
            "assets/sprites/backgrounds/sky.png",
            sizeof(def->background_layers[0].path) - 1);
    def->background_layers[0].speed = 0.25f;
    def->foreground_layer_count = 1;
    strncpy(def->foreground_layers[0].path,
            "assets/sprites/foregrounds/water.png",
            sizeof(def->foreground_layers[0].path) - 1);
    def->foreground_layers[0].speed = 1.0f;
    def->fog_layer_count = 1;
    strncpy(def->fog_layers[0].path,
            "assets/sprites/foregrounds/fog.png",
            sizeof(def->fog_layers[0].path) - 1);
    def->fog_layers[0].speed = 0.1f;

    def->player_start_x = 24.0f;
    def->player_start_y = 176.0f;
    strncpy(def->music_path, "assets/sounds/levels/water.wav",
            sizeof(def->music_path) - 1);
    def->music_volume = 42;
    strncpy(def->floor_tile_path, "assets/sprites/levels/grass_tileset.png",
            sizeof(def->floor_tile_path) - 1);
    def->initial_hearts = 3;
    def->initial_lives = 5;
    def->score_per_life = 1500;
    def->coin_score = 125;
    def->physics.walk_max_speed = 82.0f;
    def->physics.run_max_speed = 136.0f;
    def->physics.walk_ground_accel = 720.0f;
    def->physics.run_ground_accel = 980.0f;
    def->physics.ground_friction = 840.0f;
    def->physics.ground_counter_accel = 1100.0f;
    def->physics.air_accel_walk = 390.0f;
    def->physics.air_accel_run = 300.0f;
    def->physics.air_friction = 42.0f;
    def->physics.cam_lookahead_vx_factor = 0.08f;
    def->physics.cam_lookahead_max = 18.0f;
}

static int compare_rich_roundtrip(const LevelDef *before, const LevelDef *after)
{
    if (expect_str_value("rich name", after->name, before->name) != 0) return 1;
    if (expect_str_value("rich description", after->description,
                         before->description) != 0) return 1;
    if (expect_str_value("rich generated_by", after->generated_by,
                         before->generated_by) != 0) return 1;
    if (expect_int_value("rich screen_count", after->screen_count,
                         before->screen_count) != 0) return 1;
    if (expect_int_value("rich floor_gap_count", after->floor_gap_count,
                         before->floor_gap_count) != 0) return 1;
    if (expect_int_value("rich checkpoint_count", after->checkpoint_count,
                         before->checkpoint_count) != 0) return 1;
    if (expect_float_value("rich checkpoint[0] x", after->checkpoints[0].x,
                           before->checkpoints[0].x) != 0) return 1;
    if (expect_float_value("rich checkpoint[1] y", after->checkpoints[1].y,
                           before->checkpoints[1].y) != 0) return 1;
    if (expect_int_value("rich floor_gaps[0]", after->floor_gaps[0],
                         before->floor_gaps[0]) != 0) return 1;
    if (expect_int_value("rich floor_gaps[1]", after->floor_gaps[1],
                         before->floor_gaps[1]) != 0) return 1;
    if (expect_int_value("rich rail_count", after->rail_count,
                         before->rail_count) != 0) return 1;
    if (expect_int_value("rich rails[1].layout", after->rails[1].layout,
                         before->rails[1].layout) != 0) return 1;
    if (expect_int_value("rich rails[1].end_cap", after->rails[1].end_cap,
                         before->rails[1].end_cap) != 0) return 1;
    if (expect_float_value("rich platform x", after->platforms[0].x,
                           before->platforms[0].x) != 0) return 1;
    if (expect_str_value("rich platform tile_path", after->platforms[0].tile_path,
                         before->platforms[0].tile_path) != 0) return 1;

    if (expect_float_value("rich coin y", after->coins[0].y,
                           before->coins[0].y) != 0) return 1;
    if (expect_int_value("rich star_green_count", after->star_green_count,
                         before->star_green_count) != 0) return 1;
    if (expect_float_value("rich star_red x", after->star_reds[0].x,
                           before->star_reds[0].x) != 0) return 1;
    if (expect_str_value("rich next_phase", after->next_phase,
                         before->next_phase) != 0) return 1;

    if (expect_int_value("rich spider frame", after->spiders[0].frame_index,
                         before->spiders[0].frame_index) != 0) return 1;
    if (expect_float_value("rich jumping spider vx", after->jumping_spiders[0].vx,
                           before->jumping_spiders[0].vx) != 0) return 1;
    if (expect_float_value("rich bird base_y", after->birds[0].base_y,
                           before->birds[0].base_y) != 0) return 1;
    if (expect_float_value("rich faster bird vx", after->faster_birds[0].vx,
                           before->faster_birds[0].vx) != 0) return 1;
    if (expect_float_value("rich fish patrol_x1", after->fish[0].patrol_x1,
                           before->fish[0].patrol_x1) != 0) return 1;
    if (expect_float_value("rich faster fish vx", after->faster_fish[0].vx,
                           before->faster_fish[0].vx) != 0) return 1;

    if (expect_int_value("rich axe mode", after->axe_traps[0].mode,
                         before->axe_traps[0].mode) != 0) return 1;
    if (expect_int_value("rich saw direction", after->circular_saws[0].direction,
                         before->circular_saws[0].direction) != 0) return 1;
    if (expect_int_value("rich spike row count", after->spike_rows[0].count,
                         before->spike_rows[0].count) != 0) return 1;
    if (expect_int_value("rich spike platform tile_count",
                         after->spike_platforms[0].tile_count,
                         before->spike_platforms[0].tile_count) != 0) return 1;
    if (expect_float_value("rich spike block speed", after->spike_blocks[0].speed,
                           before->spike_blocks[0].speed) != 0) return 1;
    if (expect_float_value("rich blue flame x", after->blue_flames[0].x,
                           before->blue_flames[0].x) != 0) return 1;
    if (expect_float_value("rich fire flame x", after->fire_flames[0].x,
                           before->fire_flames[0].x) != 0) return 1;

    if (expect_int_value("rich float platform mode", after->float_platforms[0].mode,
                         before->float_platforms[0].mode) != 0) return 1;
    if (expect_int_value("rich bridge brick_count", after->bridges[0].brick_count,
                         before->bridges[0].brick_count) != 0) return 1;
    if (expect_int_value("rich small pad type", after->bouncepads_small[0].pad_type,
                         before->bouncepads_small[0].pad_type) != 0) return 1;
    if (expect_float_value("rich medium pad launch",
                           after->bouncepads_medium[0].launch_vy,
                           before->bouncepads_medium[0].launch_vy) != 0) return 1;
    if (expect_int_value("rich high pad type", after->bouncepads_high[0].pad_type,
                         before->bouncepads_high[0].pad_type) != 0) return 1;
    if (expect_int_value("rich vine type", after->vines[0].vine_type,
                         before->vines[0].vine_type) != 0) return 1;
    if (expect_int_value("rich ladder tile_count", after->ladders[0].tile_count,
                         before->ladders[0].tile_count) != 0) return 1;
    if (expect_int_value("rich rope tile_count", after->ropes[0].tile_count,
                         before->ropes[0].tile_count) != 0) return 1;

    if (expect_str_value("rich background path", after->background_layers[0].path,
                         before->background_layers[0].path) != 0) return 1;
    if (expect_float_value("rich foreground speed", after->foreground_layers[0].speed,
                           before->foreground_layers[0].speed) != 0) return 1;
    if (expect_str_value("rich fog path", after->fog_layers[0].path,
                         before->fog_layers[0].path) != 0) return 1;

    if (expect_float_value("rich player_start_x", after->player_start_x,
                           before->player_start_x) != 0) return 1;
    if (expect_str_value("rich music_path", after->music_path,
                         before->music_path) != 0) return 1;
    if (expect_int_value("rich music_volume", after->music_volume,
                         before->music_volume) != 0) return 1;
    if (expect_int_value("rich coin_score", after->coin_score,
                         before->coin_score) != 0) return 1;
    if (expect_float_value("rich physics air_friction",
                           after->physics.air_friction,
                           before->physics.air_friction) != 0) return 1;
    if (expect_float_value("rich physics cam lookahead",
                           after->physics.cam_lookahead_vx_factor,
                           before->physics.cam_lookahead_vx_factor) != 0) return 1;

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

static int write_too_many_checkpoints_fixture(const char *path)
{
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;

    fprintf(fp, "format_version = 1\n");
    fprintf(fp, "name = \"Too Many Checkpoints\"\n");
    fprintf(fp, "screen_count = 1\n\n");
    for (int i = 0; i < MAX_CHECKPOINTS + 1; i++) {
        fprintf(fp, "[[checkpoints]]\n");
        fprintf(fp, "x = %d.0\n", 100 + i);
        fprintf(fp, "y = 100.0\n\n");
    }
    fclose(fp);
    return 0;
}

static int write_bad_rail_link_fixture(const char *path)
{
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;

    fprintf(fp, "name = \"Bad Rail Link\"\n");
    fprintf(fp, "screen_count = 1\n\n");
    fprintf(fp, "[[spike_blocks]]\n");
    fprintf(fp, "rail_index = 0\n");
    fprintf(fp, "t_offset = 0.0\n");
    fprintf(fp, "speed = 1.0\n");

    fclose(fp);
    return 0;
}

typedef struct {
    const char *level_path;
    const char *roundtrip_path;
} ShippedLevel;

enum { EXPECTED_SHIPPED_LEVEL_COUNT = 3 };

/* Keep this explicit inventory aligned with levels/campaigns/main.toml. */
static const ShippedLevel shipped_levels[] = {
    {"levels/00_sandbox_01.toml", "out/test_roundtrip_01.toml"},
    {"levels/01_lugio_01.toml", "out/test_roundtrip_02.toml"},
    {"levels/02_lugio_02.toml", "out/test_roundtrip_03.toml"},
};

_Static_assert(sizeof(shipped_levels) / sizeof(shipped_levels[0]) ==
                   EXPECTED_SHIPPED_LEVEL_COUNT,
               "shipped level inventory count changed");

static int load_all_repo_levels(void)
{
    const int level_count = (int)(sizeof(shipped_levels) /
                                  sizeof(shipped_levels[0]));

    if (expect_int_value("shipped level inventory", level_count,
                         EXPECTED_SHIPPED_LEVEL_COUNT) != 0)
        return 1;

    for (int i = 0; i < level_count; i++) {
        LevelDef def;
        if (level_load_toml(shipped_levels[i].level_path, &def) != 0) {
            fprintf(stderr, "failed to load %s\n",
                    shipped_levels[i].level_path);
            return 1;
        }
        if (def.name[0] == '\0') return fail("level name should not be empty");
        if (def.screen_count <= 0) return fail("screen_count should be positive");
    }

    return 0;
}

/*
 * compare_shipped_roundtrip — Verify shipped LevelDef data survives TOML save/load.
 *
 * Compares top-level metadata, all entity counts, representative active entity
 * fields, layer paths/speeds, game rules, spawn data, and physics overrides.
 */
static int compare_shipped_roundtrip(const char *label, const LevelDef *before,
                                     const LevelDef *after)
{
#define CHECK_INT(field) \
    do { \
        if ((after)->field != (before)->field) { \
            fprintf(stderr, "level_serializer_test: %s changed %s\n", \
                    label, #field); \
            return 1; \
        } \
    } while (0)

#define CHECK_FLOAT(field) \
    do { \
        float diff = (after)->field - (before)->field; \
        if (diff < 0.0f) diff = -diff; \
        if (diff > 0.001f) { \
            fprintf(stderr, "level_serializer_test: %s changed %s\n", \
                    label, #field); \
            return 1; \
        } \
    } while (0)

#define CHECK_STR(field) \
    do { \
        if (strcmp((after)->field, (before)->field) != 0) { \
            fprintf(stderr, "level_serializer_test: %s changed %s\n", \
                    label, #field); \
            return 1; \
        } \
    } while (0)

    CHECK_INT(format_version);
    CHECK_STR(name);
    CHECK_STR(description);
    CHECK_STR(generated_by);
    CHECK_INT(screen_count);
    CHECK_STR(next_phase);
    CHECK_STR(music_path);
    CHECK_INT(music_volume);
    CHECK_STR(floor_tile_path);
    CHECK_INT(initial_hearts);
    CHECK_INT(initial_lives);
    CHECK_INT(score_per_life);
    CHECK_INT(coin_score);
    CHECK_FLOAT(player_start_x);
    CHECK_FLOAT(player_start_y);

    CHECK_INT(floor_gap_count);
    CHECK_INT(checkpoint_count);
    CHECK_INT(rail_count);
    CHECK_INT(platform_count);
    CHECK_INT(coin_count);
    CHECK_INT(star_yellow_count);
    CHECK_INT(star_green_count);
    CHECK_INT(star_red_count);
    CHECK_INT(spider_count);
    CHECK_INT(jumping_spider_count);
    CHECK_INT(bird_count);
    CHECK_INT(faster_bird_count);
    CHECK_INT(fish_count);
    CHECK_INT(faster_fish_count);
    CHECK_INT(axe_trap_count);
    CHECK_INT(circular_saw_count);
    CHECK_INT(spike_row_count);
    CHECK_INT(spike_platform_count);
    CHECK_INT(spike_block_count);
    CHECK_INT(blue_flame_count);
    CHECK_INT(fire_flame_count);
    CHECK_INT(float_platform_count);
    CHECK_INT(bridge_count);
    CHECK_INT(bouncepad_small_count);
    CHECK_INT(bouncepad_medium_count);
    CHECK_INT(bouncepad_high_count);
    CHECK_INT(vine_count);
    CHECK_INT(ladder_count);
    CHECK_INT(rope_count);
    CHECK_INT(background_layer_count);
    CHECK_INT(foreground_layer_count);
    CHECK_INT(fog_layer_count);

    if (before->floor_gap_count > 0) {
        int last = before->floor_gap_count - 1;
        CHECK_INT(floor_gaps[0]);
        CHECK_INT(floor_gaps[last]);
    }
    if (before->rail_count > 0) {
        int last = before->rail_count - 1;
        CHECK_INT(rails[0].layout);
        CHECK_INT(rails[last].end_cap);
        CHECK_INT(rails[last].w);
    }
    if (before->platform_count > 0) {
        int last = before->platform_count - 1;
        CHECK_FLOAT(platforms[0].x);
        CHECK_INT(platforms[last].tile_height);
        CHECK_STR(platforms[last].tile_path);
    }
    if (before->coin_count > 0) {
        int last = before->coin_count - 1;
        CHECK_FLOAT(coins[0].x);
        CHECK_FLOAT(coins[last].y);
    }
    if (before->star_yellow_count > 0) CHECK_FLOAT(star_yellows[0].x);
    if (before->star_green_count > 0) CHECK_FLOAT(star_greens[0].y);
    if (before->star_red_count > 0) CHECK_FLOAT(star_reds[0].x);
    CHECK_FLOAT(last_star.x);
    CHECK_FLOAT(last_star.y);

    if (before->spider_count > 0) CHECK_FLOAT(spiders[0].patrol_x1);
    if (before->jumping_spider_count > 0) CHECK_FLOAT(jumping_spiders[0].vx);
    if (before->bird_count > 0) CHECK_FLOAT(birds[0].base_y);
    if (before->faster_bird_count > 0) CHECK_FLOAT(faster_birds[0].vx);
    if (before->fish_count > 0) CHECK_FLOAT(fish[0].patrol_x0);
    if (before->faster_fish_count > 0) CHECK_FLOAT(faster_fish[0].patrol_x1);

    if (before->axe_trap_count > 0) CHECK_INT(axe_traps[0].mode);
    if (before->circular_saw_count > 0) CHECK_INT(circular_saws[0].direction);
    if (before->spike_row_count > 0) CHECK_INT(spike_rows[0].count);
    if (before->spike_platform_count > 0) CHECK_INT(spike_platforms[0].tile_count);
    if (before->spike_block_count > 0) CHECK_FLOAT(spike_blocks[0].speed);
    if (before->blue_flame_count > 0) CHECK_FLOAT(blue_flames[0].x);
    if (before->fire_flame_count > 0) CHECK_FLOAT(fire_flames[0].x);

    if (before->float_platform_count > 0) CHECK_INT(float_platforms[0].mode);
    if (before->bridge_count > 0) CHECK_INT(bridges[0].brick_count);
    if (before->bouncepad_small_count > 0) CHECK_INT(bouncepads_small[0].pad_type);
    if (before->bouncepad_medium_count > 0) CHECK_FLOAT(bouncepads_medium[0].launch_vy);
    if (before->bouncepad_high_count > 0) CHECK_INT(bouncepads_high[0].pad_type);
    if (before->vine_count > 0) CHECK_INT(vines[0].vine_type);
    if (before->ladder_count > 0) CHECK_INT(ladders[0].tile_count);
    if (before->rope_count > 0) CHECK_INT(ropes[0].tile_count);

    if (before->background_layer_count > 0) {
        int last = before->background_layer_count - 1;
        CHECK_STR(background_layers[0].path);
        CHECK_FLOAT(background_layers[last].speed);
    }
    if (before->foreground_layer_count > 0) {
        int last = before->foreground_layer_count - 1;
        CHECK_STR(foreground_layers[0].path);
        CHECK_FLOAT(foreground_layers[last].speed);
    }
    if (before->fog_layer_count > 0) {
        int last = before->fog_layer_count - 1;
        CHECK_STR(fog_layers[0].path);
        CHECK_FLOAT(fog_layers[last].speed);
    }

    CHECK_FLOAT(physics.walk_max_speed);
    CHECK_FLOAT(physics.run_max_speed);
    CHECK_FLOAT(physics.walk_ground_accel);
    CHECK_FLOAT(physics.run_ground_accel);
    CHECK_FLOAT(physics.ground_friction);
    CHECK_FLOAT(physics.ground_counter_accel);
    CHECK_FLOAT(physics.air_accel_walk);
    CHECK_FLOAT(physics.air_accel_run);
    CHECK_FLOAT(physics.air_friction);
    CHECK_FLOAT(physics.cam_lookahead_vx_factor);
    CHECK_FLOAT(physics.cam_lookahead_max);

#undef CHECK_INT
#undef CHECK_FLOAT
#undef CHECK_STR

    return 0;
}

/*
 * roundtrip_repo_levels — Save and reload every committed level file.
 *
 * This protects shipped TOML fixtures from serializer drift without requiring
 * level designers to maintain hand-written expected output copies.
 */
static int roundtrip_repo_levels(void)
{
    const int level_count = (int)(sizeof(shipped_levels) /
                                  sizeof(shipped_levels[0]));

    for (int i = 0; i < level_count; i++) {
        LevelDef before;
        LevelDef after;

        if (level_load_toml(shipped_levels[i].level_path, &before) != 0)
            return fail("could not load repo level for roundtrip");

        if (level_save_toml(&before, shipped_levels[i].roundtrip_path) != 0)
            return fail("could not save repo roundtrip fixture");

        if (level_load_toml(shipped_levels[i].roundtrip_path, &after) != 0)
            return fail("could not reload repo roundtrip fixture");

        if (compare_shipped_roundtrip(shipped_levels[i].level_path, &before,
                                      &after) != 0)
            return 1;

        remove(shipped_levels[i].roundtrip_path);
    }

    return 0;
}

static int escaped_strings_roundtrip(void)
{
    const char *path = "out/test_escaped_strings.toml";
    LevelDef before;
    LevelDef after;

    level_def_init_defaults(&before);
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

static int rich_level_roundtrip(void)
{
    const char *path = "out/test_rich_level_roundtrip.toml";
    LevelDef before;
    LevelDef after;

    fill_rich_roundtrip_fixture(&before);

    if (level_save_toml(&before, path) != 0)
        return fail("could not save rich roundtrip fixture");

    if (level_load_toml(path, &after) != 0)
        return fail("could not reload rich roundtrip fixture");

    if (compare_rich_roundtrip(&before, &after) != 0) return 1;

    remove(path);
    return 0;
}

static int independent_star_color_counts_roundtrip(void)
{
    const char *path = "out/test_star_color_counts.toml";
    LevelDef before;
    LevelDef after;

    level_def_init_defaults(&before);
    before.screen_count = 1;
    before.star_yellow_count = 1;
    before.star_yellows[0].x = 64.0f;
    before.star_yellows[0].y = 100.0f;
    before.star_green_count = 2;
    before.star_greens[0].x = 96.0f;
    before.star_greens[0].y = 110.0f;
    before.star_greens[1].x = 128.0f;
    before.star_greens[1].y = 120.0f;
    before.star_red_count = 3;
    before.star_reds[0].x = 160.0f;
    before.star_reds[0].y = 130.0f;
    before.star_reds[1].x = 192.0f;
    before.star_reds[1].y = 140.0f;
    before.star_reds[2].x = 224.0f;
    before.star_reds[2].y = 150.0f;

    if (level_save_toml(&before, path) != 0)
        return fail("could not save star color count fixture");

    if (level_load_toml(path, &after) != 0)
        return fail("could not reload star color count fixture");

    if (expect_int_value("star yellow count", after.star_yellow_count, 1) != 0)
        return 1;
    if (expect_int_value("star green count", after.star_green_count, 2) != 0)
        return 1;
    if (expect_int_value("star red count", after.star_red_count, 3) != 0)
        return 1;
    if (expect_float_value("star green last x", after.star_greens[1].x,
                           before.star_greens[1].x) != 0)
        return 1;
    if (expect_float_value("star red last y", after.star_reds[2].y,
                           before.star_reds[2].y) != 0)
        return 1;

    remove(path);
    return 0;
}

static int missing_physics_uses_engine_defaults(void)
{
    const char *path = "out/test_no_physics.toml";
    FILE *fp = fopen(path, "w");
    LevelDef def;

    if (!fp) return fail("could not write no-physics fixture");
    fprintf(fp, "name = \"No Physics\"\n");
    fprintf(fp, "screen_count = 1\n");
    fclose(fp);

    if (level_load_toml(path, &def) != 0)
        return fail("could not load no-physics fixture");

    if (def.physics.walk_max_speed != -1.0f)
        return fail("missing physics walk_max_speed should default to -1");
    if (def.physics.run_max_speed != -1.0f)
        return fail("missing physics run_max_speed should default to -1");
    if (def.physics.cam_lookahead_max != -1.0f)
        return fail("missing physics cam_lookahead_max should default to -1");

    remove(path);
    return 0;
}

static int write_format_version_fixture(const char *path, const char *version)
{
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;

    if (version) fprintf(fp, "format_version = %s\n", version);
    fprintf(fp, "name = \"Format Version Fixture\"\n");
    fprintf(fp, "screen_count = 1\n");
    if (fclose(fp) != 0) return -1;
    return 0;
}

static int legacy_version_loads_as_current(void)
{
    const char *path = "out/test_legacy_format_version.toml";
    LevelDef def;

    if (write_format_version_fixture(path, NULL) != 0)
        return fail("could not write legacy version fixture");
    if (level_load_toml(path, &def) != 0) {
        remove(path);
        return fail("legacy level without format_version should load");
    }
    remove(path);

    if (expect_int_value("legacy format_version", def.format_version,
                         LEVEL_FORMAT_VERSION) != 0)
        return 1;
    return 0;
}

static int explicit_version_saves_first_and_roundtrips(void)
{
    const char *path = "out/test_explicit_format_version.toml";
    LevelDef before;
    LevelDef after;
    FILE *fp;
    char first_line[64];

    level_def_init_defaults(&before);
    strncpy(before.name, "Explicit Format Version", sizeof(before.name) - 1);
    before.screen_count = 1;

    if (level_save_toml(&before, path) != 0)
        return fail("could not save explicit version fixture");

    fp = fopen(path, "r");
    if (!fp || !fgets(first_line, sizeof(first_line), fp)) {
        if (fp) fclose(fp);
        remove(path);
        return fail("could not read saved format version");
    }
    fclose(fp);
    if (strcmp(first_line, "format_version = 1\n") != 0) {
        remove(path);
        return fail("format_version must be first saved scalar");
    }

    if (level_load_toml(path, &after) != 0) {
        remove(path);
        return fail("could not reload explicit version fixture");
    }
    remove(path);

    if (expect_int_value("explicit format_version", after.format_version,
                         LEVEL_FORMAT_VERSION) != 0)
        return 1;
    if (expect_str_value("explicit version name", after.name, before.name) != 0)
        return 1;
    return 0;
}

static int rejects_invalid_format_versions_transactionally(void)
{
    const char *versions[] = {"0", "-1", "2", "\"1\"", "1.0", "true"};
    LevelDef expected;

    level_def_init_defaults(&expected);
    strncpy(expected.name, "Destination Sentinel", sizeof(expected.name) - 1);
    expected.screen_count = 7;

    for (size_t i = 0; i < sizeof(versions) / sizeof(versions[0]); i++) {
        char path[96];
        LevelDef actual = expected;

        snprintf(path, sizeof(path), "out/test_bad_format_version_%zu.toml", i);
        if (write_format_version_fixture(path, versions[i]) != 0)
            return fail("could not write invalid format version fixture");
        if (level_load_toml(path, &actual) == 0) {
            remove(path);
            return fail("invalid format_version should be rejected");
        }
        remove(path);
        if (memcmp(&actual, &expected, sizeof(actual)) != 0)
            return fail("invalid format_version changed destination LevelDef");
    }

    return 0;
}

static int strict_v1_fixture_suite(void)
{
    static const char *const invalid_fixtures[] = {
        "bad_version.toml",
        "bad_scalar_type.toml",
        "bad_fractional_integer.toml",
        "bad_integer_overflow.toml",
        "bad_string_type.toml",
        "bad_numeric_string.toml",
        "bad_nonfinite_number.toml",
        "bad_wrong_array_container.toml",
        "bad_non_table_element.toml",
        "bad_invalid_enum.toml",
        "bad_root_unknown.toml",
        "bad_nested_unknown.toml",
        "bad_floor_gaps.toml",
        "bad_nested_table_type.toml",
        "bad_root_key_embedded_nul.toml",
        "bad_nested_key_embedded_nul.toml",
        "bad_enum_embedded_nul.toml",
        "bad_path_embedded_nul.toml",
        "bad_string_embedded_nul.toml",
        "bad_legacy_format_version_key_embedded_nul.toml",
        "bad_checkpoint_missing_y.toml",
        "bad_checkpoint_type.toml",
        "bad_checkpoint_unknown.toml",
        "bad_checkpoint_nonfinite.toml",
        "bad_checkpoint_bounds.toml",
        "bad_checkpoint_duplicate.toml",
        "bad_checkpoint_array.toml",
        "bad_screen_count_max_plus_one.toml",
    };
    const char *const fixture_dir = "tests/fixtures/serializer_v1/";
    LevelDef expected;

    if (level_load_toml("tests/fixtures/serializer_v1/valid_legacy.toml",
                        &expected) != 0) {
        return fail("valid legacy schema fixture should load");
    }
    if (expected.format_version != LEVEL_FORMAT_VERSION) {
        return fail("legacy schema fixture should receive current version");
    }

    if (level_load_toml("tests/fixtures/serializer_v1/valid_v1.toml",
                        &expected) != 0) {
        return fail("valid v1 schema fixture should load");
    }

    if (level_load_toml("tests/fixtures/serializer_v1/valid_screen_count_max.toml",
                        &expected) != 0 ||
        expect_int_value("maximum screen count", expected.screen_count,
                         MAX_LEVEL_SCREENS) != 0 ||
        expect_float_value("maximum screen checkpoint x",
                           expected.checkpoints[0].x,
                           (float)(MAX_LEVEL_SCREENS * GAME_W - TILE_SIZE)) != 0) {
        return fail("valid maximum screen-count fixture should load");
    }

    level_def_init_defaults(&expected);
    strncpy(expected.name, "transaction sentinel", sizeof(expected.name) - 1);
    expected.screen_count = 7;
    expected.player_start_x = 33.5f;

    for (size_t i = 0; i < sizeof(invalid_fixtures) / sizeof(invalid_fixtures[0]); i++) {
        char path[160];
        LevelDef actual = expected;

        snprintf(path, sizeof(path), "%s%s", fixture_dir, invalid_fixtures[i]);
        if (level_load_toml(path, &actual) == 0) {
            fprintf(stderr, "level_serializer_test: accepted invalid fixture %s\n",
                    invalid_fixtures[i]);
            return 1;
        }
        if (memcmp(&actual, &expected, sizeof(actual)) != 0) {
            fprintf(stderr,
                    "level_serializer_test: invalid fixture changed LevelDef %s\n",
                    invalid_fixtures[i]);
            return 1;
        }
    }

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

static int rejects_oversized_checkpoints_transactionally(void)
{
    const char *path = "out/test_too_many_checkpoints.toml";
    LevelDef expected;
    LevelDef actual;

    level_def_init_defaults(&expected);
    strncpy(expected.name, "checkpoint sentinel", sizeof(expected.name) - 1);
    expected.screen_count = 7;
    actual = expected;
    if (write_too_many_checkpoints_fixture(path) != 0)
        return fail("could not write oversized checkpoint fixture");
    if (level_load_toml(path, &actual) == 0) {
        remove(path);
        return fail("oversized checkpoints array should fail");
    }
    remove(path);
    if (memcmp(&actual, &expected, sizeof(actual)) != 0)
        return fail("oversized checkpoints changed destination LevelDef");
    return 0;
}

static int rejects_bad_runtime_links(void)
{
    const char *path = "out/test_bad_rail_link.toml";
    LevelDef def;

    if (write_bad_rail_link_fixture(path) != 0)
        return fail("could not write bad rail link fixture");

    if (level_load_toml(path, &def) == 0)
        return fail("bad rail link should fail");

    remove(path);
    return 0;
}

static int write_unsafe_path_fixture(const char *path, const char *body)
{
    FILE *fp = fopen(path, "w");
    if (!fp) return fail("could not write unsafe path fixture");
    fputs("name = \"Unsafe Path\"\n", fp);
    fputs("screen_count = 1\n", fp);
    fputs(body, fp);
    fclose(fp);
    return 0;
}

static int expect_unsafe_toml_rejected(const char *path, const char *body)
{
    LevelDef def;
    if (write_unsafe_path_fixture(path, body) != 0) return 1;
    if (level_load_toml(path, &def) == 0) {
        remove(path);
        return fail("unsafe TOML path should fail");
    }
    remove(path);
    return 0;
}

static int rejects_unsafe_toml_paths(void)
{
    if (expect_unsafe_toml_rejected("out/test_unsafe_music.toml",
                                    "music_path = \"../assets/sounds/levels/water.wav\"\n") != 0)
        return 1;
    if (expect_unsafe_toml_rejected("out/test_unsafe_floor.toml",
                                    "floor_tile_path = \"/tmp/grass.png\"\n") != 0)
        return 1;
    if (expect_unsafe_toml_rejected("out/test_unsafe_layer.toml",
                                    "[[background_layers]]\npath = \"assets/../secret.png\"\nspeed = 0.25\n") != 0)
        return 1;
    if (expect_unsafe_toml_rejected("out/test_unsafe_platform.toml",
                                    "[[platforms]]\nx = 64\ntile_height = 1\ntile_width = 1\ntile_path = \"../secret.png\"\n") != 0)
        return 1;
    if (expect_unsafe_toml_rejected("out/test_unsafe_phase.toml",
                                    "[last_star]\nx = 100\ny = 100\nnext_phase = \"../levels/evil.toml\"\n") != 0)
        return 1;
    return 0;
}

int parser_boundary_test(void);

static int metadata_roundtrip_and_limits(void)
{
    const char *path="out/test_long_metadata.toml";
    LevelDef before, after;
    level_def_init_defaults(&before);
    before.screen_count=1;
    memset(before.description,'x',sizeof(before.description)-1);
    before.description[sizeof(before.description)-1]='\0';
    if (level_save_toml(&before,path) || level_load_toml(path,&after) ||
        strcmp(before.description,after.description)) { remove(path); return 1; }
    FILE *fp=fopen(path,"wb");
    if (!fp) return 1;
    fputs("description=\"",fp);
    for (size_t i=0;i<sizeof(before.description);i++) fputc('x',fp);
    fputs("\"\n",fp); fclose(fp);
    int failed=level_load_toml(path,&after)==0 || strcmp(before.description,after.description);
    remove(path);
    return failed;
}

int main(void)
{
    if (ensure_out_dir() != 0) return 1;
    if (parser_boundary_test() || metadata_roundtrip_and_limits()) return 1;
    const char *numeric_cases[] = {
        "music_volume = nan\n", "music_volume = 1e30\n",
        "music_volume = 9223372036854775807\n", "floor_gaps = [nan]\n",
        "[[spiders]]\nx = 100\npatrol_x0 = 0\npatrol_x1 = 200\nvx = inf\n",
        "description = \"\\u", "player_start_x = 1e100\n"
    };
    for (size_t i = 0; i < sizeof(numeric_cases) / sizeof(numeric_cases[0]); i++) {
        if (expect_unsafe_toml_rejected("out/test_unsafe_numeric.toml", numeric_cases[i])) return 1;
    }
    if (load_all_repo_levels() != 0) return 1;
    if (roundtrip_repo_levels() != 0) return 1;
    if (escaped_strings_roundtrip() != 0) return 1;
    if (rich_level_roundtrip() != 0) return 1;
    if (independent_star_color_counts_roundtrip() != 0) return 1;
    if (missing_physics_uses_engine_defaults() != 0) return 1;
    if (legacy_version_loads_as_current() != 0) return 1;
    if (explicit_version_saves_first_and_roundtrips() != 0) return 1;
    if (rejects_invalid_format_versions_transactionally() != 0) return 1;
    if (strict_v1_fixture_suite() != 0) return 1;
    if (rejects_oversized_arrays() != 0) return 1;
    if (rejects_oversized_checkpoints_transactionally() != 0) return 1;
    if (rejects_bad_runtime_links() != 0) return 1;
    if (rejects_unsafe_toml_paths() != 0) return 1;

    puts("level_serializer_test: ok");
    return 0;
}
