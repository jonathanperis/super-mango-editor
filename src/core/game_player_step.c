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
    int bounce_idx = -1;
    PlayerStepSupport support = { -1, -1 };

    unsigned int input = gs->screen.replay_input_mask | game_web_input_take_touch_mask() | game_input_sample(gs);
    input = game_experiment_input(gs, input);
    player_handle_input(&gs->world.player, gs->assets.audio.jump,
                        input, 0,
                        gs->world.vines, gs->world.vine_count,
                        gs->world.ladders, gs->world.ladder_count,
                        gs->world.ropes, gs->world.rope_count);

    /* Pass the three pad arrays as views (no per-frame copy). */
    game_bouncepads_lists(gs, pad_lists);

    player_update(&gs->world.player, dt, gs->assets.audio.jump,
                  gs->world.platforms, gs->world.platform_count,
                  gs->world.float_platforms, gs->world.float_platform_count,
                  pad_lists, GAME_BOUNCEPAD_LIST_COUNT,
                  gs->world.vines, gs->world.vine_count,
                  gs->world.ladders, gs->world.ladder_count,
                  gs->world.ropes, gs->world.rope_count,
                  gs->world.bridges, gs->world.bridge_count,
                  gs->world.spike_platforms, gs->world.spike_platform_count,
                  gs->world.floor_gaps, gs->world.floor_gap_count,
                  &bounce_idx, &support.float_platform, &support.bridge,
                  gs->screen.loop.fp_prev_riding,
                  gs->world.runtime.world_w);

    game_bouncepads_handle_hit(gs, bounce_idx);

    return support;
}
