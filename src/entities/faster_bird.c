/*
 * faster_bird.c — Faster bird enemy: quick sine-wave patrol across the sky.
 *
 * Same mechanics as the regular bird but with higher speed, faster wing
 * animation, and a tighter sine-wave frequency for more aggressive curves.
 */
#include "faster_bird.h"
#include "bird_variant.h"


/* ------------------------------------------------------------------ */

void faster_birds_init(FasterBird *birds, int *count, int world_w)
{
    const BirdVariantSpec *spec = bird_variant_spec(BIRD_VARIANT_FAST);

    *count = 2;

    /*
     * Faster Bird 0 — patrols screens 2–3, mid-sky.
     * Starts flying left for variety against the regular birds.
     */
    birds[0].x             = 600.0f;
    birds[0].base_y        = 50.0f;
    birds[0].vx            = -spec->speed;
    birds[0].patrol_x0     = 300.0f;
    birds[0].patrol_x1     = 1100.0f;
    birds[0].frame_index   = 0;
    birds[0].anim_timer_ms = 0;

    /*
     * Faster Bird 1 — patrols screens 3–4, slightly higher.
     * Starts flying right.
     */
    birds[1].x             = 1200.0f;
    birds[1].base_y        = 40.0f;
    birds[1].vx            = spec->speed;
    birds[1].patrol_x0     = 900.0f;
    birds[1].patrol_x1     = (float)(world_w - 1 * 400);  /* last screen boundary */
    birds[1].frame_index   = 2;
    birds[1].anim_timer_ms = 0;
}

/* ------------------------------------------------------------------ */

void faster_birds_update(FasterBird *birds, int count, float dt,
                         Mix_Chunk *snd_flap, float player_x, int cam_x)
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

SDL_Rect faster_bird_get_hitbox(const FasterBird *b)
{
    const BirdVariantSpec *spec = bird_variant_spec(BIRD_VARIANT_FAST);
    return bird_variant_hitbox(spec, b->x, b->base_y);
}

/* ------------------------------------------------------------------ */

void faster_birds_render(const FasterBird *birds, int count,
                         SDL_Renderer *renderer, SDL_Texture *tex, int cam_x)
{
    const BirdVariantSpec *spec = bird_variant_spec(BIRD_VARIANT_FAST);

    for (int i = 0; i < count; i++) {
        const FasterBird *b = &birds[i];
        bird_variant_render(spec, b->x, b->base_y, b->vx, b->frame_index,
                            renderer, tex, cam_x);
    }
}
