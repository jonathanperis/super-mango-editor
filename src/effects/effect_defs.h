/*
 * effect_defs.h — Level limits and art sizes of the background effects
 *                 (parallax layers, fog and the water strip).
 *
 * Only #defines live here, and nothing is included: no raylib, no runtime
 * structs.  The level format (levels/level.h) sizes its layer arrays with
 * these limits, and the editor draws the water strip and derives the fish
 * lane from its height, without pulling in the effect modules themselves.
 * See entities/enemy_defs.h for why these small headers exist.
 */
#pragma once

/* ---- Parallax background (parallax.h) ----------------------------------- */

/*
 * MAX_BACKGROUND_LAYERS — maximum number of background layers the system can hold.
 * Stored as a fixed-size array; no heap allocation needed.  Foreground
 * layers use the same limit.
 */
#define MAX_BACKGROUND_LAYERS  8

/* ---- Fog (fog.h) -------------------------------------------------------- */

/*
 * MAX_FOG_TEXTURES — maximum number of fog texture assets the system can hold.
 * Actual count is determined per-level via the fog_layers[] in LevelDef.
 */
#define MAX_FOG_TEXTURES  4

/* ---- Water strip (water.h, Water.png 384×64) ----------------------------- */

#define WATER_FRAMES        8    /* frames in the sheet                    */
#define WATER_FRAME_W      48    /* full slot width in the sheet (px)      */
#define WATER_ART_DX       16    /* left offset to art within each slot    */
#define WATER_ART_W        16    /* width of actual art pixels per frame   */
#define WATER_ART_Y        17    /* first visible row                      */
#define WATER_ART_H        31    /* height of visible art                  */
#define WATER_PERIOD      (WATER_FRAMES * WATER_ART_W)  /* 128 px repeat  */
