/*
 * yellow_star.c — Yellow Star placement and rendering.
 *
 * Yellow stars are placed in challenging-but-reachable spots across the
 * level.  Collecting a yellow star restores one heart immediately.
 * Collection logic is handled in game.c via AABB overlap.
 */

#include "yellow_star.h"
#include "game.h"   /* FLOOR_Y, TILE_SIZE */

/* ------------------------------------------------------------------ */

void yellow_stars_init(YellowStar *stars, int *count)
{
    int n = 0;

    /* Star 0 — 3-tile platform at x=256 (screen 1, top y = 124) */
    stars[n].x      = 256.0f + TILE_SIZE / 2.0f - YELLOW_STAR_DISPLAY_W / 2.0f;
    stars[n].y      = (float)(FLOOR_Y - 3 * TILE_SIZE + 16 - YELLOW_STAR_DISPLAY_H);
    stars[n].active = 1;
    n++;

    /* Star 1 — static float platform at x=172, y=200 (screen 1–2 gap) */
    stars[n].x      = 172.0f + 32.0f - YELLOW_STAR_DISPLAY_W / 2.0f;
    stars[n].y      = 200.0f - YELLOW_STAR_DISPLAY_H;
    stars[n].active = 1;
    n++;

    /* Star 2 — 3-tile platform at x=680 (screen 2, top y = 124) */
    stars[n].x      = 680.0f + TILE_SIZE / 2.0f - YELLOW_STAR_DISPLAY_W / 2.0f;
    stars[n].y      = (float)(FLOOR_Y - 3 * TILE_SIZE + 16 - YELLOW_STAR_DISPLAY_H);
    stars[n].active = 1;
    n++;

    *count = n;
}

/* ------------------------------------------------------------------ */

void yellow_stars_render(const YellowStar *stars, int count,
                         SDL_Renderer *renderer, SDL_Texture *tex, int cam_x)
{
    if (!tex) return;

    for (int i = 0; i < count; i++) {
        if (!stars[i].active) continue;

        SDL_Rect dst = {
            (int)stars[i].x - cam_x,
            (int)stars[i].y,
            YELLOW_STAR_DISPLAY_W,
            YELLOW_STAR_DISPLAY_H
        };

        SDL_RenderCopy(renderer, tex, NULL, &dst);
    }
}
