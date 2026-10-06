/*
 * circular_saw.c — CircularSaw: a fast-patrolling rotating hazard.
 *
 * The saw bounces back and forth along a horizontal patrol line,
 * spinning continuously.  It damages the player on contact with
 * knockback, just like spike blocks and axe traps.
 */

#include "../shared/graphics.h"
#include <stdio.h>

#include "circular_saw.h"
#include "../game.h"   /* FLOOR_Y, TILE_SIZE, GAME_W */

/* ------------------------------------------------------------------ */

/*
 * circular_saws_update — Advance patrol and spin for all active saws.
 *
 * Movement is a simple horizontal bounce between patrol_x0 and patrol_x1.
 * The saw reverses direction when it reaches either limit, clamping its
 * position to prevent overshoot.
 *
 * Spin advances every frame regardless of movement, wrapping within
 * [0, 360) to avoid float drift.
 */
void circular_saws_update(CircularSaw *saws, int count, float dt) {
    for (int i = 0; i < count; i++) {
        CircularSaw *s = &saws[i];
        if (!s->active) continue;

        /* Advance the spin animation (always, even if off-screen) */
        s->spin_angle += SAW_SPIN_DEG_PER_SEC * dt;
        if (s->spin_angle >= 360.0f) s->spin_angle -= 360.0f;

        /* Horizontal patrol — bounce between x0 and x1 */
        s->x += SAW_PATROL_SPEED * (float)s->direction * dt;

        if (s->x >= s->patrol_x1) {
            s->x = s->patrol_x1;
            s->direction = -1;
        } else if (s->x <= s->patrol_x0) {
            s->x = s->patrol_x0;
            s->direction = 1;
        }
    }
}

/* ------------------------------------------------------------------ */

/*
 * circular_saws_render — Draw each active saw with rotation.
 *
 * Circular_Saw.png is a single 32×32 frame.  We pass NULL as the source
 * rect so the entire texture is used.
 *
 * The draw helper rotates around the sprite's centre,
 * which is natural for a circular saw blade.
 */
void circular_saws_render(const CircularSaw *saws, int count,
                          Texture2D *tex, int cam_x) {
    if (!tex) return;

    for (int i = 0; i < count; i++) {
        const CircularSaw *s = &saws[i];
        if (!s->active) continue;

        /*
         * Off-screen culling — skip saws entirely outside the viewport.
         * SAW_DISPLAY_W is added as a margin on each side.
         */
        if (s->x + s->w < (float)cam_x ||
            s->x > (float)(cam_x + GAME_W))
            continue;

        /*
         * dst — destination on screen.
         * x − cam_x converts world space to screen space.
         */
        IntRect dst = {
            .x = (int)s->x - cam_x,
            .y = (int)s->y,
            .w = s->w,
            .h = s->h,
        };

        sprite_draw(tex, NULL, &dst, s->spin_angle, SPRITE_NORMAL, WHITE);
    }
}

/* ------------------------------------------------------------------ */

/*
 * circular_saw_get_hitbox — Return the collision rect in world coordinates.
 *
 * The hitbox is inset by 4 px on each side from the display rect to
 * account for the transparent corners of the circular blade sprite.
 * This makes collisions feel fair — the player must overlap the actual
 * blade, not just the bounding square.
 */
IntRect circular_saw_get_hitbox(const CircularSaw *saw) {
    IntRect r = {
        .x = (int)saw->x + 4,
        .y = (int)saw->y + 4,
        .w = saw->w - 8,
        .h = saw->h - 8,
    };
    return r;
}

/* ------------------------------------------------------------------ */
