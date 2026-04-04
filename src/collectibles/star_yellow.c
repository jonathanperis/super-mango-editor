/*
 * star_yellow.c — Star Yellow rendering.  Placement is handled by the
 * level loader from LevelDef data.
 */

#include "star_yellow.h"
#include "game.h"   /* GAME_W */

/* ------------------------------------------------------------------ */

void star_yellows_render(const StarYellow *stars, int count,
                         SDL_Renderer *renderer, SDL_Texture *tex, int cam_x)
{
    if (!tex) return;

    for (int i = 0; i < count; i++) {
        if (!stars[i].active) continue;

        SDL_Rect dst = {
            (int)stars[i].x - cam_x,
            (int)stars[i].y,
            STAR_YELLOW_DISPLAY_W,
            STAR_YELLOW_DISPLAY_H
        };

        SDL_RenderCopy(renderer, tex, NULL, &dst);
    }
}
