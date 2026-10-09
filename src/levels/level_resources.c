/*
 * level_resources.c — Load and release the files a level names.
 *
 * Background layers, the floor tileset, the water strip, fog layers and
 * music all come from the LevelDef, so they belong to gs->world and change
 * with the level: level_resources_apply swaps them in when a level is
 * committed, level_resources_cleanup releases them when the game closes.
 * The sprites every level shares are in gs->assets instead (game_resources.c).
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

void level_resources_apply(GameState *gs, const LevelDef *def)
{
    if (!gs || !def) return;

    parallax_cleanup(&gs->world.parallax);
    if (def->background_layer_count > 0) {
        char  paths[MAX_BACKGROUND_LAYERS][64] = {{0}};
        float speeds[MAX_BACKGROUND_LAYERS] = {0.0f};
        int   n = def->background_layer_count;

        if (n > MAX_BACKGROUND_LAYERS) n = MAX_BACKGROUND_LAYERS;
        for (int i = 0; i < n; i++) {
            str_copy(paths[i], def->background_layers[i].path, 64);
            speeds[i] = def->background_layers[i].speed;
        }
        parallax_init_from_def(&gs->world.parallax,
                               (const char (*)[64])paths, speeds, n);
    } else {
        parallax_init(&gs->world.parallax);
    }

    /*
     * Floor tileset. A level that names its own file gets its own copy in
     * gs->world.floor_tile; NULL means "draw the shared default", the grass
     * tileset in gs->assets (also the fallback when the named file fails
     * to load), so most levels decode no floor image at all.
     */
    texture_unload(gs->world.floor_tile);
    gs->world.floor_tile = NULL;
    if (def->floor_tile_path[0] != '\0' &&
        strcmp(def->floor_tile_path, DEFAULT_FLOOR_TILE_PATH) != 0) {
        gs->world.floor_tile = texture_load(def->floor_tile_path);
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
        water_reload_texture(&gs->world.water, strip);
    }

    fog_cleanup(&gs->world.fog);
    if (def->fog_layer_count > 0) {
        char fog_paths[MAX_FOG_TEXTURES][64] = {{0}};
        int  n = def->fog_layer_count;

        if (n > MAX_FOG_TEXTURES) n = MAX_FOG_TEXTURES;
        for (int i = 0; i < n; i++) {
            str_copy(fog_paths[i], def->fog_layers[i].path, 64);
        }
        fog_init(&gs->world.fog, (const char (*)[64])fog_paths, n);
    }

    if (gs->world.music) {
        music_unload(gs->world.music);
        gs->world.music = NULL;
    }
    if (def->music_path[0] != '\0') {
        gs->world.music = music_load(def->music_path);
        if (!gs->world.music) {
            fprintf(stderr, "Warning: failed to load %s\n", def->music_path);
        } else {
            music_play(gs->world.music);
            music_set_volume(def->music_volume); /* zero is an authored mute */
        }
    }
}

/*
 * level_resources_cleanup — Release every file the level loaded, plus the
 * water strip game_init loaded, in reverse order of level_resources_apply.
 * Each helper clears what it frees, so a half-started game is safe too.
 */
void level_resources_cleanup(GameState *gs)
{
    if (gs->world.music) {
        music_unload(gs->world.music);
        gs->world.music = NULL;
    }
    fog_cleanup(&gs->world.fog);
    water_cleanup(&gs->world.water);
    texture_unload(gs->world.floor_tile);
    gs->world.floor_tile = NULL;
    /* Platform tiles are shared per path; the cache unloads each once. */
    level_release_platform_tiles(gs);
    parallax_cleanup(&gs->world.parallax);
}
