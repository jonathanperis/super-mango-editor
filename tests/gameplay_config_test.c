#include <stdio.h>

#include "core/game_camera.h"
#include "game.h"  /* GameState: this file reads its fields */
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
    gs.world.runtime.current_level = &def;
    gs.world.runtime.world_w = WORLD_W;
    gs.world.player.x = 300.0f;
    gs.world.player.w = 48;
    gs.world.player.vx = 250.0f;

    game_camera_update(&gs, dt);

    return expect_float("default sentinel camera",
                        gs.world.camera.x,
                        expected_camera_step(gs.world.player.x, gs.world.player.w, gs.world.player.vx,
                                             CAM_LOOKAHEAD_VX_FACTOR,
                                             CAM_LOOKAHEAD_MAX,
                                             gs.world.runtime.world_w, dt));
}

static int camera_allows_zero_override(void)
{
    LevelDef def;
    GameState gs = {0};
    const float dt = 1.0f / 60.0f;

    level_def_init_defaults(&def);
    def.physics.cam_lookahead_vx_factor = 0.0f;
    def.physics.cam_lookahead_max = 0.0f;

    gs.world.runtime.current_level = &def;
    gs.world.runtime.world_w = WORLD_W;
    gs.world.player.x = 300.0f;
    gs.world.player.w = 48;
    gs.world.player.vx = 250.0f;

    game_camera_update(&gs, dt);

    return expect_float("zero override camera",
                        gs.world.camera.x,
                        expected_camera_step(gs.world.player.x, gs.world.player.w, gs.world.player.vx,
                                             0.0f, 0.0f,
                                             gs.world.runtime.world_w, dt));
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

/*
 * game_camera_reveal_right is how far right the camera could be showing.
 * It must never be less than the right edge of the real camera's target
 * (or a spike block would wake on screen) and never more than the camera
 * can reach (or one would start moving off screen).  At the spawn the
 * camera cannot leave the first screen, so the edge is exactly GAME_W.
 */
static int reveal_edge_matches_the_camera(void)
{
    LevelDef def;
    GameState gs = {0};

    level_def_init_defaults(&def);
    gs.world.runtime.current_level = &def;
    gs.world.runtime.world_w = WORLD_W;
    gs.world.player.w = 48;

    gs.world.player.x = 80.0f;                       /* the usual spawn */
    if (game_camera_reveal_right(&gs) != GAME_W) {
        fprintf(stderr, "gameplay_config_test: reveal at the spawn is %d, expected %d\n",
                game_camera_reveal_right(&gs), GAME_W);
        return 1;
    }
    gs.world.player.x = (float)WORLD_W - 60.0f;      /* the far end */
    if (game_camera_reveal_right(&gs) != WORLD_W) {
        fprintf(stderr, "gameplay_config_test: reveal at the end is %d, expected %d\n",
                game_camera_reveal_right(&gs), WORLD_W);
        return 1;
    }
    /* Everywhere between, running right at full speed (the most lookahead
     * there is), the target's right edge is the reveal edge. */
    for (float x = 0.0f; x < (float)WORLD_W; x += 7.0f) {
        gs.world.player.x = x;
        gs.world.player.vx = 10000.0f;
        game_camera_snap(&gs);                       /* the camera at its target */
        int target_right = (int)gs.world.camera.x + GAME_W;
        if (game_camera_reveal_right(&gs) != target_right) {
            fprintf(stderr, "gameplay_config_test: at x %.0f the reveal edge is %d, "
                    "the camera's %d\n", x, game_camera_reveal_right(&gs), target_right);
            return 1;
        }
    }
    return 0;
}

int main(void)
{
    if (camera_uses_default_for_negative_sentinel() != 0) return 1;
    if (camera_allows_zero_override() != 0) return 1;
    if (level_physics_resets_stale_overrides() != 0) return 1;
    if (reveal_edge_matches_the_camera() != 0) return 1;

    puts("gameplay_config_test: ok");
    return 0;
}
