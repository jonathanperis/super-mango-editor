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
 * platforms_init — Define pillar platforms spread across the full world width.
 *
 * Pillars sit directly on top of the floor (their bottom edge == FLOOR_Y)
 * so they look like natural extensions of the ground.
 *
 * World layout (WORLD_W = 1600 px, 4 screens of 400 px each):
 *
 *   Screen 1 (x 0–400):
 *     Pillar 1 — medium (2 tiles tall) at x=80
 *     Pillar 2 — tall   (3 tiles tall) at x=256
 *
 *   Screen 2 (x 400–800):
 *     Pillar 3 — medium (2 tiles tall) at x=500
 *     Pillar 4 — tall   (3 tiles tall) at x=680
 *
 *   Screen 3 (x 800–1200):
 *     Pillar 5 — medium (2 tiles tall) at x=880
 *     Pillar 6 — tall   (3 tiles tall) at x=1050
 *
 *   Screen 4 (x 1200–1600):
 *     Pillar 7 — medium (2 tiles tall) at x=1300
 *     Pillar 8 — tall   (3 tiles tall) at x=1480
 *
 * Jump physics note: with vy = −500 px/s, the player's apex reaches ~156 px
 * above the floor, clearing all pillars (tallest top at FLOOR_Y − 3×TILE = 108).
 */
void platforms_init(Platform *platforms, int *count) {
    /* ── Screen 1 ──────────────────────────────────────────────────── */

    /* Pillar 1: medium (2 tiles tall) */
    platforms[0].x = 80.0f;
    platforms[0].y = (float)(FLOOR_Y - 2 * TILE_SIZE);  /* y=156 */
    platforms[0].w = TILE_SIZE;                          /* 48 px */
    platforms[0].h = 2 * TILE_SIZE;                      /* 96 px */

    /* Pillar 2: tall (3 tiles tall) */
    platforms[1].x = 256.0f;
    platforms[1].y = (float)(FLOOR_Y - 3 * TILE_SIZE);  /* y=108 */
    platforms[1].w = TILE_SIZE;
    platforms[1].h = 3 * TILE_SIZE;                      /* 144 px */

    /* ── Screen 2 ──────────────────────────────────────────────────── */

    /* Pillar 3: medium */
    platforms[2].x = 500.0f;
    platforms[2].y = (float)(FLOOR_Y - 2 * TILE_SIZE);
    platforms[2].w = TILE_SIZE;
    platforms[2].h = 2 * TILE_SIZE;

    /* Pillar 4: tall */
    platforms[3].x = 680.0f;
    platforms[3].y = (float)(FLOOR_Y - 3 * TILE_SIZE);
    platforms[3].w = TILE_SIZE;
    platforms[3].h = 3 * TILE_SIZE;

    /* ── Screen 3 ──────────────────────────────────────────────────── */

    /* Pillar 5: medium */
    platforms[4].x = 880.0f;
    platforms[4].y = (float)(FLOOR_Y - 2 * TILE_SIZE);
    platforms[4].w = TILE_SIZE;
    platforms[4].h = 2 * TILE_SIZE;

    /* Pillar 6: tall */
    platforms[5].x = 1050.0f;
    platforms[5].y = (float)(FLOOR_Y - 3 * TILE_SIZE);
    platforms[5].w = TILE_SIZE;
    platforms[5].h = 3 * TILE_SIZE;

    /* ── Screen 4 ──────────────────────────────────────────────────── */

    /* Pillar 7: medium */
    platforms[6].x = 1300.0f;
    platforms[6].y = (float)(FLOOR_Y - 2 * TILE_SIZE);
    platforms[6].w = TILE_SIZE;
    platforms[6].h = 2 * TILE_SIZE;

    /* Pillar 8: tall */
    platforms[7].x = 1480.0f;
    platforms[7].y = (float)(FLOOR_Y - 3 * TILE_SIZE);
    platforms[7].w = TILE_SIZE;
    platforms[7].h = 3 * TILE_SIZE;

    *count = 8;
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
                      SDL_Renderer *renderer, SDL_Texture *tex, int cam_x) {
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
                 * dst — world → screen: subtract cam_x from the x coordinate
                 *       so the pillar scrolls with the camera.
                 */
                SDL_Rect src = { piece_col * P, piece_row * P, P, P };
                SDL_Rect dst = { (int)p->x + tx - cam_x, (int)p->y + ty, P, P };
                SDL_RenderCopy(renderer, tex, &src, &dst);
            }
        }
    }
}
