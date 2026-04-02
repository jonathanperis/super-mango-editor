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
 * ladder_init — Place ladders at their level positions.
 *
 * Ladder 0 — on the tall pillar at x=1480 (screen 4, end phase).
 *
 * Placed at the right side of the pillar, extending from the pillar
 * top (y=172 for medium pillar at 1480 — but it's actually a tall one
 * y=124) down toward the floor.  2 tiles = 96 px of climbable surface.
 */
void ladder_init(LadderDecor *ladders, int *count) {
    /*
     * Ladder 0 — at the very end of the game, after the last column.
     *
     * Last pillar (pillar 7) right edge: 1480 + 48 = 1528.
     * Place the ladder 8 px to the right of the pillar edge.
     * Starts from the floor (FLOOR_Y) and stacks 15 tiles upward.
     * y = FLOOR_Y − 15 × LADDER_STEP so it reaches high into the sky.
     */
    ladders[0].x          = 1528.0f + 24.0f;
    /*
     * y is set so the bottom tile's bottom edge sits exactly at FLOOR_Y.
     * bottom = y + (count-1) * STEP + H = FLOOR_Y  →  y = FLOOR_Y - ((count-1)*STEP + H)
     */
    ladders[0].y          = (float)(FLOOR_Y - (149 * LADDER_STEP + LADDER_H));
    ladders[0].tile_count = 150;

    *count = 1;
}

/* ------------------------------------------------------------------ */

/*
 * ladder_render — Draw each ladder as vertically stacked tiles.
 *
 * Ladder.png is a 16×48 single frame.  Tiles overlap by 2 px
 * (LADDER_STEP = 46) for seamless stacking.
 */
void ladder_render(const LadderDecor *ladders, int count,
                   SDL_Renderer *renderer, SDL_Texture *tex, int cam_x) {
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
            SDL_Rect src = { 0, LADDER_SRC_Y, LADDER_W, LADDER_SRC_H };
            SDL_Rect dst = { screen_x, tile_y, LADDER_W, LADDER_H };
            SDL_RenderCopy(renderer, tex, &src, &dst);
        }
    }
}
