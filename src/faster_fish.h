/*
 * faster_fish.h — Public interface for the faster jumping fish enemy.
 *
 * FasterFish is a variant of the Fish enemy with increased patrol speed
 * and a higher jump arc.  It uses the same Fish_2.png sprite sheet
 * (96×48, two 48×48 frames) and identical rendering logic.
 */
#pragma once

#include <SDL.h>

#define MAX_FASTER_FISH         4
#define FFISH_FRAMES            2
#define FFISH_FRAME_W          48
#define FFISH_FRAME_H          48
#define FFISH_RENDER_W         48
#define FFISH_RENDER_H         48
#define FFISH_SPEED           120.0f   /* faster patrol: 120 px/s (vs 70)       */
#define FFISH_JUMP_VY        -420.0f   /* higher jump: -420 px/s (vs -280)      */
#define FFISH_JUMP_MIN         1.0f    /* shorter delay between jumps           */
#define FFISH_JUMP_MAX         2.2f
#define FFISH_HITBOX_PAD_X    16       /* same inset as regular fish            */
#define FFISH_HITBOX_PAD_Y    13
#define FFISH_FRAME_MS        100      /* slightly faster animation (100 ms)    */

/*
 * FasterFish — state for one fast aquatic enemy.
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
} FasterFish;

/* Place initial faster fish instances. */
void faster_fish_init(FasterFish *fish, int *count);

/* Move fish, trigger jumps, and advance animation. */
void faster_fish_update(FasterFish *fish, int count, float dt);

/* Draw all faster fish with camera offset. */
void faster_fish_render(const FasterFish *fish, int count,
                        SDL_Renderer *renderer, SDL_Texture *tex, int cam_x);

/* Return a slightly inset collision box. */
SDL_Rect faster_fish_get_hitbox(const FasterFish *fish);
