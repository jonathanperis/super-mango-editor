/*
 * coin.h — Public interface for the coin collectible module.
 *
 * Coins are small static collectibles placed on the ground and on platforms.
 * The player picks them up by walking through them (AABB overlap).
 * Each coin awards COIN_SCORE points.  Every SCORE_PER_LIFE points the
 * player earns a bonus life.
 */
#pragma once

#include "../shared/graphics.h"
#include <stdint.h>  /* uint32_t */

#define MAX_COINS       64     /* coin slots across the full world         */
#define COIN_DISPLAY_W  16     /* render width  in logical pixels          */
#define COIN_DISPLAY_H  16     /* render height in logical pixels          */
#define COIN_SCORE     100     /* score awarded per coin collected         */
#define SCORE_PER_LIFE 1000    /* score multiple that grants a bonus life  */

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
 * coins_render — Draw all active coins using the shared coin texture.
 *
 * Inactive coins (active == 0) are skipped.  Each coin is drawn at its
 * position scaled to COIN_DISPLAY_W × COIN_DISPLAY_H logical pixels.
 * cam_x is the camera left-edge offset (world px); subtract it from dst.x
 * to convert world coordinates to screen coordinates.
 */
void coins_render(const Coin *coins, int count,
                  Texture2D *tex, int cam_x);

/* A coin picked up within this many fixed steps (0.75 s) of the previous
 * one continues the streak; a longer gap starts a new one. */
#define COIN_STREAK_GAP_STEPS 45
/* Semitones the chime can climb: the ninth quick coin and later ones all
 * play eight semitones up. */
#define COIN_STREAK_MAX_RISE 8

/*
 * CoinStreak — the run of quick coin pickups the chime's pitch follows.
 *
 * Audio feedback only: the simulation never reads it, and it reads only
 * the world's step clock, so a streak cannot change a run or a replay.
 * count is the pickups in the current streak (0 = none yet); last_step
 * is the world's sim_steps at the latest one.
 */
typedef struct {
    int      count;
    uint32_t last_step;
} CoinStreak;

/*
 * coin_streak_pitch — Record a coin pickup at step `step` and return the
 * pitch to play the chime at: 1.0 for the first coin of a streak, then one
 * semitone higher per quick pickup, up to COIN_STREAK_MAX_RISE semitones.
 * Deterministic: a fixed table, no randomness, no wall clock.
 */
float coin_streak_pitch(CoinStreak *streak, uint32_t step);
