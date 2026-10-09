/*
 * game_assets.h — GameAssets: the sprites and sound effects every level uses.
 *
 * game.h embeds a GameAssets in GameState, and AppSession owns the loaded
 * one; both include this small header. It needs only the texture and sound
 * types, so a file that works with assets alone (game_resources.c,
 * app_session.h) does not pull in every entity header through game.h.
 * game_resources.c loads and unloads these slots from its tables.
 */
#pragma once

#include "shared/graphics.h"   /* Texture2D */
#include "shared/audio.h"      /* SoundEffect */

/*
 * TextureResources — one slot per shared sprite sheet. game_resources.c
 * loads them all from its tables; renderers borrow them.
 */
typedef struct {
    Texture2D *floor_tile;      /* default floor tileset, for levels naming none */
    Texture2D *platform;
    Texture2D *spider;
    Texture2D *jumping_spider;
    Texture2D *bird;
    Texture2D *faster_bird;
    Texture2D *fish;
    Texture2D *faster_fish;
    Texture2D *coin;
    Texture2D *vine_green;
    Texture2D *vine_brown;
    Texture2D *ladder;
    Texture2D *rope;
    Texture2D *bouncepad_medium;
    Texture2D *bouncepad_small;
    Texture2D *bouncepad_high;
    Texture2D *rail;
    Texture2D *spike_block;
    Texture2D *float_platform;
    Texture2D *bridge;
    Texture2D *star_yellow;
    Texture2D *star_green;
    Texture2D *star_red;
    Texture2D *last_star;
    Texture2D *axe_trap;
    Texture2D *circular_saw;
    Texture2D *blue_flame;
    Texture2D *fire_flame;
    Texture2D *spike;
    Texture2D *spike_platform;
} TextureResources;

/* AudioResources — one slot per shared sound effect (music is per level). */
typedef struct {
    SoundEffect *jump;
    SoundEffect *coin;
    SoundEffect *hit;
    SoundEffect *spring;
    SoundEffect *axe;
    SoundEffect *flap;
    SoundEffect *spider_attack;
    SoundEffect *dive;
} AudioResources;

/*
 * GameAssets — the sprites and sound effects every level draws and plays.
 *
 * None of them depends on which level is loaded, so they do not belong to
 * one GameState. AppSession loads one GameAssets the first time it opens a
 * game, keeps it until the session ends, and copies it into every GameState
 * it creates (Play, Replay, Level Select -> Play): the copies hold the same
 * pointers, so a Replay decodes none of these files again.
 *
 * Level-specific files (parallax layers, fog, the water strip, the floor
 * tileset a level names, platform tiles, music) live with the level in
 * GameWorld instead, and are loaded and released with it.
 */
typedef struct GameAssets {
    TextureResources textures;    /* owned GPU textures */
    AudioResources   audio;       /* owned sound samples */
} GameAssets;
