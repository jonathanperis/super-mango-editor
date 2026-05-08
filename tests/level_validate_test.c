#include <stdio.h>

#include "levels/level_loader.h"

static int expect_valid_level(void)
{
    LevelDef def = {0};
    char err[128];

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
    LevelDef def = {0};
    char err[128];

    def.coin_count = MAX_COINS + 1;

    if (level_validate_counts(&def, err, sizeof(err)) == 0) {
        fprintf(stderr, "level_validate_test: oversized coin_count should fail\n");
        return 1;
    }

    return 0;
}

int main(void)
{
    if (expect_valid_level() != 0) return 1;
    if (expect_rejected_level() != 0) return 1;

    puts("level_validate_test: ok");
    return 0;
}
