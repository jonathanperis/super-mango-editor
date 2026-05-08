#include <stdio.h>

#include "level_loader.h"

static int fail_count(char *err, size_t err_size,
                      const char *field, int count, int max_count)
{
    if (err && err_size > 0) {
        snprintf(err, err_size, "%s has %d items (max %d)",
                 field, count, max_count);
    }
    return -1;
}

#define CHECK_COUNT(field, max_count) \
    do { \
        if (def->field < 0 || def->field > (max_count)) { \
            return fail_count(err, err_size, #field, def->field, (max_count)); \
        } \
    } while (0)

int level_validate_counts(const LevelDef *def, char *err, size_t err_size)
{
    if (!def) {
        if (err && err_size > 0) snprintf(err, err_size, "LevelDef is NULL");
        return -1;
    }

    CHECK_COUNT(floor_gap_count, MAX_FLOOR_GAPS);
    CHECK_COUNT(rail_count, MAX_RAILS);
    CHECK_COUNT(platform_count, MAX_PLATFORMS);

    CHECK_COUNT(coin_count, MAX_COINS);
    CHECK_COUNT(star_yellow_count, MAX_STAR_YELLOWS);
    CHECK_COUNT(star_green_count, MAX_STAR_GREENS);
    CHECK_COUNT(star_red_count, MAX_STAR_REDS);

    CHECK_COUNT(spider_count, MAX_SPIDERS);
    CHECK_COUNT(jumping_spider_count, MAX_JUMPING_SPIDERS);
    CHECK_COUNT(bird_count, MAX_BIRDS);
    CHECK_COUNT(faster_bird_count, MAX_FASTER_BIRDS);
    CHECK_COUNT(fish_count, MAX_FISH);
    CHECK_COUNT(faster_fish_count, MAX_FASTER_FISH);

    CHECK_COUNT(axe_trap_count, MAX_AXE_TRAPS);
    CHECK_COUNT(circular_saw_count, MAX_CIRCULAR_SAWS);
    CHECK_COUNT(spike_row_count, MAX_SPIKE_ROWS);
    CHECK_COUNT(spike_platform_count, MAX_SPIKE_PLATFORMS);
    CHECK_COUNT(spike_block_count, MAX_SPIKE_BLOCKS);
    CHECK_COUNT(blue_flame_count, MAX_BLUE_FLAMES);
    CHECK_COUNT(fire_flame_count, MAX_BLUE_FLAMES);  /* shares BlueFlame limit */

    CHECK_COUNT(float_platform_count, MAX_FLOAT_PLATFORMS);
    CHECK_COUNT(bridge_count, MAX_BRIDGES);
    CHECK_COUNT(bouncepad_small_count, MAX_BOUNCEPADS_SMALL);
    CHECK_COUNT(bouncepad_medium_count, MAX_BOUNCEPADS_MEDIUM);
    CHECK_COUNT(bouncepad_high_count, MAX_BOUNCEPADS_HIGH);

    CHECK_COUNT(vine_count, MAX_VINES);
    CHECK_COUNT(ladder_count, MAX_LADDERS);
    CHECK_COUNT(rope_count, MAX_ROPES);

    CHECK_COUNT(background_layer_count, MAX_BACKGROUND_LAYERS);
    CHECK_COUNT(foreground_layer_count, MAX_BACKGROUND_LAYERS);  /* shares layer limit */
    CHECK_COUNT(fog_layer_count, MAX_FOG_TEXTURES);

    if (err && err_size > 0) err[0] = '\0';
    return 0;
}

#undef CHECK_COUNT
