/*
 * game_resources.c — Load and release renderer/audio resources owned by GameState.
 */

#include "game_resources.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "../effects/water.h"
#include "../effects/parallax.h"

#define ARRAY_LEN(arr) ((int)(sizeof(arr) / sizeof((arr)[0])))
#define TEX_FIELD(field) offsetof(TextureResources, field)
#define CHUNK_FIELD(field) offsetof(AudioResources, field)

typedef struct {
    size_t      offset;
    const char *path;
    const char *label;
} TextureLoadSpec;

typedef struct {
    size_t      offset;
    const char *path;
    const char *label;
} ChunkLoadSpec;

static const TextureLoadSpec s_boot_textures[] = {
    { TEX_FIELD(floor_tile), "assets/sprites/levels/grass_tileset.png",
      "Failed to load Grass_Tileset.png" },
    { TEX_FIELD(platform), "assets/sprites/levels/grass_platform.png",
      "Failed to load Grass_Oneway.png" }
};

static const TextureLoadSpec s_required_textures[] = {
    { TEX_FIELD(spider), "assets/sprites/entities/spider.png",
      "Failed to load Spider_1.png" },
    { TEX_FIELD(jumping_spider), "assets/sprites/entities/jumping_spider.png",
      "Failed to load Spider_2.png" },
    { TEX_FIELD(bird), "assets/sprites/entities/bird.png",
      "Failed to load Bird_2.png" },
    { TEX_FIELD(faster_bird), "assets/sprites/entities/faster_bird.png",
      "Failed to load Bird_1.png" },
    { TEX_FIELD(fish), "assets/sprites/entities/fish.png",
      "Failed to load Fish_2.png" },
    { TEX_FIELD(coin), "assets/sprites/collectibles/coin.png",
      "Failed to load Coin.png" },
    { TEX_FIELD(bouncepad_medium), "assets/sprites/surfaces/bouncepad_medium.png",
      "Failed to load Bouncepad_Wood.png" }
};

static const TextureLoadSpec s_optional_textures[] = {
    { TEX_FIELD(vine_green), "assets/sprites/surfaces/vine_green.png",
      "Vine_Green.png" },
    { TEX_FIELD(vine_brown), "assets/sprites/surfaces/vine_brown.png",
      "Vine_Brown.png" },
    { TEX_FIELD(ladder), "assets/sprites/surfaces/ladder.png", "Ladder.png" },
    { TEX_FIELD(rope), "assets/sprites/surfaces/rope.png", "Rope.png" },
    { TEX_FIELD(bouncepad_small), "assets/sprites/surfaces/bouncepad_small.png",
      "Bouncepad_Green.png" },
    { TEX_FIELD(bouncepad_high), "assets/sprites/surfaces/bouncepad_high.png",
      "Bouncepad_Red.png" },
    { TEX_FIELD(rail), "assets/sprites/surfaces/rail.png", "Rails.png" },
    { TEX_FIELD(spike_block), "assets/sprites/hazards/spike_block.png",
      "Spike_Block.png" },
    { TEX_FIELD(float_platform), "assets/sprites/surfaces/float_platform.png",
      "Platform.png" },
    { TEX_FIELD(bridge), "assets/sprites/surfaces/bridge.png", "Bridge.png" },
    { TEX_FIELD(star_yellow), "assets/sprites/collectibles/star_yellow.png",
      "star_yellow.png" },
    { TEX_FIELD(star_green), "assets/sprites/collectibles/star_green.png",
      "star_green.png" },
    { TEX_FIELD(star_red), "assets/sprites/collectibles/star_red.png", "star_red.png" },
    { TEX_FIELD(last_star), "assets/sprites/collectibles/last_star.png",
      "last_star.png" },
    { TEX_FIELD(axe_trap), "assets/sprites/hazards/axe_trap.png", "Axe_Trap.png" },
    { TEX_FIELD(circular_saw), "assets/sprites/hazards/circular_saw.png",
      "Circular_Saw.png" },
    { TEX_FIELD(blue_flame), "assets/sprites/hazards/blue_flame.png",
      "blue_flame.png" },
    { TEX_FIELD(fire_flame), "assets/sprites/hazards/fire_flame.png",
      "fire_flame.png" },
    { TEX_FIELD(faster_fish), "assets/sprites/entities/faster_fish.png", "Fish_1.png" },
    { TEX_FIELD(spike), "assets/sprites/hazards/spike.png", "Spike.png" },
    { TEX_FIELD(spike_platform), "assets/sprites/hazards/spike_platform.png",
      "Spike_Platform.png" }
};

static const ChunkLoadSpec s_optional_chunks[] = {
    { CHUNK_FIELD(spring), "assets/sounds/surfaces/bouncepad.wav", "bouncepad.wav" },
    { CHUNK_FIELD(axe), "assets/sounds/hazards/axe_trap.wav", "axe_trap.wav" },
    { CHUNK_FIELD(flap), "assets/sounds/entities/bird.wav", "flapping.wav" },
    { CHUNK_FIELD(spider_attack), "assets/sounds/entities/spider.wav",
      "spider-attack.mp3" },
    { CHUNK_FIELD(dive), "assets/sounds/entities/fish.wav", "dive.wav" },
    { CHUNK_FIELD(jump), "assets/sounds/player/player_jump.wav", "jump.wav" },
    { CHUNK_FIELD(coin), "assets/sounds/collectibles/coin.wav", "coin.wav" },
    { CHUNK_FIELD(hit), "assets/sounds/player/player_hit.wav", "hit.wav" }
};

static Texture2D **texture_slot(GameState *gs, size_t offset)
{
    return (Texture2D **)((char *)&gs->textures + offset);
}

static SoundEffect **chunk_slot(GameState *gs, size_t offset)
{
    return (SoundEffect **)((char *)&gs->audio + offset);
}

static int game_resources_fail(const char *label, const char *detail)
{
    fprintf(stderr, "%s: %s\n", label, detail);
    return -1;
}

static Texture2D *load_required_texture(const char *path,
                                          const char *label)
{
    Texture2D *tex = texture_load(path);
    if (!tex) game_resources_fail(label, path);
    return tex;
}

static Texture2D *load_optional_texture(const char *path,
                                          const char *label)
{
    Texture2D *tex = texture_load(path);
    if (!tex) {
        fprintf(stderr, "Warning: Failed to load %s: %s\n", label, path);
    }
    return tex;
}

static SoundEffect *load_optional_chunk(const char *path, const char *label)
{
    SoundEffect *chunk = sound_load(path);
    if (!chunk) {
        fprintf(stderr, "Warning: Failed to load %s: %s\n", label, path);
    }
    return chunk;
}

static int load_required_texture_specs(GameState *gs,
                                       const TextureLoadSpec *specs, int count)
{
    for (int i = 0; i < count; i++) {
        Texture2D *tex = load_required_texture(specs[i].path,
                                                 specs[i].label);
        if (!tex) return -1;
        *texture_slot(gs, specs[i].offset) = tex;
    }
    return 0;
}

static void load_optional_texture_specs(GameState *gs,
                                        const TextureLoadSpec *specs, int count)
{
    for (int i = 0; i < count; i++) {
        *texture_slot(gs, specs[i].offset) =
            load_optional_texture(specs[i].path, specs[i].label);
    }
}

static void load_optional_chunk_specs(GameState *gs, const ChunkLoadSpec *specs,
                                      int count)
{
    for (int i = 0; i < count; i++) {
        *chunk_slot(gs, specs[i].offset) = load_optional_chunk(specs[i].path,
                                                               specs[i].label);
    }
}

static void destroy_texture_specs_reverse(GameState *gs,
                                          const TextureLoadSpec *specs,
                                          int count)
{
    for (int i = count - 1; i >= 0; i--) {
        Texture2D **slot = texture_slot(gs, specs[i].offset);
        DESTROY_TEX(*slot);
    }
}

static void free_chunk_specs_reverse(GameState *gs, const ChunkLoadSpec *specs,
                                     int count)
{
    for (int i = count - 1; i >= 0; i--) {
        SoundEffect **slot = chunk_slot(gs, specs[i].offset);
        FREE_CHUNK(*slot);
    }
}

static void game_resources_reset_owned_slots(GameState *gs)
{
    memset(&gs->textures, 0, sizeof(gs->textures));
    memset(&gs->audio, 0, sizeof(gs->audio));
    gs->water.texture = NULL;
    gs->water.scroll_x = 0.0f;
}

int game_resources_load(GameState *gs)
{
    game_resources_reset_owned_slots(gs);

    if (load_required_texture_specs(gs, s_boot_textures,
                                    ARRAY_LEN(s_boot_textures)) != 0) {
        return -1;
    }

    if (water_init(&gs->water) != 0) return -1;

    if (load_required_texture_specs(gs, s_required_textures,
                                    ARRAY_LEN(s_required_textures)) != 0) {
        return -1;
    }
    load_optional_texture_specs(gs, s_optional_textures,
                                ARRAY_LEN(s_optional_textures));
    load_optional_chunk_specs(gs, s_optional_chunks, ARRAY_LEN(s_optional_chunks));

    return 0;
}

void game_resources_cleanup(GameState *gs)
{
    /* Stop every effect channel before freeing any chunk it may reference. */
    sound_stop_all();

    /* Level-specific resources are applied after core resources; release first. */
    if (gs->audio.music) {
        music_unload(gs->audio.music);
        gs->audio.music = NULL;
    }

    parallax_cleanup(&gs->parallax);

    for (int i = 0; i < gs->platform_count; i++) {
        if (gs->platforms[i].tex) {
            texture_unload(gs->platforms[i].tex);
            gs->platforms[i].tex = NULL;
        }
    }

    /* Core audio chunks: reverse order of game_resources_load(). */
    free_chunk_specs_reverse(gs, s_optional_chunks, ARRAY_LEN(s_optional_chunks));

    /* Core textures: reverse order of game_resources_load(). */
    destroy_texture_specs_reverse(gs, s_optional_textures,
                                  ARRAY_LEN(s_optional_textures));
    destroy_texture_specs_reverse(gs, s_required_textures,
                                  ARRAY_LEN(s_required_textures));

    water_cleanup(&gs->water);

    destroy_texture_specs_reverse(gs, s_boot_textures, ARRAY_LEN(s_boot_textures));
}

static int missing_level_texture(size_t offset)
{
    for (int i = 0; i < ARRAY_LEN(s_optional_textures); i++) {
        if (s_optional_textures[i].offset == offset) {
            fprintf(stderr, "Required gameplay texture unavailable: %s\n",
                    s_optional_textures[i].path);
            break;
        }
    }
    return -1;
}

int game_resources_require_level_textures(const GameState *gs, const LevelDef *def)
{
#define REQUIRE(member, used) \
    do { if ((used) && !gs->textures.member) return missing_level_texture(TEX_FIELD(member)); } while (0)
    REQUIRE(last_star, 1);
    REQUIRE(star_yellow, 1); /* Also used by the HUD. */
    REQUIRE(star_green, def->star_green_count);
    REQUIRE(star_red, def->star_red_count);
    REQUIRE(faster_fish, def->faster_fish_count);
    REQUIRE(spike, def->spike_row_count);
    REQUIRE(spike_platform, def->spike_platform_count);
    REQUIRE(spike_block, def->spike_block_count);
    REQUIRE(circular_saw, def->circular_saw_count);
    REQUIRE(axe_trap, def->axe_trap_count);
    REQUIRE(blue_flame, def->blue_flame_count);
    REQUIRE(fire_flame, def->fire_flame_count);
    REQUIRE(float_platform, def->float_platform_count);
    REQUIRE(bridge, def->bridge_count);
    REQUIRE(bouncepad_small, def->bouncepad_small_count);
    REQUIRE(bouncepad_high, def->bouncepad_high_count);
    REQUIRE(ladder, def->ladder_count);
    REQUIRE(rope, def->rope_count);
    REQUIRE(rail, def->rail_count);
    for (int i = 0; i < def->vine_count; i++) {
        REQUIRE(vine_green, def->vines[i].vine_type == 0);
        REQUIRE(vine_brown, def->vines[i].vine_type != 0);
    }
#undef REQUIRE
    return 0;
}
