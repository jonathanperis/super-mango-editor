/*
 * bird.c — Bird enemy: slow sine-wave patrol across the sky.
 *
 * Each bird flies horizontally at BIRD_SPEED while oscillating vertically
 * along a sine curve centred at base_y.  The wave amplitude and frequency
 * create a gentle, lazy flight path.  The bird reverses at patrol boundaries.
 */
#include "bird.h"
#include "bird_variant.h"

/* ------------------------------------------------------------------ */

void birds_update(Bird *birds, int count, float dt,
                  SoundEffect *snd_flap, float player_x, int cam_x)
{
    const BirdVariantSpec *spec = bird_variant_spec(BIRD_VARIANT_REGULAR);

    for (int i = 0; i < count; i++) {
        Bird *b = &birds[i];

        bird_variant_update(spec, &b->x, &b->vx, b->patrol_x0, b->patrol_x1,
                            &b->frame_index, &b->anim_timer_ms, dt,
                            snd_flap, player_x, cam_x);
    }
}

IntRect bird_get_hitbox(const Bird *b)
{
    const BirdVariantSpec *spec = bird_variant_spec(BIRD_VARIANT_REGULAR);
    return bird_variant_hitbox(spec, b->x, b->base_y);
}

/* ------------------------------------------------------------------ */

void birds_render(const Bird *birds, int count,
                  Texture2D *tex, int cam_x)
{
    const BirdVariantSpec *spec = bird_variant_spec(BIRD_VARIANT_REGULAR);

    for (int i = 0; i < count; i++) {
        const Bird *b = &birds[i];
        bird_variant_render(spec, b->x, b->base_y, b->vx, b->frame_index,
                            tex, cam_x);
    }
}
