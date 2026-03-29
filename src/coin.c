/*
 * coin.c — Coin placement and rendering.
 *
 * Places 5 coins in the level: 3 on the ground floor and 1 on top of
 * each of the two pillar platforms.  Coins are static (no animation or
 * physics); collection is handled in game.c via AABB overlap with the
 * player's physics hitbox.
 */

#include "coin.h"
#include "game.h"   /* FLOOR_Y */

/* ------------------------------------------------------------------ */

/*
 * coins_init — Define the starting positions for all coins.
 *
 * Ground coins sit just above FLOOR_Y so their bottom edge touches the
 * grass surface.  Platform coins sit on the top surface of each pillar.
 *
 * Coin layout (logical 400×300 space):
 *
 *   Ground coins (y = FLOOR_Y − COIN_DISPLAY_H = 252 − 16 = 236):
 *     Coin 0: x = 30   (left third of the floor)
 *     Coin 1: x = 170  (center of the floor)
 *     Coin 2: x = 350  (right third of the floor)
 *
 *   Platform coins:
 *     Coin 3: x = 96,  y = 140  (centered on platform 0; x=80, y=156, w=48)
 *     Coin 4: x = 272, y = 92   (centered on platform 1; x=256, y=108, w=48)
 */
void coins_init(Coin *coins, int *count)
{
    *count = 5;

    /* ── 3 coins on the ground floor ─────────────────────────────── */
    coins[0].x      = 30.0f;
    coins[0].y      = (float)(FLOOR_Y - COIN_DISPLAY_H);
    coins[0].active = 1;

    coins[1].x      = 170.0f;
    coins[1].y      = (float)(FLOOR_Y - COIN_DISPLAY_H);
    coins[1].active = 1;

    coins[2].x      = 350.0f;
    coins[2].y      = (float)(FLOOR_Y - COIN_DISPLAY_H);
    coins[2].active = 1;

    /* ── 1 coin on top of platform 0 (x=80, y=156, w=48) ────────── */
    /*
     * Centered horizontally: platform center = 80 + 48/2 = 104.
     * Coin left edge = 104 − COIN_DISPLAY_W/2 = 104 − 8 = 96.
     * Sitting on top:  y = 156 − COIN_DISPLAY_H = 156 − 16 = 140.
     */
    coins[3].x      = 96.0f;
    coins[3].y      = 140.0f;
    coins[3].active = 1;

    /* ── 1 coin on top of platform 1 (x=256, y=108, w=48) ───────── */
    /*
     * Centered horizontally: platform center = 256 + 48/2 = 280.
     * Coin left edge = 280 − COIN_DISPLAY_W/2 = 280 − 8 = 272.
     * Sitting on top:  y = 108 − COIN_DISPLAY_H = 108 − 16 = 92.
     */
    coins[4].x      = 272.0f;
    coins[4].y      = 92.0f;
    coins[4].active = 1;
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
                  SDL_Renderer *renderer, SDL_Texture *tex)
{
    for (int i = 0; i < count; i++) {
        if (!coins[i].active) continue;

        SDL_Rect dst = {
            (int)coins[i].x,
            (int)coins[i].y,
            COIN_DISPLAY_W,
            COIN_DISPLAY_H
        };

        /*
         * SDL_RenderCopy — draw the coin texture at the destination rect.
         *   renderer → GPU drawing context
         *   tex      → shared Coin.png texture (entire image)
         *   NULL     → source rect: use the full texture (single-frame asset)
         *   &dst     → destination: position and display size
         */
        SDL_RenderCopy(renderer, tex, NULL, &dst);
    }
}
