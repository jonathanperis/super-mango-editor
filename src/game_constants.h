/*
 * game_constants.h — The game's plain numbers: window and logical screen
 * size, tile and floor geometry, gravity, level limits and camera tuning.
 *
 * game.h includes this file, so code that includes game.h sees all of it.
 * Code that only needs these numbers (a hazard that sits on the floor, the
 * level validator, an editor panel) includes this header instead: it pulls
 * in no other header, so editing GameState or one of the ~34 headers
 * game.h needs for it does not recompile those files, and the #include
 * line says honestly that they never touch GameState.
 *
 * Only #defines belong here.  Types and anything that needs another
 * header stay in game.h or the module that owns them.
 */
#pragma once

/* ------------------------------------------------------------------ */
/* Window and world                                                    */
/* ------------------------------------------------------------------ */

#define WINDOW_TITLE  "Super Mango"   /* title bar text                */
#define WINDOW_W      800             /* OS window width  in pixels    */
#define WINDOW_H      600             /* OS window height in pixels    */
#define TARGET_FPS    60             /* desired frames per second      */

/*
 * GAME_W / GAME_H — the internal (logical) rendering resolution.
 *
 * All game objects are positioned and sized in this coordinate space.
 * The render target scales this canvas up to fill the OS window, giving
 * a 2× pixel scale (800/400 = 2, 600/300 = 2). This makes every sprite
 * and tile appear twice as large on screen without changing any game logic.
 */
#define GAME_W        400
#define GAME_H        300

/*
 * TILE_SIZE — display size of one grass tile in pixels.
 * The Grass_Tileset.png is 48×48; we render it at its natural size.
 */
#define TILE_SIZE     48

/*
 * FLOOR_Y — the Y coordinate of the top edge of the floor.
 * Anything at or below this Y is "inside" the floor.
 * Uses GAME_H because all positions live in logical (400×300) space.
 */
#define FLOOR_Y       (GAME_H - TILE_SIZE)

/*
 * GRAVITY — downward acceleration in pixels per second squared.
 * Applied every frame so the player accelerates toward the floor.
 */
#define GRAVITY       800.0f

/*
 * WORLD_W — total logical width of the level in pixels.
 * The visible window is still GAME_W (400 px); the camera scrolls to reveal
 * the rest. WORLD_W = 4 × GAME_W gives four screens of horizontal space.
 */
#define WORLD_W       1600

/*
 * FLOOR_GAP_W — width of each floor gap in logical pixels.
 * MAX_FLOOR_GAPS — maximum number of gaps the level can hold.
 *
 * Floor gaps are holes in the ground floor that expose the water below.
 * Falling into any gap costs a life (instant death, not a hurt point).
 * Each gap is defined by its left-edge x coordinate; all are FLOOR_GAP_W wide.
 */
#define FLOOR_GAP_W         32
#define MAX_FLOOR_GAPS      16

/*
 * FLOOR_PIECE_W — width of one 9-slice floor piece (TILE_SIZE / 3 = 16 px).
 *
 * The floor is drawn in whole pieces starting at x = 0, and a piece is left
 * out only when it lies completely inside a gap. A gap whose left edge is
 * off this grid would therefore be drawn half covered by grass while the
 * player still falls through the full FLOOR_GAP_W, so the level validator
 * requires every gap to start on a multiple of FLOOR_PIECE_W.
 */
#define FLOOR_PIECE_W       (TILE_SIZE / 3)
#define MAX_CHECKPOINTS     99
#define MAX_LEVEL_SCREENS   99
/* Upper magnitude for authored motion values; keeps integration and render
 * conversions bounded, well above the shipped speeds/accelerations. */
#define MAX_LEVEL_MOTION    10000

/*
 * MAX_PATROL_SPEED — fastest |vx| a level may give a patrolling enemy, px/s.
 *
 * Spiders look for floor gaps once per fixed step (1/TARGET_FPS s): is the
 * centre of their art over a gap right now? A spider that moved a whole gap
 * width in one step could be on one side of the gap in one step and past it
 * in the next, and would walk over the hole. Half a gap per step
 * (32 / 2 × 60 = 960 px/s) leaves a safe margin for float rounding. It is
 * still eight times the fastest shipped enemy, so one limit serves every
 * patrolling enemy, birds and fish too. A vx of 0 is rejected as well: the
 * enemy would never move.
 */
#define MAX_PATROL_SPEED    (FLOOR_GAP_W * TARGET_FPS / 2)
#define GAME_LEVEL_PATH_MAX 1024 /* UTF-8 native document path; matches editor capacity */

/* ------------------------------------------------------------------ */
/* Camera tuning                                                       */
/* ------------------------------------------------------------------ */

/*
 * CAM_LOOKAHEAD_VX_FACTOR — how many pixels of lookahead per px/s of player
 * horizontal velocity.  The lookahead scales continuously with vx: at rest
 * it is exactly 0 (player centred), at full run speed it peaks near
 * CAM_LOOKAHEAD_MAX.  The camera therefore reveals more terrain the faster
 * the player is moving, and smoothly recentres when they stop.
 *
 * Example: factor 0.20 × 220 px/s (run max) = 44 px of lookahead.
 */
#define CAM_LOOKAHEAD_VX_FACTOR  0.20f

/*
 * CAM_LOOKAHEAD_MAX — hard cap (pixels) on the lookahead in either direction.
 * Prevents the offset from growing excessively if vx ever exceeds normal max.
 */
#define CAM_LOOKAHEAD_MAX  50.0f

/*
 * CAM_SMOOTHING — lerp speed factor (dimensionless, applied per second).
 * Each frame the camera closes (CAM_SMOOTHING × dt) of the remaining gap to
 * the target. 8.0 gives responsive follow without snapping. Lower = laggier.
 */
#define CAM_SMOOTHING  8.0f

/*
 * CAM_SNAP_THRESHOLD — when the remaining gap between cam_x and its target
 * is smaller than this many pixels, snap exactly instead of lerping.
 * Prevents endless sub-pixel micro-drift each frame.
 */
#define CAM_SNAP_THRESHOLD  0.5f
