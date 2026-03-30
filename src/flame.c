/*
 * flame.c — Flame: an erupting hazard that rises from sea gaps.
 *
 * Each flame cycles through four states:
 *   1. WAITING  — hidden below the floor, counting down.
 *   2. RISING   — moving upward from the gap toward the apex.
 *   3. FLIPPING — paused at the apex, rotating 180° to face downward.
 *   4. FALLING  — descending back into the gap, sprite upside-down.
 *
 * The 2-frame animation (Flame_2.png, 96×48) plays during rise and fall
 * to give a flickering fire effect.
 */

#include <SDL.h>
#include <stdio.h>

#include "flame.h"
#include "game.h"   /* FLOOR_Y, TILE_SIZE, GAME_W, SEA_GAP_W */

/* ------------------------------------------------------------------ */

/*
 * flames_init — Place one flame per sea gap (except the world-edge gap).
 *
 * Each flame is centred horizontally within its sea gap.  The start_y
 * is set just below the floor so the flame begins hidden and rises up.
 *
 * Staggered initial timers prevent all flames from erupting at the same
 * moment — each subsequent flame waits an extra 0.5 s.
 */
void flames_init(Flame *flames, int *count,
                 const int *gap_xs, int gap_count) {
    int n = 0;
    for (int i = 0; i < gap_count && n < MAX_FLAMES; i++) {
        /*
         * Skip the world-edge gap at x=0 — the player can't see it and
         * a flame there would erupt off-screen to the left.
         */
        if (gap_xs[i] <= 0) continue;

        Flame *f = &flames[n];

        f->gap_x    = (float)gap_xs[i];
        /*
         * Centre the flame within the sea gap.
         * gap centre = gap_x + SEA_GAP_W/2; flame left = centre − DISPLAY_W/2.
         */
        f->x        = (float)gap_xs[i] + (SEA_GAP_W - FLAME_DISPLAY_W) / 2.0f;
        f->start_y  = (float)(FLOOR_Y + TILE_SIZE);  /* below the floor, hidden */
        f->y        = f->start_y;
        f->vy       = 0.0f;
        f->w        = FLAME_DISPLAY_W;
        f->h        = FLAME_DISPLAY_H;
        f->angle    = 0.0f;
        f->state    = FLAME_WAITING;
        /*
         * Stagger eruption times: each flame waits (n × 0.5) extra seconds
         * so they don't all fire simultaneously.  The first flame waits the
         * minimum (FLAME_WAIT_DURATION − 0 s), the second slightly longer, etc.
         */
        f->timer      = FLAME_WAIT_DURATION - (float)n * 0.5f;
        f->anim_timer = 0.0f;
        f->anim_frame = 0;
        f->active     = 1;

        n++;
    }
    *count = n;
}

/* ------------------------------------------------------------------ */

/*
 * flames_update — Advance all flames through their eruption cycle.
 *
 * State transitions:
 *   WAITING  → timer expires     → RISING   (reset y to start_y)
 *   RISING   → y reaches APEX_Y  → FLIPPING (start 180° rotation)
 *   FLIPPING → rotation complete  → FALLING  (angle locked at 180°)
 *   FALLING  → y reaches start_y  → WAITING  (reset angle, restart timer)
 */
void flames_update(Flame *flames, int count, float dt) {
    for (int i = 0; i < count; i++) {
        Flame *f = &flames[i];
        if (!f->active) continue;

        /* Advance the 2-frame animation in all visible states */
        if (f->state != FLAME_WAITING) {
            f->anim_timer += dt;
            if (f->anim_timer >= FLAME_ANIM_SPEED) {
                f->anim_timer -= FLAME_ANIM_SPEED;
                f->anim_frame = (f->anim_frame + 1) % FLAME_FRAME_COUNT;
            }
        }

        switch (f->state) {
        case FLAME_WAITING:
            f->timer -= dt;
            if (f->timer <= 0.0f) {
                f->state = FLAME_RISING;
                f->y     = f->start_y;
                f->vy    = FLAME_LAUNCH_VY;   /* fast initial upward impulse */
                f->angle = 0.0f;
                f->timer = 0.0f;
            }
            break;

        case FLAME_RISING:
            /*
             * Physics-based rise: the flame decelerates as it climbs,
             * naturally easing into the apex like a projectile.
             * vy starts at FLAME_LAUNCH_VY (negative = up) and
             * FLAME_RISE_DECEL pulls it back toward zero each frame.
             */
            f->vy += FLAME_RISE_DECEL * dt;
            f->y  += f->vy * dt;

            /* Transition to flipping when velocity reaches zero (apex) */
            if (f->vy >= 0.0f) {
                f->vy    = 0.0f;
                f->state = FLAME_FLIPPING;
                f->timer = 0.0f;
            }
            break;

        case FLAME_FLIPPING:
            /*
             * Quick 180° rotation at the apex.
             * Very fast (FLAME_FLIP_DURATION = 0.12 s) so the flame
             * barely pauses before descending.
             */
            f->timer += dt;
            f->angle = 180.0f * (f->timer / FLAME_FLIP_DURATION);

            if (f->timer >= FLAME_FLIP_DURATION) {
                f->angle = 180.0f;
                f->vy    = 0.0f;   /* start falling from rest */
                f->state = FLAME_FALLING;
            }
            break;

        case FLAME_FALLING:
            /*
             * Gravity-based fall: accelerates downward like the player.
             * GRAVITY (800 px/s²) from game.h gives a natural weight.
             */
            f->vy += GRAVITY * dt;
            f->y  += f->vy * dt;

            if (f->y >= f->start_y) {
                f->y     = f->start_y;
                f->vy    = 0.0f;
                f->angle = 0.0f;
                f->state = FLAME_WAITING;
                f->timer = FLAME_WAIT_DURATION;
            }
            break;
        }
    }
}

/* ------------------------------------------------------------------ */

/*
 * flames_render — Draw each visible flame with animation and rotation.
 *
 * Flame_2.png is a 96×48 sheet with two 48×48 frames side by side.
 * The source rect selects the current animation frame.
 *
 * SDL_RenderCopyEx applies the rotation angle around the sprite's centre
 * (NULL pivot), which creates a smooth flip at the apex.
 */
void flames_render(const Flame *flames, int count,
                   SDL_Renderer *renderer, SDL_Texture *tex, int cam_x) {
    if (!tex) return;

    for (int i = 0; i < count; i++) {
        const Flame *f = &flames[i];
        if (!f->active) continue;

        /* Skip hidden flames (waiting state, fully below the floor) */
        if (f->state == FLAME_WAITING) continue;

        /* Off-screen culling — skip flames outside the viewport */
        if (f->x + f->w < (float)cam_x ||
            f->x > (float)(cam_x + GAME_W))
            continue;

        /*
         * src — select the current animation frame from the 96×48 sheet.
         * Frame 0: {0, 0, 48, 48}    Frame 1: {48, 0, 48, 48}
         */
        SDL_Rect src = {
            .x = f->anim_frame * FLAME_FRAME_W,
            .y = 0,
            .w = FLAME_FRAME_W,
            .h = FLAME_FRAME_H,
        };

        /*
         * dst — destination on screen.
         * x − cam_x converts world space to screen space.
         */
        SDL_Rect dst = {
            .x = (int)f->x - cam_x,
            .y = (int)f->y,
            .w = f->w,
            .h = f->h,
        };

        /*
         * SDL_RenderCopyEx — blit with rotation.
         *
         *   angle  : 0° during rise, 0→180° during flip, 180° during fall.
         *   center : NULL = rotate around the sprite's own centre.
         *   flip   : SDL_FLIP_NONE — rotation handles the upside-down effect.
         */
        SDL_RenderCopyEx(renderer, tex, &src, &dst,
                         (double)f->angle, NULL, SDL_FLIP_NONE);
    }
}

/* ------------------------------------------------------------------ */

/*
 * flame_get_hitbox — Return the collision rect in world coordinates.
 *
 * The hitbox is inset by 6 px on each side to match the visible flame
 * area (the outer pixels are mostly glow/transparent).
 */
SDL_Rect flame_get_hitbox(const Flame *flame) {
    /*
     * Content occupies x=17..30, y=17..31 within the 48×48 frame
     * (14×15 px art).  Inset the hitbox to match the visible flame core.
     */
    SDL_Rect r = {
        .x = (int)flame->x + 17,
        .y = (int)flame->y + 17,
        .w = 14,
        .h = 15,
    };
    return r;
}
