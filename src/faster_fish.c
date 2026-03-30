/*
 * faster_fish.c — FasterFish: a fast variant of the jumping fish enemy.
 *
 * Same mechanics as the regular Fish but with higher speed (120 px/s),
 * a stronger jump impulse (-420 px/s), and shorter delay between jumps.
 */

#include <stdlib.h>   /* rand */

#include "faster_fish.h"
#include "game.h"     /* FLOOR_Y, GRAVITY, WORLD_W */
#include "water.h"    /* WATER_ART_H */

/* ------------------------------------------------------------------ */

static float ffish_random_delay(float min_s, float max_s) {
    float t = (float)(rand() % 1001) / 1000.0f;
    return min_s + (max_s - min_s) * t;
}

/* ------------------------------------------------------------------ */

void faster_fish_init(FasterFish *fish, int *count) {
    float water_y = (float)(GAME_H - WATER_ART_H) - FFISH_RENDER_H / 2.0f;

    *count = 1;

    /*
     * FasterFish 0 — patrols the end of the world (screens 3–4).
     * Faster and jumps much higher than the regular fish.
     */
    fish[0].x             = 1100.0f;
    fish[0].y             = water_y;
    fish[0].vx            = FFISH_SPEED;
    fish[0].vy            = 0.0f;
    fish[0].patrol_x0     = 900.0f;
    fish[0].patrol_x1     = 1400.0f;
    fish[0].jump_timer    = ffish_random_delay(FFISH_JUMP_MIN, FFISH_JUMP_MAX);
    fish[0].water_y       = water_y;
    fish[0].frame_index   = 0;
    fish[0].anim_timer_ms = 0;
}

/* ------------------------------------------------------------------ */

void faster_fish_update(FasterFish *fish, int count, float dt) {
    for (int i = 0; i < count; i++) {
        FasterFish *f = &fish[i];

        /* Countdown to the next jump while swimming */
        if (f->y >= f->water_y && f->vy == 0.0f) {
            f->jump_timer -= dt;
            if (f->jump_timer <= 0.0f) {
                f->vy = FFISH_JUMP_VY;
                f->jump_timer = ffish_random_delay(FFISH_JUMP_MIN, FFISH_JUMP_MAX);
            }
        }

        /* Horizontal patrol */
        f->x += f->vx * dt;

        if (f->vx > 0.0f && f->x + FFISH_RENDER_W >= f->patrol_x1) {
            f->x  = f->patrol_x1 - FFISH_RENDER_W;
            f->vx = -FFISH_SPEED;
        } else if (f->vx < 0.0f && f->x <= f->patrol_x0) {
            f->x  = f->patrol_x0;
            f->vx = FFISH_SPEED;
        }

        /* Clamp to world edges */
        if (f->x < 0.0f) { f->x = 0.0f; f->vx = FFISH_SPEED; }
        if (f->x > WORLD_W - FFISH_RENDER_W) {
            f->x = (float)(WORLD_W - FFISH_RENDER_W);
            f->vx = -FFISH_SPEED;
        }

        /* Gravity during jump arc */
        if (f->y < f->water_y || f->vy != 0.0f) {
            f->vy += GRAVITY * dt;
            f->y  += f->vy * dt;

            if (f->y >= f->water_y) {
                f->y  = f->water_y;
                f->vy = 0.0f;
            }
        }

        /* Animation */
        f->anim_timer_ms += (Uint32)(dt * 1000.0f);
        if (f->anim_timer_ms >= FFISH_FRAME_MS) {
            f->anim_timer_ms -= FFISH_FRAME_MS;
            f->frame_index = (f->frame_index + 1) % FFISH_FRAMES;
        }
    }
}

/* ------------------------------------------------------------------ */

void faster_fish_render(const FasterFish *fish, int count,
                        SDL_Renderer *renderer, SDL_Texture *tex, int cam_x) {
    for (int i = 0; i < count; i++) {
        const FasterFish *f = &fish[i];

        SDL_Rect src = {
            f->frame_index * FFISH_FRAME_W, 0,
            FFISH_FRAME_W, FFISH_FRAME_H
        };
        SDL_Rect dst = {
            (int)f->x - cam_x, (int)f->y,
            FFISH_RENDER_W, FFISH_RENDER_H
        };

        SDL_RenderCopyEx(renderer, tex, &src, &dst, 0.0, NULL,
                         (f->vx > 0.0f) ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    }
}

/* ------------------------------------------------------------------ */

SDL_Rect faster_fish_get_hitbox(const FasterFish *fish) {
    SDL_Rect hitbox;
    hitbox.x = (int)fish->x + FFISH_HITBOX_PAD_X;
    hitbox.y = (int)fish->y + FFISH_HITBOX_PAD_Y;
    hitbox.w = FFISH_RENDER_W - 2 * FFISH_HITBOX_PAD_X;
    hitbox.h = FFISH_RENDER_H - FFISH_HITBOX_PAD_Y - 16;
    return hitbox;
}
