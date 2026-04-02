/*
 * last_star.c — LastStar: the end-of-level collectible.
 *
 * Placed at the far right of the level, on top of the last tall pillar.
 * Collecting it sets the `collected` flag, which will eventually trigger
 * a phase-passed event.
 */

#include <SDL.h>

#include "last_star.h"
#include "game.h"   /* FLOOR_Y, TILE_SIZE, GAME_W */

/* ------------------------------------------------------------------ */

/*
 * last_star_init — Place the star at the end phase.
 *
 * Positioned on top of the last tall pillar (x=1480, 3-tile tall).
 * Pillar top: FLOOR_Y − 3×TILE_SIZE + 16 = 124.
 * Star y: pillar_top − LAST_STAR_DISPLAY_H = 124 − 16 = 108.
 * Star x: centred on the pillar = 1480 + (48 − 16) / 2 = 1496.
 */
void last_star_init(LastStar *star) {
    star->x         = 1480.0f + (TILE_SIZE - LAST_STAR_DISPLAY_W) / 2.0f;
    star->y         = (float)(FLOOR_Y - 3 * TILE_SIZE + 16 - LAST_STAR_DISPLAY_H);
    star->w         = LAST_STAR_DISPLAY_W;
    star->h         = LAST_STAR_DISPLAY_H;
    star->active    = 1;
    star->collected = 0;
}

/* ------------------------------------------------------------------ */

/*
 * last_star_render — Draw the star if still active.
 *
 * Uses the full Stars_Ui.png texture (single 16×16 frame).
 */
void last_star_render(const LastStar *star,
                      SDL_Renderer *renderer, SDL_Texture *tex, int cam_x) {
    if (!star->active || !tex) return;

    /* Off-screen culling */
    if (star->x + star->w < (float)cam_x ||
        star->x > (float)(cam_x + GAME_W))
        return;

    SDL_Rect dst = {
        .x = (int)star->x - cam_x,
        .y = (int)star->y,
        .w = star->w,
        .h = star->h,
    };
    SDL_RenderCopy(renderer, tex, NULL, &dst);
}

/* ------------------------------------------------------------------ */

SDL_Rect last_star_get_hitbox(const LastStar *star) {
    /*
     * Inset the hitbox by 2 px on each side so the player must visually
     * overlap the star's core, not just graze the edge.
     */
    SDL_Rect r = {
        .x = (int)star->x + 2,
        .y = (int)star->y + 2,
        .w = star->w - 4,
        .h = star->h - 4,
    };
    return r;
}
