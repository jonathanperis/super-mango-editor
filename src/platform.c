/*
 * platform.c — Implementation of platform init and rendering.
 *
 * Each platform is a "pillar" made by tiling the Grass_Oneway.png texture
 * (48×48 px) vertically.  The top surface of each pillar acts as a one-way
 * landing zone — collision logic lives in player_update (player.c).
 */

#include <SDL.h>
#include <stdio.h>

#include "platform.h"
#include "game.h"   /* FLOOR_Y, TILE_SIZE */

/* ------------------------------------------------------------------ */

/*
 * platforms_init — Define the two pillar platforms.
 *
 * Pillars sit directly on top of the floor (their bottom edge == FLOOR_Y)
 * so they look like natural extensions of the ground.
 *
 * Platform layout (logical 400×300 space):
 *
 *   Pillar 1 — MEDIUM (2 tiles tall, 48×96 px)
 *     x  = 80    (roughly 1/5 from the left)
 *     y  = FLOOR_Y − 2 × TILE_SIZE  = 252 − 96 = 156
 *     Top surface visible at y = 156; floor base at y = 252.
 *
 *   Pillar 2 — BIGGER (3 tiles tall, 48×144 px)
 *     x  = 256   (roughly 2/3 across the screen)
 *     y  = FLOOR_Y − 3 × TILE_SIZE  = 252 − 144 = 108
 *     Top surface visible at y = 108; floor base at y = 252.
 *
 * Jump physics note: with vy = −500 px/s (set in player.c), the player's
 * apex reaches ~156 px above the floor, clearing the top of the taller
 * pillar (108 px up) and allowing a clean one-way landing on both.
 */
void platforms_init(Platform *platforms, int *count) {
    /* --- Pillar 1: medium (2 tiles tall) -------------------------------- */
    platforms[0].x = 80.0f;
    platforms[0].y = (float)(FLOOR_Y - 2 * TILE_SIZE);  /* 156 */
    platforms[0].w = TILE_SIZE;                          /* 48  */
    platforms[0].h = 2 * TILE_SIZE;                      /* 96  */

    /* --- Pillar 2: bigger (3 tiles tall) -------------------------------- */
    platforms[1].x = 256.0f;
    platforms[1].y = (float)(FLOOR_Y - 3 * TILE_SIZE);  /* 108 */
    platforms[1].w = TILE_SIZE;                          /* 48  */
    platforms[1].h = 3 * TILE_SIZE;                      /* 144 */

    *count = 2;
}

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
                      SDL_Renderer *renderer, SDL_Texture *tex) {
    const int P = TILE_SIZE / 3;   /* 9-slice piece size: 16 px */

    for (int i = 0; i < count; i++) {
        const Platform *p = &platforms[i];

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
                 * src — the 16×16 cell to cut from Grass_Oneway.png.
                 *   x = piece_col × P  (0, 16, or 32)
                 *   y = piece_row × P  (0, 16, or 32)
                 * dst — where on the logical canvas to draw this piece.
                 */
                SDL_Rect src = { piece_col * P, piece_row * P, P, P };
                SDL_Rect dst = { (int)p->x + tx, (int)p->y + ty, P, P };
                SDL_RenderCopy(renderer, tex, &src, &dst);
            }
        }
    }
}
