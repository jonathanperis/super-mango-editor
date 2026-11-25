/*
 * surface_defs.h — Level limits, sprite sizes and modes of the surfaces
 *                  (platforms, bouncepads, rails, bridges and climbables).
 *
 * Only #defines and enums live here, and nothing is included: no raylib,
 * no runtime structs.  The level format (levels/level.h), the serializer
 * and the editor need these to size placement arrays, read a platform's
 * mode from TOML and draw or hit-test a surface; the game uses the same
 * values.  See entities/enemy_defs.h for why these small headers exist.
 *
 * Behaviour tuning (bounce impulses, crumble timing...) stays in each
 * surface's own header, next to the code that uses it.
 */
#pragma once

/* ---- Platforms (platform.h) --------------------------------------------- */

/*
 * MAX_PLATFORMS — upper bound on how many platforms the game can hold.
 * Stored as a fixed-size array inside GameState; no dynamic allocation needed.
 */
#define MAX_PLATFORMS 32

/* ---- Float platforms (float_platform.h) --------------------------------- */

/*
 * FLOAT_PLATFORM_PIECE_W / _H — size of each 3-slice piece in logical pixels.
 * Platform.png is 48×16, so each of the three pieces is 16×16.
 */
#define FLOAT_PLATFORM_PIECE_W  16
#define FLOAT_PLATFORM_H        16

#define MAX_FLOAT_PLATFORMS 16   /* max FloatPlatform instances in one level */

/*
 * FloatPlatformMode — which of the three behaviours this platform uses.
 * Set once at init; never changes during a play session.
 */
typedef enum {
    FLOAT_PLATFORM_STATIC,   /* hovers at a fixed world position forever     */
    FLOAT_PLATFORM_CRUMBLE,  /* falls after the player stands on it          */
    FLOAT_PLATFORM_RAIL,     /* follows a Rail path (closed loop or bouncing)*/
} FloatPlatformMode;

/* ---- Bouncepads (bouncepad.h) ------------------------------------------- */

/* Display size of one bouncepad frame in logical pixels. */
#define BOUNCEPAD_W      48
#define BOUNCEPAD_H      48

/*
 * BOUNCEPAD_SRC_Y / BOUNCEPAD_SRC_H — vertical crop of the sprite sheet.
 *
 * Pixel analysis of Bouncepad_Wood.png shows the actual art occupies
 * rows 14–31 (18 px) within each 48-px frame; the remaining rows are
 * fully transparent padding.  Cropping to this region lets us position
 * the pad so its art bottom lands exactly on FLOOR_Y without rendering
 * invisible transparent space below it.
 */
#define BOUNCEPAD_SRC_Y  14   /* first non-transparent row in the frame  */
#define BOUNCEPAD_SRC_H  18   /* height of the art region (rows 14–31)   */

/*
 * BOUNCEPAD_ART_X / BOUNCEPAD_ART_W — horizontal crop of each frame.
 *
 * The art occupies cols 16–31 (16 px) within each 48-px frame slot;
 * the left 16 px and right 16 px are fully transparent padding.
 * These constants define the collision box width so it matches the
 * visible art, not the full frame slot.
 */
#define BOUNCEPAD_ART_X  16   /* first non-transparent col in the frame  */
#define BOUNCEPAD_ART_W  16   /* width  of the art region (cols 16–31)   */

/*
 * The three bouncepad arrays of a level, one per sprite: small pads use
 * bouncepad_small.png, medium bouncepad_medium.png, high bouncepad_high.png.
 */
#define MAX_BOUNCEPADS_SMALL  16
#define MAX_BOUNCEPADS_MEDIUM 16
#define MAX_BOUNCEPADS_HIGH   16

/*
 * BouncepadType — the pad colour named in the level file (pad_type).
 *
 * It is validated and saved back to the TOML, but the game never reads it:
 * the texture comes from which array the pad is in (bouncepads_small,
 * _medium or _high, drawn in game_render.c) and the launch from launch_vy.
 */
typedef enum {
    BOUNCEPAD_GREEN,   /* usual for small pads  (bouncepad_small.png)  */
    BOUNCEPAD_WOOD,    /* usual for medium pads (bouncepad_medium.png) */
    BOUNCEPAD_RED      /* usual for high pads   (bouncepad_high.png)   */
} BouncepadType;

/* ---- Rails (rail.h) ----------------------------------------------------- */

/*
 * Rails.png is 64×64 px arranged as a 4×4 grid.
 * Each tile in the sheet is RAIL_TILE_W × RAIL_TILE_H pixels.
 */
#define RAIL_TILE_W   16   /* width  of one tile in the sprite sheet, px */
#define RAIL_TILE_H   16   /* height of one tile in the sprite sheet, px */

#define MAX_RAIL_TILES  128   /* max tiles in a single Rail path      */
#define MAX_RAILS        16   /* max Rail instances in one level      */

/*
 * MAX_RAIL_SPEED — fastest speed, in tiles per second, a level may give
 * anything that rides a rail (spike blocks and float platforms).
 *
 * At the fixed 60 Hz step, 30 tiles/s moves a rider 30 × 16 / 60 = 8 px
 * per step: half a rail tile, and less than the player's 18 px wide hitbox,
 * so a spike block cannot pass through the player between two hitbox
 * checks. (A float platform carries its rider by however far it moved, and
 * landings are tested against the platform's movement, so riders do not
 * set this limit.) The shipped levels use at most 6 tiles/s. Speeds must
 * also be positive: an open rail turns
 * its rider around at each end, and a negative speed would push it back
 * past the start every step, so it would never move.
 */
#define MAX_RAIL_SPEED   30

/* ---- Bridges (bridge.h) ------------------------------------------------- */

#define MAX_BRIDGES         16
#define MAX_BRIDGE_BRICKS   16     /* max bricks in a single bridge          */
#define BRIDGE_TILE_W       16     /* width  of one Bridge.png tile (px)     */
#define BRIDGE_TILE_H       16     /* height of one Bridge.png tile (px)     */

/* ---- Vines (vine.h) ----------------------------------------------------- */

#define MAX_VINES   24              /* upper bound on decoration instances   */
#define VINE_W      16              /* sprite width  (natural size)          */
#define VINE_H      32              /* content height after removing 8 px transparent padding top+bottom */
#define VINE_SRC_Y   8              /* first pixel row with content in Vine.png */
#define VINE_SRC_H  32              /* height of content area in Vine.png    */
#define VINE_STEP   19              /* vertical spacing between stacked tiles */

/*
 * VineType — visual variant for vine decorations.
 *
 * VINE_GREEN : lush green vine (forest/fertile themes)
 * VINE_BROWN : dried brown vine (arid/volcanic/cave themes)
 */
typedef enum {
    VINE_GREEN = 0,
    VINE_BROWN
} VineType;

/* ---- Ladders (ladder.h) ------------------------------------------------- */

#define MAX_LADDERS   16
#define LADDER_W      16              /* sprite width  (px)                    */
#define LADDER_H      22              /* content height after cropping padding */
#define LADDER_SRC_Y  13              /* first pixel row with content          */
#define LADDER_SRC_H  22              /* height of content area (rows 13–34)   */
#define LADDER_STEP    8              /* tight overlap so tiles merge visually */

/* ---- Ropes (rope.h) ----------------------------------------------------- */

#define MAX_ROPES    16
#define ROPE_W       16              /* display width (full sprite width)       */
#define ROPE_H       36              /* display height with padding cropped       */
#define ROPE_SRC_X   0               /* source crop x (no crop, use full width)   */
#define ROPE_SRC_Y   6               /* source crop y (6 px top padding)          */
#define ROPE_SRC_W  16               /* source crop width (full sprite width)    */
#define ROPE_SRC_H  36               /* source crop height                       */
#define ROPE_STEP   23               /* vertical spacing for 13px overlap (36-23)*/
