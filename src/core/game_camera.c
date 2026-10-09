/*
 * game_camera.c — Smooth camera follow logic.
 */

#include "game_camera.h"

#include "../levels/level.h"
#include "../levels/level_physics.h"
#include "game_profile.h"

/*
 * game_camera_target — Where the camera wants its left edge to be: the
 * player's centre in the middle of the screen, pushed ahead in the direction
 * of travel (the lookahead), and clamped so the view never leaves the world.
 */
static float game_camera_target(const GameState *gs)
{
    const LevelDef *cam_def = (const LevelDef *)gs->runtime.current_level;
    float cam_vx_factor = level_camera_lookahead_vx_factor(cam_def);
    float cam_max = level_camera_lookahead_max(cam_def);

    float lookahead = gs->player.vx * cam_vx_factor;
    if (gs->profile && gs->profile->data.settings.reduced_motion) lookahead = 0.0f;
    if (lookahead >  cam_max) lookahead =  cam_max;
    if (lookahead < -cam_max) lookahead = -cam_max;

    float cam_target = (gs->player.x + gs->player.w * 0.5f)
                       - (GAME_W * 0.5f)
                       + lookahead;

    if (cam_target < 0.0f) cam_target = 0.0f;
    if (cam_target > gs->runtime.world_w - GAME_W) {
        cam_target = (float)(gs->runtime.world_w - GAME_W);
    }
    return cam_target;
}

int game_camera_update(GameState *gs, float dt)
{
    float cam_target = game_camera_target(gs);

    float cam_diff = cam_target - gs->camera.x;
    if (cam_diff > CAM_SNAP_THRESHOLD || cam_diff < -CAM_SNAP_THRESHOLD) {
        gs->camera.x += cam_diff * CAM_SMOOTHING * dt;
    } else {
        gs->camera.x = cam_target;
    }

    return (int)gs->camera.x;
}

void game_camera_snap(GameState *gs)
{
    gs->camera.x = game_camera_target(gs);
}
