/*
 * level_resources.c — Get and release the files a level names.
 *
 * Background layers, the floor tileset, the water strip, fog layers and
 * music all come from the LevelDef, so they belong to gs->world and change
 * with the level: level_resources_apply swaps them in when a level is
 * committed, level_resources_cleanup releases them when the game closes.
 * The sprites every level shares are in gs->assets instead (game_resources.c).
 *
 * Every file here is acquired from gs->cache, the session's asset cache
 * (shared/asset_cache.h), and released to it. Releasing does not unload: a
 * Replay, or a next level with the same scenery, acquires the same paths a
 * moment later and reads nothing from disk. game_level_commit trims the
 * cache once the new level holds everything it needs.
 */

#include "level_resources.h"
#include "../game.h"  /* GameState: this file reads its fields */
#include "level_loader.h"        /* level_release_platform_tiles */
#include "../core/game_resources.h"  /* DEFAULT_FLOOR_TILE_PATH */
#include "../shared/platform.h"  /* str_copy */

#include <stdio.h>
#include <string.h>   /* strcmp */

#include "../effects/fog.h"
#include "../effects/parallax.h"
#include "../effects/water.h"

/*
 * The most cache entries one game can hold: every background and fog layer,
 * a different tile image on every platform, a floor tileset, the default and
 * the level's water strip, the player sheet, the HUD coin icon, the music
 * and the font. The cache must have room for all of them at once.
 */
_Static_assert(ASSET_CACHE_MAX >= MAX_BACKGROUND_LAYERS + MAX_FOG_TEXTURES + MAX_PLATFORMS + 7,
               "the asset cache must hold everything one game can acquire");

void level_resources_apply(GameState *gs, const LevelDef *def)
{
    if (!gs || !def) return;

    AssetCache *cache = gs->cache;

    parallax_cleanup(&gs->world.parallax, cache);
    if (def->background_layer_count > 0) {
        char  paths[MAX_BACKGROUND_LAYERS][64] = {{0}};
        float speeds[MAX_BACKGROUND_LAYERS] = {0.0f};
        int   n = def->background_layer_count;

        if (n > MAX_BACKGROUND_LAYERS) n = MAX_BACKGROUND_LAYERS;
        for (int i = 0; i < n; i++) {
            str_copy(paths[i], def->background_layers[i].path, 64);
            speeds[i] = def->background_layers[i].speed;
        }
        parallax_init_from_def(&gs->world.parallax, cache,
                               (const char (*)[64])paths, speeds, n);
    } else {
        parallax_init(&gs->world.parallax, cache);
    }

    /*
     * Floor tileset. A level that names its own file holds it in
     * gs->world.floor_tile; NULL means "draw the shared default", the grass
     * tileset in gs->assets (also the fallback when the named file fails
     * to load), so most levels decode no floor image at all.
     */
    asset_cache_texture_release(cache, gs->world.floor_tile);
    gs->world.floor_tile = NULL;
    if (def->floor_tile_path[0] != '\0' &&
        strcmp(def->floor_tile_path, DEFAULT_FLOOR_TILE_PATH) != 0) {
        gs->world.floor_tile = asset_cache_texture_acquire(cache, def->floor_tile_path);
        if (!gs->world.floor_tile)
            fprintf(stderr, "Warning: failed to load floor tile %s\n", def->floor_tile_path);
    }

    {
        const char *strip = "assets/sprites/foregrounds/water.png";
        int n = def->foreground_layer_count;
        if (n > MAX_BACKGROUND_LAYERS) n = MAX_BACKGROUND_LAYERS;
        if (n > 0) {
            const char *level_strip =
                def->foreground_layers[n - 1].path;
            if (level_strip[0] != '\0') strip = level_strip;
        }
        water_reload_texture(&gs->world.water, cache, strip);
    }

    fog_cleanup(&gs->world.fog, cache);
    if (def->fog_layer_count > 0) {
        char fog_paths[MAX_FOG_TEXTURES][64] = {{0}};
        int  n = def->fog_layer_count;

        if (n > MAX_FOG_TEXTURES) n = MAX_FOG_TEXTURES;
        for (int i = 0; i < n; i++) {
            str_copy(fog_paths[i], def->fog_layers[i].path, 64);
        }
        fog_init(&gs->world.fog, cache, (const char (*)[64])fog_paths, n);
    }

    /*
     * Music. Releasing the old track stops and rewinds it (it has no other
     * holder), so when the new level names the same file, music_play below
     * starts it from the beginning, as loading it again used to.
     */
    asset_cache_music_release(cache, gs->world.music);
    gs->world.music = NULL;
    if (def->music_path[0] != '\0') {
        gs->world.music = asset_cache_music_acquire(cache, def->music_path);
        if (!gs->world.music) {
            fprintf(stderr, "Warning: failed to load %s\n", def->music_path);
        } else {
            music_play(gs->world.music);
            music_set_volume(def->music_volume); /* zero is an authored mute */
        }
    }
}

/*
 * level_resources_cleanup — Release every file the level acquired, plus the
 * water strip game_init acquired, in reverse order of level_resources_apply.
 * Each helper clears the pointers it releases, so a half-started game is
 * safe too, and so is a second call.
 */
void level_resources_cleanup(GameState *gs)
{
    AssetCache *cache = gs->cache;

    asset_cache_music_release(cache, gs->world.music);  /* also stops it */
    gs->world.music = NULL;
    fog_cleanup(&gs->world.fog, cache);
    water_cleanup(&gs->world.water, cache);
    asset_cache_texture_release(cache, gs->world.floor_tile);
    gs->world.floor_tile = NULL;
    /* Each distinct tile image is held once, however many platforms use it. */
    level_release_platform_tiles(gs);
    parallax_cleanup(&gs->world.parallax, cache);
}
