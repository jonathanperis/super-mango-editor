/*
 * game_update.c — Active-frame game update pipeline.
 */

#include "game_update.h"

#include "game_actors.h"
#include "game_bouncepads.h"
#include "game_bridges.h"
#include "game_camera.h"
#include "game_checkpoint.h"
#include "game_float_platforms.h"
#include "game_hazards.h"
#include "game_player_step.h"
#include "../collision/game_collision.h"
#include "../effects/game_effects.h"

int game_update_active(GameState *gs, float dt, int cam_x)
{
    int fp_landed_idx;

    gs->level_elapsed += dt;

    fp_landed_idx = game_player_step(gs, dt);
    game_actors_update(gs, dt, cam_x);
    game_float_platforms_update(gs, dt, fp_landed_idx);
    game_bridges_update(gs, dt);
    game_collide(gs, dt);
    game_checkpoint_update(gs);
    game_effects_update(gs, dt);
    game_bouncepads_update_animations(gs, dt);
    game_hazards_update(gs, dt, cam_x);

    return game_camera_update(gs, dt);
}
