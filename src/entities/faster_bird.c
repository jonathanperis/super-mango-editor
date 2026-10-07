/*
 * faster_bird.c — FasterBird: a fast variant of the sky-patrol bird.
 *
 * Same mechanics as the regular bird but with higher speed, faster wing
 * animation, and a tighter sine-wave frequency for more aggressive curves.
 * The behaviour lives once, in bird.c; this file only supplies the tuning.
 */
#include "faster_bird.h"

static const BirdSpec s_faster_bird = {
    FBIRD_FRAMES,
    FBIRD_FRAME_W,
    FBIRD_ART_X,
    FBIRD_ART_Y,
    FBIRD_ART_W,
    FBIRD_ART_H,
    FBIRD_FRAME_MS,
    FBIRD_SPEED,
    FBIRD_WAVE_AMP,
    FBIRD_WAVE_FREQ
};

/* ------------------------------------------------------------------ */

void faster_birds_update(FasterBird *birds, int count, float dt,
                         SoundEffect *snd_flap, float player_x, int cam_x)
{
    bird_variant_update(&s_faster_bird, birds, count, dt,
                        snd_flap, player_x, cam_x);
}

void faster_birds_render(const FasterBird *birds, int count,
                         Texture2D *tex, int cam_x)
{
    bird_variant_render(&s_faster_bird, birds, count, tex, cam_x);
}

IntRect faster_bird_get_hitbox(const FasterBird *b)
{
    return bird_variant_hitbox(&s_faster_bird, b);
}
