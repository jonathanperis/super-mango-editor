/*
 * bird.c — Bird enemy: slow sine-wave patrol across the sky.
 *
 * Each bird flies horizontally at BIRD_SPEED while oscillating vertically
 * along a sine curve centred at base_y.  The wave amplitude and frequency
 * create a gentle, lazy flight path.  The bird reverses at patrol boundaries.
 */
#include <math.h>   /* sinf */

#include "bird.h"
#include "game.h"   /* GAME_W, GAME_H, WORLD_W */

/* ------------------------------------------------------------------ */

void birds_init(Bird *birds, int *count)
{
    *count = 2;

    /*
     * Bird 0 — patrols across screens 1–2 in the upper sky.
     * Starts mid-patrol, flying right.
     */
    birds[0].x             = 100.0f;
    birds[0].base_y        = 60.0f;
    birds[0].vx            = BIRD_SPEED;
    birds[0].patrol_x0     = 0.0f;
    birds[0].patrol_x1     = 700.0f;
    birds[0].frame_index   = 0;
    birds[0].anim_timer_ms = 0;

    /*
     * Bird 1 — patrols across screens 3–4, slightly lower.
     * Starts flying left for variety.
     */
    birds[1].x             = 1100.0f;
    birds[1].base_y        = 80.0f;
    birds[1].vx            = -BIRD_SPEED;
    birds[1].patrol_x0     = 800.0f;
    birds[1].patrol_x1     = 1600.0f;
    birds[1].frame_index   = 1;
    birds[1].anim_timer_ms = 0;
}

/* ------------------------------------------------------------------ */

void birds_update(Bird *birds, int count, float dt)
{
    for (int i = 0; i < count; i++) {
        Bird *b = &birds[i];

        /* ── horizontal movement ──────────────────────────────────── */
        b->x += b->vx * dt;

        /* Patrol boundary reversal */
        if (b->vx > 0.0f && b->x + BIRD_FRAME_W >= b->patrol_x1) {
            b->x  = b->patrol_x1 - BIRD_FRAME_W;
            b->vx = -BIRD_SPEED;
        } else if (b->vx < 0.0f && b->x <= b->patrol_x0) {
            b->x  = b->patrol_x0;
            b->vx =  BIRD_SPEED;
        }

        /* ── advance animation frame ──────────────────────────────── */
        b->anim_timer_ms += (Uint32)(dt * 1000.0f);
        if (b->anim_timer_ms >= BIRD_FRAME_MS) {
            b->anim_timer_ms -= BIRD_FRAME_MS;
            b->frame_index    = (b->frame_index + 1) % BIRD_FRAMES;
        }
    }
}

/* ------------------------------------------------------------------ */

/*
 * bird_screen_y — Compute the current screen-space y of the bird's art top.
 *
 * The y position oscillates around base_y using a sine wave keyed to
 * the bird's world-space x, so the curve is tied to position (not time).
 * This means a bird that reverses direction retraces the same path.
 */
static float bird_screen_y(const Bird *b)
{
    float wave = sinf(b->x * BIRD_WAVE_FREQ) * BIRD_WAVE_AMP;
    return b->base_y + wave;
}

/* ------------------------------------------------------------------ */

SDL_Rect bird_get_hitbox(const Bird *b)
{
    float sy = bird_screen_y(b);
    SDL_Rect r;
    r.x = (int)b->x + BIRD_ART_X;
    r.y = (int)sy;
    r.w = BIRD_ART_W;
    r.h = BIRD_ART_H;
    return r;
}

/* ------------------------------------------------------------------ */

void birds_render(const Bird *birds, int count,
                  SDL_Renderer *renderer, SDL_Texture *tex, int cam_x)
{
    for (int i = 0; i < count; i++) {
        const Bird *b = &birds[i];
        float sy = bird_screen_y(b);

        SDL_Rect src = {
            b->frame_index * BIRD_FRAME_W,
            BIRD_ART_Y,
            BIRD_FRAME_W,
            BIRD_ART_H
        };

        SDL_Rect dst = {
            (int)b->x - cam_x,
            (int)sy,
            BIRD_FRAME_W,
            BIRD_ART_H
        };

        /*
         * The base sprite faces left; flip when flying right (vx > 0).
         */
        SDL_RendererFlip flip = (b->vx > 0.0f)
                                 ? SDL_FLIP_HORIZONTAL
                                 : SDL_FLIP_NONE;
        SDL_RenderCopyEx(renderer, tex, &src, &dst, 0.0, NULL, flip);
    }
}
