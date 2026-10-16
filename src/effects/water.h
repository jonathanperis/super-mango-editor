/*
 * water.h — Public interface for the animated water strip module.
 *
 * Sprite facts (Water.png, 384×64):
 *   - 8 animation frames, each 48×64 px in the sheet.
 *   - Actual art: x=16..31, y=17..47 per frame (16×31 px visible area).
 *   - 16 px transparent padding on both left AND right of every frame.
 *
 * Rendering strategy:
 *   - Source crop per tile: { frame*48+16, 17, 16, 31 }
 *   - Tiles placed every 16 px, cycling through frames 0-7 in order.
 *   - Pattern period = 8 x 16 = 128 px; scroll wraps at 128 px.
 *   - Scroll is rightward — continuous wave motion, no transparent gaps.
 */
#pragma once

#include "../mangolib/massets.h"    /* AssetCache — the strips are held from it */
#include "../mangolib/mgfx.h"

#define WATER_FRAMES        8    /* frames in the sheet                    */
#define WATER_FRAME_W      48    /* full slot width in the sheet (px)      */
#define WATER_ART_DX       16    /* left offset to art within each slot    */
#define WATER_ART_W        16    /* width of actual art pixels per frame   */
#define WATER_ART_Y        17    /* first visible row                      */
#define WATER_ART_H        31    /* height of visible art                  */
#define WATER_PERIOD      (WATER_FRAMES * WATER_ART_W)  /* 128 px repeat  */
#define WATER_SCROLL_SPEED 40.0f /* rightward px/s                         */

/*
 * Water — the strip drawn along the bottom, and the two images behind it.
 *
 * default_strip is water.png. It is held for the whole game even while a
 * level draws its own strip (lava, say): a game that let go of it would
 * make the next game load it again. level_strip is the level's own image,
 * or NULL when the level uses the default. Both are held from the asset
 * cache, one reference each. texture is only a second name for whichever
 * of the two is drawn; it is never released on its own.
 */
typedef struct {
    Texture2D *texture;       /* the strip drawn: level_strip or default_strip */
    Texture2D *default_strip; /* water.png, held from the asset cache          */
    Texture2D *level_strip;   /* the level's own strip, held; NULL = default   */
    float        scroll_x;    /* rightward offset, wraps at WATER_PERIOD       */
} Water;

/* Get the default strip from cache (NULL = load it for this Water alone).
 * Returns -1 when water.png cannot be loaded. */
int water_init(Water *w, AssetCache *cache);
/* Draw the strip at path from now on. When it cannot be loaded, say so and
 * keep drawing the current one. */
void water_reload_texture(Water *w, AssetCache *cache, const char *path);
void water_update(Water *w, float dt);

/* Draw the animated water strip across the bottom of the canvas. */
void water_render(const Water *w);

/* Give both strips back to the cache water_init took them from. */
void water_cleanup(Water *w, AssetCache *cache);
