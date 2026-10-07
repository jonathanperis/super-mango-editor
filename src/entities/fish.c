/*
 * fish.c — Jumping fish enemies that patrol the water lane.
 *
 * This file owns the swim/jump behaviour for BOTH fish variants. The regular
 * fish and the faster fish (faster_fish.c) differ only in tuning — patrol
 * speed, jump impulse, delay between jumps and animation speed — which each
 * variant passes in as a FishSpec. bird.c does the same for birds.
 */

#include "../core/game_random.h"

#include "fish.h"
#include "../game.h"               /* GRAVITY */
#include "../core/entity_utils.h"  /* animate_frame_ms */

/* Tuning for the regular fish; faster_fish.c defines its own FishSpec. */
static const FishSpec s_regular_fish = {
    FISH_SPEED,
    FISH_JUMP_VY,
    FISH_JUMP_MIN,
    FISH_JUMP_MAX,
    FISH_FRAME_MS
};

/* ------------------------------------------------------------------ */

/* Return a random float in the inclusive range [min_s, max_s]. */
static float fish_random_jump_delay(float min_s, float max_s)
{
    float t = game_random_unit();
    return min_s + (max_s - min_s) * t;
}

/* ------------------------------------------------------------------ */

void fish_variant_update(const FishSpec *spec, Fish *fish, int count,
                         float dt, int world_w)
{
    for (int i = 0; i < count; i++) {
        Fish *f = &fish[i];

        /* Countdown to the next jump while the fish is swimming. */
        if (f->y >= f->water_y && f->vy == 0.0f) {
            f->jump_timer -= dt;
            if (f->jump_timer <= 0.0f) {
                f->vy = spec->jump_vy;
                f->jump_timer = fish_random_jump_delay(spec->jump_min, spec->jump_max);
            }
        }

        /* Horizontal patrol runs continuously, both in water and airborne. */
        f->x += f->vx * dt;

        if (f->vx > 0.0f && f->x + FISH_RENDER_W >= f->patrol_x1) {
            f->x  = f->patrol_x1 - FISH_RENDER_W;
            f->vx = -spec->speed;
        } else if (f->vx < 0.0f && f->x <= f->patrol_x0) {
            f->x  = f->patrol_x0;
            f->vx = spec->speed;
        }

        /* Clamp to world edges as a final safety net. */
        if (f->x < 0.0f) {
            f->x = 0.0f;
            f->vx = spec->speed;
        }
        if (f->x > world_w - FISH_RENDER_W) {
            f->x = (float)(world_w - FISH_RENDER_W);
            f->vx = -spec->speed;
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
         * Timed animation: cycle the two swim frames every spec->frame_ms.
         * Both frames are left-facing in the sheet; direction is handled by
         * horizontal flipping in fish_render, not by frame selection.
         * animate_frame_ms accumulates dt and advances frame_index on overflow.
         */
        animate_frame_ms(&f->frame_index, &f->anim_timer_ms,
                         dt, spec->frame_ms, FISH_FRAMES);
    }
}

void fish_update(Fish *fish, int count, float dt, int world_w)
{
    fish_variant_update(&s_regular_fish, fish, count, dt, world_w);
}

/* ------------------------------------------------------------------ */

void fish_render(const Fish *fish, int count,
                 Texture2D *tex, int cam_x)
{
    for (int i = 0; i < count; i++) {
        const Fish *f = &fish[i];

        IntRect src = {
            f->frame_index * FISH_FRAME_W,
            0,
            FISH_FRAME_W,
            FISH_FRAME_H
        };
        IntRect dst = {
            (int)f->x - cam_x,
            (int)f->y,
            FISH_RENDER_W,
            FISH_RENDER_H
        };

        sprite_draw(tex, &src, &dst, 0,
                    f->vx > 0.0f ? SPRITE_FLIP_X : SPRITE_NORMAL, WHITE);
    }
}

/* ------------------------------------------------------------------ */

IntRect fish_get_hitbox(const Fish *fish)
{
    IntRect hitbox;

    hitbox.x = (int)fish->x + FISH_HITBOX_PAD_X;
    hitbox.y = (int)fish->y + FISH_HITBOX_PAD_Y;
    hitbox.w = FISH_RENDER_W  - 2 * FISH_HITBOX_PAD_X;   /* 48 − 32 = 16 */
    hitbox.h = FISH_RENDER_H  - FISH_HITBOX_PAD_Y - 16;  /* 48 − 13 − 16 = 19 */

    return hitbox;
}
