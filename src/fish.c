/*
 * fish.c — Jumping fish enemy that patrols the water lane.
 */

#include <stdlib.h>   /* rand */

#include "fish.h"
#include "game.h"    /* FLOOR_Y, GRAVITY, WORLD_W */
#include "water.h"   /* WATER_ART_H */

/* ------------------------------------------------------------------ */

/* Return a random float in the inclusive range [min_s, max_s]. */
static float fish_random_jump_delay(float min_s, float max_s)
{
    float t = (float)(rand() % 1001) / 1000.0f;
    return min_s + (max_s - min_s) * t;
}

/* ------------------------------------------------------------------ */

void fish_init(Fish *fish, int *count)
{
    /*
     * water_y is the y coordinate of the fish's top-left corner while swimming.
     *
     * The water art surface sits at y = GAME_H - WATER_ART_H = 269.
     * We want the fish centred on that surface so exactly half of the 48-px
     * sprite is visible above the waterline and half is submerged:
     *
     *   water_y = water_surface - FISH_RENDER_H / 2
     *           = 269 - 24 = 245
     *
     * Safety check — fish hitbox top while swimming = 245 + 8 = 253.
     * Player hitbox bottom = 252.  SDL intersection uses strict < so
     * 253 < 252 is FALSE: the fish CANNOT hurt the player while swimming,
     * only during a jump (when y drops below ~244).
     */
    float water_y = (float)(GAME_H - WATER_ART_H) - FISH_RENDER_H / 2.0f;

    *count = 2;

    /*
     * Fish 0 — patrols the middle of the world (screens 2–3).
     * Starts swimming right with a short random delay before its first jump.
     */
    fish[0].x          = 700.0f;
    fish[0].y          = water_y;
    fish[0].vx         = FISH_SPEED;
    fish[0].vy         = 0.0f;
    fish[0].patrol_x0  = 500.0f;
    fish[0].patrol_x1  = 950.0f;
    fish[0].jump_timer = fish_random_jump_delay(FISH_JUMP_MIN, FISH_JUMP_MAX);
    fish[0].water_y    = water_y;
    fish[0].frame_index   = 0;
    fish[0].anim_timer_ms = 0;

    /*
     * Fish 1 — patrols the end of the world (screens 3–4).
     * Starts swimming left to create variety in movement direction.
     */
    fish[1].x          = 1200.0f;
    fish[1].y          = water_y;
    fish[1].vx         = -FISH_SPEED;
    fish[1].vy         = 0.0f;
    fish[1].patrol_x0  = 1000.0f;
    fish[1].patrol_x1  = 1500.0f;
    fish[1].jump_timer = fish_random_jump_delay(FISH_JUMP_MIN, FISH_JUMP_MAX);
    fish[1].water_y    = water_y;
    fish[1].frame_index   = 0;
    fish[1].anim_timer_ms = 0;
}

/* ------------------------------------------------------------------ */

void fish_update(Fish *fish, int count, float dt)
{
    for (int i = 0; i < count; i++) {
        Fish *f = &fish[i];

        /* Countdown to the next jump while the fish is swimming. */
        if (f->y >= f->water_y && f->vy == 0.0f) {
            f->jump_timer -= dt;
            if (f->jump_timer <= 0.0f) {
                f->vy = FISH_JUMP_VY;
                f->jump_timer = fish_random_jump_delay(FISH_JUMP_MIN, FISH_JUMP_MAX);
            }
        }

        /* Horizontal patrol runs continuously, both in water and airborne. */
        f->x += f->vx * dt;

        if (f->vx > 0.0f && f->x + FISH_RENDER_W >= f->patrol_x1) {
            f->x  = f->patrol_x1 - FISH_RENDER_W;
            f->vx = -FISH_SPEED;
        } else if (f->vx < 0.0f && f->x <= f->patrol_x0) {
            f->x  = f->patrol_x0;
            f->vx = FISH_SPEED;
        }

        /* Clamp to world edges as a final safety net. */
        if (f->x < 0.0f) {
            f->x = 0.0f;
            f->vx = FISH_SPEED;
        }
        if (f->x > WORLD_W - FISH_RENDER_W) {
            f->x = (float)(WORLD_W - FISH_RENDER_W);
            f->vx = -FISH_SPEED;
        }

        /* Gravity only affects the fish while it is in its jump arc. */
        if (f->y < f->water_y || f->vy != 0.0f) {
            f->vy += GRAVITY * dt;
            f->y  += f->vy * dt;

            if (f->y >= f->water_y) {
                f->y  = f->water_y;
                f->vy = 0.0f;
            }
        }

        /*
         * Timed animation: cycle the two swim frames every FISH_FRAME_MS ms.
         * Both frames are left-facing in the sheet; direction is handled by
         * SDL_FLIP_HORIZONTAL in fish_render, not by frame selection.
         */
        f->anim_timer_ms += (Uint32)(dt * 1000.0f);
        if (f->anim_timer_ms >= FISH_FRAME_MS) {
            f->anim_timer_ms -= FISH_FRAME_MS;
            f->frame_index = (f->frame_index + 1) % FISH_FRAMES;
        }
    }
}

/* ------------------------------------------------------------------ */

void fish_render(const Fish *fish, int count,
                 SDL_Renderer *renderer, SDL_Texture *tex, int cam_x)
{
    for (int i = 0; i < count; i++) {
        const Fish *f = &fish[i];

        SDL_Rect src = {
            f->frame_index * FISH_FRAME_W,
            0,
            FISH_FRAME_W,
            FISH_FRAME_H
        };
        SDL_Rect dst = {
            (int)f->x - cam_x,
            (int)f->y,
            FISH_RENDER_W,
            FISH_RENDER_H
        };

        SDL_RenderCopyEx(renderer, tex, &src, &dst, 0.0, NULL,
                         (f->vx > 0.0f) ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    }
}

/* ------------------------------------------------------------------ */

SDL_Rect fish_get_hitbox(const Fish *fish)
{
    SDL_Rect hitbox;

    hitbox.x = (int)fish->x + FISH_HITBOX_PAD_X;
    hitbox.y = (int)fish->y + FISH_HITBOX_PAD_Y;
    hitbox.w = FISH_RENDER_W  - 2 * FISH_HITBOX_PAD_X;   /* 48 − 32 = 16 */
    hitbox.h = FISH_RENDER_H  - FISH_HITBOX_PAD_Y - 16;  /* 48 − 13 − 16 = 19 */

    return hitbox;
}