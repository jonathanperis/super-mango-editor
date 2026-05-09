/*
 * level_resources.c — Apply LevelDef-driven runtime resources.
 */

#include "level_resources.h"

#include <SDL_image.h>
#include <SDL_mixer.h>
#include <stdio.h>

#include "../effects/fog.h"
#include "../effects/parallax.h"
#include "../effects/water.h"

void level_resources_apply(GameState *gs, const LevelDef *def)
{
    if (!gs || !def) return;

    parallax_cleanup(&gs->parallax);
    if (def->background_layer_count > 0) {
        char  paths[MAX_BACKGROUND_LAYERS][64] = {{0}};
        float speeds[MAX_BACKGROUND_LAYERS] = {0.0f};
        int   n = def->background_layer_count;

        if (n > MAX_BACKGROUND_LAYERS) n = MAX_BACKGROUND_LAYERS;
        for (int i = 0; i < n; i++) {
            SDL_strlcpy(paths[i], def->background_layers[i].path, 64);
            speeds[i] = def->background_layers[i].speed;
        }
        parallax_init_from_def(&gs->parallax, gs->renderer,
                               (const char (*)[64])paths, speeds, n);
    } else {
        parallax_init(&gs->parallax, gs->renderer);
    }

    {
        const char *floor_path = def->floor_tile_path[0] != '\0'
                               ? def->floor_tile_path
                               : "assets/sprites/levels/grass_tileset.png";
        SDL_Texture *new_floor = IMG_LoadTexture(gs->renderer, floor_path);
        if (!new_floor) {
            fprintf(stderr, "Warning: failed to load floor tile %s: %s\n",
                    floor_path, IMG_GetError());
            new_floor = IMG_LoadTexture(gs->renderer,
                                        "assets/sprites/levels/grass_tileset.png");
        }
        if (new_floor) {
            if (gs->textures.floor_tile) SDL_DestroyTexture(gs->textures.floor_tile);
            gs->textures.floor_tile = new_floor;
        }
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
        water_reload_texture(&gs->water, gs->renderer, strip);
    }

    fog_cleanup(&gs->fog);
    if (def->fog_layer_count > 0) {
        char fog_paths[MAX_FOG_TEXTURES][64] = {{0}};
        int  n = def->fog_layer_count;

        if (n > MAX_FOG_TEXTURES) n = MAX_FOG_TEXTURES;
        for (int i = 0; i < n; i++) {
            SDL_strlcpy(fog_paths[i], def->fog_layers[i].path, 64);
        }
        fog_init(&gs->fog, gs->renderer, (const char (*)[64])fog_paths, n);
    }

    if (gs->audio.music) {
        Mix_HaltMusic();
        Mix_FreeMusic(gs->audio.music);
        gs->audio.music = NULL;
    }
    if (def->music_path[0] != '\0') {
        gs->audio.music = Mix_LoadMUS(def->music_path);
        if (!gs->audio.music) {
            fprintf(stderr, "Warning: failed to load %s: %s\n",
                    def->music_path, Mix_GetError());
        } else {
            Mix_PlayMusic(gs->audio.music, -1);
            Mix_VolumeMusic(def->music_volume > 0 ? def->music_volume : 13);
        }
    }
}
