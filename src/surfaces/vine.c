/*
 * vine.c — Hanging vine decorations that drape from platform tops toward
 *           the ground floor.
 *
 * Vines are purely visual — no state changes, no collision. Their
 * positions come from the level's [[vines]] records (see level_loader.c).
 *
 * Each vine is drawn as tile_count stacked tiles, VINE_STEP px apart
 * (19 px, so neighbouring tiles overlap and look continuous), and stops
 * at FLOOR_Y so it never hangs into the ground.
 *
 * The sprite is rendered flipped vertically so the plant's base
 * (thicker, root end) attaches to the platform and the leafy tip
 * hangs toward the ground, matching the classic hanging-vine look.
 */
#include "vine.h"
#include "game_constants.h" /* FLOOR_Y, GAME_W */

/* ------------------------------------------------------------------ */

void vines_render(const VineDecor *vines, int count,
                  Texture2D *green_tex, Texture2D *brown_tex,
                  int cam_x)
{
    for (int i = 0; i < count; i++) {
        const VineDecor *v = &vines[i];
        int screen_x = (int)v->x - cam_x;

        /* Cull the entire vine chain when it is off the current viewport */
        if (screen_x + VINE_W < 0 || screen_x >= GAME_W) continue;

        /* Select texture by vine type — green for lush, brown for arid */
        Texture2D *tex = (v->type == VINE_BROWN) ? brown_tex : green_tex;
        if (!tex) continue;

        for (int t = 0; t < v->tile_count; t++) {
            /* Step by VINE_STEP to overlap tiles, hiding transparent edge pixels. */
            int tile_y = (int)v->y + t * VINE_STEP;

            /* Safety guard — tiles always stay within world bounds */
            if (tile_y >= FLOOR_Y) break;

            IntRect src = { 0, VINE_SRC_Y, VINE_W, VINE_SRC_H };
            IntRect dst = { screen_x, tile_y, VINE_W, VINE_H };

            /*
             * Render the sprite upside-down.
             * The Vine.png has its root/base at the bottom (upright plant).
             * Flipping puts the base at the TOP so it visually attaches to
             * the platform surface, while the leafy tip hangs downward.
             * src crops the 8 px transparent rows at top and bottom of the
             * sprite so tiles stack flush with no visible gap.
             */
            sprite_draw(tex, &src, &dst, 0, SPRITE_FLIP_Y, WHITE);
        }
    }
}
