/*
 * game_actors.c — Per-frame enemy and moving hazard updates.
 */

#include "game_actors.h"

#include "../entities/bird.h"
#include "../entities/faster_bird.h"
#include "../entities/faster_fish.h"
#include "../entities/fish.h"
#include "../entities/jumping_spider.h"
#include "../entities/spider.h"
#include "../hazards/spike_block.h"

void game_actors_update(GameState *gs, float dt, int cam_x)
{
    float player_cx = gs->player.x + gs->player.w / 2.0f;

    spiders_update(gs->spiders, gs->spider_count, dt,
                   gs->floor_gaps, gs->floor_gap_count);
    jumping_spiders_update(gs->jumping_spiders, gs->jumping_spider_count, dt,
                           gs->floor_gaps, gs->floor_gap_count,
                           gs->audio.spider_attack, player_cx, cam_x);
    birds_update(gs->birds, gs->bird_count, dt, gs->audio.flap,
                 player_cx, cam_x);
    faster_birds_update(gs->faster_birds, gs->faster_bird_count, dt,
                        gs->audio.flap, player_cx, cam_x);
    fish_update(gs->fish, gs->fish_count, dt, gs->runtime.world_w);
    faster_fish_update(gs->faster_fish, gs->faster_fish_count, dt,
                       gs->runtime.world_w);
    spike_blocks_update(gs->spike_blocks, gs->spike_block_count, dt, cam_x);
}
