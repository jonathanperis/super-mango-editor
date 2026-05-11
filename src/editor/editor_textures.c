/*
 * editor_textures.c — Editor entity texture loading and cleanup helpers.
 */

#include "editor_textures.h"

#include <SDL_image.h>  /* IMG_LoadTexture, IMG_GetError */
#include <stdio.h>     /* fprintf, stderr */

#include "../game.h"   /* DESTROY_TEX */

/*
 * editor_textures_load — Load all entity sprite sheets from assets/.
 *
 * Called once from editor_init after the renderer is created.  Loads are
 * non-fatal: missing sprites print warnings and leave NULL pointers so canvas
 * and palette rendering can fall back to placeholders.
 */
void editor_textures_load(EditorState *es)
{
    #define LOAD_TEX(field, path) \
        do { \
            es->textures.field = IMG_LoadTexture(es->renderer, path); \
            if (!es->textures.field) { \
                fprintf(stderr, "Warning: could not load %s: %s\n", \
                        path, IMG_GetError()); \
            } \
        } while (0)

    /* Environment textures — sky, floor, water (reloaded per-level). */
    LOAD_TEX(sky,              "assets/sprites/backgrounds/sky_blue.png");
    LOAD_TEX(floor_tile,       "assets/sprites/levels/grass_tileset.png");
    LOAD_TEX(water,            "assets/sprites/foregrounds/water.png");

    /* Static geometry. */
    LOAD_TEX(platform,         "assets/sprites/levels/grass_platform.png");
    LOAD_TEX(platform_stone,   "assets/sprites/levels/stone_platform.png");
    LOAD_TEX(platform_leaf,    "assets/sprites/levels/leaf_platform.png");

    /* Enemies — ground, air, and water patrol types. */
    LOAD_TEX(spider,           "assets/sprites/entities/spider.png");
    LOAD_TEX(jumping_spider,   "assets/sprites/entities/jumping_spider.png");
    LOAD_TEX(bird,             "assets/sprites/entities/bird.png");
    LOAD_TEX(faster_bird,      "assets/sprites/entities/faster_bird.png");
    LOAD_TEX(fish,             "assets/sprites/entities/fish.png");
    LOAD_TEX(faster_fish,      "assets/sprites/entities/faster_fish.png");

    /* Collectibles — coins, stars. */
    LOAD_TEX(coin,             "assets/sprites/collectibles/coin.png");
    LOAD_TEX(star_yellow,      "assets/sprites/collectibles/star_yellow.png");
    LOAD_TEX(star_green,       "assets/sprites/collectibles/star_green.png");
    LOAD_TEX(star_red,         "assets/sprites/collectibles/star_red.png");
    LOAD_TEX(last_star,        "assets/sprites/collectibles/last_star.png");

    /* Hazards — traps that damage the player on contact. */
    LOAD_TEX(axe_trap,         "assets/sprites/hazards/axe_trap.png");
    LOAD_TEX(circular_saw,     "assets/sprites/hazards/circular_saw.png");
    LOAD_TEX(blue_flame,       "assets/sprites/hazards/blue_flame.png");
    LOAD_TEX(fire_flame,       "assets/sprites/hazards/fire_flame.png");
    LOAD_TEX(spike,            "assets/sprites/hazards/spike.png");
    LOAD_TEX(spike_platform,   "assets/sprites/hazards/spike_platform.png");
    LOAD_TEX(spike_block,      "assets/sprites/hazards/spike_block.png");

    /* Surfaces — platforms, bridges, bouncepads. */
    LOAD_TEX(float_platform,   "assets/sprites/surfaces/float_platform.png");
    LOAD_TEX(bridge,           "assets/sprites/surfaces/bridge.png");
    LOAD_TEX(bouncepad_small,  "assets/sprites/surfaces/bouncepad_small.png");
    LOAD_TEX(bouncepad_medium, "assets/sprites/surfaces/bouncepad_medium.png");
    LOAD_TEX(bouncepad_high,   "assets/sprites/surfaces/bouncepad_high.png");

    /* Climbables — vertical traversal. */
    LOAD_TEX(vine_green,       "assets/sprites/surfaces/vine_green.png");
    LOAD_TEX(vine_brown,       "assets/sprites/surfaces/vine_brown.png");
    LOAD_TEX(ladder,           "assets/sprites/surfaces/ladder.png");
    LOAD_TEX(rope,             "assets/sprites/surfaces/rope.png");

    /* Rail paths — spike blocks and platforms ride on these. */
    LOAD_TEX(rail,             "assets/sprites/surfaces/rail.png");

    /* Player — used for spawn point preview in the editor. */
    LOAD_TEX(player,           "assets/sprites/player/player.png");

    #undef LOAD_TEX
}

/*
 * editor_textures_cleanup — Destroy all editor preview textures.
 *
 * Textures must be destroyed before the renderer because each SDL_Texture is
 * owned by the renderer that created it. DESTROY_TEX nulls pointers after free.
 */
void editor_textures_cleanup(EditorState *es)
{
    DESTROY_TEX(es->textures.sky);
    DESTROY_TEX(es->textures.floor_tile);
    DESTROY_TEX(es->textures.water);
    DESTROY_TEX(es->textures.platform);
    DESTROY_TEX(es->textures.platform_stone);
    DESTROY_TEX(es->textures.platform_leaf);
    DESTROY_TEX(es->textures.spider);
    DESTROY_TEX(es->textures.jumping_spider);
    DESTROY_TEX(es->textures.bird);
    DESTROY_TEX(es->textures.faster_bird);
    DESTROY_TEX(es->textures.fish);
    DESTROY_TEX(es->textures.faster_fish);
    DESTROY_TEX(es->textures.coin);
    DESTROY_TEX(es->textures.star_yellow);
    DESTROY_TEX(es->textures.star_green);
    DESTROY_TEX(es->textures.star_red);
    DESTROY_TEX(es->textures.last_star);
    DESTROY_TEX(es->textures.axe_trap);
    DESTROY_TEX(es->textures.circular_saw);
    DESTROY_TEX(es->textures.blue_flame);
    DESTROY_TEX(es->textures.fire_flame);
    DESTROY_TEX(es->textures.spike);
    DESTROY_TEX(es->textures.spike_platform);
    DESTROY_TEX(es->textures.spike_block);
    DESTROY_TEX(es->textures.float_platform);
    DESTROY_TEX(es->textures.bridge);
    DESTROY_TEX(es->textures.bouncepad_small);
    DESTROY_TEX(es->textures.bouncepad_medium);
    DESTROY_TEX(es->textures.bouncepad_high);
    DESTROY_TEX(es->textures.vine_green);
    DESTROY_TEX(es->textures.vine_brown);
    DESTROY_TEX(es->textures.ladder);
    DESTROY_TEX(es->textures.rope);
    DESTROY_TEX(es->textures.rail);
    DESTROY_TEX(es->textures.player);
}
