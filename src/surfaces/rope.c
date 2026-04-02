/*
 * rope.c — Rope: a climbable decoration placed on a smaller column.
 *
 * Ropes work like vines for climbing but use a different sprite.
 * Placed on one of the medium (2-tile) pillars.
 */

#include "rope.h"
#include "game.h"   /* FLOOR_Y, TILE_SIZE, GAME_W */

/* ------------------------------------------------------------------ */

/*
 * rope_init — Place ropes at their level positions.
 *
 * Rope 0 — on the medium pillar at x=452 (screen 2).
 *
 * Medium pillars have top at FLOOR_Y − 2×TILE_SIZE + 16 = 172.
 * The rope hangs from the pillar top, 2 tiles tall (covers 172 → ~264).
 * Placed at the left side of the pillar.
 */
void rope_init(RopeDecor *ropes, int *count) {
    ropes[0].x          = 452.0f + 8.0f;   /* 8 px inset from pillar left edge */
    ropes[0].y          = (float)(FLOOR_Y - 2 * TILE_SIZE + 16);
    ropes[0].tile_count = 1;              /* single tile, no stacking        */

    *count = 1;
}

/* ------------------------------------------------------------------ */

/*
 * rope_render — Draw each rope as vertically stacked tiles.
 *
 * Rope.png is a 16×48 single frame.  Tiles overlap by 2 px
 * (ROPE_STEP = 46) for seamless stacking.
 */
void rope_render(const RopeDecor *ropes, int count,
                 SDL_Renderer *renderer, SDL_Texture *tex, int cam_x) {
    if (!tex) return;

    for (int i = 0; i < count; i++) {
        const RopeDecor *rp = &ropes[i];
        int screen_x = (int)rp->x - cam_x;

        if (screen_x + ROPE_W < 0 || screen_x >= GAME_W) continue;

        for (int t = 0; t < rp->tile_count; t++) {
            int tile_y = (int)rp->y + t * ROPE_STEP;

            if (tile_y >= FLOOR_Y) break;

            SDL_Rect src = { ROPE_SRC_X, ROPE_SRC_Y, ROPE_SRC_W, ROPE_SRC_H };
            SDL_Rect dst = { screen_x, tile_y, ROPE_W, ROPE_H };
            SDL_RenderCopy(renderer, tex, &src, &dst);
        }
    }
}
