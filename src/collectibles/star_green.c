/*
 * star_green.c — Rendering for Star Green collectibles.
 *
 * Reuses the star_yellow render pipeline since struct layout is identical.
 */

#include "star_green.h"
#include "star_yellow.h"

void star_greens_render(const StarGreen *stars, int count,
                        Texture2D *tex, int cam_x) {
    if (!tex) return;
    for (int i = 0; i < count; i++) {
        if (!stars[i].active) continue;
        IntRect dst = { (int)stars[i].x - cam_x, (int)stars[i].y,
                         STAR_GREEN_DISPLAY_W, STAR_GREEN_DISPLAY_H };
        sprite_draw(tex, NULL, &dst, 0, SPRITE_NORMAL, WHITE);
    }
}
