/*
 * fish.h — Public interface for the jumping fish enemy module.
 *
 * Fish live in the bottom water lane, patrol horizontally, and occasionally
 * perform a short jump arc that can reach the player on the ground floor.
 */
#pragma once

#include <SDL.h>

#define MAX_FISH           16       /* maximum simultaneous fish instances     */
#define FISH_FRAMES         2       /* horizontal frames in Fish_2.png (96×48) */
#define FISH_FRAME_W       48       /* width of one frame slot in the sheet    */
#define FISH_FRAME_H       48       /* height of one frame slot in the sheet   */
#define FISH_RENDER_W      48       /* on-screen render width  (matches player) */
#define FISH_RENDER_H      48       /* on-screen render height (matches player) */
#define FISH_SPEED         70.0f    /* horizontal patrol speed in px/s         */
#define FISH_JUMP_VY     -280.0f    /* upward jump impulse in px/s             */
#define FISH_JUMP_MIN      1.4f     /* minimum seconds before next jump        */
#define FISH_JUMP_MAX      3.0f     /* maximum seconds before next jump        */
/*
 * Pixel analysis: Fish art occupies x=16..31 (16 px), y=13..31 (19 px)
 * within each 48×48 frame.  Pad values trim transparent padding so the
 * hitbox matches the visible art.
 */
#define FISH_HITBOX_PAD_X  16       /* left/right inset → hitbox width  = 16 px */
#define FISH_HITBOX_PAD_Y  13       /* top inset        → hitbox height = 19 px */
#define FISH_FRAME_MS      120      /* ms per swim animation frame (fast tail flip) */

/*
 * Fish — state for one aquatic enemy.
 *
 * x, y         : world-space top-left position in logical pixels.
 * vx, vy       : horizontal patrol speed and current vertical velocity.
 * patrol_x0/x1 : world-space left/right bounds for patrol reversal.
 * jump_timer   : countdown in seconds until the next jump launch.
 * water_y      : resting top-left y-position when the fish is in the water.
 * frame_index  : current animation frame (0 or 1); advances every FISH_FRAME_MS.
 * anim_timer_ms: accumulator driving frame advances.
 */
typedef struct {
    float  x;
    float  y;
    float  vx;
    float  vy;
    float  patrol_x0;
    float  patrol_x1;
    float  jump_timer;
    float  water_y;
    int    frame_index;
    Uint32 anim_timer_ms;
} Fish;

/* Place the initial fish instances and reset their movement state. */
void fish_init(Fish *fish, int *count);

/* Move fish, trigger random jumps, and advance animation. */
void fish_update(Fish *fish, int count, float dt);

/* Draw all fish with camera-aware world-to-screen conversion. */
void fish_render(const Fish *fish, int count,
                 SDL_Renderer *renderer, SDL_Texture *tex, int cam_x);

/* Return a slightly inset collision box for fair player contact. */
SDL_Rect fish_get_hitbox(const Fish *fish);