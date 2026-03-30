/*
 * faster_bird.c — Faster bird enemy: quick sine-wave patrol across the sky.
 *
 * Same mechanics as the regular bird but with higher speed, faster wing
 * animation, and a tighter sine-wave frequency for more aggressive curves.
 */
#include <math.h>   /* sinf */

#include "faster_bird.h"
#include "game.h"   /* GAME_W, GAME_H, WORLD_W */

/* ------------------------------------------------------------------ */

void faster_birds_init(FasterBird *birds, int *count)
{
    *count = 2;

    /*
     * Faster Bird 0 — patrols screens 2–3, mid-sky.
     * Starts flying left for variety against the regular birds.
     */
    birds[0].x             = 600.0f;
    birds[0].base_y        = 50.0f;
    birds[0].vx            = -FBIRD_SPEED;
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
    birds[1].vx            = FBIRD_SPEED;
    birds[1].patrol_x0     = 900.0f;
    birds[1].patrol_x1     = 1600.0f;
    birds[1].frame_index   = 2;
    birds[1].anim_timer_ms = 0;
}

/* ------------------------------------------------------------------ */

void faster_birds_update(FasterBird *birds, int count, float dt)
{
    for (int i = 0; i < count; i++) {
        FasterBird *b = &birds[i];

        /* ── horizontal movement ──────────────────────────────────── */
        b->x += b->vx * dt;

        if (b->vx > 0.0f && b->x + FBIRD_FRAME_W >= b->patrol_x1) {
            b->x  = b->patrol_x1 - FBIRD_FRAME_W;
            b->vx = -FBIRD_SPEED;
        } else if (b->vx < 0.0f && b->x <= b->patrol_x0) {
            b->x  = b->patrol_x0;
            b->vx =  FBIRD_SPEED;
        }

        /* ── advance animation frame ──────────────────────────────── */
        b->anim_timer_ms += (Uint32)(dt * 1000.0f);
        if (b->anim_timer_ms >= FBIRD_FRAME_MS) {
            b->anim_timer_ms -= FBIRD_FRAME_MS;
            b->frame_index    = (b->frame_index + 1) % FBIRD_FRAMES;
        }
    }
}

/* ------------------------------------------------------------------ */

static float fbird_screen_y(const FasterBird *b)
{
    float wave = sinf(b->x * FBIRD_WAVE_FREQ) * FBIRD_WAVE_AMP;
    return b->base_y + wave;
}

/* ------------------------------------------------------------------ */

SDL_Rect faster_bird_get_hitbox(const FasterBird *b)
{
    float sy = fbird_screen_y(b);
    SDL_Rect r;
    r.x = (int)b->x + FBIRD_ART_X;
    r.y = (int)sy;
    r.w = FBIRD_ART_W;
    r.h = FBIRD_ART_H;
    return r;
}

/* ------------------------------------------------------------------ */

void faster_birds_render(const FasterBird *birds, int count,
                         SDL_Renderer *renderer, SDL_Texture *tex, int cam_x)
{
    for (int i = 0; i < count; i++) {
        const FasterBird *b = &birds[i];
        float sy = fbird_screen_y(b);

        SDL_Rect src = {
            b->frame_index * FBIRD_FRAME_W,
            FBIRD_ART_Y,
            FBIRD_FRAME_W,
            FBIRD_ART_H
        };

        SDL_Rect dst = {
            (int)b->x - cam_x,
            (int)sy,
            FBIRD_FRAME_W,
            FBIRD_ART_H
        };

        SDL_RendererFlip flip = (b->vx > 0.0f)
                                 ? SDL_FLIP_HORIZONTAL
                                 : SDL_FLIP_NONE;
        SDL_RenderCopyEx(renderer, tex, &src, &dst, 0.0, NULL, flip);
    }
}
