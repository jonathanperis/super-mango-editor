/*
 * game_resources.c — Load and release renderer/audio resources owned by GameState.
 */

#include "game_resources.h"

#include <SDL_image.h>
#include <SDL_mixer.h>
#include <stdio.h>
#include <stdlib.h>

#include "../effects/water.h"
#include "../effects/parallax.h"

static void game_resources_fail(GameState *gs, const char *label, const char *detail)
{
    fprintf(stderr, "%s: %s\n", label, detail);
    game_cleanup(gs);
    exit(EXIT_FAILURE);
}

void game_resources_load(GameState *gs)
{
    gs->textures.floor_tile = IMG_LoadTexture(gs->renderer, "assets/sprites/levels/grass_tileset.png");
    if (!gs->textures.floor_tile) {
        game_resources_fail(gs, "Failed to load Grass_Tileset.png", IMG_GetError());
    }

    gs->textures.platform = IMG_LoadTexture(gs->renderer, "assets/sprites/levels/grass_platform.png");
    if (!gs->textures.platform) {
        game_resources_fail(gs, "Failed to load Grass_Oneway.png", IMG_GetError());
    }

    water_init(&gs->water, gs->renderer);

    gs->textures.spider = IMG_LoadTexture(gs->renderer, "assets/sprites/entities/spider.png");
    if (!gs->textures.spider) {
        game_resources_fail(gs, "Failed to load Spider_1.png", IMG_GetError());
    }

    gs->textures.jumping_spider = IMG_LoadTexture(gs->renderer, "assets/sprites/entities/jumping_spider.png");
    if (!gs->textures.jumping_spider) {
        game_resources_fail(gs, "Failed to load Spider_2.png", IMG_GetError());
    }

    gs->textures.bird = IMG_LoadTexture(gs->renderer, "assets/sprites/entities/bird.png");
    if (!gs->textures.bird) game_resources_fail(gs, "Failed to load Bird_2.png", IMG_GetError());
    gs->textures.faster_bird = IMG_LoadTexture(gs->renderer, "assets/sprites/entities/faster_bird.png");
    if (!gs->textures.faster_bird) game_resources_fail(gs, "Failed to load Bird_1.png", IMG_GetError());
    gs->textures.fish = IMG_LoadTexture(gs->renderer, "assets/sprites/entities/fish.png");
    if (!gs->textures.fish) game_resources_fail(gs, "Failed to load Fish_2.png", IMG_GetError());
    gs->textures.coin = IMG_LoadTexture(gs->renderer, "assets/sprites/collectibles/coin.png");
    if (!gs->textures.coin) game_resources_fail(gs, "Failed to load Coin.png", IMG_GetError());
    gs->textures.bouncepad_medium = IMG_LoadTexture(gs->renderer, "assets/sprites/surfaces/bouncepad_medium.png");
    if (!gs->textures.bouncepad_medium) game_resources_fail(gs, "Failed to load Bouncepad_Wood.png", IMG_GetError());

    gs->textures.vine_green = IMG_LoadTexture(gs->renderer, "assets/sprites/surfaces/vine_green.png");
    if (!gs->textures.vine_green) fprintf(stderr, "Warning: Failed to load Vine_Green.png: %s\n", IMG_GetError());
    gs->textures.vine_brown = IMG_LoadTexture(gs->renderer, "assets/sprites/surfaces/vine_brown.png");
    if (!gs->textures.vine_brown) fprintf(stderr, "Warning: Failed to load Vine_Brown.png: %s\n", IMG_GetError());
    gs->textures.ladder = IMG_LoadTexture(gs->renderer, "assets/sprites/surfaces/ladder.png");
    if (!gs->textures.ladder) fprintf(stderr, "Warning: Failed to load Ladder.png: %s\n", IMG_GetError());
    gs->textures.rope = IMG_LoadTexture(gs->renderer, "assets/sprites/surfaces/rope.png");
    if (!gs->textures.rope) fprintf(stderr, "Warning: Failed to load Rope.png: %s\n", IMG_GetError());
    gs->textures.bouncepad_small = IMG_LoadTexture(gs->renderer, "assets/sprites/surfaces/bouncepad_small.png");
    if (!gs->textures.bouncepad_small) fprintf(stderr, "Warning: Failed to load Bouncepad_Green.png: %s\n", IMG_GetError());
    gs->textures.bouncepad_high = IMG_LoadTexture(gs->renderer, "assets/sprites/surfaces/bouncepad_high.png");
    if (!gs->textures.bouncepad_high) fprintf(stderr, "Warning: Failed to load Bouncepad_Red.png: %s\n", IMG_GetError());
    gs->textures.rail = IMG_LoadTexture(gs->renderer, "assets/sprites/surfaces/rail.png");
    if (!gs->textures.rail) fprintf(stderr, "Warning: Failed to load Rails.png: %s\n", IMG_GetError());
    gs->textures.spike_block = IMG_LoadTexture(gs->renderer, "assets/sprites/hazards/spike_block.png");
    if (!gs->textures.spike_block) fprintf(stderr, "Warning: Failed to load Spike_Block.png: %s\n", IMG_GetError());
    gs->textures.float_platform = IMG_LoadTexture(gs->renderer, "assets/sprites/surfaces/float_platform.png");
    if (!gs->textures.float_platform) fprintf(stderr, "Warning: Failed to load Platform.png: %s\n", IMG_GetError());
    gs->textures.bridge = IMG_LoadTexture(gs->renderer, "assets/sprites/surfaces/bridge.png");
    if (!gs->textures.bridge) fprintf(stderr, "Warning: Failed to load Bridge.png: %s\n", IMG_GetError());
    gs->textures.star_yellow = IMG_LoadTexture(gs->renderer, "assets/sprites/collectibles/star_yellow.png");
    if (!gs->textures.star_yellow) fprintf(stderr, "Warning: Failed to load star_yellow.png: %s\n", IMG_GetError());
    gs->textures.star_green = IMG_LoadTexture(gs->renderer, "assets/sprites/collectibles/star_green.png");
    if (!gs->textures.star_green) fprintf(stderr, "Warning: Failed to load star_green.png: %s\n", IMG_GetError());
    gs->textures.star_red = IMG_LoadTexture(gs->renderer, "assets/sprites/collectibles/star_red.png");
    if (!gs->textures.star_red) fprintf(stderr, "Warning: Failed to load star_red.png: %s\n", IMG_GetError());
    gs->textures.last_star = IMG_LoadTexture(gs->renderer, "assets/sprites/collectibles/last_star.png");
    if (!gs->textures.last_star) fprintf(stderr, "Warning: Failed to load last_star.png: %s\n", IMG_GetError());
    gs->textures.axe_trap = IMG_LoadTexture(gs->renderer, "assets/sprites/hazards/axe_trap.png");
    if (!gs->textures.axe_trap) fprintf(stderr, "Warning: Failed to load Axe_Trap.png: %s\n", IMG_GetError());
    gs->textures.circular_saw = IMG_LoadTexture(gs->renderer, "assets/sprites/hazards/circular_saw.png");
    if (!gs->textures.circular_saw) fprintf(stderr, "Warning: Failed to load Circular_Saw.png: %s\n", IMG_GetError());
    gs->textures.blue_flame = IMG_LoadTexture(gs->renderer, "assets/sprites/hazards/blue_flame.png");
    if (!gs->textures.blue_flame) fprintf(stderr, "Warning: Failed to load blue_flame.png: %s\n", IMG_GetError());
    gs->textures.fire_flame = IMG_LoadTexture(gs->renderer, "assets/sprites/hazards/fire_flame.png");
    if (!gs->textures.fire_flame) fprintf(stderr, "Warning: Failed to load fire_flame.png: %s\n", IMG_GetError());
    gs->textures.faster_fish = IMG_LoadTexture(gs->renderer, "assets/sprites/entities/faster_fish.png");
    if (!gs->textures.faster_fish) fprintf(stderr, "Warning: Failed to load Fish_1.png: %s\n", IMG_GetError());
    gs->textures.spike = IMG_LoadTexture(gs->renderer, "assets/sprites/hazards/spike.png");
    if (!gs->textures.spike) fprintf(stderr, "Warning: Failed to load Spike.png: %s\n", IMG_GetError());
    gs->textures.spike_platform = IMG_LoadTexture(gs->renderer, "assets/sprites/hazards/spike_platform.png");
    if (!gs->textures.spike_platform) fprintf(stderr, "Warning: Failed to load Spike_Platform.png: %s\n", IMG_GetError());

    gs->audio.spring = Mix_LoadWAV("assets/sounds/surfaces/bouncepad.wav");
    if (!gs->audio.spring) fprintf(stderr, "Warning: Failed to load bouncepad.wav: %s\n", Mix_GetError());
    gs->audio.axe = Mix_LoadWAV("assets/sounds/hazards/axe_trap.wav");
    if (!gs->audio.axe) fprintf(stderr, "Warning: Failed to load axe_trap.wav: %s\n", Mix_GetError());
    gs->audio.flap = Mix_LoadWAV("assets/sounds/entities/bird.wav");
    if (!gs->audio.flap) fprintf(stderr, "Warning: Failed to load flapping.wav: %s\n", Mix_GetError());
    gs->audio.spider_attack = Mix_LoadWAV("assets/sounds/entities/spider.wav");
    if (!gs->audio.spider_attack) fprintf(stderr, "Warning: Failed to load spider-attack.mp3: %s\n", Mix_GetError());
    gs->audio.dive = Mix_LoadWAV("assets/sounds/entities/fish.wav");
    if (!gs->audio.dive) fprintf(stderr, "Warning: Failed to load dive.wav: %s\n", Mix_GetError());
    gs->audio.jump = Mix_LoadWAV("assets/sounds/player/player_jump.wav");
    if (!gs->audio.jump) fprintf(stderr, "Warning: Failed to load jump.wav: %s\n", Mix_GetError());
    gs->audio.coin = Mix_LoadWAV("assets/sounds/collectibles/coin.wav");
    if (!gs->audio.coin) fprintf(stderr, "Warning: Failed to load coin.wav: %s\n", Mix_GetError());
    gs->audio.hit = Mix_LoadWAV("assets/sounds/player/player_hit.wav");
    if (!gs->audio.hit) fprintf(stderr, "Warning: Failed to load hit.wav: %s\n", Mix_GetError());

    gs->audio.music = NULL;
}

void game_resources_cleanup(GameState *gs)
{
    if (gs->audio.music) {
        Mix_HaltMusic();
        Mix_FreeMusic(gs->audio.music);
        gs->audio.music = NULL;
    }

    FREE_CHUNK(gs->audio.jump);
    FREE_CHUNK(gs->audio.coin);
    FREE_CHUNK(gs->audio.hit);

    water_cleanup(&gs->water);

    DESTROY_TEX(gs->textures.ctrl_init_msg);

    DESTROY_TEX(gs->textures.spike_block);
    DESTROY_TEX(gs->textures.bridge);
    DESTROY_TEX(gs->textures.float_platform);
    DESTROY_TEX(gs->textures.rail);
    DESTROY_TEX(gs->textures.vine_green);
    DESTROY_TEX(gs->textures.vine_brown);
    DESTROY_TEX(gs->textures.ladder);
    DESTROY_TEX(gs->textures.rope);

    FREE_CHUNK(gs->audio.spring);
    DESTROY_TEX(gs->textures.bouncepad_medium);
    DESTROY_TEX(gs->textures.bouncepad_small);
    DESTROY_TEX(gs->textures.bouncepad_high);

    DESTROY_TEX(gs->textures.coin);
    DESTROY_TEX(gs->textures.star_yellow);
    DESTROY_TEX(gs->textures.star_green);
    DESTROY_TEX(gs->textures.star_red);
    DESTROY_TEX(gs->textures.last_star);

    FREE_CHUNK(gs->audio.dive);
    FREE_CHUNK(gs->audio.spider_attack);
    FREE_CHUNK(gs->audio.flap);
    FREE_CHUNK(gs->audio.axe);

    DESTROY_TEX(gs->textures.axe_trap);
    DESTROY_TEX(gs->textures.circular_saw);
    DESTROY_TEX(gs->textures.blue_flame);
    DESTROY_TEX(gs->textures.fire_flame);
    DESTROY_TEX(gs->textures.faster_fish);
    DESTROY_TEX(gs->textures.spike);
    DESTROY_TEX(gs->textures.spike_platform);
    DESTROY_TEX(gs->textures.fish);
    DESTROY_TEX(gs->textures.faster_bird);
    DESTROY_TEX(gs->textures.bird);
    DESTROY_TEX(gs->textures.jumping_spider);
    DESTROY_TEX(gs->textures.spider);

    for (int i = 0; i < gs->platform_count; i++) {
        if (gs->platforms[i].tex) {
            SDL_DestroyTexture(gs->platforms[i].tex);
            gs->platforms[i].tex = NULL;
        }
    }
    DESTROY_TEX(gs->textures.platform);
    DESTROY_TEX(gs->textures.floor_tile);

    parallax_cleanup(&gs->parallax);
}
