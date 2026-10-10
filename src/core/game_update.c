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
#include "../levels/level_session.h"  /* game_level_start_point_respawn_y */
#include "../shared/geometry.h"      /* float_same_value */

int game_update_active(GameState *gs, float dt, int cam_x)
{
    PlayerStepSupport support;
    const int lives_before = gs->world.lives;

    game_checkpoint_feedback_tick(gs, dt);
    gs->screen.completion.level_elapsed += dt;

    support = game_player_step(gs, dt);
    /* Player movement and surface collision resolve before checkpoint sampling. */
    game_checkpoint_update_authored(gs);
    /* Lethal gap damage must run after checkpoint sampling. */
    floor_gap_handle_collision(gs);
    if (gs->screen.game_over || gs->world.lives != lives_before) return game_camera_update(gs, dt);
    game_actors_update(gs, dt, cam_x);
    game_float_platforms_update(gs, dt, support.float_platform);
    game_bridges_update(gs, dt, support.bridge);
    /* All moving hazards reach this frame's position before hitbox sampling. */
    game_hazards_update(gs, dt, cam_x);
    game_collide(gs, dt);
    if (gs->screen.game_over || gs->world.lives != lives_before) return game_camera_update(gs, dt);
    /* Legacy screen checkpoints remain sampled after collision damage. They
     * move respawn_x only; a start-point run also moves respawn_y to the
     * ground there (level_session.c says why). */
    float respawn_x = gs->world.respawn_x;
    game_checkpoint_update(gs);
    if (!float_same_value(gs->world.respawn_x, respawn_x)) game_level_start_point_respawn_y(gs);
    game_effects_update(gs, dt);
    game_bouncepads_update_animations(gs, dt);

    return game_camera_update(gs, dt);
}
