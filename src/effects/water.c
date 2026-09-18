/*
 * water.c — Animated water strip rendered at the bottom of the canvas.
 *
 * See water.h for sprite layout details and rendering strategy.
 */
#include "water.h"
#include "game.h"   /* GAME_W, GAME_H */
#include <stdio.h>

/* ─── public API ──────────────────────────────────────────────────── */

int water_init(Water *w)
{
    w->texture = texture_load("assets/sprites/foregrounds/water.png");
    if (!w->texture) {
        fprintf(stderr, "water_init: cannot load assets/sprites/foregrounds/water.png\n");
        return -1;
    }
    w->scroll_x = 0.0f;
    return 0;
}

/*
 * water_reload_texture — Replace the water texture with a different asset.
 * Used when the level's foreground layers specify lava or another strip
 * instead of the default water.  Keeps scroll state intact.
 */
void water_reload_texture(Water *w, const char *path)
{
    if (!path || path[0] == '\0') return;

    Texture2D *new_tex = texture_load(path);
    if (new_tex) {
        texture_unload(w->texture);
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

void water_cleanup(Water *w)
{
    if (w->texture) {
        texture_unload(w->texture);
        w->texture = NULL;
    }
}
