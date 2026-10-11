/*
 * water.c — Animated water strip rendered at the bottom of the canvas.
 *
 * See water.h for sprite layout details and rendering strategy.
 */
#include "water.h"
#include "game_constants.h" /* GAME_W, GAME_H */
#include <stdio.h>

#define WATER_DEFAULT_PATH "assets/sprites/foregrounds/water.png"

/* ─── public API ──────────────────────────────────────────────────── */

int water_init(Water *w, AssetCache *cache)
{
    w->level_strip = NULL;
    w->default_strip = asset_cache_texture_acquire(cache, WATER_DEFAULT_PATH);
    w->texture = w->default_strip;
    if (!w->default_strip) {
        fprintf(stderr, "water_init: cannot load " WATER_DEFAULT_PATH "\n");
        return -1;
    }
    w->scroll_x = 0.0f;
    return 0;
}

/*
 * water_reload_texture — Draw a different strip from now on.
 * Used when the level's foreground layers specify lava or another strip
 * instead of the default water.  Keeps scroll state intact.
 *
 * The new strip is acquired before the old one is released, and only a
 * successful acquire replaces anything: a strip that fails to load leaves
 * the current one on screen.
 */
void water_reload_texture(Water *w, AssetCache *cache, const char *path)
{
    if (!path || path[0] == '\0') return;

    Texture2D *new_tex = asset_cache_texture_acquire(cache, path);
    if (new_tex) {
        asset_cache_texture_release(cache, w->level_strip);
        w->level_strip = new_tex;
        w->texture = new_tex;
    } else {
        fprintf(stderr, "water_reload_texture: cannot load %s\n", path);
    }
}

void water_update(Water *w, float dt)
{
    /* Advance rightward; wrap at the 128-px pattern period. */
    w->scroll_x += WATER_SCROLL_SPEED * dt;
    if (w->scroll_x >= (float)WATER_PERIOD)
        w->scroll_x -= (float)WATER_PERIOD;
}

void water_render(const Water *w)
{
    /*
     * Each frame in Water.png has its 16-px art at x=16..31 inside a 48-px
     * slot — 16 px transparent on each side.  Rendering the full 48-px slot
     * leaves 32 px of empty space between wave crests.
     *
     * Fix: crop source to the exact 16-px art band and tile every 16 px,
     * cycling through frames 0-7.  Pattern repeats every 128 px.  As
     * scroll_x advances rightward the entire pattern shifts, wrapping
     * seamlessly so the canvas is always gapless.
     */
    int dest_y    = GAME_H - WATER_ART_H;
    int period    = WATER_PERIOD;                      /* 128 px          */
    int pattern_x = (int)w->scroll_x % period - period; /* start off left */

    while (pattern_x < GAME_W) {
        for (int f = 0; f < WATER_FRAMES; f++) {
            int dest_x = pattern_x + f * WATER_ART_W;
            if (dest_x + WATER_ART_W <= 0) continue;   /* off left edge   */
            if (dest_x >= GAME_W)          break;       /* off right edge  */

            IntRect src = {
                f * WATER_FRAME_W + WATER_ART_DX,
                WATER_ART_Y,
                WATER_ART_W,
                WATER_ART_H
            };
            IntRect dst = { dest_x, dest_y, WATER_ART_W, WATER_ART_H };
            sprite_draw(w->texture, &src, &dst, 0, SPRITE_NORMAL, WHITE);
        }
        pattern_x += period;
    }
}

void water_cleanup(Water *w, AssetCache *cache)
{
    /* One release per held strip. texture only named one of them. */
    asset_cache_texture_release(cache, w->level_strip);
    asset_cache_texture_release(cache, w->default_strip);
    w->level_strip = NULL;
    w->default_strip = NULL;
    w->texture = NULL;
}
