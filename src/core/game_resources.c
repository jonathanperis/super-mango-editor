/*
 * game_resources.c — Load and release the shared sprites and sounds (GameAssets).
 *
 * Three tables below list every shared texture and sound effect: where it
 * goes in GameAssets (a byte offset, see TEX_FIELD), its file and a label
 * for messages. game_resources_load walks them forwards; game_resources_unload
 * walks them backwards. AppSession calls both once per session; game_init
 * calls them only for a GameState started without a session.
 */

#include "game_resources.h"
#include "../game.h"  /* GameState: this file reads its fields */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

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
    { TEX_FIELD(floor_tile), DEFAULT_FLOOR_TILE_PATH,
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
    { CHUNK_FIELD(flap), "assets/sounds/entities/bird.wav", "bird.wav" },
    { CHUNK_FIELD(spider_attack), "assets/sounds/entities/spider.wav", "spider.wav" },
    { CHUNK_FIELD(dive), "assets/sounds/entities/fish.wav", "dive.wav" },
    { CHUNK_FIELD(jump), "assets/sounds/player/player_jump.wav", "jump.wav" },
    { CHUNK_FIELD(coin), "assets/sounds/collectibles/coin.wav", "coin.wav" },
    { CHUNK_FIELD(hit), "assets/sounds/player/player_hit.wav", "hit.wav" }
};

static Texture2D **texture_slot(GameAssets *assets, size_t offset)
{
    return (Texture2D **)((char *)&assets->textures + offset);
}

/* texture_slot for reading only. */
static Texture2D *const *texture_slot_read(const GameAssets *assets, size_t offset)
{
    return (Texture2D *const *)((const char *)&assets->textures + offset);
}

static SoundEffect **chunk_slot(GameAssets *assets, size_t offset)
{
    return (SoundEffect **)((char *)&assets->audio + offset);
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

static int load_required_texture_specs(GameAssets *assets,
                                       const TextureLoadSpec *specs, int count)
{
    for (int i = 0; i < count; i++) {
        Texture2D *tex = load_required_texture(specs[i].path,
                                                 specs[i].label);
        if (!tex) return -1;
        *texture_slot(assets, specs[i].offset) = tex;
    }
    return 0;
}

static void load_optional_texture_specs(GameAssets *assets,
                                        const TextureLoadSpec *specs, int count)
{
    for (int i = 0; i < count; i++) {
        *texture_slot(assets, specs[i].offset) =
            load_optional_texture(specs[i].path, specs[i].label);
    }
}

static void load_optional_chunk_specs(GameAssets *assets, const ChunkLoadSpec *specs,
                                      int count)
{
    for (int i = 0; i < count; i++) {
        *chunk_slot(assets, specs[i].offset) = load_optional_chunk(specs[i].path,
                                                               specs[i].label);
    }
}

static void destroy_texture_specs_reverse(GameAssets *assets,
                                          const TextureLoadSpec *specs,
                                          int count)
{
    for (int i = count - 1; i >= 0; i--) {
        Texture2D **slot = texture_slot(assets, specs[i].offset);
        DESTROY_TEX(*slot);
    }
}

static void free_chunk_specs_reverse(GameAssets *assets, const ChunkLoadSpec *specs,
                                     int count)
{
    for (int i = count - 1; i >= 0; i--) {
        SoundEffect **slot = chunk_slot(assets, specs[i].offset);
        FREE_CHUNK(*slot);
    }
}

int game_resources_load(GameAssets *assets)
{
    /* Start from empty slots, so game_resources_unload after a failure
     * below frees exactly what was loaded. */
    memset(assets, 0, sizeof(*assets));

    if (load_required_texture_specs(assets, s_boot_textures,
                                    ARRAY_LEN(s_boot_textures)) != 0) {
        return -1;
    }
    if (load_required_texture_specs(assets, s_required_textures,
                                    ARRAY_LEN(s_required_textures)) != 0) {
        return -1;
    }
    load_optional_texture_specs(assets, s_optional_textures,
                                ARRAY_LEN(s_optional_textures));
    load_optional_chunk_specs(assets, s_optional_chunks, ARRAY_LEN(s_optional_chunks));

    return 0;
}

void game_resources_unload(GameAssets *assets)
{
    /* Stop every effect channel before freeing any chunk it may reference. */
    sound_stop_all();

    /* Sound effects, then textures: reverse order of game_resources_load(). */
    free_chunk_specs_reverse(assets, s_optional_chunks, ARRAY_LEN(s_optional_chunks));
    destroy_texture_specs_reverse(assets, s_optional_textures,
                                  ARRAY_LEN(s_optional_textures));
    destroy_texture_specs_reverse(assets, s_required_textures,
                                  ARRAY_LEN(s_required_textures));
    destroy_texture_specs_reverse(assets, s_boot_textures, ARRAY_LEN(s_boot_textures));
}

int game_resources_loaded(const GameAssets *assets)
{
    return assets->textures.floor_tile != NULL;
}

const char *game_resources_missing_required(const GameAssets *assets)
{
    /* game_resources_load stops at the first required sprite that fails
     * and leaves the slots after it empty, so the first empty slot, in
     * load order, is the file that was missing. */
    for (int i = 0; i < ARRAY_LEN(s_boot_textures); i++)
        if (!*texture_slot_read(assets, s_boot_textures[i].offset)) return s_boot_textures[i].path;
    for (int i = 0; i < ARRAY_LEN(s_required_textures); i++)
        if (!*texture_slot_read(assets, s_required_textures[i].offset)) return s_required_textures[i].path;
    return NULL;
}

void game_resources_reload_missing(GameAssets *assets)
{
    /* Only the optional slots can be empty in a loaded set: a missing
     * required sprite fails the whole load instead. */
    for (int i = 0; i < ARRAY_LEN(s_optional_textures); i++) {
        Texture2D **slot = texture_slot(assets, s_optional_textures[i].offset);
        if (!*slot) *slot = load_optional_texture(s_optional_textures[i].path, s_optional_textures[i].label);
    }
    for (int i = 0; i < ARRAY_LEN(s_optional_chunks); i++) {
        SoundEffect **slot = chunk_slot(assets, s_optional_chunks[i].offset);
        if (!*slot) *slot = load_optional_chunk(s_optional_chunks[i].path, s_optional_chunks[i].label);
    }
}

/* The file of the optional texture stored at offset. */
static const char *optional_texture_path(size_t offset)
{
    for (int i = 0; i < ARRAY_LEN(s_optional_textures); i++)
        if (s_optional_textures[i].offset == offset) return s_optional_textures[i].path;
    return "an unknown sprite";
}

const char *game_resources_missing_level_texture(const GameAssets *assets, const LevelDef *def)
{
#define REQUIRE(member, used) \
    do { if ((used) && !assets->textures.member) return optional_texture_path(TEX_FIELD(member)); } while (0)
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
    return NULL;
}

int game_resources_require_level_textures(const GameState *gs, const LevelDef *def)
{
    const char *missing = game_resources_missing_level_texture(&gs->assets, def);
    if (!missing) return 0;
    fprintf(stderr, "Required gameplay texture unavailable: %s\n", missing);
    return -1;
}
