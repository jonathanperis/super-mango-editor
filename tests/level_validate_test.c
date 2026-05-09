#include <stdio.h>

#include "levels/level_loader.h"

static int expect_valid_level(void)
{
    LevelDef def;
    char err[128];

    level_def_init_defaults(&def);

    def.name[0] = 'O';
    def.screen_count = 1;
    def.coin_count = MAX_COINS;
    def.platform_count = MAX_PLATFORMS;

    if (level_validate_counts(&def, err, sizeof(err)) != 0) {
        fprintf(stderr, "level_validate_test: expected valid counts, got %s\n", err);
        return 1;
    }

    return 0;
}

static int expect_rejected_level(void)
{
    LevelDef def;
    char err[128];

    level_def_init_defaults(&def);

    def.coin_count = MAX_COINS + 1;

    if (level_validate_counts(&def, err, sizeof(err)) == 0) {
        fprintf(stderr, "level_validate_test: oversized coin_count should fail\n");
        return 1;
    }

    return 0;
}

static int expect_rejected_bad_rail_index(void)
{
    LevelDef def;
    char err[128];

    level_def_init_defaults(&def);
    def.spike_block_count = 1;
    def.spike_blocks[0].rail_index = 0;

    if (level_validate_runtime(&def, err, sizeof(err)) == 0) {
        fprintf(stderr, "level_validate_test: missing rail should fail\n");
        return 1;
    }

    return 0;
}

static int expect_rejected_oversized_rail(void)
{
    LevelDef def;
    char err[128];

    level_def_init_defaults(&def);
    def.rail_count = 1;
    def.rails[0].layout = RAIL_LAYOUT_RECT;
    def.rails[0].w = MAX_RAIL_TILES;
    def.rails[0].h = MAX_RAIL_TILES;

    if (level_validate_runtime(&def, err, sizeof(err)) == 0) {
        fprintf(stderr, "level_validate_test: oversized rail should fail\n");
        return 1;
    }

    return 0;
}

static int expect_rejected_bridge_overflow(void)
{
    LevelDef def;
    char err[128];

    level_def_init_defaults(&def);
    def.bridge_count = 1;
    def.bridges[0].brick_count = MAX_BRIDGE_BRICKS + 1;

    if (level_validate_runtime(&def, err, sizeof(err)) == 0) {
        fprintf(stderr, "level_validate_test: oversized bridge should fail\n");
        return 1;
    }

    return 0;
}

static int expect_physics_defaults_are_sentinels(void)
{
    LevelDef def;

    level_def_init_defaults(&def);

    if (def.physics.walk_max_speed >= 0.0f ||
        def.physics.run_max_speed >= 0.0f ||
        def.physics.air_friction >= 0.0f ||
        def.physics.cam_lookahead_max >= 0.0f) {
        fprintf(stderr, "level_validate_test: physics defaults should use negative sentinels\n");
        return 1;
    }

    return 0;
}

static int expect_rejected_floor_gap_outside_world(void)
{
    LevelDef def;
    char err[128];

    level_def_init_defaults(&def);
    def.screen_count = 1;
    def.floor_gap_count = 1;
    def.floor_gaps[0] = GAME_W;

    if (level_validate_runtime(&def, err, sizeof(err)) == 0) {
        fprintf(stderr, "level_validate_test: out-of-world floor gap should fail\n");
        return 1;
    }

    return 0;
}

static int expect_rejected_platform_outside_world(void)
{
    LevelDef def;
    char err[128];

    level_def_init_defaults(&def);
    def.screen_count = 1;
    def.platform_count = 1;
    def.platforms[0].x = (float)(GAME_W - 16);
    def.platforms[0].tile_height = 1;
    def.platforms[0].tile_width = 1;

    if (level_validate_runtime(&def, err, sizeof(err)) == 0) {
        fprintf(stderr, "level_validate_test: out-of-world platform should fail\n");
        return 1;
    }

    return 0;
}

static int expect_rejected_reversed_patrol(void)
{
    LevelDef def;
    char err[128];

    level_def_init_defaults(&def);
    def.screen_count = 1;
    def.spider_count = 1;
    def.spiders[0].x = 120.0f;
    def.spiders[0].patrol_x0 = 160.0f;
    def.spiders[0].patrol_x1 = 80.0f;

    if (level_validate_runtime(&def, err, sizeof(err)) == 0) {
        fprintf(stderr, "level_validate_test: reversed patrol should fail\n");
        return 1;
    }

    return 0;
}

static int expect_rejected_bad_rule_values(void)
{
    LevelDef def;
    char err[128];

    level_def_init_defaults(&def);
    def.initial_hearts = MAX_HEARTS + 1;

    if (level_validate_runtime(&def, err, sizeof(err)) == 0) {
        fprintf(stderr, "level_validate_test: oversized initial_hearts should fail\n");
        return 1;
    }

    level_def_init_defaults(&def);
    def.music_volume = 129;
    if (level_validate_runtime(&def, err, sizeof(err)) == 0) {
        fprintf(stderr, "level_validate_test: oversized music_volume should fail\n");
        return 1;
    }

    return 0;
}

int main(void)
{
    if (expect_valid_level() != 0) return 1;
    if (expect_rejected_level() != 0) return 1;
    if (expect_rejected_bad_rail_index() != 0) return 1;
    if (expect_rejected_oversized_rail() != 0) return 1;
    if (expect_rejected_bridge_overflow() != 0) return 1;
    if (expect_physics_defaults_are_sentinels() != 0) return 1;
    if (expect_rejected_floor_gap_outside_world() != 0) return 1;
    if (expect_rejected_platform_outside_world() != 0) return 1;
    if (expect_rejected_reversed_patrol() != 0) return 1;
    if (expect_rejected_bad_rule_values() != 0) return 1;

    puts("level_validate_test: ok");
    return 0;
}
