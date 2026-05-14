#include <math.h>
#include <stdio.h>

#include "level_loader.h"

#define MAX_LEVEL_SCREENS 99
#define MAX_INITIAL_LIVES 999
#define MAX_SCORE_PER_LIFE 999999
#define MAX_COIN_SCORE 999999

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
    CHECK_COUNT(fire_flame_count, MAX_FIRE_FLAMES);

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

static int fail_float_range(char *err, size_t err_size, const char *field,
                            float value, float lo, float hi)
{
    if (err && err_size > 0) {
        snprintf(err, err_size, "%s is %.2f (expected %.2f..%.2f)",
                 field, value, lo, hi);
    }
    return -1;
}

static int validate_finite_float(char *err, size_t err_size,
                                 const char *field, float value)
{
    if (!isfinite(value)) {
        return fail_value(err, err_size, field, "must be finite");
    }
    return 0;
}

static int validate_world_x(char *err, size_t err_size,
                            const char *field, float x, float world_w)
{
    if (validate_finite_float(err, err_size, field, x) != 0) return -1;
    if (x < 0.0f || x > world_w) {
        return fail_float_range(err, err_size, field, x, 0.0f, world_w);
    }
    return 0;
}

static int validate_world_y(char *err, size_t err_size,
                            const char *field, float y)
{
    if (validate_finite_float(err, err_size, field, y) != 0) return -1;
    if (y < 0.0f || y > (float)GAME_H) {
        return fail_float_range(err, err_size, field, y, 0.0f, (float)GAME_H);
    }
    return 0;
}

static int validate_world_rect(char *err, size_t err_size,
                               const char *field, float x, float y,
                               float w, float h, float world_w)
{
    if (validate_finite_float(err, err_size, field, x) != 0) return -1;
    if (validate_finite_float(err, err_size, field, y) != 0) return -1;
    if (validate_finite_float(err, err_size, field, w) != 0) return -1;
    if (validate_finite_float(err, err_size, field, h) != 0) return -1;
    if (x < 0.0f) {
        return fail_float_range(err, err_size, field, x, 0.0f, world_w);
    }
    if (x + w > world_w) {
        return fail_float_range(err, err_size, field, x + w, 0.0f, world_w);
    }
    if (y < 0.0f) {
        return fail_float_range(err, err_size, field, y, 0.0f, (float)GAME_H);
    }
    if (y + h > (float)GAME_H) {
        return fail_float_range(err, err_size, field, y + h, 0.0f, (float)GAME_H);
    }
    return 0;
}

static int validate_gap_x(char *err, size_t err_size,
                          const char *field, float x, float world_w)
{
    if (validate_finite_float(err, err_size, field, x) != 0) return -1;
    if (x < 0.0f || x + (float)FLOOR_GAP_W > world_w) {
        return fail_float_range(err, err_size, field, x,
                                0.0f, world_w - (float)FLOOR_GAP_W);
    }
    return 0;
}

static int validate_climbable_rect(char *err, size_t err_size,
                                   const char *field, float x, float y,
                                   int tile_count, int width,
                                   int content_h, int step, float world_w)
{
    float h;

    if (tile_count < 1) {
        return fail_range(err, err_size, field, tile_count, 1, 999);
    }

    h = (float)content_h + (float)(tile_count - 1) * (float)step;
    return validate_world_rect(err, err_size, field, x, y,
                               (float)width, h, world_w);
}

static int validate_physics_finite(const LevelDef *def,
                                   char *err, size_t err_size)
{
#define CHECK_PHYSICS_FIELD(field) \
    do { \
        if (validate_finite_float(err, err_size, "physics." #field, \
                                  def->physics.field) != 0) return -1; \
    } while (0)

    CHECK_PHYSICS_FIELD(walk_max_speed);
    CHECK_PHYSICS_FIELD(run_max_speed);
    CHECK_PHYSICS_FIELD(walk_ground_accel);
    CHECK_PHYSICS_FIELD(run_ground_accel);
    CHECK_PHYSICS_FIELD(ground_friction);
    CHECK_PHYSICS_FIELD(ground_counter_accel);
    CHECK_PHYSICS_FIELD(air_accel_walk);
    CHECK_PHYSICS_FIELD(air_accel_run);
    CHECK_PHYSICS_FIELD(air_friction);
    CHECK_PHYSICS_FIELD(cam_lookahead_vx_factor);
    CHECK_PHYSICS_FIELD(cam_lookahead_max);

    return 0;

#undef CHECK_PHYSICS_FIELD
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

static int validate_patrol(char *err, size_t err_size, const char *field,
                           float x, float patrol_x0, float patrol_x1,
                           float world_w)
{
    char child[96];

    snprintf(child, sizeof(child), "%s.x", field);
    if (validate_finite_float(err, err_size, child, x) != 0) return -1;
    if (patrol_x0 > patrol_x1) {
        snprintf(child, sizeof(child), "%s.patrol", field);
        return fail_value(err, err_size, child, "has reversed bounds");
    }
    snprintf(child, sizeof(child), "%s.patrol_x0", field);
    if (validate_world_x(err, err_size, child, patrol_x0, world_w) != 0)
        return -1;
    snprintf(child, sizeof(child), "%s.patrol_x1", field);
    if (validate_world_x(err, err_size, child, patrol_x1, world_w) != 0)
        return -1;
    if (x < patrol_x0 || x > patrol_x1) {
        snprintf(child, sizeof(child), "%s.x", field);
        return fail_float_range(err, err_size, child, x, patrol_x0, patrol_x1);
    }
    return 0;
}

static int validate_point(char *err, size_t err_size, const char *field,
                          float x, float y, float world_w)
{
    char child[96];

    snprintf(child, sizeof(child), "%s.x", field);
    if (validate_world_x(err, err_size, child, x, world_w) != 0) return -1;
    snprintf(child, sizeof(child), "%s.y", field);
    if (validate_world_y(err, err_size, child, y) != 0) return -1;
    return 0;
}

int level_validate_runtime(const LevelDef *def, char *err, size_t err_size)
{
    char field[64];
    int screens;
    float world_w;

    if (level_validate_counts(def, err, err_size) != 0) return -1;

    if (def->screen_count < 0 || def->screen_count > MAX_LEVEL_SCREENS) {
        return fail_range(err, err_size, "screen_count", def->screen_count,
                          0, MAX_LEVEL_SCREENS);
    }

    screens = (def->screen_count > 0) ? def->screen_count : 4;
    world_w = (float)screens * (float)GAME_W;

    if (def->music_volume < 0 || def->music_volume > 128) {
        return fail_range(err, err_size, "music_volume", def->music_volume, 0, 128);
    }
    if (def->initial_hearts < 0 || def->initial_hearts > MAX_HEARTS) {
        return fail_range(err, err_size, "initial_hearts", def->initial_hearts, 0, MAX_HEARTS);
    }
    if (def->initial_lives < 0 || def->initial_lives > MAX_INITIAL_LIVES) {
        return fail_range(err, err_size, "initial_lives", def->initial_lives,
                          0, MAX_INITIAL_LIVES);
    }
    if (def->score_per_life < 0 || def->score_per_life > MAX_SCORE_PER_LIFE) {
        return fail_range(err, err_size, "score_per_life", def->score_per_life,
                          0, MAX_SCORE_PER_LIFE);
    }
    if (def->coin_score < 0 || def->coin_score > MAX_COIN_SCORE) {
        return fail_range(err, err_size, "coin_score", def->coin_score,
                          0, MAX_COIN_SCORE);
    }
    if (validate_physics_finite(def, err, err_size) != 0) return -1;

    if (def->player_start_x != 0.0f || def->player_start_y != 0.0f) {
        if (validate_point(err, err_size, "player_start",
                           def->player_start_x, def->player_start_y, world_w) != 0)
            return -1;
    }

    for (int i = 0; i < def->floor_gap_count; i++) {
        if (def->floor_gaps[i] < 0 ||
            def->floor_gaps[i] + FLOOR_GAP_W > (int)world_w) {
            snprintf(field, sizeof(field), "floor_gaps[%d]", i);
            return fail_range(err, err_size, field, def->floor_gaps[i],
                              0, (int)world_w - FLOOR_GAP_W);
        }
    }

    for (int i = 0; i < def->rail_count; i++) {
        if (validate_rail(&def->rails[i], i, err, err_size) != 0) return -1;
        if (def->rails[i].x < 0 || def->rails[i].y < 0) {
            snprintf(field, sizeof(field), "rails[%d]", i);
            return fail_value(err, err_size, field, "has negative origin");
        }
        if (def->rails[i].layout == RAIL_LAYOUT_RECT) {
            snprintf(field, sizeof(field), "rails[%d]", i);
            if (validate_world_rect(err, err_size, field,
                                    (float)def->rails[i].x,
                                    (float)def->rails[i].y,
                                    (float)(def->rails[i].w * RAIL_TILE_W),
                                    (float)(def->rails[i].h * RAIL_TILE_H),
                                    world_w) != 0) return -1;
        } else if (def->rails[i].layout == RAIL_LAYOUT_HORIZ) {
            snprintf(field, sizeof(field), "rails[%d]", i);
            if (validate_world_rect(err, err_size, field,
                                    (float)def->rails[i].x,
                                    (float)def->rails[i].y,
                                    (float)(def->rails[i].w * RAIL_TILE_W),
                                    (float)RAIL_TILE_H,
                                    world_w) != 0) return -1;
        }
    }

    for (int i = 0; i < def->platform_count; i++) {
        const PlatformPlacement *p = &def->platforms[i];
        int tw = p->tile_width > 0 ? p->tile_width : 1;
        if (p->tile_height < 1) {
            snprintf(field, sizeof(field), "platforms[%d].tile_height", i);
            return fail_range(err, err_size, field, p->tile_height, 1, 999);
        }
        if (p->tile_width < 0) {
            snprintf(field, sizeof(field), "platforms[%d].tile_width", i);
            return fail_range(err, err_size, field, p->tile_width, 0, 999);
        }
        snprintf(field, sizeof(field), "platforms[%d]", i);
        if (validate_world_rect(err, err_size, field, p->x, 0.0f,
                                (float)(tw * TILE_SIZE), 1.0f, world_w) != 0)
            return -1;
        if (FLOOR_Y - p->tile_height * TILE_SIZE + 16 < 0) {
            snprintf(field, sizeof(field), "platforms[%d].tile_height", i);
            return fail_range(err, err_size, field, p->tile_height, 1,
                              (FLOOR_Y + 16) / TILE_SIZE);
        }
    }

    for (int i = 0; i < def->coin_count; i++) {
        snprintf(field, sizeof(field), "coins[%d]", i);
        if (validate_point(err, err_size, field, def->coins[i].x,
                           def->coins[i].y, world_w) != 0) return -1;
    }
    for (int i = 0; i < def->star_yellow_count; i++) {
        snprintf(field, sizeof(field), "star_yellows[%d]", i);
        if (validate_point(err, err_size, field, def->star_yellows[i].x,
                           def->star_yellows[i].y, world_w) != 0) return -1;
    }
    for (int i = 0; i < def->star_green_count; i++) {
        snprintf(field, sizeof(field), "star_greens[%d]", i);
        if (validate_point(err, err_size, field, def->star_greens[i].x,
                           def->star_greens[i].y, world_w) != 0) return -1;
    }
    for (int i = 0; i < def->star_red_count; i++) {
        snprintf(field, sizeof(field), "star_reds[%d]", i);
        if (validate_point(err, err_size, field, def->star_reds[i].x,
                           def->star_reds[i].y, world_w) != 0) return -1;
    }
    if (def->last_star.x != 0.0f || def->last_star.y != 0.0f) {
        if (validate_point(err, err_size, "last_star", def->last_star.x,
                           def->last_star.y, world_w) != 0) return -1;
    }

    for (int i = 0; i < def->spider_count; i++) {
        snprintf(field, sizeof(field), "spiders[%d]", i);
        if (validate_patrol(err, err_size, field, def->spiders[i].x,
                            def->spiders[i].patrol_x0,
                            def->spiders[i].patrol_x1, world_w) != 0) return -1;
    }
    for (int i = 0; i < def->jumping_spider_count; i++) {
        snprintf(field, sizeof(field), "jumping_spiders[%d]", i);
        if (validate_patrol(err, err_size, field, def->jumping_spiders[i].x,
                            def->jumping_spiders[i].patrol_x0,
                            def->jumping_spiders[i].patrol_x1, world_w) != 0) return -1;
    }
    for (int i = 0; i < def->bird_count; i++) {
        snprintf(field, sizeof(field), "birds[%d]", i);
        if (validate_patrol(err, err_size, field, def->birds[i].x,
                            def->birds[i].patrol_x0,
                            def->birds[i].patrol_x1, world_w) != 0) return -1;
        if (validate_world_y(err, err_size, "birds[].base_y",
                             def->birds[i].base_y) != 0) return -1;
    }
    for (int i = 0; i < def->faster_bird_count; i++) {
        snprintf(field, sizeof(field), "faster_birds[%d]", i);
        if (validate_patrol(err, err_size, field, def->faster_birds[i].x,
                            def->faster_birds[i].patrol_x0,
                            def->faster_birds[i].patrol_x1, world_w) != 0) return -1;
        if (validate_world_y(err, err_size, "faster_birds[].base_y",
                             def->faster_birds[i].base_y) != 0) return -1;
    }
    for (int i = 0; i < def->fish_count; i++) {
        snprintf(field, sizeof(field), "fish[%d]", i);
        if (validate_patrol(err, err_size, field, def->fish[i].x,
                            def->fish[i].patrol_x0,
                            def->fish[i].patrol_x1, world_w) != 0) return -1;
    }
    for (int i = 0; i < def->faster_fish_count; i++) {
        snprintf(field, sizeof(field), "faster_fish[%d]", i);
        if (validate_patrol(err, err_size, field, def->faster_fish[i].x,
                            def->faster_fish[i].patrol_x0,
                            def->faster_fish[i].patrol_x1, world_w) != 0) return -1;
    }

    for (int i = 0; i < def->spike_row_count; i++) {
        int n = def->spike_rows[i].count;
        if (n < 1 || n > MAX_SPIKE_TILES) {
            snprintf(field, sizeof(field), "spike_rows[%d].count", i);
            return fail_range(err, err_size, field, n, 1, MAX_SPIKE_TILES);
        }
        snprintf(field, sizeof(field), "spike_rows[%d]", i);
        if (validate_world_rect(err, err_size, field, def->spike_rows[i].x,
                                (float)(FLOOR_Y - SPIKE_TILE_H),
                                (float)(n * SPIKE_TILE_W),
                                (float)SPIKE_TILE_H, world_w) != 0) return -1;
    }

    for (int i = 0; i < def->spike_platform_count; i++) {
        int n = def->spike_platforms[i].tile_count;
        if (n < 1 || n > MAX_SPIKE_TILES) {
            snprintf(field, sizeof(field), "spike_platforms[%d].tile_count", i);
            return fail_range(err, err_size, field, n, 1, MAX_SPIKE_TILES);
        }
        snprintf(field, sizeof(field), "spike_platforms[%d]", i);
        if (validate_world_rect(err, err_size, field, def->spike_platforms[i].x,
                                def->spike_platforms[i].y,
                                (float)(n * SPIKE_PLAT_PIECE_W),
                                (float)SPIKE_PLAT_SRC_H, world_w) != 0) return -1;
    }

    for (int i = 0; i < def->spike_block_count; i++) {
        snprintf(field, sizeof(field), "spike_blocks[%d].rail_index", i);
        if (validate_rail_index(err, err_size, field,
                                def->spike_blocks[i].rail_index,
                                def->rail_count) != 0) return -1;
    }

    for (int i = 0; i < def->float_platform_count; i++) {
        const FloatPlatformPlacement *fp = &def->float_platforms[i];
        if (fp->mode != FLOAT_PLATFORM_STATIC &&
            fp->mode != FLOAT_PLATFORM_CRUMBLE &&
            fp->mode != FLOAT_PLATFORM_RAIL) {
            snprintf(field, sizeof(field), "float_platforms[%d].mode", i);
            return fail_value(err, err_size, field, "is invalid");
        }
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
        } else {
            snprintf(field, sizeof(field), "float_platforms[%d]", i);
            if (validate_world_rect(err, err_size, field, fp->x, fp->y,
                                    (float)(fp->tile_count * FLOAT_PLATFORM_PIECE_W),
                                    (float)FLOAT_PLATFORM_H,
                                    world_w) != 0) return -1;
        }
    }

    for (int i = 0; i < def->bridge_count; i++) {
        int n = def->bridges[i].brick_count;
        if (n < 1 || n > MAX_BRIDGE_BRICKS) {
            snprintf(field, sizeof(field), "bridges[%d].brick_count", i);
            return fail_range(err, err_size, field, n, 1, MAX_BRIDGE_BRICKS);
        }
        snprintf(field, sizeof(field), "bridges[%d]", i);
        if (validate_world_rect(err, err_size, field, def->bridges[i].x,
                                def->bridges[i].y, (float)(n * 16), 16.0f,
                                world_w) != 0) return -1;
    }

    for (int i = 0; i < def->axe_trap_count; i++) {
        snprintf(field, sizeof(field), "axe_traps[%d].pillar_x", i);
        if (validate_world_x(err, err_size, field,
                             def->axe_traps[i].pillar_x, world_w) != 0) return -1;
        if (def->axe_traps[i].y != 0.0f &&
            validate_world_y(err, err_size, "axe_traps[].y",
                             def->axe_traps[i].y) != 0) return -1;
        if (def->axe_traps[i].mode != AXE_MODE_PENDULUM &&
            def->axe_traps[i].mode != AXE_MODE_SPIN) {
            snprintf(field, sizeof(field), "axe_traps[%d].mode", i);
            return fail_value(err, err_size, field, "is invalid");
        }
    }
    for (int i = 0; i < def->circular_saw_count; i++) {
        snprintf(field, sizeof(field), "circular_saws[%d]", i);
        if (def->circular_saws[i].y != 0.0f &&
            validate_world_y(err, err_size, "circular_saws[].y",
                             def->circular_saws[i].y) != 0) return -1;
        if (def->circular_saws[i].direction != -1 &&
            def->circular_saws[i].direction != 1) {
            snprintf(field, sizeof(field), "circular_saws[%d].direction", i);
            return fail_value(err, err_size, field, "must be -1 or 1");
        }
        if (validate_patrol(err, err_size, field, def->circular_saws[i].x,
                            def->circular_saws[i].patrol_x0,
                            def->circular_saws[i].patrol_x1,
                            world_w) != 0) return -1;
    }

    for (int i = 0; i < def->blue_flame_count; i++) {
        snprintf(field, sizeof(field), "blue_flames[%d].x", i);
        if (validate_gap_x(err, err_size, field,
                           def->blue_flames[i].x, world_w) != 0) return -1;
    }
    for (int i = 0; i < def->fire_flame_count; i++) {
        snprintf(field, sizeof(field), "fire_flames[%d].x", i);
        if (validate_gap_x(err, err_size, field,
                           def->fire_flames[i].x, world_w) != 0) return -1;
    }

    for (int i = 0; i < def->bouncepad_small_count; i++) {
        snprintf(field, sizeof(field), "bouncepads_small[%d].x", i);
        if (validate_world_x(err, err_size, field,
                             def->bouncepads_small[i].x, world_w) != 0) return -1;
        if (def->bouncepads_small[i].pad_type != BOUNCEPAD_GREEN &&
            def->bouncepads_small[i].pad_type != BOUNCEPAD_WOOD &&
            def->bouncepads_small[i].pad_type != BOUNCEPAD_RED) {
            snprintf(field, sizeof(field), "bouncepads_small[%d].pad_type", i);
            return fail_value(err, err_size, field, "is invalid");
        }
    }
    for (int i = 0; i < def->bouncepad_medium_count; i++) {
        snprintf(field, sizeof(field), "bouncepads_medium[%d].x", i);
        if (validate_world_x(err, err_size, field,
                             def->bouncepads_medium[i].x, world_w) != 0) return -1;
        if (def->bouncepads_medium[i].pad_type != BOUNCEPAD_GREEN &&
            def->bouncepads_medium[i].pad_type != BOUNCEPAD_WOOD &&
            def->bouncepads_medium[i].pad_type != BOUNCEPAD_RED) {
            snprintf(field, sizeof(field), "bouncepads_medium[%d].pad_type", i);
            return fail_value(err, err_size, field, "is invalid");
        }
    }
    for (int i = 0; i < def->bouncepad_high_count; i++) {
        snprintf(field, sizeof(field), "bouncepads_high[%d].x", i);
        if (validate_world_x(err, err_size, field,
                             def->bouncepads_high[i].x, world_w) != 0) return -1;
        if (def->bouncepads_high[i].pad_type != BOUNCEPAD_GREEN &&
            def->bouncepads_high[i].pad_type != BOUNCEPAD_WOOD &&
            def->bouncepads_high[i].pad_type != BOUNCEPAD_RED) {
            snprintf(field, sizeof(field), "bouncepads_high[%d].pad_type", i);
            return fail_value(err, err_size, field, "is invalid");
        }
    }

    for (int i = 0; i < def->vine_count; i++) {
        snprintf(field, sizeof(field), "vines[%d]", i);
        if (def->vines[i].vine_type != VINE_GREEN &&
            def->vines[i].vine_type != VINE_BROWN) {
            snprintf(field, sizeof(field), "vines[%d].vine_type", i);
            return fail_value(err, err_size, field, "is invalid");
        }
        if (validate_climbable_rect(err, err_size, field,
                                    def->vines[i].x, def->vines[i].y,
                                    def->vines[i].tile_count,
                                    VINE_W, VINE_H, VINE_STEP,
                                    world_w) != 0)
            return -1;
    }
    for (int i = 0; i < def->ladder_count; i++) {
        snprintf(field, sizeof(field), "ladders[%d]", i);
        if (validate_climbable_rect(err, err_size, field,
                                    def->ladders[i].x, def->ladders[i].y,
                                    def->ladders[i].tile_count,
                                    LADDER_W, LADDER_H, LADDER_STEP,
                                    world_w) != 0)
            return -1;
    }
    for (int i = 0; i < def->rope_count; i++) {
        snprintf(field, sizeof(field), "ropes[%d]", i);
        if (validate_climbable_rect(err, err_size, field,
                                    def->ropes[i].x, def->ropes[i].y,
                                    def->ropes[i].tile_count,
                                    ROPE_W, ROPE_H, ROPE_STEP,
                                    world_w) != 0)
            return -1;
    }

    if (err && err_size > 0) err[0] = '\0';
    return 0;
}
