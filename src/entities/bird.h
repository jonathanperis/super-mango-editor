/*
 * bird.h — Public interface for the Bird enemy (slow, curved patrol).
 *
 * Sprite facts (Bird_2.png, 144×48):
 *   - 3 animation frames, each 48×48 px in the sheet.
 *   - Visible art occupies x=17..31 (15 px), y=17..30 (14 px) per frame.
 *
 * Behaviour:
 *   - Flies along a sine-wave patrol path in the sky.
 *   - Reverses direction at patrol_x0 and patrol_x1.
 *   - The sprite is flipped horizontally based on flight direction.
 *   - Deals hurt damage on contact (same as spiders).
 */
#pragma once

#include <SDL.h>
#include <SDL_mixer.h>

/* ---- Constants ---------------------------------------------------------- */

#define MAX_BIRDS       16
#define BIRD_FRAMES      3       /* animation frames in the sheet            */
#define BIRD_FRAME_W     48      /* width  of one frame slot (px)            */
#define BIRD_ART_X       17      /* first visible col within each frame      */
#define BIRD_ART_W       15      /* width  of visible art (cols 17..31)      */
#define BIRD_ART_Y       17      /* first visible row within each frame      */
#define BIRD_ART_H       14      /* height of visible art (rows 17..30)      */
#define BIRD_SPEED       45.0f   /* horizontal flight speed in px/s          */
#define BIRD_FRAME_MS    140     /* ms each animation frame is held          */

/*
 * Sine-wave patrol curve parameters.
 * BIRD_WAVE_AMP  — vertical amplitude in logical pixels (peak-to-peak / 2).
 * BIRD_WAVE_FREQ — how many full sine cycles per logical pixel of horizontal
 *                  travel.  Lower = wider, lazier curves.
 */
#define BIRD_WAVE_AMP    20.0f
#define BIRD_WAVE_FREQ   0.015f

/* ---- Types -------------------------------------------------------------- */

typedef struct {
    float  x;              /* left edge of the frame slot in world space     */
    float  base_y;         /* centre of the sine wave in logical pixels      */
    float  vx;             /* horizontal velocity; sign = direction          */
    float  patrol_x0;      /* left patrol boundary                          */
    float  patrol_x1;      /* right patrol boundary                         */
    int    frame_index;    /* current animation frame (0–2)                  */
    Uint32 anim_timer_ms;  /* accumulator for frame advances                */
} Bird;

/* ---- Function declarations ---------------------------------------------- */

void birds_init(Bird *birds, int *count);

/*
 * birds_update — Move and animate birds.
 *
 * snd_flap  : wing flap SFX, played once per animation cycle per bird.
 * player_x  : player's world-space x (for distance-based volume).
 * cam_x     : camera left edge (sound only plays when bird is on-screen).
 */
void birds_update(Bird *birds, int count, float dt,
                  Mix_Chunk *snd_flap, float player_x, int cam_x);

void birds_render(const Bird *birds, int count,
                  SDL_Renderer *renderer, SDL_Texture *tex, int cam_x);

/*
 * bird_get_hitbox — Return the screen-space AABB for collision checks.
 * The hitbox matches the visible art bounds, offset by the sine-wave y.
 */
SDL_Rect bird_get_hitbox(const Bird *b);
