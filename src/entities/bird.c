/*
 * bird.c — Bird enemies: sine-wave patrol across the sky.
 *
 * Each bird flies horizontally while oscillating vertically along a sine
 * curve centred at base_y, and reverses at its patrol boundaries.
 *
 * This file owns the flight behaviour for BOTH bird variants. The regular
 * bird and the faster bird (faster_bird.c) differ only in tuning — speed,
 * wing animation and wave shape — which each variant passes in as a
 * BirdSpec. fish.c does the same for the two fish.
 */
#include "bird.h"

#include <math.h>   /* fabsf, sinf */

#include "../core/entity_utils.h"
#include "../game_constants.h"     /* GAME_W */

/* Loudest wing flap, heard right next to the player. */
#define BIRD_FLAP_VOL_MAX 67

/* Tuning for the regular bird; faster_bird.c defines its own BirdSpec. */
static const BirdSpec s_regular_bird = {
    BIRD_FRAMES,
    BIRD_FRAME_W,
    BIRD_ART_X,
    BIRD_ART_Y,
    BIRD_ART_W,
    BIRD_ART_H,
    BIRD_FRAME_MS,
    BIRD_SPEED,
    BIRD_WAVE_AMP,
    BIRD_WAVE_FREQ
};

/* ------------------------------------------------------------------ */

/* The bird's top y at horizontal position x: base_y plus the sine wave. */
static float bird_screen_y(const BirdSpec *spec, float x, float base_y)
{
    return base_y + sinf(x * spec->wave_freq) * spec->wave_amp;
}

/*
 * bird_flap_sound — Play the wing flap once per animation cycle, only while
 * the bird is on screen, quieter the further it is from the player (silent
 * beyond one screen width).
 */
static void bird_flap_sound(const BirdSpec *spec, const Bird *b,
                            SoundEffect *snd_flap, float player_x, int cam_x)
{
    float bird_cx = b->x + (float)spec->frame_w / 2.0f;
    int on_screen = (bird_cx >= (float)cam_x - (float)spec->frame_w &&
                     bird_cx <= (float)cam_x + GAME_W + (float)spec->frame_w);

    if (on_screen) {
        float dist = fabsf(player_x - bird_cx);
        int vol = sound_volume_for_distance(dist, (float)GAME_W,
                                            BIRD_FLAP_VOL_MAX);
        if (vol > 0) {
            sound_play(snd_flap, vol);
        }
    }
}

void bird_variant_update(const BirdSpec *spec, Bird *birds, int count,
                         float dt, SoundEffect *snd_flap,
                         float player_x, int cam_x)
{
    for (int i = 0; i < count; i++) {
        Bird *b = &birds[i];

        patrol_update(&b->x, &b->vx, (float)spec->frame_w,
                      b->patrol_x0, b->patrol_x1, spec->speed, dt);

        /* animate_frame_ms returns 1 when the animation wraps to frame 0,
         * which is one full wing beat. */
        int wrapped = animate_frame_ms(&b->frame_index, &b->anim_timer_ms,
                                       dt, spec->frame_ms, spec->frames);
        if (wrapped && snd_flap) {
            bird_flap_sound(spec, b, snd_flap, player_x, cam_x);
        }
    }
}

void birds_update(Bird *birds, int count, float dt,
                  SoundEffect *snd_flap, float player_x, int cam_x)
{
    bird_variant_update(&s_regular_bird, birds, count, dt,
                        snd_flap, player_x, cam_x);
}

/* ------------------------------------------------------------------ */

IntRect bird_variant_hitbox(const BirdSpec *spec, const Bird *b)
{
    IntRect r;

    r.x = (int)b->x + spec->art_x;
    r.y = (int)bird_screen_y(spec, b->x, b->base_y);
    r.w = spec->art_w;
    r.h = spec->art_h;
    return r;
}

IntRect bird_get_hitbox(const Bird *b)
{
    return bird_variant_hitbox(&s_regular_bird, b);
}

/* ------------------------------------------------------------------ */

void bird_variant_render(const BirdSpec *spec, const Bird *birds, int count,
                         Texture2D *tex, int cam_x)
{
    for (int i = 0; i < count; i++) {
        const Bird *b = &birds[i];
        IntRect src;
        IntRect dst;

        /* Only the visible art rows are cut from the frame, so the sine
         * wave y is the top of the art, matching the hitbox. */
        src.x = b->frame_index * spec->frame_w;
        src.y = spec->art_y;
        src.w = spec->frame_w;
        src.h = spec->art_h;

        dst.x = (int)b->x - cam_x;
        dst.y = (int)bird_screen_y(spec, b->x, b->base_y);
        dst.w = spec->frame_w;
        dst.h = spec->art_h;

        sprite_draw(tex, &src, &dst, 0,
                    b->vx > 0.0f ? SPRITE_FLIP_X : SPRITE_NORMAL, WHITE);
    }
}

void birds_render(const Bird *birds, int count,
                  Texture2D *tex, int cam_x)
{
    bird_variant_render(&s_regular_bird, birds, count, tex, cam_x);
}
