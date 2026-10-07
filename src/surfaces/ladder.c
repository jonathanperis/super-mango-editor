/*
 * ladder.c — Ladder: a climbable decoration placed in the end phase.
 *
 * Ladders work like vines for climbing but use a different sprite.
 * They are stacked vertically from a platform top downward.
 */

#include "ladder.h"
#include "game.h"   /* FLOOR_Y, TILE_SIZE, GAME_W */

/* ------------------------------------------------------------------ */



/*
 * ladders_render — Draw each ladder as vertically stacked tiles.
 *
 * Ladder.png is a 16×48 single frame.  Tiles overlap by 2 px
 * (LADDER_STEP = 46) for seamless stacking.
 */
void ladders_render(const LadderDecor *ladders, int count,
                    Texture2D *tex, int cam_x) {
    if (!tex) return;

    for (int i = 0; i < count; i++) {
        const LadderDecor *ld = &ladders[i];
        int screen_x = (int)ld->x - cam_x;

        /* Cull the entire ladder when off-viewport */
        if (screen_x + LADDER_W < 0 || screen_x >= GAME_W) continue;

        for (int t = 0; t < ld->tile_count; t++) {
            int tile_y = (int)ld->y + t * LADDER_STEP;

            if (tile_y >= FLOOR_Y) break;

            /*
             * Crop the transparent padding from Ladder.png.
             * Content occupies rows LADDER_SRC_Y..+(LADDER_SRC_H-1).
             */
            IntRect src = { 0, LADDER_SRC_Y, LADDER_W, LADDER_SRC_H };
            IntRect dst = { screen_x, tile_y, LADDER_W, LADDER_H };
            sprite_draw(tex, &src, &dst, 0, SPRITE_NORMAL, WHITE);
        }
    }
}
