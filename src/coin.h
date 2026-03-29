/*
 * coin.h — Public interface for the coin collectible module.
 *
 * Coins are small static collectibles placed on the ground and on platforms.
 * The player picks them up by walking through them (AABB overlap).
 * Each coin awards COIN_SCORE points; collecting COINS_PER_HEART coins
 * restores one heart if the player has fewer than MAX_HEARTS.
 */
#pragma once

#include <SDL.h>

#define MAX_COINS       24     /* coin slots across the full world         */
#define COIN_DISPLAY_W  16     /* render width  in logical pixels          */
#define COIN_DISPLAY_H  16     /* render height in logical pixels          */
#define COIN_SCORE     100     /* score awarded per coin collected         */
#define COINS_PER_HEART  3     /* coins needed to restore one heart        */

/*
 * Coin — state for one coin collectible.
 *
 * x, y   : top-left position in logical pixels (400×300 space).
 * active : 1 = visible and collectible, 0 = already collected this phase.
 */
typedef struct {
    float x;              /* horizontal position in logical pixels         */
    float y;              /* vertical   position in logical pixels         */
    int   active;         /* 1 = on screen, 0 = collected                  */
} Coin;

/*
 * coins_init — Place the initial set of coins on the ground and platforms.
 *
 * Sets *count to the number of coins placed (currently 5).  Can be called
 * again to re-activate all coins when the phase restarts.
 */
void coins_init(Coin *coins, int *count);

/*
 * coins_render — Draw all active coins using the shared coin texture.
 *
 * Inactive coins (active == 0) are skipped.  Each coin is drawn at its
 * position scaled to COIN_DISPLAY_W × COIN_DISPLAY_H logical pixels.
 * cam_x is the camera left-edge offset (world px); subtract it from dst.x
 * to convert world coordinates to screen coordinates.
 */
void coins_render(const Coin *coins, int count,
                  SDL_Renderer *renderer, SDL_Texture *tex, int cam_x);
