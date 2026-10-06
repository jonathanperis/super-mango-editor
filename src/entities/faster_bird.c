/*
 * faster_bird.c — Faster bird enemy: quick sine-wave patrol across the sky.
 *
 * Same mechanics as the regular bird but with higher speed, faster wing
 * animation, and a tighter sine-wave frequency for more aggressive curves.
 */
#include "faster_bird.h"
#include "bird_variant.h"

/* ------------------------------------------------------------------ */

void faster_birds_update(FasterBird *birds, int count, float dt,
                         SoundEffect *snd_flap, float player_x, int cam_x)
{
    const BirdVariantSpec *spec = bird_variant_spec(BIRD_VARIANT_FAST);

    for (int i = 0; i < count; i++) {
        FasterBird *b = &birds[i];

        bird_variant_update(spec, &b->x, &b->vx, b->patrol_x0, b->patrol_x1,
                            &b->frame_index, &b->anim_timer_ms, dt,
                            snd_flap, player_x, cam_x);
    }
}

/* ------------------------------------------------------------------ */

IntRect faster_bird_get_hitbox(const FasterBird *b)
{
    const BirdVariantSpec *spec = bird_variant_spec(BIRD_VARIANT_FAST);
    return bird_variant_hitbox(spec, b->x, b->base_y);
}

/* ------------------------------------------------------------------ */

void faster_birds_render(const FasterBird *birds, int count,
                         Texture2D *tex, int cam_x)
{
    const BirdVariantSpec *spec = bird_variant_spec(BIRD_VARIANT_FAST);

    for (int i = 0; i < count; i++) {
        const FasterBird *b = &birds[i];
        bird_variant_render(spec, b->x, b->base_y, b->vx, b->frame_index,
                            tex, cam_x);
    }
}
