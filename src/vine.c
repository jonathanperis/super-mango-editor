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
     * Platform layout — mirrors platform.c (local constants to avoid a
     * circular-include dependency on the platform array at call time).
     *   x : left edge of the pillar in world pixels
     *   y : top surface (landing surface) in world pixels
     */
    static const float plat_x[8] = {  80, 256, 500,  680,  880, 1050, 1300, 1480 };
    static const float plat_y[8] = { 172, 124, 172,  124,  172,  124,  172,  124 };

    /*
     * Randomly pick 2 or 3 platforms to receive a vine.
     * Fisher-Yates partial shuffle on an index array selects them without
     * repetition.
     */
    int indices[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    int vine_count = 3 + rand() % 3;  /* 3, 4, or 5 platforms */
    for (int i = 0; i < vine_count; i++) {
        int j = i + rand() % (8 - i);
        int tmp = indices[i]; indices[i] = indices[j]; indices[j] = tmp;
    }

    for (int i = 0; i < vine_count && n < MAX_VINES; i++) {
        int p = indices[i];
        /*
         * Tile count: capped at 2 or 3 tiles randomly — purely decorative,
         * not required to reach the floor.
         */
        int tiles = 2 + rand() % 2;

        /*
         * Alternate left/right by loop index, then flip with rand() for
         * extra variety — guarantees at least visual alternation even when
         * rand() returns the same parity repeatedly at startup.
         */
        float vine_x;
        int side = (i % 2) ^ (rand() % 2);  /* 0 = left, 1 = right */
        if (side == 0)
            vine_x = plat_x[p] + VINE_BORDER;
        else
            vine_x = plat_x[p] + TILE_SIZE - VINE_BORDER - VINE_W;

        vines[n++] = (VineDecor){ vine_x, plat_y[p], tiles };
    }

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
