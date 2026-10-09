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
    float player_cx = gs->world.player.x + gs->world.player.w / 2.0f;

    spiders_update(gs->world.spiders, gs->world.spider_count, dt,
                   gs->world.floor_gaps, gs->world.floor_gap_count);
    jumping_spiders_update(gs->world.jumping_spiders, gs->world.jumping_spider_count, dt,
                           gs->world.floor_gaps, gs->world.floor_gap_count,
                           gs->assets.audio.spider_attack, player_cx, cam_x);
    birds_update(gs->world.birds, gs->world.bird_count, dt, gs->assets.audio.flap,
                 player_cx, cam_x);
    faster_birds_update(gs->world.faster_birds, gs->world.faster_bird_count, dt,
                        gs->assets.audio.flap, player_cx, cam_x);
    fish_update(gs->world.fish, gs->world.fish_count, dt, gs->world.runtime.world_w);
    faster_fish_update(gs->world.faster_fish, gs->world.faster_fish_count, dt,
                       gs->world.runtime.world_w);
    spike_blocks_update(gs->world.spike_blocks, gs->world.spike_block_count, dt, cam_x);
}
