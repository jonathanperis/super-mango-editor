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
    def->faster_birds[0].frame_index = 3;
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
    const char *levels[] = {
        "levels/00_sandbox_01.toml",
        "levels/01_lugio_01.toml",
        "levels/02_lugio_02.toml",
    };
    const char *paths[] = {
        "out/test_roundtrip_00.toml",
        "out/test_roundtrip_01.toml",
        "out/test_roundtrip_02.toml",
    };

    for (int i = 0; i < (int)(sizeof(levels) / sizeof(levels[0])); i++) {
        LevelDef before;
        LevelDef after;

        if (level_load_toml(levels[i], &before) != 0)
            return fail("could not load repo level for roundtrip");

        if (level_save_toml(&before, paths[i]) != 0)
            return fail("could not save repo roundtrip fixture");

        if (level_load_toml(paths[i], &after) != 0)
            return fail("could not reload repo roundtrip fixture");

        if (compare_shipped_roundtrip(levels[i], &before, &after) != 0)
            return 1;

        remove(paths[i]);
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
    strncpy(before.music_path, "assets\\sounds\\screens\\confirm_ui.wav",
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

int main(void)
{
    if (ensure_out_dir() != 0) return 1;
    if (load_all_repo_levels() != 0) return 1;
    if (roundtrip_repo_levels() != 0) return 1;
    if (escaped_strings_roundtrip() != 0) return 1;
    if (rich_level_roundtrip() != 0) return 1;
    if (missing_physics_uses_engine_defaults() != 0) return 1;
    if (rejects_oversized_arrays() != 0) return 1;
    if (rejects_bad_runtime_links() != 0) return 1;

    puts("level_serializer_test: ok");
    return 0;
}
