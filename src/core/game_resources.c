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

#define ARRAY_LEN(arr) ((int)(sizeof(arr) / sizeof((arr)[0])))

typedef struct {
    SDL_Texture **slot;
    const char  *path;
    const char  *label;
} TextureLoadSpec;

typedef struct {
    Mix_Chunk  **slot;
    const char  *path;
    const char  *label;
} ChunkLoadSpec;

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

static void load_required_texture_specs(GameState *gs,
                                        const TextureLoadSpec *specs, int count)
{
    for (int i = 0; i < count; i++) {
        *specs[i].slot = load_required_texture(gs, specs[i].path, specs[i].label);
    }
}

static void load_optional_texture_specs(GameState *gs,
                                        const TextureLoadSpec *specs, int count)
{
    for (int i = 0; i < count; i++) {
        *specs[i].slot = load_optional_texture(gs, specs[i].path, specs[i].label);
    }
}

static void load_optional_chunk_specs(const ChunkLoadSpec *specs, int count)
{
    for (int i = 0; i < count; i++) {
        *specs[i].slot = load_optional_chunk(specs[i].path, specs[i].label);
    }
}

void game_resources_load(GameState *gs)
{
    const TextureLoadSpec boot_textures[] = {
        { &gs->textures.floor_tile, "assets/sprites/levels/grass_tileset.png",
          "Failed to load Grass_Tileset.png" },
        { &gs->textures.platform, "assets/sprites/levels/grass_platform.png",
          "Failed to load Grass_Oneway.png" }
    };

    load_required_texture_specs(gs, boot_textures, ARRAY_LEN(boot_textures));

    water_init(&gs->water, gs->renderer);

    const TextureLoadSpec required_textures[] = {
        { &gs->textures.spider, "assets/sprites/entities/spider.png",
          "Failed to load Spider_1.png" },
        { &gs->textures.jumping_spider, "assets/sprites/entities/jumping_spider.png",
          "Failed to load Spider_2.png" },
        { &gs->textures.bird, "assets/sprites/entities/bird.png",
          "Failed to load Bird_2.png" },
        { &gs->textures.faster_bird, "assets/sprites/entities/faster_bird.png",
          "Failed to load Bird_1.png" },
        { &gs->textures.fish, "assets/sprites/entities/fish.png",
          "Failed to load Fish_2.png" },
        { &gs->textures.coin, "assets/sprites/collectibles/coin.png",
          "Failed to load Coin.png" },
        { &gs->textures.bouncepad_medium,
          "assets/sprites/surfaces/bouncepad_medium.png",
          "Failed to load Bouncepad_Wood.png" }
    };
    const TextureLoadSpec optional_textures[] = {
        { &gs->textures.vine_green, "assets/sprites/surfaces/vine_green.png",
          "Vine_Green.png" },
        { &gs->textures.vine_brown, "assets/sprites/surfaces/vine_brown.png",
          "Vine_Brown.png" },
        { &gs->textures.ladder, "assets/sprites/surfaces/ladder.png", "Ladder.png" },
        { &gs->textures.rope, "assets/sprites/surfaces/rope.png", "Rope.png" },
        { &gs->textures.bouncepad_small, "assets/sprites/surfaces/bouncepad_small.png",
          "Bouncepad_Green.png" },
        { &gs->textures.bouncepad_high, "assets/sprites/surfaces/bouncepad_high.png",
          "Bouncepad_Red.png" },
        { &gs->textures.rail, "assets/sprites/surfaces/rail.png", "Rails.png" },
        { &gs->textures.spike_block, "assets/sprites/hazards/spike_block.png",
          "Spike_Block.png" },
        { &gs->textures.float_platform, "assets/sprites/surfaces/float_platform.png",
          "Platform.png" },
        { &gs->textures.bridge, "assets/sprites/surfaces/bridge.png", "Bridge.png" },
        { &gs->textures.star_yellow, "assets/sprites/collectibles/star_yellow.png",
          "star_yellow.png" },
        { &gs->textures.star_green, "assets/sprites/collectibles/star_green.png",
          "star_green.png" },
        { &gs->textures.star_red, "assets/sprites/collectibles/star_red.png",
          "star_red.png" },
        { &gs->textures.last_star, "assets/sprites/collectibles/last_star.png",
          "last_star.png" },
        { &gs->textures.axe_trap, "assets/sprites/hazards/axe_trap.png",
          "Axe_Trap.png" },
        { &gs->textures.circular_saw, "assets/sprites/hazards/circular_saw.png",
          "Circular_Saw.png" },
        { &gs->textures.blue_flame, "assets/sprites/hazards/blue_flame.png",
          "blue_flame.png" },
        { &gs->textures.fire_flame, "assets/sprites/hazards/fire_flame.png",
          "fire_flame.png" },
        { &gs->textures.faster_fish, "assets/sprites/entities/faster_fish.png",
          "Fish_1.png" },
        { &gs->textures.spike, "assets/sprites/hazards/spike.png", "Spike.png" },
        { &gs->textures.spike_platform, "assets/sprites/hazards/spike_platform.png",
          "Spike_Platform.png" }
    };
    const ChunkLoadSpec optional_chunks[] = {
        { &gs->audio.spring, "assets/sounds/surfaces/bouncepad.wav", "bouncepad.wav" },
        { &gs->audio.axe, "assets/sounds/hazards/axe_trap.wav", "axe_trap.wav" },
        { &gs->audio.flap, "assets/sounds/entities/bird.wav", "flapping.wav" },
        { &gs->audio.spider_attack, "assets/sounds/entities/spider.wav",
          "spider-attack.mp3" },
        { &gs->audio.dive, "assets/sounds/entities/fish.wav", "dive.wav" },
        { &gs->audio.jump, "assets/sounds/player/player_jump.wav", "jump.wav" },
        { &gs->audio.coin, "assets/sounds/collectibles/coin.wav", "coin.wav" },
        { &gs->audio.hit, "assets/sounds/player/player_hit.wav", "hit.wav" }
    };

    load_required_texture_specs(gs, required_textures, ARRAY_LEN(required_textures));
    load_optional_texture_specs(gs, optional_textures, ARRAY_LEN(optional_textures));
    load_optional_chunk_specs(optional_chunks, ARRAY_LEN(optional_chunks));

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

    DESTROY_TEX(gs->textures.platform);
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
}
