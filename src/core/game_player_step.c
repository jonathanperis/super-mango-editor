/*
 * game_player_step.c — Player input, movement, and bounce handling.
 */

#include "game_player_step.h"

#include "game_bouncepads.h"
#include "game_experiment.h"
#include "../input/game_input.h"
#include "../input/game_web_input.h"
#include "../player/player.h"

PlayerStepSupport game_player_step(GameState *gs, float dt)
{
    BouncepadList pad_lists[GAME_BOUNCEPAD_LIST_COUNT];
    PlayerLanding landed;
    PlayerStepSupport support = { -1, -1 };

    /* Pass the three pad arrays as views (no per-frame copy). */
    game_bouncepads_lists(gs, pad_lists);

    /* Everything the player can stand on, climb or bounce off: borrowed
     * views of the world's arrays, named once for both calls below. */
    const PlayerSurfaces surfaces = {
        .platforms       = gs->world.platforms,       .platform_count       = gs->world.platform_count,
        .float_platforms = gs->world.float_platforms, .float_platform_count = gs->world.float_platform_count,
        .bouncepad_lists = pad_lists,                 .bouncepad_list_count = GAME_BOUNCEPAD_LIST_COUNT,
        .vines           = gs->world.vines,           .vine_count           = gs->world.vine_count,
        .ladders         = gs->world.ladders,         .ladder_count         = gs->world.ladder_count,
        .ropes           = gs->world.ropes,           .rope_count           = gs->world.rope_count,
        .bridges         = gs->world.bridges,         .bridge_count         = gs->world.bridge_count,
        .spike_platforms = gs->world.spike_platforms, .spike_platform_count = gs->world.spike_platform_count,
        .floor_gaps      = gs->world.floor_gaps,      .floor_gap_count      = gs->world.floor_gap_count,
        .world_w         = gs->world.runtime.world_w,
    };

    unsigned int input = gs->screen.replay_input_mask | game_web_input_take_touch_mask() | game_input_sample(gs);
    input = game_experiment_input(gs, input);
    player_handle_input(&gs->world.player, gs->assets.audio.jump, input, 0, &surfaces);

    player_update(&gs->world.player, dt, gs->assets.audio.jump, &surfaces,
                  gs->screen.loop.fp_prev_riding, &landed);

    game_bouncepads_handle_hit(gs, landed.bouncepad_list, landed.bouncepad_index);

    support.float_platform = landed.float_platform;
    support.bridge = landed.bridge;
    return support;
}
