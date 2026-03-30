/*
 * vine.c — Hanging vine decorations that drape from platform tops toward
 *           the ground floor.
 *
 * Vines are placed once at init time and are purely visual — no state
 * changes, no collision.
 *
 * One vine per pillar, placed at either the left or the right inset
 * position chosen randomly at startup.  VINE_BORDER keeps the vine
 * centred within the platform edge rather than flush against it.
 *
 * With TILE_SIZE=48, VINE_W=16, VINE_BORDER=8:
 *   Left  position: plat_x + VINE_BORDER          (= plat_x +  8)
 *   Right position: plat_x + TILE_SIZE - VINE_BORDER - VINE_W (= plat_x + 24)
 *
 * The vine always fills completely from the platform's top surface down to
 * FLOOR_Y — tile count = (FLOOR_Y − plat_y) / VINE_H:
 *
 *   Medium pillar (top y=172): 3 tiles — covers 172 → 268 (96 px / 32)
 *   Tall   pillar (top y=124): 4 tiles — covers 124 → 268 (144 px / 32)
 *
 * The sprite is rendered with SDL_FLIP_VERTICAL so the plant's base
 * (thicker, root end) attaches to the platform and the leafy tip
 * hangs toward the ground, matching the classic hanging-vine look.
 */
#include "vine.h"
#include "game.h"   /* FLOOR_Y, TILE_SIZE, GAME_W */
#include <stdlib.h> /* rand */

/* Horizontal inset from each edge of the platform. */
#define VINE_BORDER 8
/* VINE_STEP is defined in vine.h (19 px — tiles overlap for flush stacking). */

/* ------------------------------------------------------------------ */

void vine_init(VineDecor *vines, int *count)
{
    int n = 0;

    /*
     * Platform layout — mirrors platform.c.
     *   x : left edge of the pillar in world pixels
     *   y : top surface (landing surface) in world pixels
     *
     *   Index  Screen  Size    x      y
     *     0      1     medium   80   172
     *     1      1     tall    256   124
     *     2      2     medium  452   172  ← rope goes here, skip vine
     *     3      2     tall    680   124
     *     4      3     medium  880   172
     *     5      3     tall   1050   124
     *     6      4     medium 1300   172
     *     7      4     tall   1480   124
     */
    static const float plat_x[8] = {  80, 256, 452,  680,  880, 1050, 1300, 1480 };
    static const float plat_y[8] = { 172, 124, 172,  124,  172,  124,  172,  124 };

    /*
     * Fixed vine placements — no randomness.
     * Skip pillar 2 (x=452) because the rope is placed there.
     *
     * Vines on pillars: 0 (left), 1 (right), 3 (left), 5 (right), 6 (left).
     */
    /* Pillar 0 (medium, x=80) — left side, 2 tiles */
    vines[n++] = (VineDecor){ plat_x[0] + VINE_BORDER, plat_y[0], 2 };

    /* Pillar 1 (tall, x=256) — right side, 3 tiles */
    vines[n++] = (VineDecor){ plat_x[1] + TILE_SIZE - VINE_BORDER - VINE_W, plat_y[1], 3 };

    /* Pillar 3 (tall, x=680) — left side, 3 tiles */
    vines[n++] = (VineDecor){ plat_x[3] + VINE_BORDER, plat_y[3], 3 };

    /* Pillar 5 (tall, x=1050) — right side, 3 tiles */
    vines[n++] = (VineDecor){ plat_x[5] + TILE_SIZE - VINE_BORDER - VINE_W, plat_y[5], 3 };

    /* Pillar 6 (medium, x=1300) — left side, 2 tiles */
    vines[n++] = (VineDecor){ plat_x[6] + VINE_BORDER, plat_y[6], 2 };

    *count = n;
}

/* ------------------------------------------------------------------ */

void vine_render(const VineDecor *vines, int count,
                 SDL_Renderer *renderer, SDL_Texture *tex, int cam_x)
{
    for (int i = 0; i < count; i++) {
        const VineDecor *v = &vines[i];
        int screen_x = (int)v->x - cam_x;

        /* Cull the entire vine chain when it is off the current viewport */
        if (screen_x + VINE_W < 0 || screen_x >= GAME_W) continue;

        for (int t = 0; t < v->tile_count; t++) {
            /* Step by VINE_STEP to overlap tiles, hiding transparent edge pixels. */
            int tile_y = (int)v->y + t * VINE_STEP;

            /* Safety guard — tiles always stay within world bounds */
            if (tile_y >= FLOOR_Y) break;

            SDL_Rect src = { 0, VINE_SRC_Y, VINE_W, VINE_SRC_H };
            SDL_Rect dst = { screen_x, tile_y, VINE_W, VINE_H };

            /*
             * SDL_FLIP_VERTICAL — render the sprite upside-down.
             * The Vine.png has its root/base at the bottom (upright plant).
             * Flipping puts the base at the TOP so it visually attaches to
             * the platform surface, while the leafy tip hangs downward.
             * src crops the 8 px transparent rows at top and bottom of the
             * sprite so tiles stack flush with no visible gap.
             */
            SDL_RenderCopyEx(renderer, tex, &src, &dst,
                             0.0, NULL, SDL_FLIP_VERTICAL);
        }
    }
}
