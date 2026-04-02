/*
 * coin.c — Coin placement and rendering across the full world width.
 *
 * Places 19 coins: 12 on the ground floor (3 per screen section) and
 * 7 on top of the pillar platforms (Platform 0 is the spawn point).
 * Coins are static; collection is handled in game.c via AABB overlap.
 */

#include "coin.h"
#include "game.h"   /* FLOOR_Y, TILE_SIZE, WORLD_W */

/* ------------------------------------------------------------------ */

/*
 * Helper macro — coin centered horizontally on a platform.
 *   plat_x : platform left edge in world px
 *   plat_w : platform width (TILE_SIZE = 48)
 *   → coin left = plat_x + plat_w/2 − COIN_DISPLAY_W/2
 */
#define COIN_ON_PLAT_X(plat_x)  ((plat_x) + TILE_SIZE / 2 - COIN_DISPLAY_W / 2)

/*
 * coins_init — Distribute coins throughout the 1600-px world.
 *
 * World layout (4 screens × 400 px, pillars per screen):
 *
 *   Screen 1 (  0– 400): pillars at x=80  (2-tile) and x=256 (3-tile)
 *   Screen 2 (400– 800): pillars at x=500 (2-tile) and x=680 (3-tile)
 *   Screen 3 (800–1200): pillars at x=880 (2-tile) and x=1050 (3-tile)
 *   Screen 4 (1200–1600): pillars at x=1300 (2-tile) and x=1480 (3-tile)
 *
 * Ground coin y = FLOOR_Y − COIN_DISPLAY_H  =  252 − 16 = 236.
 * Platform coin y (2-tile top): FLOOR_Y − 2×TILE_SIZE + 16 − COIN_DISPLAY_H = 156.
 * Platform coin y (3-tile top): FLOOR_Y − 3×TILE_SIZE + 16 − COIN_DISPLAY_H = 108.
 *
 * The +16 accounts for platforms being shifted 16 px into the floor so
 * they visually grow from the ground rather than floating on top of it.
 */
void coins_init(Coin *coins, int *count)
{
    const float gy = (float)(FLOOR_Y - COIN_DISPLAY_H);                   /* ground y = 236 */
    const float p2 = (float)(FLOOR_Y - 2 * TILE_SIZE + 16 - COIN_DISPLAY_H); /* 156 */
    const float p3 = (float)(FLOOR_Y - 3 * TILE_SIZE + 16 - COIN_DISPLAY_H); /* 108 */

    int n = 0;   /* running coin index */

    /* ── Screen 1: ground coins ──────────────────────────────────── */
    coins[n].x = 46.0f;   coins[n].y = gy; coins[n].active = 1; n++;   /* left   */
    coins[n].x = 170.0f;  coins[n].y = gy; coins[n].active = 1; n++;   /* middle */
    coins[n].x = 368.0f;  coins[n].y = gy; coins[n].active = 1; n++;   /* right (past bouncepad 0) */

    /* ── Screen 1: platform coins ───────────────────────────────── */
    /* Platform 0 (x=80, 2-tile): no coin — player spawn point.  */
    /* Platform 1: x=256, 3-tile, coin x = 256 + 24 - 8 = 272 */
    coins[n].x = (float)COIN_ON_PLAT_X(256); coins[n].y = p3; coins[n].active = 1; n++;

    /* ── Screen 2: ground coins ──────────────────────────────────── */
    coins[n].x = 430.0f;  coins[n].y = gy; coins[n].active = 1; n++;
    coins[n].x = 595.0f;  coins[n].y = gy; coins[n].active = 1; n++;
    coins[n].x = 904.0f;  coins[n].y = gy; coins[n].active = 1; n++;   /* after spike row  */

    /* ── Screen 2: platform coins ───────────────────────────────── */
    /* Platform 2: x=452, 2-tile */
    coins[n].x = (float)COIN_ON_PLAT_X(452); coins[n].y = p2; coins[n].active = 1; n++;
    /* Platform 3: x=680, 3-tile — replaced by a yellow star */

    /* ── Screen 3: ground coins ──────────────────────────────────── */
    coins[n].x = 855.0f;  coins[n].y = gy; coins[n].active = 1; n++;   /* after spike row */
    coins[n].x = 965.0f;  coins[n].y = gy; coins[n].active = 1; n++;
    coins[n].x = 1130.0f; coins[n].y = gy; coins[n].active = 1; n++;

    /* ── Screen 3: platform coins ───────────────────────────────── */
    /* Platform 4: x=880, 2-tile */
    coins[n].x = (float)COIN_ON_PLAT_X(880);  coins[n].y = p2; coins[n].active = 1; n++;
    /* Platform 5: x=1050, 3-tile — replaced by a yellow star */

    /* ── Screen 4: ground coins ──────────────────────────────────── */
    coins[n].x = 1230.0f; coins[n].y = gy; coins[n].active = 1; n++;
    coins[n].x = 1390.0f; coins[n].y = gy; coins[n].active = 1; n++;   /* left of red bouncepad */

    /* ── Screen 4: platform coins ───────────────────────────────── */
    /* Platform 6: x=1300, 2-tile */
    coins[n].x = (float)COIN_ON_PLAT_X(1300); coins[n].y = p2; coins[n].active = 1; n++;
    /* Platform 7: x=1480, 2-tile (same height as platform 6) */
    coins[n].x = (float)COIN_ON_PLAT_X(1480); coins[n].y = p2; coins[n].active = 1; n++;

    *count = n;   /* 20 coins total */
}

/* ------------------------------------------------------------------ */

/*
 * coins_render — Draw every active coin to the back buffer.
 *
 * Uses SDL_RenderCopy to blit the full Coin.png texture (NULL source rect
 * means "use the entire image") scaled to COIN_DISPLAY_W × COIN_DISPLAY_H.
 * Inactive coins are skipped so collected coins disappear immediately.
 */
void coins_render(const Coin *coins, int count,
                  SDL_Renderer *renderer, SDL_Texture *tex, int cam_x)
{
    for (int i = 0; i < count; i++) {
        if (!coins[i].active) continue;

        /*
         * World → screen: subtract cam_x from x so the coin scrolls with
         * the camera.  y is unaffected (no vertical scrolling).
         * Coins outside the viewport produce a dst.x outside [0, GAME_W]
         * and are silently discarded by SDL's internal clipping.
         */
        SDL_Rect dst = {
            (int)coins[i].x - cam_x,
            (int)coins[i].y,
            COIN_DISPLAY_W,
            COIN_DISPLAY_H
        };

        /*
         * SDL_RenderCopy — blit the full Coin.png (NULL src = whole texture)
         * scaled to COIN_DISPLAY_W × COIN_DISPLAY_H at the screen position.
         */
        SDL_RenderCopy(renderer, tex, NULL, &dst);
    }
}
