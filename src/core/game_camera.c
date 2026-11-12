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

/*
 * game_camera_reveal_right — The furthest right the camera could be showing
 * for where the player stands now, as a world x.
 *
 * It is the right edge of a view centred on the player and pushed ahead by
 * the level's whole lookahead. The real camera eases toward a target at or
 * behind that, so it never shows more than this while the player moves
 * right, and it only moves left once they turn back.
 *
 * Things that wait to be revealed (spike blocks on open rails) ask this
 * instead of the camera, because the camera depends on the Reduced motion
 * setting and a setting must not change what happens in the level: two
 * runs with the same input have to stay step-for-step identical.
 */
int game_camera_reveal_right(const GameState *gs)
{
    const LevelDef *cam_def = (const LevelDef *)gs->world.runtime.current_level;
    float left = (gs->world.player.x + gs->world.player.w * 0.5f) - (GAME_W * 0.5f);
    if (left < 0.0f) left = 0.0f;   /* the view stops at the world's left edge */
    return (int)(left + level_camera_lookahead_max(cam_def)) + GAME_W;
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
