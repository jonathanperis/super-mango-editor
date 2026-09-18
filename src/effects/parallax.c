/*
 * parallax.c — Multi-layer parallax background: init, tiled render, cleanup.
 *
 * All layer PNG files are 384×216 RGBA.  The game canvas is 400×300.
 * Each layer is stretched to GAME_W × GAME_H per tile and repeated
 * horizontally so the seam never falls within the visible window.
 *
 * Scroll math per layer:
 *   parallax_x = cam_x × speed          (virtual position of this layer)
 *   offset     = (int)parallax_x % tex_w (wrap into one tile width)
 *   First tile drawn at x = -offset; subsequent tiles step by tex_w.
 *   Loop runs while dx < GAME_W, guaranteeing full coverage with no gaps.
 */

#include <stdio.h>      /* fprintf, stderr                        */

#include "parallax.h"
#include "game.h"       /* GAME_W, GAME_H                         */

/* ------------------------------------------------------------------ */
/* Layer configuration table                                          */
/* ------------------------------------------------------------------ */

/*
 * LAYER_CONFIG — defines every parallax layer in back-to-front order.
 *
 * To add a layer: append a new { path, speed } entry.
 * To remove a layer: delete its row.
 * To swap an asset (e.g. use the _lightened variant): change the path.
 * No other file needs to change.
 *
 * Speed guide:
 *   0.00 — completely static; good for the base sky gradient.
 *   0.08 — near-static; silhouetted mountains far in the distance.
 *   0.15 — slow drift; distant clouds behind the main cloud layer.
 *   0.25 — visible parallax; mid-ground cloud layer 1.
 *   0.38 — moderate; mid-ground cloud layer 2.
 *   0.50 — half camera speed; closest cloud layer before the gameplay plane.
 */
static const struct {
    const char *path;   /* PNG path relative to the repo root     */
    float       speed;  /* parallax scroll fraction (0 = static)  */
} LAYER_CONFIG[] = {
    { "assets/sprites/backgrounds/sky_blue.png",               0.00f },
    { "assets/sprites/backgrounds/clouds_bg.png",         0.08f },
    { "assets/sprites/backgrounds/glacial_mountains.png", 0.15f },
    { "assets/sprites/backgrounds/clouds_mg_3.png",       0.25f },
    { "assets/sprites/backgrounds/clouds_mg_2.png",       0.38f },
    { "assets/sprites/backgrounds/clouds_lonely.png",      0.44f },
    { "assets/sprites/backgrounds/clouds_mg_1.png",       0.50f },
};

/* Number of entries in LAYER_CONFIG, computed at compile time. */
#define LAYER_COUNT  (int)(sizeof(LAYER_CONFIG) / sizeof(LAYER_CONFIG[0]))

/* ------------------------------------------------------------------ */

/*
 * parallax_init — Load every layer texture defined in LAYER_CONFIG.
 *
 * Each layer is loaded independently and non-fatally: if a PNG is missing
 * or corrupt, a warning is printed and that layer's texture is left NULL.
 * The game continues running; NULL layers are simply skipped at render time.
 *
 * Texture metadata supplies the natural pixel dimensions of each loaded
 * texture, which the render function uses to compute the tile offset and
 * ensure full horizontal coverage of the canvas.
 */
void parallax_init(ParallaxSystem *ps)
{
    ps->count = LAYER_COUNT;

    for (int i = 0; i < LAYER_COUNT; i++) {
        ParallaxLayer *layer = &ps->layers[i];

        layer->speed   = LAYER_CONFIG[i].speed;
        layer->tex_w   = 0;
        layer->tex_h   = 0;
        layer->texture = NULL;

        /*
         * Decode the PNG and upload it to GPU memory.
         * Returns NULL on failure (file not found, unsupported format, etc.).
         */
        layer->texture = texture_load(LAYER_CONFIG[i].path);
        if (!layer->texture) {
            fprintf(stderr, "Warning: parallax layer %s not loaded\n", LAYER_CONFIG[i].path);
            continue;   /* leave tex_w/tex_h at 0; render will skip this layer */
        }

        /*
         * Read the dimensions retained with the texture handle.
         * These are stored per-layer so the render function can compute the
         * correct tile repeat period without knowing the source asset size.
         */
        layer->tex_w = layer->texture->width;
        layer->tex_h = layer->texture->height;
    }
}

/* ------------------------------------------------------------------ */

/*
 * parallax_init_from_def — Load layers from level-definition path/speed arrays.
 *
 * Same logic as parallax_init but reads from caller-provided arrays instead
 * of the internal LAYER_CONFIG table.  Used when the LevelDef provides its
 * own parallax configuration.  count is clamped to MAX_BACKGROUND_LAYERS.
 */
void parallax_init_from_def(ParallaxSystem *ps,
                            const char (*paths)[64], const float *speeds, int count)
{
    if (count > MAX_BACKGROUND_LAYERS) count = MAX_BACKGROUND_LAYERS;
    ps->count = count;

    for (int i = 0; i < count; i++) {
        ParallaxLayer *layer = &ps->layers[i];

        layer->speed   = speeds[i];
        layer->tex_w   = 0;
        layer->tex_h   = 0;
        layer->texture = NULL;

        layer->texture = texture_load(paths[i]);
        if (!layer->texture) {
            fprintf(stderr, "Warning: parallax layer %s not loaded\n", paths[i]);
            continue;
        }

        layer->tex_w = layer->texture->width;
        layer->tex_h = layer->texture->height;
    }
}

/* ------------------------------------------------------------------ */

/*
 * parallax_render — Draw every layer with horizontal tiling and parallax offset.
 *
 * For each layer the horizontal scroll offset is:
 *
 *   parallax_x = cam_x × layer.speed
 *
 * This gives the "virtual camera position" for this layer.  Layers with a
 * low speed factor barely move as the camera scrolls, simulating distance.
 *
 * The tile offset (how many pixels the tile has shifted leftward):
 *
 *   offset = (int)parallax_x % tex_w
 *
 * Modulo wraps the offset back into [0, tex_w), guaranteeing seamless
 * repetition regardless of how far the player has scrolled.
 *
 * Tiles are drawn from x = -offset (the partial tile peeking in from the
 * left) stepping by tex_w until dx >= GAME_W.  The first tile covers the
 * left edge; the last covers whatever gap remains at the right edge.
 * The render target clips anything drawn outside [0, GAME_W).
 *
 * The destination rect stretches each tile to tex_w × GAME_H so the layer
 * fills the full canvas height.  (Natural height is 216 px; GAME_H is 300 px.)
 */
void parallax_render(const ParallaxSystem *ps, int cam_x)
{
    for (int i = 0; i < ps->count; i++) {
        const ParallaxLayer *layer = &ps->layers[i];

        /* Skip layers that failed to load */
        if (!layer->texture || layer->tex_w <= 0) continue;

        /*
         * Compute the horizontal scroll offset for this layer.
         *
         * cam_x × speed gives the virtual pan distance.  The modulo wraps it
         * into one tile width so tiles repeat seamlessly.
         *
         * Adding tex_w before the final modulo ensures the result is
         * always non-negative even when cam_x × speed rounds to a small
         * negative float (e.g. floating-point noise near cam_x == 0).
         */
        int parallax_x = (int)(cam_x * layer->speed);
        int offset      = ((parallax_x % layer->tex_w) + layer->tex_w) % layer->tex_w;

        /*
         * Tile the texture horizontally to cover the full canvas width.
         *
         * dx starts at -offset: the leftmost tile is shifted partially
         * off-screen to the left so its visible right edge aligns seamlessly
         * with the next tile.  The loop advances by tex_w until the right
         * edge of the canvas (GAME_W) is covered.
         */
        for (int dx = -offset; dx < GAME_W; dx += layer->tex_w) {
            /*
             * dst — destination rect on the logical canvas.
             *   x = dx        : screen position (may be negative for the first tile)
             *   y = 0         : always anchored to the top of the canvas
             *   w = layer->tex_w : natural width; no horizontal scaling
             *   h = GAME_H    : stretch to full canvas height (216 → 300 px)
             */
            IntRect dst = { dx, 0, layer->tex_w, GAME_H };

            /*
             * Draw the full texture (NULL src = entire image) onto the canvas.
             */
            sprite_draw(layer->texture, NULL, &dst, 0, SPRITE_NORMAL, WHITE);
        }
    }
}

/* ------------------------------------------------------------------ */

/*
 * parallax_cleanup — Destroy every GPU texture owned by the parallax system.
 *
 * Must be called before closing the graphics context. Failed asset slots
 * remain NULL and are skipped during cleanup.
 */
void parallax_cleanup(ParallaxSystem *ps)
{
    for (int i = 0; i < ps->count; i++) {
        if (ps->layers[i].texture) {
            texture_unload(ps->layers[i].texture);
            ps->layers[i].texture = NULL;
        }
        ps->layers[i].tex_w = 0;
        ps->layers[i].tex_h = 0;
        ps->layers[i].speed = 0.0f;
    }
    ps->count = 0;
}
