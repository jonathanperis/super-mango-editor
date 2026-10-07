#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "levels/level_loader.h"
#include "levels/level_ref.h"

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

static int expect_rejected_format_version(void)
{
    LevelDef def;
    char err[128];

    level_def_init_defaults(&def);
    def.format_version = 2;

    if (level_validate_runtime(&def, err, sizeof(err)) == 0 ||
        strstr(err, "format_version") == NULL) {
        fprintf(stderr, "level_validate_test: unsupported format version should fail clearly\n");
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

/*
 * The floor is drawn in FLOOR_PIECE_W pieces. Level 02 once shipped gaps at
 * x = 680 (8 px off the grid): the renderer kept grass over half the hole
 * while the player still fell through all of it.
 */
static int expect_floor_gaps_on_piece_grid(void)
{
    LevelDef def;
    char err[128];

    level_def_init_defaults(&def);
    def.screen_count = 2;
    def.floor_gap_count = 2;
    def.floor_gaps[0] = 0;
    def.floor_gaps[1] = 672;
    if (level_validate_runtime(&def, err, sizeof(err)) != 0) {
        fprintf(stderr, "level_validate_test: on-grid floor gaps rejected: %s\n", err);
        return 1;
    }

    def.floor_gaps[1] = 680;
    if (level_validate_runtime(&def, err, sizeof(err)) == 0 ||
        strstr(err, "floor_gaps[1]") == NULL ||
        strstr(err, "multiple of 16") == NULL) {
        fprintf(stderr, "level_validate_test: off-grid floor gap should fail clearly\n");
        return 1;
    }

    return 0;
}

static void checkpoint_fixture(LevelDef *def)
{
    level_def_init_defaults(def);
    def->screen_count = 1;
    def->checkpoint_count = 1;
    def->checkpoints[0].x = 160.0f;
    def->checkpoints[0].y = 120.0f;
}

static int expect_valid_authored_checkpoints(void)
{
    LevelDef def;
    char err[128];

    checkpoint_fixture(&def);
    def.checkpoint_count = 2;
    def.checkpoints[0].x = 320.0f;
    def.checkpoints[0].y = 240.0f;
    def.checkpoints[1].x = 160.0f;
    def.checkpoints[1].y = 0.0f;
    if (level_validate_runtime(&def, err, sizeof(err)) != 0) {
        fprintf(stderr, "level_validate_test: valid checkpoints rejected: %s\n", err);
        return 1;
    }
    return 0;
}

static int expect_rejected_authored_checkpoints(void)
{
    LevelDef def;
    char err[128];

    checkpoint_fixture(&def);
    def.checkpoints[0].x = 80.0f;
    if (level_validate_runtime(&def, err, sizeof(err)) == 0) return 1;
    checkpoint_fixture(&def);
    def.checkpoints[0].x = (float)GAME_W - TILE_SIZE + 1.0f;
    if (level_validate_runtime(&def, err, sizeof(err)) == 0) return 1;
    checkpoint_fixture(&def);
    def.checkpoints[0].y = (float)GAME_H + 1.0f;
    if (level_validate_runtime(&def, err, sizeof(err)) == 0) return 1;
    checkpoint_fixture(&def);
    def.checkpoint_count = 2;
    def.checkpoints[1] = def.checkpoints[0];
    if (level_validate_runtime(&def, err, sizeof(err)) == 0) return 1;
    checkpoint_fixture(&def);
    def.checkpoint_count = MAX_CHECKPOINTS + 1;
    if (level_validate_counts(&def, err, sizeof(err)) == 0) return 1;
    return 0;
}

/*
 * A checkpoint respawns the player in the TILE_SIZE column starting at its
 * x. Any overlap with a floor gap drops them into the water every life.
 */
static int expect_checkpoints_clear_of_floor_gaps(void)
{
    LevelDef def;
    char err[128];

    checkpoint_fixture(&def);              /* checkpoint column 160..208 */
    def.floor_gap_count = 2;
    def.floor_gaps[0] = 128;               /* ends exactly where it starts */
    def.floor_gaps[1] = 208;               /* starts exactly where it ends */
    if (level_validate_runtime(&def, err, sizeof(err)) != 0) {
        fprintf(stderr, "level_validate_test: checkpoint beside gaps rejected: %s\n", err);
        return 1;
    }

    const int overlapping[] = { 144, 160, 176, 192 };
    for (size_t i = 0; i < sizeof(overlapping) / sizeof(overlapping[0]); i++) {
        def.floor_gaps[1] = overlapping[i];
        if (level_validate_runtime(&def, err, sizeof(err)) == 0 ||
            strstr(err, "checkpoints[0].x") == NULL ||
            strstr(err, "floor_gaps[1]") == NULL) {
            fprintf(stderr, "level_validate_test: checkpoint over gap %d accepted\n",
                    overlapping[i]);
            return 1;
        }
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

static int expect_rejected_rule_upper_bounds(void)
{
    LevelDef def;
    char err[128];

    level_def_init_defaults(&def);
    def.initial_lives = 1000;
    if (level_validate_runtime(&def, err, sizeof(err)) == 0) {
        fprintf(stderr, "level_validate_test: oversized initial_lives should fail\n");
        return 1;
    }

    level_def_init_defaults(&def);
    def.score_per_life = 1000000;
    if (level_validate_runtime(&def, err, sizeof(err)) == 0) {
        fprintf(stderr, "level_validate_test: oversized score_per_life should fail\n");
        return 1;
    }

    level_def_init_defaults(&def);
    def.coin_score = 1000000;
    if (level_validate_runtime(&def, err, sizeof(err)) == 0) {
        fprintf(stderr, "level_validate_test: oversized coin_score should fail\n");
        return 1;
    }

    return 0;
}

static int expect_rejected_oversized_screen_count(void)
{
    LevelDef def;
    char err[128];

    level_def_init_defaults(&def);
    def.screen_count = 100;
    if (level_validate_runtime(&def, err, sizeof(err)) == 0) {
        fprintf(stderr, "level_validate_test: screen_count above editor max should fail\n");
        return 1;
    }

    level_def_init_defaults(&def);
    def.screen_count = INT_MAX;
    if (level_validate_runtime(&def, err, sizeof(err)) == 0) {
        fprintf(stderr, "level_validate_test: overflow-sized screen_count should fail\n");
        return 1;
    }

    return 0;
}

static int expect_rejected_nonfinite_values(void)
{
    LevelDef def;
    char err[128];

    level_def_init_defaults(&def);
    def.screen_count = 1;
    def.coin_count = 1;
    def.coins[0].x = NAN;
    def.coins[0].y = 32.0f;
    if (level_validate_runtime(&def, err, sizeof(err)) == 0) {
        fprintf(stderr, "level_validate_test: NaN coin coordinate should fail\n");
        return 1;
    }

    level_def_init_defaults(&def);
    def.screen_count = 1;
    def.platform_count = 1;
    def.platforms[0].x = INFINITY;
    def.platforms[0].tile_height = 1;
    def.platforms[0].tile_width = 1;
    if (level_validate_runtime(&def, err, sizeof(err)) == 0) {
        fprintf(stderr, "level_validate_test: infinite platform coordinate should fail\n");
        return 1;
    }

    level_def_init_defaults(&def);
    def.physics.run_max_speed = INFINITY;
    if (level_validate_runtime(&def, err, sizeof(err)) == 0) {
        fprintf(stderr, "level_validate_test: infinite physics override should fail\n");
        return 1;
    }

    return 0;
}

static int expect_rejected_bad_enum_values(void)
{
    LevelDef def;
    char err[128];

    level_def_init_defaults(&def);
    def.axe_trap_count = 1;
    def.axe_traps[0].pillar_x = 16.0f;
    def.axe_traps[0].mode = (AxeTrapMode)99;
    if (level_validate_runtime(&def, err, sizeof(err)) == 0) {
        fprintf(stderr, "level_validate_test: invalid axe mode should fail\n");
        return 1;
    }

    level_def_init_defaults(&def);
    def.screen_count = 1;
    def.circular_saw_count = 1;
    def.circular_saws[0].x = 64.0f;
    def.circular_saws[0].patrol_x0 = 32.0f;
    def.circular_saws[0].patrol_x1 = 96.0f;
    def.circular_saws[0].direction = 0;
    if (level_validate_runtime(&def, err, sizeof(err)) == 0 ||
        strstr(err, "must be -1 or 1") == NULL) {
        fprintf(stderr, "level_validate_test: invalid saw direction should fail clearly\n");
        return 1;
    }

    level_def_init_defaults(&def);
    def.float_platform_count = 1;
    def.float_platforms[0].mode = (FloatPlatformMode)99;
    def.float_platforms[0].x = 16.0f;
    def.float_platforms[0].y = 16.0f;
    def.float_platforms[0].tile_count = 1;
    if (level_validate_runtime(&def, err, sizeof(err)) == 0) {
        fprintf(stderr, "level_validate_test: invalid float-platform mode should fail\n");
        return 1;
    }

    level_def_init_defaults(&def);
    def.bouncepad_small_count = 1;
    def.bouncepads_small[0].x = 16.0f;
    def.bouncepads_small[0].launch_vy = BOUNCEPAD_VY_SMALL;
    def.bouncepads_small[0].pad_type = (BouncepadType)99;
    if (level_validate_runtime(&def, err, sizeof(err)) == 0 ||
        strstr(err, "pad_type") == NULL) {
        fprintf(stderr, "level_validate_test: invalid bouncepad type should fail\n");
        return 1;
    }

    level_def_init_defaults(&def);
    def.vine_count = 1;
    def.vines[0].x = 16.0f;
    def.vines[0].y = 16.0f;
    def.vines[0].tile_count = 1;
    def.vines[0].vine_type = 99;
    if (level_validate_runtime(&def, err, sizeof(err)) == 0) {
        fprintf(stderr, "level_validate_test: invalid vine type should fail\n");
        return 1;
    }

    return 0;
}

static int expect_rejected_flame_gap_outside_world(void)
{
    LevelDef def;
    char err[128];

    level_def_init_defaults(&def);
    def.screen_count = 1;
    def.blue_flame_count = 1;
    def.blue_flames[0].x = (float)GAME_W;
    if (level_validate_runtime(&def, err, sizeof(err)) == 0) {
        fprintf(stderr, "level_validate_test: out-of-world blue flame gap should fail\n");
        return 1;
    }

    level_def_init_defaults(&def);
    def.screen_count = 1;
    def.fire_flame_count = 1;
    def.fire_flames[0].x = (float)GAME_W;
    if (level_validate_runtime(&def, err, sizeof(err)) == 0) {
        fprintf(stderr, "level_validate_test: out-of-world fire flame gap should fail\n");
        return 1;
    }

    return 0;
}

/*
 * A pad relaunches the player every step and never lets them jump, so a
 * launch weaker than JUMP_VY (or pointing down) would trap them on it.
 */
static int expect_bouncepads_launch_upward(void)
{
    LevelDef def;
    char err[128];
    const float rejected[] = { 0.0f, 100.0f, -10.0f, JUMP_VY + 1.0f, NAN };

    level_def_init_defaults(&def);
    def.screen_count = 1;
    def.bouncepad_medium_count = 1;
    def.bouncepads_medium[0].x = 160.0f;
    def.bouncepads_medium[0].pad_type = BOUNCEPAD_WOOD;
    def.bouncepads_medium[0].launch_vy = JUMP_VY;
    if (level_validate_runtime(&def, err, sizeof(err)) != 0) {
        fprintf(stderr, "level_validate_test: jump-strength pad rejected: %s\n", err);
        return 1;
    }

    for (size_t i = 0; i < sizeof(rejected) / sizeof(rejected[0]); i++) {
        def.bouncepads_medium[0].launch_vy = rejected[i];
        if (level_validate_runtime(&def, err, sizeof(err)) == 0 ||
            strstr(err, "bouncepads_medium[0].launch_vy") == NULL) {
            fprintf(stderr, "level_validate_test: weak pad launch %.1f accepted\n",
                    rejected[i]);
            return 1;
        }
    }
    return 0;
}

/*
 * Rail riders need a positive speed (a negative one pins them to the start
 * of an open rail) no faster than MAX_RAIL_SPEED (the rider tolerance).
 */
static int expect_rail_speeds_forward_and_bounded(void)
{
    LevelDef def;
    char err[128];
    const float rejected[] = { 0.0f, -2.0f, (float)MAX_RAIL_SPEED + 0.5f, INFINITY };

    level_def_init_defaults(&def);
    def.screen_count = 1;
    def.rail_count = 1;
    def.rails[0] = (RailPlacement){RAIL_LAYOUT_HORIZ, 80, 40, 6, 0, 0};
    def.spike_block_count = 1;
    def.spike_blocks[0].rail_index = 0;
    def.spike_blocks[0].speed = (float)MAX_RAIL_SPEED;
    def.float_platform_count = 1;
    def.float_platforms[0].mode = FLOAT_PLATFORM_RAIL;
    def.float_platforms[0].rail_index = 0;
    def.float_platforms[0].tile_count = 2;
    def.float_platforms[0].speed = 3.0f;
    if (level_validate_runtime(&def, err, sizeof(err)) != 0) {
        fprintf(stderr, "level_validate_test: valid rail speeds rejected: %s\n", err);
        return 1;
    }

    for (size_t i = 0; i < sizeof(rejected) / sizeof(rejected[0]); i++) {
        def.spike_blocks[0].speed = rejected[i];
        if (level_validate_runtime(&def, err, sizeof(err)) == 0 ||
            strstr(err, "spike_blocks[0].speed") == NULL) {
            fprintf(stderr, "level_validate_test: spike block speed %.1f accepted\n",
                    rejected[i]);
            return 1;
        }
        def.spike_blocks[0].speed = 3.0f;
        def.float_platforms[0].speed = rejected[i];
        if (level_validate_runtime(&def, err, sizeof(err)) == 0 ||
            strstr(err, "float_platforms[0].speed") == NULL) {
            fprintf(stderr, "level_validate_test: rail platform speed %.1f accepted\n",
                    rejected[i]);
            return 1;
        }
        def.float_platforms[0].speed = 3.0f;
    }

    /* A parked (STATIC) platform never moves, so its speed is not a rail speed. */
    def.float_platforms[0].mode = FLOAT_PLATFORM_STATIC;
    def.float_platforms[0].x = 16.0f;
    def.float_platforms[0].y = 16.0f;
    def.float_platforms[0].speed = 0.0f;
    if (level_validate_runtime(&def, err, sizeof(err)) != 0) {
        fprintf(stderr, "level_validate_test: static platform speed 0 rejected: %s\n", err);
        return 1;
    }
    return 0;
}

/*
 * The patrol code turns an enemy when its right edge reaches patrol_x1, so a
 * range narrower than the sprite would teleport it between the ends.
 */
static int expect_patrols_fit_the_sprite(void)
{
    LevelDef def;
    char err[128];

    level_def_init_defaults(&def);
    def.screen_count = 1;
    def.spider_count = 1;
    def.spiders[0].x = 100.0f;
    def.spiders[0].patrol_x0 = 100.0f;
    def.spiders[0].patrol_x1 = 100.0f + SPIDER_FRAME_W;
    def.fish_count = 1;
    def.fish[0].x = 200.0f;
    def.fish[0].patrol_x0 = 200.0f;
    def.fish[0].patrol_x1 = 200.0f + FISH_RENDER_W;
    def.circular_saw_count = 1;
    def.circular_saws[0].x = 300.0f;
    def.circular_saws[0].patrol_x0 = 300.0f;
    def.circular_saws[0].patrol_x1 = 300.0f;   /* saws have no width rule */
    def.circular_saws[0].direction = 1;
    if (level_validate_runtime(&def, err, sizeof(err)) != 0) {
        fprintf(stderr, "level_validate_test: sprite-wide patrols rejected: %s\n", err);
        return 1;
    }

    def.spiders[0].patrol_x1 = 100.0f + SPIDER_FRAME_W - 1.0f;
    if (level_validate_runtime(&def, err, sizeof(err)) == 0 ||
        strstr(err, "spiders[0].patrol") == NULL) {
        fprintf(stderr, "level_validate_test: narrow spider patrol accepted\n");
        return 1;
    }
    def.spiders[0].patrol_x1 = 100.0f + SPIDER_FRAME_W;
    def.fish[0].patrol_x1 = 200.0f + FISH_RENDER_W / 2.0f;
    if (level_validate_runtime(&def, err, sizeof(err)) == 0 ||
        strstr(err, "fish[0].patrol") == NULL) {
        fprintf(stderr, "level_validate_test: narrow fish patrol accepted\n");
        return 1;
    }
    return 0;
}

static int expect_rejected_climbable_extents(void)
{
    LevelDef def;
    char err[128];

    level_def_init_defaults(&def);
    def.screen_count = 1;
    def.vine_count = 1;
    def.vines[0].x = 16.0f;
    def.vines[0].y = (float)(GAME_H - 1);
    def.vines[0].tile_count = 2;
    if (level_validate_runtime(&def, err, sizeof(err)) == 0) {
        fprintf(stderr, "level_validate_test: vine extending below screen should fail\n");
        return 1;
    }

    level_def_init_defaults(&def);
    def.screen_count = 1;
    def.ladder_count = 1;
    def.ladders[0].x = 16.0f;
    def.ladders[0].y = 16.0f;
    def.ladders[0].tile_count = 10000;
    if (level_validate_runtime(&def, err, sizeof(err)) == 0) {
        fprintf(stderr, "level_validate_test: oversized ladder should fail\n");
        return 1;
    }

    level_def_init_defaults(&def);
    def.screen_count = 1;
    def.rope_count = 1;
    def.ropes[0].x = 16.0f;
    def.ropes[0].y = (float)(GAME_H - 1);
    def.ropes[0].tile_count = 2;
    if (level_validate_runtime(&def, err, sizeof(err)) == 0) {
        fprintf(stderr, "level_validate_test: rope extending below screen should fail\n");
        return 1;
    }

    return 0;
}

static int reject_path_case(const char *label, void (*set_path)(LevelDef *))
{
    LevelDef def;
    char err[128];

    level_def_init_defaults(&def);
    def.screen_count = 1;
    set_path(&def);
    if (level_validate_runtime(&def, err, sizeof(err)) == 0) {
        fprintf(stderr, "level_validate_test: unsafe %s should fail\n", label);
        return 1;
    }
    return 0;
}

static void set_unsafe_music_parent(LevelDef *def)
{
    strncpy(def->music_path, "../assets/sounds/levels/water.wav",
            sizeof(def->music_path) - 1);
}

static void set_unsafe_floor_absolute(LevelDef *def)
{
    strncpy(def->floor_tile_path, "/tmp/grass_tileset.png",
            sizeof(def->floor_tile_path) - 1);
}

static void set_unsafe_platform_tile(LevelDef *def)
{
    def->platform_count = 1;
    def->platforms[0].x = 64.0f;
    def->platforms[0].tile_height = 1;
    def->platforms[0].tile_width = 1;
    strncpy(def->platforms[0].tile_path, "../secret.png",
            sizeof(def->platforms[0].tile_path) - 1);
}

static void set_wrong_music_type(LevelDef *def)
{
    strncpy(def->music_path, "assets/sprites/levels/grass_tileset.png",
            sizeof(def->music_path) - 1);
}

static void set_wrong_floor_root(LevelDef *def)
{
    strncpy(def->floor_tile_path, "assets/sprites/player/player.png",
            sizeof(def->floor_tile_path) - 1);
}

static void set_unsafe_background_absolute(LevelDef *def)
{
    def->background_layer_count = 1;
    strncpy(def->background_layers[0].path, "/etc/passwd",
            sizeof(def->background_layers[0].path) - 1);
}

static void set_unsafe_foreground_parent(LevelDef *def)
{
    def->foreground_layer_count = 1;
    strncpy(def->foreground_layers[0].path, "assets/../secret.png",
            sizeof(def->foreground_layers[0].path) - 1);
}

static void set_unsafe_fog_backslash(LevelDef *def)
{
    def->fog_layer_count = 1;
    strncpy(def->fog_layers[0].path, "assets\\..\\secret.png",
            sizeof(def->fog_layers[0].path) - 1);
}

static void set_unsafe_next_phase(LevelDef *def)
{
    strncpy(def->next_phase, "../levels/evil.toml",
            sizeof(def->next_phase) - 1);
}

static void set_unsafe_windows_drive(LevelDef *def)
{
    strncpy(def->music_path, "C:/Users/Public/evil.wav",
            sizeof(def->music_path) - 1);
}

static void set_unsafe_unc_path(LevelDef *def)
{
    strncpy(def->floor_tile_path, "//server/share/evil.png",
            sizeof(def->floor_tile_path) - 1);
}

static void set_unsafe_control_char(LevelDef *def)
{
    strncpy(def->music_path, "assets/sounds/levels/bad\nname.wav",
            sizeof(def->music_path) - 1);
}

static int expect_rejected_unsafe_paths(void)
{
    if (reject_path_case("music traversal", set_unsafe_music_parent) != 0) return 1;
    if (reject_path_case("floor absolute path", set_unsafe_floor_absolute) != 0) return 1;
    if (reject_path_case("platform tile traversal", set_unsafe_platform_tile) != 0) return 1;
    if (reject_path_case("music wrong type", set_wrong_music_type) != 0) return 1;
    if (reject_path_case("floor wrong root", set_wrong_floor_root) != 0) return 1;
    if (reject_path_case("background absolute path", set_unsafe_background_absolute) != 0) return 1;
    if (reject_path_case("foreground traversal", set_unsafe_foreground_parent) != 0) return 1;
    if (reject_path_case("fog backslash traversal", set_unsafe_fog_backslash) != 0) return 1;
    if (reject_path_case("next phase traversal", set_unsafe_next_phase) != 0) return 1;
    if (reject_path_case("Windows drive path", set_unsafe_windows_drive) != 0) return 1;
    if (reject_path_case("UNC-style path", set_unsafe_unc_path) != 0) return 1;
    if (reject_path_case("control character path", set_unsafe_control_char) != 0) return 1;
    return 0;
}

/*
 * One table drives both level_ref_valid and next_phase validation, so the
 * shared rule cannot drift between them.  tests/validate_levels_test.py keeps
 * the same cases for the Python validator.
 */
static int level_reference_rule(void)
{
    static const struct { const char *path; int valid; } cases[] = {
        { "levels/01_lugio_01.toml", 1 },
        { "levels/caf\xC3\xA9.toml", 1 },
        { "levels/console.toml", 1 },
        { "levels/com10.toml", 1 },
        { "levels/x.y.toml", 1 },
        { "levels/labs/01_collision.toml", 0 },
        { "levels/con.toml", 0 },
        { "levels/CON.toml", 0 },
        { "levels/nul.x.toml", 0 },
        { "levels/Aux .toml", 0 },
        { "levels/prn.toml", 0 },
        { "levels/com1.toml", 0 },
        { "levels/LPT9.toml", 0 },
        { "levels/com0.toml", 0 },
        { "levels/com\xC2\xB9.toml", 0 },
        { "levels/lpt\xC2\xB3.toml", 0 },
        { "levels/a:b.toml", 0 },
        { "levels/a?.toml", 0 },
        { "levels/a\x7F.toml", 0 },
        { "levels/a\\b.toml", 0 },
        { "levels/.toml", 0 },
        { "levels/..toml", 0 },
        { "levels/.hidden.toml", 0 },
        { "levels/a.txt", 0 },
        { "assets/a.toml", 0 },
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        LevelDef def;
        char err[160];
        int ref_ok = level_ref_valid(cases[i].path, strlen(cases[i].path));

        level_def_init_defaults(&def);
        def.screen_count = 1;
        strncpy(def.next_phase, cases[i].path, sizeof(def.next_phase) - 1);
        if (ref_ok != cases[i].valid ||
            (level_validate_runtime(&def, err, sizeof(err)) == 0) != cases[i].valid) {
            fprintf(stderr, "level_validate_test: level reference '%s' expected %s\n",
                    cases[i].path, cases[i].valid ? "valid" : "rejected");
            return 1;
        }
    }
    return 0;
}

static int rejects_numeric_boundaries(void)
{
    LevelDef def;
    char error[128];
    for (int test = 0; test < 7; test++) {
        level_def_init_defaults(&def);
        if (test == 0) { def.floor_gap_count = 1; def.floor_gaps[0] = 2147483647; }
        if (test == 1) { def.platform_count = 1; def.platforms[0].tile_height = 1; def.platforms[0].tile_width = 89478486; }
        if (test == 2) { def.platform_count = 1; def.platforms[0].tile_height = 89478486; }
        if (test == 3 || test == 4) {
            def.rail_count = 1;
            def.rails[0] = (RailPlacement){RAIL_LAYOUT_RECT, 80, 40, 4, 3, 1};
            def.spike_block_count = 1;
            def.spike_blocks[0].speed = 3.0f;
            def.spike_blocks[0].t_offset = test == 3 ? -1.0f : 1e30f;
        }
        if (test == 5) def.physics.walk_max_speed = 1e30f;
        if (test == 6) { def.faster_bird_count = 1; def.faster_birds[0].frame_index = FBIRD_FRAMES; }
        if (level_validate_runtime(&def, error, sizeof(error)) == 0) {
            fprintf(stderr, "accepted unsafe numeric case %d\n", test);
            return 1;
        }
    }
    return 0;
}

int main(void)
{
    if (rejects_numeric_boundaries() != 0) return 1;
    if (level_reference_rule() != 0) return 1;
    if (expect_valid_level() != 0) return 1;
    if (expect_rejected_level() != 0) return 1;
    if (expect_rejected_bad_rail_index() != 0) return 1;
    if (expect_rejected_oversized_rail() != 0) return 1;
    if (expect_rejected_bridge_overflow() != 0) return 1;
    if (expect_physics_defaults_are_sentinels() != 0) return 1;
    if (expect_rejected_format_version() != 0) return 1;
    if (expect_rejected_floor_gap_outside_world() != 0) return 1;
    if (expect_floor_gaps_on_piece_grid() != 0) return 1;
    if (expect_valid_authored_checkpoints() != 0) return 1;
    if (expect_rejected_authored_checkpoints() != 0) return 1;
    if (expect_checkpoints_clear_of_floor_gaps() != 0) return 1;
    if (expect_rejected_platform_outside_world() != 0) return 1;
    if (expect_rejected_reversed_patrol() != 0) return 1;
    if (expect_rejected_bad_rule_values() != 0) return 1;
    if (expect_rejected_rule_upper_bounds() != 0) return 1;
    if (expect_rejected_oversized_screen_count() != 0) return 1;
    if (expect_rejected_nonfinite_values() != 0) return 1;
    if (expect_rejected_bad_enum_values() != 0) return 1;
    if (expect_rejected_flame_gap_outside_world() != 0) return 1;
    if (expect_rejected_climbable_extents() != 0) return 1;
    if (expect_bouncepads_launch_upward() != 0) return 1;
    if (expect_rail_speeds_forward_and_bounded() != 0) return 1;
    if (expect_patrols_fit_the_sprite() != 0) return 1;
    if (expect_rejected_unsafe_paths() != 0) return 1;

    puts("level_validate_test: ok");
    return 0;
}
