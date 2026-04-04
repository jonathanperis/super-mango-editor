/*
 * parallax.h — Public interface for the multi-layer parallax background system.
 *
 * Each layer is a full-width PNG that tiles horizontally and scrolls at a
 * fraction of the camera speed.  Layers further back (lower speed factor)
 * move less than the game world, creating a sense of depth.
 *
 * Rendering order: layers are drawn back-to-front (index 0 = farthest).
 * Each layer is tiled seamlessly: the tile offset is computed as
 *   offset = (int)(cam_x × speed) % tex_w
 * and tiles are drawn from -offset leftward to cover GAME_W.
 *
 * speed = 0.0  → completely static (sky never moves)
 * speed = 0.5  → layer moves at half the camera speed
 * speed = 1.0  → layer moves with the world (not used for backgrounds)
 *
 * To add or remove layers, edit the LAYER_CONFIG table in parallax.c —
 * no changes needed anywhere else.
 */
#pragma once

#include <SDL.h>   /* SDL_Texture, SDL_Renderer */

/*
 * PARALLAX_MAX_LAYERS — maximum number of background layers the system can hold.
 * Stored as a fixed-size array; no heap allocation needed.
 */
#define PARALLAX_MAX_LAYERS  8

/* ------------------------------------------------------------------ */

/*
 * ParallaxLayer — data for one background layer.
 *
 * texture  : GPU image; NULL when the asset failed to load (layer is skipped).
 * tex_w/h  : natural size of the PNG in pixels, queried after loading.
 * speed    : fraction of cam_x used as the horizontal scroll offset.
 *            0.0 = static, 0.5 = half speed, 1.0 = full world speed.
 */
typedef struct {
    SDL_Texture *texture;   /* GPU image handle; NULL = failed to load         */
    int          tex_w;     /* natural width  of the loaded PNG in pixels      */
    int          tex_h;     /* natural height of the loaded PNG in pixels      */
    float        speed;     /* parallax fraction: how fast this layer scrolls  */
} ParallaxLayer;

/*
 * ParallaxSystem — owns all layers and the active layer count.
 * Stored by value inside GameState — no heap allocation needed.
 */
typedef struct {
    ParallaxLayer layers[PARALLAX_MAX_LAYERS];
    int           count;    /* number of layers actually configured            */
} ParallaxSystem;

/* ------------------------------------------------------------------ */

/*
 * parallax_init — Load all configured layer textures.
 *
 * Layer paths and speed factors are defined in a static table inside
 * parallax.c.  Each layer is non-fatal: a missing PNG prints a warning
 * and leaves that layer's texture NULL (it will be skipped at render time).
 */
void parallax_init(ParallaxSystem *ps, SDL_Renderer *renderer);

/*
 * parallax_init_from_def — Load layers from level definition data.
 *
 * Instead of reading from the hardcoded LAYER_CONFIG table, this function
 * accepts parallel arrays of PNG paths and speed factors extracted from a
 * LevelDef.  Each layer is non-fatal: a missing PNG prints a warning and
 * leaves that layer's texture NULL (skipped at render time).
 *
 * count must be <= PARALLAX_MAX_LAYERS; excess entries are silently ignored.
 */
void parallax_init_from_def(ParallaxSystem *ps, SDL_Renderer *renderer,
                            const char (*paths)[64], const float *speeds, int count);

/*
 * parallax_render — Draw all layers back-to-front with horizontal tiling.
 *
 * cam_x is the integer camera offset for this frame (from game_loop).
 * Each layer computes its own scroll offset as (int)(cam_x × speed) % tex_w,
 * then tiles the texture to cover the full GAME_W canvas width.
 */
void parallax_render(const ParallaxSystem *ps, SDL_Renderer *renderer, int cam_x);

/*
 * parallax_cleanup — Destroy all GPU textures owned by the parallax system.
 *
 * Must be called before the renderer is destroyed.
 * NULL textures (failed loads) are safely skipped.
 */
void parallax_cleanup(ParallaxSystem *ps);
