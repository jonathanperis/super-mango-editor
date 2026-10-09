/*
 * faster_fish.h — Public interface for the faster jumping fish enemy.
 *
 * FasterFish is a variant of the Fish enemy with increased patrol speed
 * and a higher jump arc.  It uses the same Fish_2.png sprite sheet
 * (96×48, two 48×48 frames), the same size, hitbox and rendering, and the
 * same update code in fish.c — only the FFISH_* tuning below differs.
 */
#pragma once

#include "fish.h"   /* Fish, FishSpec, FISH_* sprite and hitbox sizes */

#define MAX_FASTER_FISH        16
/* FFISH_SPEED — the usual patrol speed, 120 px/s (fish: 70); the level's
 * vx is the speed actually used (shipped levels author this value). */
#define FFISH_SPEED           120.0f
#define FFISH_JUMP_VY        -420.0f   /* higher jump: -420 px/s (vs -280)      */
#define FFISH_JUMP_MIN         1.0f    /* shorter delay between jumps           */
#define FFISH_JUMP_MAX         2.2f
#define FFISH_FRAME_MS        100      /* slightly faster animation (100 ms)    */

/*
 * FasterFish — state for one fast aquatic enemy.
 *
 * The state is exactly a Fish; the separate name keeps GameState and level
 * loading readable (gs->faster_fish[i] is clearly the fast variant).
 */
typedef Fish FasterFish;

/* Move fish, trigger jumps, and advance animation (fish.c with FFISH_* tuning). */
void faster_fish_update(FasterFish *fish, int count, float dt, int world_w);

/* Draw all faster fish with camera offset. */
void faster_fish_render(const FasterFish *fish, int count,
                        Texture2D *tex, int cam_x);

/* Return a slightly inset collision box. */
IntRect faster_fish_get_hitbox(const FasterFish *fish);
