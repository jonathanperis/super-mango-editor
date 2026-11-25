/*
 * collectible_defs.h — Level limits and display sizes of the collectibles.
 *
 * Only #defines live here, and nothing is included: no raylib, no runtime
 * structs.  The level format (levels/level.h), the serializer and the
 * editor need these numbers to size placement arrays and to draw or
 * hit-test a coin or star; the game uses the same numbers.  See
 * entities/enemy_defs.h for why these small headers exist.
 *
 * The scoring rules (COIN_SCORE, SCORE_PER_LIFE) stay in coin.h.
 */
#pragma once

/* ---- Coins (coin.h) ----------------------------------------------------- */

#define MAX_COINS       64     /* coin slots across the full world         */
#define COIN_DISPLAY_W  16     /* render width  in logical pixels          */
#define COIN_DISPLAY_H  16     /* render height in logical pixels          */

/* ---- Health stars (health_star.h) --------------------------------------- */

#define MAX_STAR_YELLOWS       16    /* maximum yellow stars per level         */
#define MAX_STAR_GREENS        16    /* maximum green stars per level          */
#define MAX_STAR_REDS          16    /* maximum red stars per level            */
#define HEALTH_STAR_DISPLAY_W  16    /* render width  in logical pixels        */
#define HEALTH_STAR_DISPLAY_H  16    /* render height in logical pixels        */

/* ---- Last star (last_star.h) -------------------------------------------- */

#define LAST_STAR_DISPLAY_W  24   /* on-screen width  in logical pixels  */
#define LAST_STAR_DISPLAY_H  24   /* on-screen height in logical pixels  */
