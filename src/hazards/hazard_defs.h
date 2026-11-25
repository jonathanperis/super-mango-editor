/*
 * hazard_defs.h — Level limits, sprite sizes and modes of the hazards.
 *
 * Only #defines and enums live here, and nothing is included: no raylib,
 * no runtime structs.  The level format (levels/level.h), the serializer
 * and the editor need these to size placement arrays, read a trap's mode
 * from TOML and draw or hit-test a hazard; the game uses the same values.
 * See entities/enemy_defs.h for why these small headers exist.
 *
 * Motion tuning (swing period, spin speed, push impulse...) stays in each
 * hazard's own header, next to the code that uses it.
 */
#pragma once

/* ---- Axe trap (axe_trap.h) ---------------------------------------------- */

/*
 * AXE_FRAME_W / AXE_FRAME_H — source dimensions of Axe_Trap.png.
 *
 * The sprite is a single 48×64 px image (no sprite sheet grid).
 * The handle is at the top; the blade fans out at the bottom.
 */
#define AXE_FRAME_W  48
#define AXE_FRAME_H  64

/*
 * AXE_DISPLAY_W / AXE_DISPLAY_H — on-screen size in logical pixels.
 *
 * Rendered at native sprite size so each pixel maps 1:1 in the 400×300
 * logical canvas (then 2× scaled to the 800×600 OS window).
 */
#define AXE_DISPLAY_W  48
#define AXE_DISPLAY_H  64

/*
 * MAX_AXE_TRAPS — upper bound on how many axe traps the level can hold.
 * Stored as a fixed-size array inside GameState.
 */
#define MAX_AXE_TRAPS 16

/*
 * AxeTrapMode — selects the swing behaviour for each trap instance.
 *
 * AXE_MODE_PENDULUM : grandfather-clock sinusoidal swing (−60° to +60°).
 * AXE_MODE_SPIN     : continuous 360° clockwise rotation.
 */
typedef enum {
    AXE_MODE_PENDULUM,
    AXE_MODE_SPIN
} AxeTrapMode;

/* ---- Blue flame and fire flame (blue_flame.h) --------------------------- */

/*
 * BLUE_FLAME_FRAME_W / BLUE_FLAME_FRAME_H — size of one animation frame in the sheet.
 *
 * blue_flame.png is 96×48: two 48×48 frames laid out horizontally.
 * The fire flame uses the same layout.
 */
#define BLUE_FLAME_FRAME_W  48
#define BLUE_FLAME_FRAME_H  48

/*
 * BLUE_FLAME_DISPLAY_W / BLUE_FLAME_DISPLAY_H — on-screen size in logical pixels.
 *
 * Rendered at 48×48 so the blue flame fits within the sea gap
 * and doesn't look oversized relative to other hazards.  Fire flames are
 * drawn at the same size.
 */
#define BLUE_FLAME_DISPLAY_W  48
#define BLUE_FLAME_DISPLAY_H  48

#define MAX_BLUE_FLAMES 16  /* max blue flame instances in one level */
#define MAX_FIRE_FLAMES 16  /* max fire flame instances in one level */

/* ---- Circular saw (circular_saw.h) -------------------------------------- */

/*
 * SAW_FRAME_W / SAW_FRAME_H — source dimensions of Circular_Saw.png.
 *
 * The sprite is a single 32×32 px image (no sprite sheet grid).
 */
#define SAW_FRAME_W  32
#define SAW_FRAME_H  32

/*
 * SAW_DISPLAY_W / SAW_DISPLAY_H — on-screen size in logical pixels.
 *
 * Rendered at native sprite size so each pixel maps 1:1 in the 400×300
 * logical canvas (then 2× scaled to the 800×600 OS window).
 */
#define SAW_DISPLAY_W  32
#define SAW_DISPLAY_H  32

#define MAX_CIRCULAR_SAWS 16  /* max CircularSaw instances in one level */

/* ---- Spike rows (spike.h) ----------------------------------------------- */

#define MAX_SPIKE_ROWS     16
#define MAX_SPIKE_TILES    16     /* max tiles in a single spike row          */
#define SPIKE_TILE_W       16     /* width  of one Spike.png tile (px)        */
#define SPIKE_TILE_H       16     /* height of one Spike.png tile (px)        */

/* ---- Spike platforms (spike_platform.h) --------------------------------- */

#define MAX_SPIKE_PLATFORMS    16
#define SPIKE_PLAT_PIECE_W     16   /* width  of one 3-slice piece (px)       */
#define SPIKE_PLAT_SRC_Y        5   /* first content row in each piece        */
#define SPIKE_PLAT_SRC_H       11   /* content height (rows 5–15)             */

/* ---- Spike blocks (spike_block.h) --------------------------------------- */

/*
 * SPIKE_DISPLAY_W / SPIKE_DISPLAY_H — on-screen size in logical pixels.
 *
 * Spike_Block.png is a single 16×16 frame.  We display it at 24×24 to make
 * it more visible against rail tiles (which are also 16×16).  Nearest-
 * neighbour scaling (set in game_init) keeps the pixel art sharp.
 */
#define SPIKE_DISPLAY_W  24
#define SPIKE_DISPLAY_H  24

#define MAX_SPIKE_BLOCKS 16   /* max SpikeBlock instances in one level */
