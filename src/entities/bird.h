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

#include "../shared/graphics.h"
#include "../shared/audio.h"

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
    float  anim_timer_ms;  /* ms accumulated toward the next frame advance */
} Bird;

/*
 * BirdSpec — the tuning and sprite facts that differ between bird variants.
 *
 * The regular and faster bird share their state (Bird) and behaviour; only
 * these numbers change. faster_bird.c passes its own BirdSpec to the
 * bird_variant_* functions instead of copying them — the same pattern
 * fish.c and faster_fish.c use with FishSpec.
 */
typedef struct {
    int      frames;     /* animation frames in the sheet                  */
    int      frame_w;    /* width of one frame slot (px)                   */
    int      art_x;      /* first visible column within each frame         */
    int      art_y;      /* first visible row within each frame            */
    int      art_w;      /* width of the visible art (px)                  */
    int      art_h;      /* height of the visible art (px)                 */
    uint32_t frame_ms;   /* ms each animation frame is held                */
    float    speed;      /* horizontal flight speed in px/s (positive)     */
    float    wave_amp;   /* sine-wave amplitude in px                      */
    float    wave_freq;  /* sine cycles per px of horizontal travel        */
} BirdSpec;

/* ---- Function declarations ---------------------------------------------- */

/* Move and animate birds with the given tuning; see birds_update. */
void bird_variant_update(const BirdSpec *spec, Bird *birds, int count,
                         float dt, SoundEffect *snd_flap,
                         float player_x, int cam_x);

/* Draw birds with the given sprite facts and camera offset. */
void bird_variant_render(const BirdSpec *spec, const Bird *birds, int count,
                         Texture2D *tex, int cam_x);

/* Collision box of one bird: its visible art, offset by the sine-wave y. */
IntRect bird_variant_hitbox(const BirdSpec *spec, const Bird *b);

/*
 * birds_update — Move and animate regular birds.
 *
 * snd_flap  : wing flap SFX, played once per animation cycle per bird.
 * player_x  : player's world-space x (for distance-based volume).
 * cam_x     : camera left edge (sound only plays when bird is on-screen).
 */
void birds_update(Bird *birds, int count, float dt,
                  SoundEffect *snd_flap, float player_x, int cam_x);

void birds_render(const Bird *birds, int count,
                  Texture2D *tex, int cam_x);

/*
 * bird_get_hitbox — Return the screen-space AABB for collision checks.
 * The hitbox matches the visible art bounds, offset by the sine-wave y.
 */
IntRect bird_get_hitbox(const Bird *b);
