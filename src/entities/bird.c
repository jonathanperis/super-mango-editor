/*
 * bird.c — Bird enemy: slow sine-wave patrol across the sky.
 *
 * Each bird flies horizontally at BIRD_SPEED while oscillating vertically
 * along a sine curve centred at base_y.  The wave amplitude and frequency
 * create a gentle, lazy flight path.  The bird reverses at patrol boundaries.
 */
#include "bird.h"
#include "bird_variant.h"

void birds_init(Bird *birds, int *count, int world_w)
{
    const BirdVariantSpec *spec = bird_variant_spec(BIRD_VARIANT_REGULAR);

    *count = 2;

    /*
     * Bird 0 — patrols across screens 1–2, slightly higher.
     * Starts flying right.
     */
    birds[0].x             = 300.0f;
    birds[0].base_y        = 70.0f;
    birds[0].vx            = spec->speed;
    birds[0].patrol_x0     = 100.0f;
    birds[0].patrol_x1     = 700.0f;
    birds[0].frame_index   = 0;
    birds[0].anim_timer_ms = 0;

    /*
     * Bird 1 — patrols across screens 3–4, slightly lower.
     * Starts flying left for variety.
     */
    birds[1].x             = 1100.0f;
    birds[1].base_y        = 80.0f;
    birds[1].vx            = -spec->speed;
    birds[1].patrol_x0     = 800.0f;
    birds[1].patrol_x1     = (float)(world_w - 1 * 400);  /* last screen boundary */
    birds[1].frame_index   = 1;
    birds[1].anim_timer_ms = 0;
}

/* ------------------------------------------------------------------ */

void birds_update(Bird *birds, int count, float dt,
                  Mix_Chunk *snd_flap, float player_x, int cam_x)
{
    const BirdVariantSpec *spec = bird_variant_spec(BIRD_VARIANT_REGULAR);

    for (int i = 0; i < count; i++) {
        Bird *b = &birds[i];

        bird_variant_update(spec, &b->x, &b->vx, b->patrol_x0, b->patrol_x1,
                            &b->frame_index, &b->anim_timer_ms, dt,
                            snd_flap, player_x, cam_x);
    }
}

SDL_Rect bird_get_hitbox(const Bird *b)
{
    const BirdVariantSpec *spec = bird_variant_spec(BIRD_VARIANT_REGULAR);
    return bird_variant_hitbox(spec, b->x, b->base_y);
}

/* ------------------------------------------------------------------ */

void birds_render(const Bird *birds, int count,
                  SDL_Renderer *renderer, SDL_Texture *tex, int cam_x)
{
    const BirdVariantSpec *spec = bird_variant_spec(BIRD_VARIANT_REGULAR);

    for (int i = 0; i < count; i++) {
        const Bird *b = &birds[i];
        bird_variant_render(spec, b->x, b->base_y, b->vx, b->frame_index,
                            renderer, tex, cam_x);
    }
}
