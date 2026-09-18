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
#include "../collision/floor_gap_collision.h"
#include "../collision/game_collision.h"
#include "../effects/game_effects.h"

int game_update_active(GameState *gs, float dt, int cam_x)
{
    int fp_landed_idx;
    const int lives_before = gs->lives;

    game_checkpoint_feedback_clear_expired(gs, (uint32_t)clock_millis());
    gs->completion.level_elapsed += dt;

    fp_landed_idx = game_player_step(gs, dt);
    /* Player movement and surface collision resolve before checkpoint sampling. */
    game_checkpoint_update_authored(gs);
    /* Lethal gap damage must run after checkpoint sampling. */
    floor_gap_handle_collision(gs);
    if (gs->game_over || gs->lives != lives_before) return game_camera_update(gs, dt);
    game_actors_update(gs, dt, cam_x);
    game_float_platforms_update(gs, dt, fp_landed_idx);
    game_bridges_update(gs, dt);
    /* All moving hazards reach this frame's position before hitbox sampling. */
    game_hazards_update(gs, dt, cam_x);
    game_collide(gs, dt);
    if (gs->game_over || gs->lives != lives_before) return game_camera_update(gs, dt);
    /* Legacy screen checkpoints remain sampled after collision damage. */
    game_checkpoint_update(gs);
    game_effects_update(gs, dt);
    game_bouncepads_update_animations(gs, dt);

    return game_camera_update(gs, dt);
}
