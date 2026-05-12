#include <stdio.h>
#include <string.h>

#include "levels/level_loader.h"

#define TEST_PLAYER_W 48
#define TEST_PLAYER_H 48
#define TEST_FLOOR_SINK 16.0f

static int expect_int(const char *name, int actual, int expected)
{
    if (actual != expected) {
        fprintf(stderr, "runtime_load_test: %s got %d expected %d\n",
                name, actual, expected);
        return 1;
    }
    return 0;
}

static int expect_float(const char *name, float actual, float expected)
{
    float diff = actual - expected;
    if (diff < 0.0f) diff = -diff;
    if (diff > 0.001f) {
        fprintf(stderr, "runtime_load_test: %s got %.3f expected %.3f\n",
                name, actual, expected);
        return 1;
    }
    return 0;
}

static int expect_ptr(const char *name, const void *actual, const void *expected)
{
    if (actual != expected) {
        fprintf(stderr, "runtime_load_test: %s got %p expected %p\n",
                name, actual, expected);
        return 1;
    }
    return 0;
}

static void init_test_player(GameState *gs)
{
    gs->player.w = TEST_PLAYER_W;
    gs->player.h = TEST_PLAYER_H;
}

static void fill_runtime_fixture(LevelDef *def)
{
    level_def_init_defaults(def);

    strncpy(def->name, "Runtime Fixture", sizeof(def->name) - 1);
    def->screen_count = 3;

    def->floor_gap_count = 1;
    def->floor_gaps[0] = 128;

    def->rail_count = 1;
    def->rails[0].layout = RAIL_LAYOUT_HORIZ;
    def->rails[0].x = 192;
    def->rails[0].y = 96;
    def->rails[0].w = 5;
    def->rails[0].h = 0;
    def->rails[0].end_cap = 1;

    def->platform_count = 1;
    def->platforms[0].x = 96.0f;
    def->platforms[0].tile_height = 2;
    def->platforms[0].tile_width = 2;

    def->coin_count = 2;
    def->coins[0].x = 64.0f;
    def->coins[0].y = 120.0f;
    def->coins[1].x = 84.0f;
    def->coins[1].y = 130.0f;
    def->star_yellow_count = 1;
    def->star_yellows[0].x = 110.0f;
    def->star_yellows[0].y = 122.0f;
    def->star_green_count = 1;
    def->star_greens[0].x = 140.0f;
    def->star_greens[0].y = 124.0f;
    def->star_red_count = 1;
    def->star_reds[0].x = 170.0f;
    def->star_reds[0].y = 126.0f;
    def->last_star.x = 760.0f;
    def->last_star.y = 150.0f;

    def->spider_count = 1;
    def->spiders[0].x = 210.0f;
    def->spiders[0].vx = -24.0f;
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
    def->spike_blocks[0].t_offset = 1.0f;
    def->spike_blocks[0].speed = 2.25f;
    def->blue_flame_count = 1;
    def->blue_flames[0].x = 128.0f;
    def->fire_flame_count = 1;
    def->fire_flames[0].x = 224.0f;

    def->float_platform_count = 2;
    def->float_platforms[0].mode = FLOAT_PLATFORM_STATIC;
    def->float_platforms[0].x = 260.0f;
    def->float_platforms[0].y = 180.0f;
    def->float_platforms[0].tile_count = 4;
    def->float_platforms[1].mode = FLOAT_PLATFORM_RAIL;
    def->float_platforms[1].tile_count = 3;
    def->float_platforms[1].rail_index = 0;
    def->float_platforms[1].t_offset = 2.0f;
    def->float_platforms[1].speed = 1.5f;
    def->bridge_count = 1;
    def->bridges[0].x = 320.0f;
    def->bridges[0].y = 192.0f;
    def->bridges[0].brick_count = 3;
    def->bouncepad_small_count = 1;
    def->bouncepads_small[0].x = 360.0f;
    def->bouncepads_small[0].launch_vy = BOUNCEPAD_VY_SMALL;
    def->bouncepads_small[0].pad_type = BOUNCEPAD_GREEN;
    def->bouncepad_medium_count = 1;
    def->bouncepads_medium[0].x = 400.0f;
    def->bouncepads_medium[0].launch_vy = BOUNCEPAD_VY_MEDIUM;
    def->bouncepads_medium[0].pad_type = BOUNCEPAD_WOOD;
    def->bouncepad_high_count = 1;
    def->bouncepads_high[0].x = 440.0f;
    def->bouncepads_high[0].launch_vy = BOUNCEPAD_VY_HIGH;
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

    def->player_start_x = 48.0f;
    def->player_start_y = 176.0f;
    def->music_volume = 42;
    def->initial_hearts = 2;
    def->initial_lives = 4;
    def->score_per_life = 1500;
    def->coin_score = 125;
    def->physics.walk_max_speed = 82.0f;
    def->physics.air_friction = 0.0f;
}

static int load_applies_runtime_state(void)
{
    GameState gs = {0};
    LevelDef def;

    fill_runtime_fixture(&def);
    init_test_player(&gs);
    gs.score = 999;

    level_load(&gs, &def);

    if (expect_ptr("current level", gs.runtime.current_level, &def) != 0)
        return 1;
    if (expect_int("world width", gs.runtime.world_w, 3 * GAME_W) != 0)
        return 1;
    if (expect_int("fog enabled", gs.runtime.fog_enabled, 1) != 0) return 1;
    if (expect_int("water enabled", gs.runtime.water_enabled, 1) != 0)
        return 1;

    if (expect_int("floor gap count", gs.floor_gap_count, 1) != 0) return 1;
    if (expect_int("rail count", gs.rail_count, 1) != 0) return 1;
    if (expect_int("rail tile count", gs.rails[0].count, 5) != 0) return 1;
    if (expect_int("platform count", gs.platform_count, 1) != 0) return 1;
    if (expect_float("platform x", gs.platforms[0].x, 96.0f) != 0)
        return 1;
    if (expect_float("platform y", gs.platforms[0].y,
                     (float)(FLOOR_Y - 2 * TILE_SIZE + 16)) != 0)
        return 1;
    if (expect_int("platform width", gs.platforms[0].w, 2 * TILE_SIZE) != 0)
        return 1;

    if (expect_int("coin count", gs.coin_count, 2) != 0) return 1;
    if (expect_int("coin active", gs.coins[0].active, 1) != 0) return 1;
    if (expect_int("last star active", gs.last_star.active, 1) != 0) return 1;
    if (expect_int("bird count", gs.bird_count, 1) != 0) return 1;
    if (expect_float("faster bird vx", gs.faster_birds[0].vx, -62.0f) != 0)
        return 1;
    if (expect_float("fish water y", gs.fish[0].water_y, gs.fish[0].y) != 0)
        return 1;

    if (expect_int("spike block count", gs.spike_block_count, 1) != 0)
        return 1;
    if (expect_ptr("spike block rail", gs.spike_blocks[0].rail,
                   &gs.rails[0]) != 0) return 1;
    if (expect_int("blue flame state", gs.blue_flames[0].state,
                   BLUE_FLAME_WAITING) != 0) return 1;
    if (expect_float("blue flame x", gs.blue_flames[0].x,
                     128.0f + (FLOOR_GAP_W - BLUE_FLAME_DISPLAY_W) / 2.0f) != 0)
        return 1;

    if (expect_int("float platform count", gs.float_platform_count, 2) != 0)
        return 1;
    if (expect_int("rail platform mode", gs.float_platforms[1].mode,
                   FLOAT_PLATFORM_RAIL) != 0) return 1;
    if (expect_ptr("rail platform rail", gs.float_platforms[1].rail,
                   &gs.rails[0]) != 0) return 1;
    if (expect_int("bridge count", gs.bridge_count, 1) != 0) return 1;
    if (expect_int("bridge brick active", gs.bridges[0].bricks[0].active, 1) != 0)
        return 1;
    if (expect_int("small pad idle", gs.bouncepads_small[0].state,
                   BOUNCE_IDLE) != 0) return 1;
    if (expect_float("small pad y", gs.bouncepads_small[0].y,
                     (float)(FLOOR_Y - BOUNCEPAD_SRC_H)) != 0) return 1;
    if (expect_int("vine count", gs.vine_count, 1) != 0) return 1;
    if (expect_int("ladder count", gs.ladder_count, 1) != 0) return 1;
    if (expect_int("rope count", gs.rope_count, 1) != 0) return 1;

    if (expect_float("player spawn x", gs.player.x, def.player_start_x) != 0)
        return 1;
    if (expect_float("player spawn y", gs.player.y,
                     def.player_start_y - TEST_PLAYER_H + TEST_FLOOR_SINK) != 0)
        return 1;
    if (expect_int("hearts", gs.hearts, 2) != 0) return 1;
    if (expect_int("lives", gs.lives, 4) != 0) return 1;
    if (expect_int("score reset", gs.score, 0) != 0) return 1;
    if (expect_int("score per life", gs.rules.score_per_life, 1500) != 0)
        return 1;
    if (expect_int("score next", gs.score_life_next, 1500) != 0) return 1;
    if (expect_int("coin score", gs.rules.coin_score, 125) != 0) return 1;
    if (expect_float("walk max speed", gs.player.walk_max_speed, 82.0f) != 0)
        return 1;
    if (expect_float("zero air friction", gs.player.air_friction, 0.0f) != 0)
        return 1;

    return 0;
}

static int reset_restores_mutable_state_only(void)
{
    GameState gs = {0};
    LevelDef def;

    fill_runtime_fixture(&def);
    init_test_player(&gs);
    level_load(&gs, &def);

    gs.coins[0].x = -10.0f;
    gs.coins[0].active = 0;
    gs.last_star.active = 0;
    gs.last_star.collected = 1;
    gs.spiders[0].x = -20.0f;
    gs.blue_flames[0].state = BLUE_FLAME_RISING;
    gs.blue_flames[0].y = 0.0f;
    gs.float_platforms[0].falling = 1;
    gs.float_platforms[0].y = 999.0f;
    gs.bridges[0].bricks[0].active = 0;
    gs.bridges[0].bricks[0].falling = 1;
    gs.bouncepads_small[0].state = BOUNCE_ACTIVE;
    gs.bouncepads_small[0].anim_frame = 0;
    gs.player.x = 999.0f;
    gs.player.y = 999.0f;
    gs.player.vx = 12.0f;
    gs.player.on_ground = 0;
    gs.platforms[0].x = 777.0f;

    level_reset(&gs, &def);

    if (expect_float("reset coin x", gs.coins[0].x, def.coins[0].x) != 0)
        return 1;
    if (expect_int("reset coin active", gs.coins[0].active, 1) != 0) return 1;
    if (expect_int("reset last star active", gs.last_star.active, 1) != 0)
        return 1;
    if (expect_int("reset last star collected", gs.last_star.collected, 0) != 0)
        return 1;
    if (expect_float("reset spider x", gs.spiders[0].x, def.spiders[0].x) != 0)
        return 1;
    if (expect_int("reset flame state", gs.blue_flames[0].state,
                   BLUE_FLAME_WAITING) != 0) return 1;
    if (expect_float("reset flame y", gs.blue_flames[0].y,
                     (float)(FLOOR_Y + TILE_SIZE)) != 0) return 1;
    if (expect_int("reset platform falling", gs.float_platforms[0].falling, 0) != 0)
        return 1;
    if (expect_float("reset platform y", gs.float_platforms[0].y,
                     def.float_platforms[0].y) != 0) return 1;
    if (expect_int("reset brick active", gs.bridges[0].bricks[0].active, 1) != 0)
        return 1;
    if (expect_int("reset brick falling", gs.bridges[0].bricks[0].falling, 0) != 0)
        return 1;
    if (expect_float("reset brick delay", gs.bridges[0].bricks[0].fall_delay,
                     -1.0f) != 0) return 1;
    if (expect_int("reset pad state", gs.bouncepads_small[0].state,
                   BOUNCE_IDLE) != 0) return 1;
    if (expect_int("reset pad frame", gs.bouncepads_small[0].anim_frame, 2) != 0)
        return 1;
    if (expect_float("reset player x", gs.player.x, def.player_start_x) != 0)
        return 1;
    if (expect_int("reset player ground", gs.player.on_ground, 1) != 0)
        return 1;

    if (expect_float("static platform preserved", gs.platforms[0].x, 777.0f) != 0)
        return 1;

    return 0;
}

static int load_applies_defaults_for_missing_optional_config(void)
{
    GameState gs = {0};
    LevelDef def;

    level_def_init_defaults(&def);
    init_test_player(&gs);

    level_load(&gs, &def);

    if (expect_int("default world width", gs.runtime.world_w, WORLD_W) != 0)
        return 1;
    if (expect_int("default fog disabled", gs.runtime.fog_enabled, 0) != 0)
        return 1;
    if (expect_int("default water disabled", gs.runtime.water_enabled, 0) != 0)
        return 1;
    if (expect_float("default last star x", gs.last_star.x, 145.0f) != 0)
        return 1;
    if (expect_float("default last star y", gs.last_star.y, 167.0f) != 0)
        return 1;
    if (expect_int("default hearts", gs.hearts, MAX_HEARTS) != 0) return 1;
    if (expect_int("default lives", gs.lives, DEFAULT_LIVES) != 0) return 1;
    if (expect_int("default score per life", gs.rules.score_per_life,
                   SCORE_PER_LIFE) != 0) return 1;
    if (expect_int("default coin score", gs.rules.coin_score, COIN_SCORE) != 0)
        return 1;

    return 0;
}

int main(void)
{
    if (load_applies_runtime_state() != 0) return 1;
    if (reset_restores_mutable_state_only() != 0) return 1;
    if (load_applies_defaults_for_missing_optional_config() != 0) return 1;

    puts("runtime_load_test: ok");
    return 0;
}
