/*
 * game_player_step.c — Player input, movement, and bounce handling.
 */

#include "game_player_step.h"

#include "game_bouncepads.h"
#include "game_experiment.h"
#include "../input/game_input.h"
#include "../input/game_web_input.h"
#include "../player/player.h"

int game_player_step(GameState *gs, float dt)
{
    BouncepadList pad_lists[GAME_BOUNCEPAD_LIST_COUNT];
    int bounce_idx = -1;
    int fp_landed_idx = -1;

    unsigned int input = gs->replay_input_mask | game_web_input_take_touch_mask() | game_input_sample(gs);
    input = game_experiment_input(gs, dt, input);
    player_handle_input(&gs->player, gs->audio.jump,
                        input, 0,
                        gs->vines, gs->vine_count,
                        gs->ladders, gs->ladder_count,
                        gs->ropes, gs->rope_count);

    /* Pass the three pad arrays as views (no per-frame copy). */
    game_bouncepads_lists(gs, pad_lists);

    player_update(&gs->player, dt, gs->audio.jump,
                  gs->platforms, gs->platform_count,
                  gs->float_platforms, gs->float_platform_count,
                  pad_lists, GAME_BOUNCEPAD_LIST_COUNT,
                  gs->vines, gs->vine_count,
                  gs->ladders, gs->ladder_count,
                  gs->ropes, gs->rope_count,
                  gs->bridges, gs->bridge_count,
                  gs->spike_platforms, gs->spike_platform_count,
                  gs->floor_gaps, gs->floor_gap_count,
                  &bounce_idx, &fp_landed_idx,
                  gs->loop.fp_prev_riding,
                  gs->runtime.world_w);

    game_bouncepads_handle_hit(gs, bounce_idx);

    return fp_landed_idx;
}
