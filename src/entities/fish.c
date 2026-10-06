/*
 * fish.c — Jumping fish enemy that patrols the water lane.
 */

#include "../core/game_random.h"

#include "fish.h"
#include "../game.h"               /* FLOOR_Y, GRAVITY */
#include "../effects/water.h"      /* WATER_ART_H */
#include "../core/entity_utils.h"  /* animate_frame_ms */

/* ------------------------------------------------------------------ */

/* Return a random float in the inclusive range [min_s, max_s]. */
static float fish_random_jump_delay(float min_s, float max_s)
{
    float t = game_random_unit();
    return min_s + (max_s - min_s) * t;
}

/* ------------------------------------------------------------------ */

void fish_update(Fish *fish, int count, float dt, int world_w)
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
        if (f->x > world_w - FISH_RENDER_W) {
            f->x = (float)(world_w - FISH_RENDER_W);
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
         * horizontal flipping in fish_render, not by frame selection.
         * animate_frame_ms accumulates dt and advances frame_index on overflow.
         */
        animate_frame_ms(&f->frame_index, &f->anim_timer_ms,
                         dt, FISH_FRAME_MS, FISH_FRAMES);
    }
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
