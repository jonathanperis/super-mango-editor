/*
 * coin.c — Coin rendering.  Placement is handled by the level loader
 * from LevelDef data; this file only provides the render function.
 */

#include "coin.h"

/* 2^(n/12) for n = 0 .. COIN_STREAK_MAX_RISE: equal-tempered semitones,
 * written out so every build plays exactly the same pitches. */
static const float s_semitones[COIN_STREAK_MAX_RISE + 1] = {
    1.0f, 1.0594631f, 1.1224620f, 1.1892071f, 1.2599210f,
    1.3348399f, 1.4142135f, 1.4983071f, 1.5874011f
};

float coin_streak_pitch(CoinStreak *streak, uint32_t step)
{
    /* Unsigned subtraction: a step clock that went back is "before" the
     * last pickup, a huge gap, so a new streak. Level loads and respawns
     * also clear the streak (level_loader.c). */
    if (streak->count > 0 && step - streak->last_step <= COIN_STREAK_GAP_STEPS) streak->count++;
    else streak->count = 1;
    streak->last_step = step;
    int rise = streak->count - 1;
    if (rise > COIN_STREAK_MAX_RISE) rise = COIN_STREAK_MAX_RISE;
    return s_semitones[rise];
}

/* ------------------------------------------------------------------ */

/*
 * coins_render — Draw every active coin to the back buffer.
 *
 * Draws the full Coin.png texture (NULL source rect
 * means "use the entire image") scaled to COIN_DISPLAY_W × COIN_DISPLAY_H.
 * Inactive coins are skipped so collected coins disappear immediately.
 */
void coins_render(const Coin *coins, int count,
                  Texture2D *tex, int cam_x)
{
    for (int i = 0; i < count; i++) {
        if (!coins[i].active) continue;

        /*
         * World → screen: subtract cam_x from x so the coin scrolls with
         * the camera.  y is unaffected (no vertical scrolling).
         * Coins outside the viewport produce a dst.x outside [0, GAME_W]
         * and are discarded outside the render target.
         */
        IntRect dst = {
            (int)coins[i].x - cam_x,
            (int)coins[i].y,
            COIN_DISPLAY_W,
            COIN_DISPLAY_H
        };

        /*
         * Draw the full Coin.png (NULL src = whole texture)
         * scaled to COIN_DISPLAY_W × COIN_DISPLAY_H at the screen position.
         */
        sprite_draw(tex, NULL, &dst, 0, SPRITE_NORMAL, WHITE);
    }
}
