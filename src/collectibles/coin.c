/*
 * coin.c — Coin rendering.  Placement is handled by the level loader
 * from LevelDef data; this file only provides the render function.
 */

#include "coin.h"
#include "game.h"   /* GAME_W */

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
