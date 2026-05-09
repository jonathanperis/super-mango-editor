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

static int fail_value(char *err, size_t err_size, const char *field,
                      const char *msg)
{
    if (err && err_size > 0) {
        snprintf(err, err_size, "%s %s", field, msg);
    }
    return -1;
}

static int fail_range(char *err, size_t err_size, const char *field,
                      int value, int lo, int hi)
{
    if (err && err_size > 0) {
        snprintf(err, err_size, "%s is %d (expected %d..%d)",
                 field, value, lo, hi);
    }
    return -1;
}

static int validate_rail(const RailPlacement *rail, int index,
                         char *err, size_t err_size)
{
    char field[64];
    int tile_count;

    if (rail->layout == RAIL_LAYOUT_RECT) {
        if (rail->w < 2 || rail->w > MAX_RAIL_TILES) {
            snprintf(field, sizeof(field), "rails[%d].w", index);
            return fail_range(err, err_size, field, rail->w, 2, MAX_RAIL_TILES);
        }
        if (rail->h < 2 || rail->h > MAX_RAIL_TILES) {
            snprintf(field, sizeof(field), "rails[%d].h", index);
            return fail_range(err, err_size, field, rail->h, 2, MAX_RAIL_TILES);
        }

        tile_count = rail->w * 2 + (rail->h - 2) * 2;
        if (tile_count > MAX_RAIL_TILES) {
            snprintf(field, sizeof(field), "rails[%d]", index);
            return fail_range(err, err_size, field, tile_count, 1, MAX_RAIL_TILES);
        }
    } else if (rail->layout == RAIL_LAYOUT_HORIZ) {
        if (rail->w < 2 || rail->w > MAX_RAIL_TILES) {
            snprintf(field, sizeof(field), "rails[%d].w", index);
            return fail_range(err, err_size, field, rail->w, 2, MAX_RAIL_TILES);
        }
        if (rail->end_cap != 0 && rail->end_cap != 1) {
            snprintf(field, sizeof(field), "rails[%d].end_cap", index);
            return fail_range(err, err_size, field, rail->end_cap, 0, 1);
        }
    } else {
        snprintf(field, sizeof(field), "rails[%d].layout", index);
        return fail_value(err, err_size, field, "is invalid");
    }

    return 0;
}

static int validate_rail_index(char *err, size_t err_size,
                               const char *field, int value, int rail_count)
{
    if (rail_count <= 0) {
        if (err && err_size > 0) {
            snprintf(err, err_size, "%s references rail %d but no rails exist",
                     field, value);
        }
        return -1;
    }
    if (value < 0 || value >= rail_count) {
        return fail_range(err, err_size, field, value, 0, rail_count - 1);
    }
    return 0;
}

int level_validate_runtime(const LevelDef *def, char *err, size_t err_size)
{
    char field[64];

    if (level_validate_counts(def, err, err_size) != 0) return -1;

    for (int i = 0; i < def->rail_count; i++) {
        if (validate_rail(&def->rails[i], i, err, err_size) != 0) return -1;
    }

    for (int i = 0; i < def->spike_row_count; i++) {
        int n = def->spike_rows[i].count;
        if (n < 1 || n > MAX_SPIKE_TILES) {
            snprintf(field, sizeof(field), "spike_rows[%d].count", i);
            return fail_range(err, err_size, field, n, 1, MAX_SPIKE_TILES);
        }
    }

    for (int i = 0; i < def->spike_platform_count; i++) {
        int n = def->spike_platforms[i].tile_count;
        if (n < 1 || n > MAX_SPIKE_TILES) {
            snprintf(field, sizeof(field), "spike_platforms[%d].tile_count", i);
            return fail_range(err, err_size, field, n, 1, MAX_SPIKE_TILES);
        }
    }

    for (int i = 0; i < def->spike_block_count; i++) {
        snprintf(field, sizeof(field), "spike_blocks[%d].rail_index", i);
        if (validate_rail_index(err, err_size, field,
                                def->spike_blocks[i].rail_index,
                                def->rail_count) != 0) return -1;
    }

    for (int i = 0; i < def->float_platform_count; i++) {
        const FloatPlatformPlacement *fp = &def->float_platforms[i];
        if (fp->tile_count < 1 || fp->tile_count > MAX_SPIKE_TILES) {
            snprintf(field, sizeof(field), "float_platforms[%d].tile_count", i);
            return fail_range(err, err_size, field, fp->tile_count,
                              1, MAX_SPIKE_TILES);
        }
        if (fp->mode == FLOAT_PLATFORM_RAIL) {
            snprintf(field, sizeof(field), "float_platforms[%d].rail_index", i);
            if (validate_rail_index(err, err_size, field,
                                    fp->rail_index, def->rail_count) != 0)
                return -1;
        }
    }

    for (int i = 0; i < def->bridge_count; i++) {
        int n = def->bridges[i].brick_count;
        if (n < 1 || n > MAX_BRIDGE_BRICKS) {
            snprintf(field, sizeof(field), "bridges[%d].brick_count", i);
            return fail_range(err, err_size, field, n, 1, MAX_BRIDGE_BRICKS);
        }
    }

    if (err && err_size > 0) err[0] = '\0';
    return 0;
}
