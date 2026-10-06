/*
 * last_star.c — LastStar: the end-of-level collectible.
 *
 * Placed at the far right of the level, on top of the last tall pillar.
 * Collecting it sets the `collected` flag, which will eventually trigger
 * a phase-passed event.
 */

#include "../shared/graphics.h"

#include "last_star.h"
#include "game.h"   /* FLOOR_Y, TILE_SIZE, GAME_W */

/* ------------------------------------------------------------------ */

/*
 * last_star_render — Draw the star if still active.
 *
 * Uses the full Stars_Ui.png texture (single 16×16 frame).
 */
void last_star_render(const LastStar *star,
                      Texture2D *tex, int cam_x) {
    if (!star->active || !tex) return;

    /* Off-screen culling */
    if (star->x + star->w < (float)cam_x ||
        star->x > (float)(cam_x + GAME_W))
        return;

    IntRect dst = {
        .x = (int)star->x - cam_x,
        .y = (int)star->y,
        .w = star->w,
        .h = star->h,
    };
    sprite_draw(tex, NULL, &dst, 0, SPRITE_NORMAL, WHITE);
}

/* ------------------------------------------------------------------ */

IntRect last_star_get_hitbox(const LastStar *star) {
    /*
     * Inset the hitbox by 2 px on each side so the player must visually
     * overlap the star's core, not just graze the edge.
     */
    IntRect r = {
        .x = (int)star->x + 2,
        .y = (int)star->y + 2,
        .w = star->w - 4,
        .h = star->h - 4,
    };
    return r;
}
