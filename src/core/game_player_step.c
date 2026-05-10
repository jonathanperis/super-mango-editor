/*
 * game_player_step.c — Player input, movement, bounce, and gap response.
 */

#include "game_player_step.h"

#include "game_bouncepads.h"
#include "../collision/floor_gap_collision.h"
#include "../player/player.h"

static Bouncepad s_all_pads[MAX_BOUNCEPADS_MEDIUM + MAX_BOUNCEPADS_SMALL +
                            MAX_BOUNCEPADS_HIGH];

int game_player_step(GameState *gs, float dt)
{
    int all_pad_count;
    int bounce_idx = -1;
    int fp_landed_idx = -1;

    player_handle_input(&gs->player, gs->audio.jump, gs->controller,
                        gs->vines, gs->vine_count,
                        gs->ladders, gs->ladder_count,
                        gs->ropes, gs->rope_count);

    all_pad_count = game_bouncepads_collect(gs, s_all_pads);

    player_update(&gs->player, dt, gs->audio.jump,
                  gs->platforms, gs->platform_count,
                  gs->float_platforms, gs->float_platform_count,
                  s_all_pads, all_pad_count,
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
    floor_gap_handle_collision(gs);

    return fp_landed_idx;
}
