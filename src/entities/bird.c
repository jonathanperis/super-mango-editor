/*
 * bird.c — Bird enemy: slow sine-wave patrol across the sky.
 *
 * Each bird flies horizontally at BIRD_SPEED while oscillating vertically
 * along a sine curve centred at base_y.  The wave amplitude and frequency
 * create a gentle, lazy flight path.  The bird reverses at patrol boundaries.
 */
#include <math.h>   /* sinf, fabsf */

#include "bird.h"
#include "../game.h"                /* GAME_W */
#include "../core/entity_utils.h"  /* patrol_update, animate_frame_ms */

/*
 * BIRD_AUDIBLE_RANGE — max distance (px) at which the flap SFX is audible.
 * Set to GAME_W: the sound fades to complete silence exactly when the player
 * is one full screen-width away from the bird.
 */
#define BIRD_AUDIBLE_RANGE  ((float)GAME_W)
#define BIRD_VOL_MAX        67


/* ------------------------------------------------------------------ */

void birds_init(Bird *birds, int *count, int world_w)
{
    *count = 2;

    /*
     * Bird 0 — patrols across screens 1–2, slightly higher.
     * Starts flying right.
     */
    birds[0].x             = 300.0f;
    birds[0].base_y        = 70.0f;
    birds[0].vx            = BIRD_SPEED;
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
    birds[1].vx            = -BIRD_SPEED;
    birds[1].patrol_x0     = 800.0f;
    birds[1].patrol_x1     = (float)(world_w - 1 * 400);  /* last screen boundary */
    birds[1].frame_index   = 1;
    birds[1].anim_timer_ms = 0;
}

/* ------------------------------------------------------------------ */

void birds_update(Bird *birds, int count, float dt,
                  Mix_Chunk *snd_flap, float player_x, int cam_x)
{
    for (int i = 0; i < count; i++) {
        Bird *b = &birds[i];

        /* ── horizontal movement + patrol boundary reversal ──────── */
        patrol_update(&b->x, &b->vx, BIRD_FRAME_W,
                      b->patrol_x0, b->patrol_x1, BIRD_SPEED, dt);

        /* ── advance animation frame ──────────────────────────────── */
        /*
         * animate_frame_ms returns 1 when the animation wraps back to
         * frame 0.  We use that signal to play the wing-flap sound once
         * per cycle, with distance-based volume.
         */
        int wrapped = animate_frame_ms(&b->frame_index, &b->anim_timer_ms,
                                       dt, BIRD_FRAME_MS, BIRD_FRAMES);
        if (wrapped && snd_flap) {
            float bird_cx = b->x + BIRD_FRAME_W / 2.0f;
            int on_screen = (bird_cx >= (float)cam_x - BIRD_FRAME_W &&
                             bird_cx <= (float)cam_x + GAME_W + BIRD_FRAME_W);
            if (on_screen) {
                float dist = fabsf(player_x - bird_cx);
                int vol = sound_volume_for_distance(dist, BIRD_AUDIBLE_RANGE, BIRD_VOL_MAX);
                if (vol > 0) {
                    int ch = Mix_PlayChannel(-1, snd_flap, 0);
                    if (ch >= 0) Mix_Volume(ch, vol);
                }
            }
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
