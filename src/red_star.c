/*
 * red_star.c — Red Star placement and rendering.
 *
 * Three red stars are placed in challenging-but-reachable spots across the
 * level — one on a tall platform, one on a float platform, and one near
 * the bridge.  Collecting a red star restores one heart immediately.
 * Collection logic is handled in game.c via AABB overlap.
 */

#include "red_star.h"
#include "game.h"   /* FLOOR_Y, TILE_SIZE */

/* ------------------------------------------------------------------ */

/*
 * red_stars_init — Hand-place red stars across the 1600-px world.
 *
 * Positions are chosen so the player must explore and take risks to reach
 * them — rewarding skillful play with health recovery.
 *
 *   Star 0 — Screen 1, on top of the 3-tile tall platform at x=256.
 *            Requires a careful jump to the tallest pillar.
 *
 *   Star 1 — Screen 2, on the static float platform at x=172, y=200.
 *            Reachable by jumping from nearby pillar.
 *
 *   Star 2 — Screen 4, on top of the 3-tile tall platform at x=1480.
 *            Deep in the level, rewarding full exploration.
 */
void red_stars_init(RedStar *stars, int *count)
{
    int n = 0;

    /* Star 0 — 3-tile platform at x=256 (screen 1, top y = 124) */
    stars[n].x      = 256.0f + TILE_SIZE / 2.0f - RED_STAR_DISPLAY_W / 2.0f;
    stars[n].y      = (float)(FLOOR_Y - 3 * TILE_SIZE + 16 - RED_STAR_DISPLAY_H);
    stars[n].active = 1;
    n++;

    /* Star 1 — static float platform at x=172, y=200 (screen 1–2 gap) */
    stars[n].x      = 172.0f + 32.0f - RED_STAR_DISPLAY_W / 2.0f;
    stars[n].y      = 200.0f - RED_STAR_DISPLAY_H;
    stars[n].active = 1;
    n++;

    /* Star 2 — 3-tile platform at x=1480 (screen 4, top y = 124) */
    stars[n].x      = 1480.0f + TILE_SIZE / 2.0f - RED_STAR_DISPLAY_W / 2.0f;
    stars[n].y      = (float)(FLOOR_Y - 3 * TILE_SIZE + 16 - RED_STAR_DISPLAY_H);
    stars[n].active = 1;
    n++;

    *count = n;   /* 3 red stars total */
}

/* ------------------------------------------------------------------ */

/*
 * red_stars_render — Draw every active red star to the back buffer.
 *
 * Uses SDL_RenderCopy with the full Star_Red.png texture (NULL source rect)
 * scaled to RED_STAR_DISPLAY_W × RED_STAR_DISPLAY_H.  Inactive stars are
 * skipped so collected stars disappear immediately.
 */
void red_stars_render(const RedStar *stars, int count,
                      SDL_Renderer *renderer, SDL_Texture *tex, int cam_x)
{
    if (!tex) return;

    for (int i = 0; i < count; i++) {
        if (!stars[i].active) continue;

        /*
         * World → screen: subtract cam_x so the star scrolls with
         * the camera.  Stars outside the viewport are clipped by SDL.
         */
        SDL_Rect dst = {
            (int)stars[i].x - cam_x,
            (int)stars[i].y,
            RED_STAR_DISPLAY_W,
            RED_STAR_DISPLAY_H
        };

        SDL_RenderCopy(renderer, tex, NULL, &dst);
    }
}
