/*
 * star_red.c — Rendering for Star Red collectibles.
 *
 * Reuses the star_yellow render pipeline since struct layout is identical.
 */

#include "star_red.h"
#include "star_yellow.h"

void star_reds_render(const StarRed *stars, int count,
                      Texture2D *tex, int cam_x) {
    if (!tex) return;
    for (int i = 0; i < count; i++) {
        if (!stars[i].active) continue;
        IntRect dst = { (int)stars[i].x - cam_x, (int)stars[i].y,
                         STAR_RED_DISPLAY_W, STAR_RED_DISPLAY_H };
        sprite_draw(tex, NULL, &dst, 0, SPRITE_NORMAL, WHITE);
    }
}
