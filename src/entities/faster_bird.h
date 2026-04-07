/*
 * faster_bird.h — Public interface for the Faster Bird enemy.
 *
 * Sprite facts (Bird_1.png, 144×48):
 *   - 3 animation frames, each 48×48 px in the sheet.
 *   - Visible art occupies x=17..31 (15 px), y=17..30 (14 px) per frame.
 *   - Same layout as Bird_2.png but different colours.
 *
 * Behaviour:
 *   - Faster horizontal speed and quicker wing animation than the Bird.
 *   - Tighter, more aggressive sine-wave curve (higher frequency).
 *   - Deals hurt damage on contact.
 */
#pragma once

#include <SDL.h>
#include <SDL_mixer.h>

/* ---- Constants ---------------------------------------------------------- */

#define MAX_FASTER_BIRDS    16
#define FBIRD_FRAMES         3       /* animation frames in the sheet         */
#define FBIRD_FRAME_W        48      /* width  of one frame slot (px)         */
#define FBIRD_ART_X          17      /* first visible col within each frame   */
#define FBIRD_ART_W          15      /* width  of visible art (cols 17..31)   */
#define FBIRD_ART_Y          17      /* first visible row within each frame   */
#define FBIRD_ART_H          14      /* height of visible art (rows 17..30)   */
#define FBIRD_SPEED          80.0f   /* horizontal speed — nearly 2× the bird */
#define FBIRD_FRAME_MS       90      /* faster wing animation                 */

/*
 * Faster sine-wave: tighter curves make the flight more erratic.
 */
#define FBIRD_WAVE_AMP       15.0f
#define FBIRD_WAVE_FREQ      0.025f

/* ---- Types -------------------------------------------------------------- */

typedef struct {
    float  x;
    float  base_y;
    float  vx;
    float  patrol_x0;
    float  patrol_x1;
    int    frame_index;
    Uint32 anim_timer_ms;
} FasterBird;

/* ---- Function declarations ---------------------------------------------- */

void faster_birds_init(FasterBird *birds, int *count, int world_w);

void faster_birds_update(FasterBird *birds, int count, float dt,
                         Mix_Chunk *snd_flap, float player_x, int cam_x);

void faster_birds_render(const FasterBird *birds, int count,
                         SDL_Renderer *renderer, SDL_Texture *tex, int cam_x);

SDL_Rect faster_bird_get_hitbox(const FasterBird *b);
