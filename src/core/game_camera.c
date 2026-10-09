/*
 * game_camera.c — Smooth camera follow logic.
 */

#include "game_camera.h"
#include "../game.h"  /* GameState: this file reads its fields */

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
    const LevelDef *cam_def = (const LevelDef *)gs->world.runtime.current_level;
    float cam_vx_factor = level_camera_lookahead_vx_factor(cam_def);
    float cam_max = level_camera_lookahead_max(cam_def);

    float lookahead = gs->world.player.vx * cam_vx_factor;
    if (gs->screen.profile && gs->screen.profile->data.settings.reduced_motion) lookahead = 0.0f;
    if (lookahead >  cam_max) lookahead =  cam_max;
    if (lookahead < -cam_max) lookahead = -cam_max;

    float cam_target = (gs->world.player.x + gs->world.player.w * 0.5f)
                       - (GAME_W * 0.5f)
                       + lookahead;

    if (cam_target < 0.0f) cam_target = 0.0f;
    if (cam_target > gs->world.runtime.world_w - GAME_W) {
        cam_target = (float)(gs->world.runtime.world_w - GAME_W);
    }
    return cam_target;
}

int game_camera_update(GameState *gs, float dt)
{
    float cam_target = game_camera_target(gs);

    float cam_diff = cam_target - gs->world.camera.x;
    if (cam_diff > CAM_SNAP_THRESHOLD || cam_diff < -CAM_SNAP_THRESHOLD) {
        gs->world.camera.x += cam_diff * CAM_SMOOTHING * dt;
    } else {
        gs->world.camera.x = cam_target;
    }

    return (int)gs->world.camera.x;
}

void game_camera_snap(GameState *gs)
{
    gs->world.camera.x = game_camera_target(gs);
}
