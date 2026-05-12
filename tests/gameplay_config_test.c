#include <stdio.h>

#include "core/game_camera.h"
#include "levels/level_physics.h"

static int expect_float(const char *name, float actual, float expected)
{
    float diff = actual - expected;
    if (diff < 0.0f) diff = -diff;
    if (diff > 0.001f) {
        fprintf(stderr, "gameplay_config_test: %s got %.4f expected %.4f\n",
                name, actual, expected);
        return 1;
    }
    return 0;
}

static float expected_camera_step(float player_x, int player_w, float player_vx,
                                  float factor, float max_lookahead,
                                  int world_w, float dt)
{
    float lookahead = player_vx * factor;
    float target;
    float diff;

    if (lookahead >  max_lookahead) lookahead =  max_lookahead;
    if (lookahead < -max_lookahead) lookahead = -max_lookahead;

    target = player_x + player_w * 0.5f - GAME_W * 0.5f + lookahead;
    if (target < 0.0f) target = 0.0f;
    if (target > world_w - GAME_W) target = (float)(world_w - GAME_W);

    diff = target;
    if (diff > CAM_SNAP_THRESHOLD || diff < -CAM_SNAP_THRESHOLD) {
        return diff * CAM_SMOOTHING * dt;
    }
    return target;
}

static int camera_uses_default_for_negative_sentinel(void)
{
    LevelDef def;
    GameState gs = {0};
    const float dt = 1.0f / 60.0f;

    level_def_init_defaults(&def);
    gs.runtime.current_level = &def;
    gs.runtime.world_w = WORLD_W;
    gs.player.x = 300.0f;
    gs.player.w = 48;
    gs.player.vx = 250.0f;

    game_camera_update(&gs, dt);

    return expect_float("default sentinel camera",
                        gs.camera.x,
                        expected_camera_step(gs.player.x, gs.player.w, gs.player.vx,
                                             CAM_LOOKAHEAD_VX_FACTOR,
                                             CAM_LOOKAHEAD_MAX,
                                             gs.runtime.world_w, dt));
}

static int camera_allows_zero_override(void)
{
    LevelDef def;
    GameState gs = {0};
    const float dt = 1.0f / 60.0f;

    level_def_init_defaults(&def);
    def.physics.cam_lookahead_vx_factor = 0.0f;
    def.physics.cam_lookahead_max = 0.0f;

    gs.runtime.current_level = &def;
    gs.runtime.world_w = WORLD_W;
    gs.player.x = 300.0f;
    gs.player.w = 48;
    gs.player.vx = 250.0f;

    game_camera_update(&gs, dt);

    return expect_float("zero override camera",
                        gs.camera.x,
                        expected_camera_step(gs.player.x, gs.player.w, gs.player.vx,
                                             0.0f, 0.0f,
                                             gs.runtime.world_w, dt));
}

static int level_physics_resets_stale_overrides(void)
{
    Player defaults = {0};
    Player player = {0};
    LevelDef tuned;
    LevelDef defaulted;

    player_apply_default_physics(&defaults);

    level_def_init_defaults(&tuned);
    tuned.physics.walk_max_speed = 42.0f;
    tuned.physics.air_friction = 0.0f;
    level_apply_player_physics(&player, &tuned);

    if (expect_float("tuned walk", player.walk_max_speed, 42.0f) != 0) return 1;
    if (expect_float("default run", player.run_max_speed, defaults.run_max_speed) != 0) return 1;
    if (expect_float("zero air friction", player.air_friction, 0.0f) != 0) return 1;

    level_def_init_defaults(&defaulted);
    level_apply_player_physics(&player, &defaulted);

    if (expect_float("reset walk", player.walk_max_speed, defaults.walk_max_speed) != 0)
        return 1;
    if (expect_float("reset air friction", player.air_friction, defaults.air_friction) != 0)
        return 1;

    return 0;
}

int main(void)
{
    if (camera_uses_default_for_negative_sentinel() != 0) return 1;
    if (camera_allows_zero_override() != 0) return 1;
    if (level_physics_resets_stale_overrides() != 0) return 1;

    puts("gameplay_config_test: ok");
    return 0;
}
