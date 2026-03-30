/*
 * circular_saw.c — CircularSaw: a fast-patrolling rotating hazard.
 *
 * The saw bounces back and forth along a horizontal patrol line,
 * spinning continuously.  It damages the player on contact with
 * knockback, just like spike blocks and axe traps.
 */

#include <SDL.h>
#include <math.h>   /* sqrtf */
#include <stdio.h>

#include "circular_saw.h"
#include "game.h"   /* FLOOR_Y, TILE_SIZE, GAME_W */

/* ------------------------------------------------------------------ */

/*
 * circular_saws_init — Place circular saws at their level positions.
 *
 * Saw 0 — patrols on top of the bridge at the end phase (screen 4).
 *
 * The bridge spans x=1350 to x=1478 (8 bricks × 16 px = 128 px).
 * Bridge top y = FLOOR_Y − 2×TILE_SIZE + 16 = 172.
 * The saw sits on top of the bridge surface, so its bottom aligns with
 * bridge top: saw_y = bridge_y − SAW_DISPLAY_H = 172 − 32 = 140.
 *
 * Patrol limits are inset slightly from the bridge edges so the saw
 * doesn't overhang visually.
 */
void circular_saws_init(CircularSaw *saws, int *count) {
    /*
     * Saw 0 — bridge patrol (screen 4, end phase).
     *
     * Bridge left edge: 1350, right edge: 1350 + 128 = 1478.
     * Patrol from 1350 to (1478 − SAW_DISPLAY_W) = 1446 so the saw
     * stays visually within the bridge bounds.
     */
    saws[0].x          = 1350.0f;
    saws[0].y          = (float)(FLOOR_Y - 2 * TILE_SIZE + 16 - SAW_DISPLAY_H);
    saws[0].w          = SAW_DISPLAY_W;
    saws[0].h          = SAW_DISPLAY_H;
    saws[0].patrol_x0  = 1350.0f;
    saws[0].patrol_x1  = 1478.0f - SAW_DISPLAY_W;
    saws[0].direction   = 1;
    saws[0].spin_angle  = 0.0f;
    saws[0].active      = 1;

    *count = 1;
}

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
 * rect to SDL_RenderCopyEx so the entire texture is used.
 *
 * SDL_RenderCopyEx rotates around the sprite's centre (NULL pivot = centre),
 * which is natural for a circular saw blade.
 */
void circular_saws_render(const CircularSaw *saws, int count,
                          SDL_Renderer *renderer, SDL_Texture *tex, int cam_x) {
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
        SDL_Rect dst = {
            .x = (int)s->x - cam_x,
            .y = (int)s->y,
            .w = s->w,
            .h = s->h,
        };

        /*
         * SDL_RenderCopyEx — blit the full texture with rotation.
         *
         *   angle  : spin_angle in degrees, advances each frame.
         *   center : NULL = rotate around the rect's own centre — natural
         *            for a circular blade.
         *   flip   : SDL_FLIP_NONE — rotation alone drives the spin.
         */
        SDL_RenderCopyEx(renderer, tex, NULL, &dst,
                         (double)s->spin_angle, NULL, SDL_FLIP_NONE);
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
SDL_Rect circular_saw_get_hitbox(const CircularSaw *saw) {
    SDL_Rect r = {
        .x = (int)saw->x + 4,
        .y = (int)saw->y + 4,
        .w = saw->w - 8,
        .h = saw->h - 8,
    };
    return r;
}

/* ------------------------------------------------------------------ */

/*
 * circular_saw_push_player — Apply push impulse opposite to player movement.
 *
 * Same algorithm as spike_block_push_player:
 *   1. If the player is moving, reverse and normalise their velocity,
 *      then scale to SAW_PUSH_SPEED.
 *   2. If stationary, push horizontally away from the saw's centre.
 *   3. Always add SAW_PUSH_VY upward for a visible bounce-back effect.
 *   4. Clear on_ground so the upward component takes effect immediately.
 */
void circular_saw_push_player(const CircularSaw *saw, Player *player) {
    float vx = player->vx;
    float vy = player->vy;
    float len = sqrtf(vx * vx + vy * vy);

    if (len > 1.0f) {
        /*
         * Normalise the player's velocity, reverse it, and scale to
         * SAW_PUSH_SPEED for a consistent knockback impulse.
         */
        player->vx = -(vx / len) * SAW_PUSH_SPEED;
        player->vy = -(vy / len) * SAW_PUSH_SPEED + SAW_PUSH_VY;
    } else {
        /*
         * Player was stationary: push horizontally away from the saw's
         * centre, plus a fixed upward component.
         */
        float saw_cx = saw->x + saw->w * 0.5f;
        float player_cx = player->x + player->w * 0.5f;
        float dir = (player_cx >= saw_cx) ? 1.0f : -1.0f;
        player->vx = dir * SAW_PUSH_SPEED;
        player->vy = SAW_PUSH_VY;
    }

    /*
     * Lift the player off the ground so the upward push takes effect.
     * Without this, player_update would snap them back to the floor.
     */
    player->on_ground = 0;
}
