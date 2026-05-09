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

int main(void)
{
    if (expect_valid_level() != 0) return 1;
    if (expect_rejected_level() != 0) return 1;
    if (expect_rejected_bad_rail_index() != 0) return 1;
    if (expect_rejected_oversized_rail() != 0) return 1;
    if (expect_rejected_bridge_overflow() != 0) return 1;

    puts("level_validate_test: ok");
    return 0;
}
