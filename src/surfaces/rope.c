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
 * ropes_render — Draw each rope as vertically stacked tiles.
 *
 * Rope.png is a 16×48 single frame.  Tiles overlap by 2 px
 * (ROPE_STEP = 46) for seamless stacking.
 */
void ropes_render(const RopeDecor *ropes, int count,
                  Texture2D *tex, int cam_x) {
    if (!tex) return;

    for (int i = 0; i < count; i++) {
        const RopeDecor *rp = &ropes[i];
        int screen_x = (int)rp->x - cam_x;

        if (screen_x + ROPE_W < 0 || screen_x >= GAME_W) continue;

        for (int t = 0; t < rp->tile_count; t++) {
            int tile_y = (int)rp->y + t * ROPE_STEP;

            if (tile_y >= FLOOR_Y) break;

            IntRect src = { ROPE_SRC_X, ROPE_SRC_Y, ROPE_SRC_W, ROPE_SRC_H };
            IntRect dst = { screen_x, tile_y, ROPE_W, ROPE_H };
            sprite_draw(tex, &src, &dst, 0, SPRITE_NORMAL, WHITE);
        }
    }
}
