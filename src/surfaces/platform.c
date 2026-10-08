/*
 * platform.c — Implementation of platform init and rendering.
 *
 * Each platform is a "pillar" made by tiling the Grass_Oneway.png texture
 * (48×48 px) vertically.  The top surface of each pillar acts as a one-way
 * landing zone — collision logic lives in player_update (player.c).
 */

#include "../shared/graphics.h"
#include <stdio.h>

#include "platform.h"
#include "game_constants.h" /* FLOOR_Y, TILE_SIZE */

/* ------------------------------------------------------------------ */

/*
 * platforms_render — Draw all platforms using 9-slice rendering.
 *
 * Grass_Oneway.png is 48×48 px, treated as a 3×3 grid of 16×16 pieces
 * (TILE_SIZE / 3 = 16).  Each piece has a structural role:
 *
 *   [TL][TC][TR]   row 0  y= 0..15  ← grass/top edge
 *   [ML][MC][MR]   row 1  y=16..31  ← dirt interior
 *   [BL][BC][BR]   row 2  y=32..47  ← base/bottom edge
 *
 * Selecting pieces per position within each pillar:
 *   Cols  → 0 = left cap, 1 = center fill, 2 = right cap
 *   Rows  → 0 = top edge, 1 = dirt interior, 2 = bottom base
 *
 * Platform dimensions are multiples of TILE_SIZE (48), which is 3×P (16),
 * so every piece fits without partial-pixel crops and no seams appear.
 * The result looks like a single carved stone/dirt pillar with clean
 * corners instead of a stack of identical repeated tiles.
 */
void platforms_render(const Platform *platforms, int count,
                       Texture2D *default_tex, int cam_x) {
    const int P = TILE_SIZE / 3;   /* 9-slice piece size: 16 px */

    for (int i = 0; i < count; i++) {
        const Platform *p = &platforms[i];

        /* Use per-platform texture if set, otherwise fall back to default */
        Texture2D *tex = p->tex ? p->tex : default_tex;
        if (!tex) continue;

        /*
         * Walk every 16×16 piece position inside the pillar bounding box.
         * ty and tx step in P-pixel increments across height and width.
         */
        for (int ty = 0; ty < p->h; ty += P) {
            /* Determine which texture row based on vertical position */
            int piece_row;
            if (ty == 0)              piece_row = 0;   /* top:    grass edge */
            else if (ty + P >= p->h)  piece_row = 2;   /* bottom: base edge  */
            else                      piece_row = 1;   /* middle: dirt fill  */

            for (int tx = 0; tx < p->w; tx += P) {
                /* Determine which texture column based on horizontal position */
                int piece_col;
                if (tx == 0)              piece_col = 0;   /* left cap   */
                else if (tx + P >= p->w)  piece_col = 2;   /* right cap  */
                else                      piece_col = 1;   /* center fill*/

                /*
                 * src — the 16×16 cell to cut from the tileset.
                 * dst — world → screen: subtract cam_x from the x coordinate
                 *       so the pillar scrolls with the camera.
                 */
                IntRect src = { piece_col * P, piece_row * P, P, P };
                IntRect dst = { (int)p->x + tx - cam_x, (int)p->y + ty, P, P };
                sprite_draw(tex, &src, &dst, 0, SPRITE_NORMAL, WHITE);
            }
        }
    }
}
