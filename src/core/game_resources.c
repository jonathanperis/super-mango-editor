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

static SDL_Texture *load_required_texture(GameState *gs, const char *path,
                                          const char *label)
{
    SDL_Texture *tex = IMG_LoadTexture(gs->renderer, path);
    if (!tex) game_resources_fail(gs, label, IMG_GetError());
    return tex;
}

static SDL_Texture *load_optional_texture(GameState *gs, const char *path,
                                          const char *label)
{
    SDL_Texture *tex = IMG_LoadTexture(gs->renderer, path);
    if (!tex) {
        fprintf(stderr, "Warning: Failed to load %s: %s\n", label, IMG_GetError());
    }
    return tex;
}

static Mix_Chunk *load_optional_chunk(const char *path, const char *label)
{
    Mix_Chunk *chunk = Mix_LoadWAV(path);
    if (!chunk) {
        fprintf(stderr, "Warning: Failed to load %s: %s\n", label, Mix_GetError());
    }
    return chunk;
}

void game_resources_load(GameState *gs)
{
    gs->textures.floor_tile = load_required_texture(gs,
        "assets/sprites/levels/grass_tileset.png",
        "Failed to load Grass_Tileset.png");
    gs->textures.platform = load_required_texture(gs,
        "assets/sprites/levels/grass_platform.png",
        "Failed to load Grass_Oneway.png");

    water_init(&gs->water, gs->renderer);

    gs->textures.spider = load_required_texture(gs,
        "assets/sprites/entities/spider.png", "Failed to load Spider_1.png");
    gs->textures.jumping_spider = load_required_texture(gs,
        "assets/sprites/entities/jumping_spider.png", "Failed to load Spider_2.png");
    gs->textures.bird = load_required_texture(gs,
        "assets/sprites/entities/bird.png", "Failed to load Bird_2.png");
    gs->textures.faster_bird = load_required_texture(gs,
        "assets/sprites/entities/faster_bird.png", "Failed to load Bird_1.png");
    gs->textures.fish = load_required_texture(gs,
        "assets/sprites/entities/fish.png", "Failed to load Fish_2.png");
    gs->textures.coin = load_required_texture(gs,
        "assets/sprites/collectibles/coin.png", "Failed to load Coin.png");
    gs->textures.bouncepad_medium = load_required_texture(gs,
        "assets/sprites/surfaces/bouncepad_medium.png",
        "Failed to load Bouncepad_Wood.png");

    gs->textures.vine_green = load_optional_texture(gs,
        "assets/sprites/surfaces/vine_green.png", "Vine_Green.png");
    gs->textures.vine_brown = load_optional_texture(gs,
        "assets/sprites/surfaces/vine_brown.png", "Vine_Brown.png");
    gs->textures.ladder = load_optional_texture(gs,
        "assets/sprites/surfaces/ladder.png", "Ladder.png");
    gs->textures.rope = load_optional_texture(gs,
        "assets/sprites/surfaces/rope.png", "Rope.png");
    gs->textures.bouncepad_small = load_optional_texture(gs,
        "assets/sprites/surfaces/bouncepad_small.png", "Bouncepad_Green.png");
    gs->textures.bouncepad_high = load_optional_texture(gs,
        "assets/sprites/surfaces/bouncepad_high.png", "Bouncepad_Red.png");
    gs->textures.rail = load_optional_texture(gs,
        "assets/sprites/surfaces/rail.png", "Rails.png");
    gs->textures.spike_block = load_optional_texture(gs,
        "assets/sprites/hazards/spike_block.png", "Spike_Block.png");
    gs->textures.float_platform = load_optional_texture(gs,
        "assets/sprites/surfaces/float_platform.png", "Platform.png");
    gs->textures.bridge = load_optional_texture(gs,
        "assets/sprites/surfaces/bridge.png", "Bridge.png");
    gs->textures.star_yellow = load_optional_texture(gs,
        "assets/sprites/collectibles/star_yellow.png", "star_yellow.png");
    gs->textures.star_green = load_optional_texture(gs,
        "assets/sprites/collectibles/star_green.png", "star_green.png");
    gs->textures.star_red = load_optional_texture(gs,
        "assets/sprites/collectibles/star_red.png", "star_red.png");
    gs->textures.last_star = load_optional_texture(gs,
        "assets/sprites/collectibles/last_star.png", "last_star.png");
    gs->textures.axe_trap = load_optional_texture(gs,
        "assets/sprites/hazards/axe_trap.png", "Axe_Trap.png");
    gs->textures.circular_saw = load_optional_texture(gs,
        "assets/sprites/hazards/circular_saw.png", "Circular_Saw.png");
    gs->textures.blue_flame = load_optional_texture(gs,
        "assets/sprites/hazards/blue_flame.png", "blue_flame.png");
    gs->textures.fire_flame = load_optional_texture(gs,
        "assets/sprites/hazards/fire_flame.png", "fire_flame.png");
    gs->textures.faster_fish = load_optional_texture(gs,
        "assets/sprites/entities/faster_fish.png", "Fish_1.png");
    gs->textures.spike = load_optional_texture(gs,
        "assets/sprites/hazards/spike.png", "Spike.png");
    gs->textures.spike_platform = load_optional_texture(gs,
        "assets/sprites/hazards/spike_platform.png", "Spike_Platform.png");

    gs->audio.spring = load_optional_chunk(
        "assets/sounds/surfaces/bouncepad.wav", "bouncepad.wav");
    gs->audio.axe = load_optional_chunk(
        "assets/sounds/hazards/axe_trap.wav", "axe_trap.wav");
    gs->audio.flap = load_optional_chunk(
        "assets/sounds/entities/bird.wav", "flapping.wav");
    gs->audio.spider_attack = load_optional_chunk(
        "assets/sounds/entities/spider.wav", "spider-attack.mp3");
    gs->audio.dive = load_optional_chunk(
        "assets/sounds/entities/fish.wav", "dive.wav");
    gs->audio.jump = load_optional_chunk(
        "assets/sounds/player/player_jump.wav", "jump.wav");
    gs->audio.coin = load_optional_chunk(
        "assets/sounds/collectibles/coin.wav", "coin.wav");
    gs->audio.hit = load_optional_chunk(
        "assets/sounds/player/player_hit.wav", "hit.wav");

    gs->audio.music = NULL;
}

void game_resources_cleanup(GameState *gs)
{
    /* Level-specific resources are applied after core resources; release first. */
    if (gs->audio.music) {
        Mix_HaltMusic();
        Mix_FreeMusic(gs->audio.music);
        gs->audio.music = NULL;
    }

    DESTROY_TEX(gs->textures.ctrl_init_msg);

    water_cleanup(&gs->water);

    DESTROY_TEX(gs->textures.floor_tile);

    parallax_cleanup(&gs->parallax);

    for (int i = 0; i < gs->platform_count; i++) {
        if (gs->platforms[i].tex) {
            SDL_DestroyTexture(gs->platforms[i].tex);
            gs->platforms[i].tex = NULL;
        }
    }

    /* Core audio chunks: reverse order of game_resources_load(). */
    FREE_CHUNK(gs->audio.hit);
    FREE_CHUNK(gs->audio.coin);
    FREE_CHUNK(gs->audio.jump);
    FREE_CHUNK(gs->audio.dive);
    FREE_CHUNK(gs->audio.spider_attack);
    FREE_CHUNK(gs->audio.flap);
    FREE_CHUNK(gs->audio.axe);
    FREE_CHUNK(gs->audio.spring);

    /* Core textures: reverse order of game_resources_load(). */
    DESTROY_TEX(gs->textures.spike_platform);
    DESTROY_TEX(gs->textures.spike);
    DESTROY_TEX(gs->textures.faster_fish);
    DESTROY_TEX(gs->textures.fire_flame);
    DESTROY_TEX(gs->textures.blue_flame);
    DESTROY_TEX(gs->textures.circular_saw);
    DESTROY_TEX(gs->textures.axe_trap);
    DESTROY_TEX(gs->textures.last_star);
    DESTROY_TEX(gs->textures.star_red);
    DESTROY_TEX(gs->textures.star_green);
    DESTROY_TEX(gs->textures.star_yellow);
    DESTROY_TEX(gs->textures.bridge);
    DESTROY_TEX(gs->textures.float_platform);
    DESTROY_TEX(gs->textures.spike_block);
    DESTROY_TEX(gs->textures.rail);
    DESTROY_TEX(gs->textures.bouncepad_high);
    DESTROY_TEX(gs->textures.bouncepad_small);
    DESTROY_TEX(gs->textures.rope);
    DESTROY_TEX(gs->textures.ladder);
    DESTROY_TEX(gs->textures.vine_brown);
    DESTROY_TEX(gs->textures.vine_green);
    DESTROY_TEX(gs->textures.bouncepad_medium);
    DESTROY_TEX(gs->textures.coin);
    DESTROY_TEX(gs->textures.fish);
    DESTROY_TEX(gs->textures.faster_bird);
    DESTROY_TEX(gs->textures.bird);
    DESTROY_TEX(gs->textures.jumping_spider);
    DESTROY_TEX(gs->textures.spider);
    DESTROY_TEX(gs->textures.platform);
}
