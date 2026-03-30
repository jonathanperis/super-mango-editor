/*
 * float_platform.c — FloatPlatform: hovering, crumbling, and rail-riding surfaces.
 */

#include <SDL.h>
#include <stdio.h>

#include "float_platform.h"
#include "rail.h"
#include "game.h"   /* GAME_H, GRAVITY */

/* ------------------------------------------------------------------ */

/*
 * float_platform_init — Configure one platform instance.
 *
 * For RAIL mode the x/y arguments are ignored; the starting position is
 * derived immediately from rail_get_world_pos so the platform is never
 * drawn at (0,0) on the first frame before update runs.
 */
void float_platform_init(FloatPlatform *fp, FloatPlatformMode mode,
                         float x, float y, int w_tiles,
                         float stand_limit,
                         const Rail *rail, float t, float speed) {
    fp->mode   = mode;
    fp->w      = w_tiles * FLOAT_PLATFORM_PIECE_W;
    fp->h      = FLOAT_PLATFORM_H;
    fp->active = 1;

    /* CRUMBLE defaults */
    fp->stand_timer = 0.0f;
    fp->stand_limit = (mode == FLOAT_PLATFORM_CRUMBLE) ? stand_limit : 0.0f;
    fp->falling     = 0;
    fp->fall_vy     = 0.0f;

    /* RAIL defaults */
    fp->rail      = (mode == FLOAT_PLATFORM_RAIL) ? rail : NULL;
    fp->t         = t;
    fp->speed     = speed;
    fp->direction = 1;   /* always start moving forward */
    fp->prev_x    = 0.0f;

    if (mode == FLOAT_PLATFORM_RAIL && rail != NULL) {
        /*
         * Derive the world position from the rail immediately so the platform
         * is placed correctly on the very first frame before update runs.
         * rail_get_world_pos returns the interpolated centre of tile t;
         * subtract half the display size to get the top-left corner.
         */
        float cx, cy;
        rail_get_world_pos(rail, t, &cx, &cy);
        fp->x      = cx - fp->w * 0.5f;
        fp->y      = cy - fp->h * 0.5f;
        fp->prev_x = fp->x;
    } else {
        fp->x = x;
        fp->y = y;
    }
}

/* ------------------------------------------------------------------ */

/*
 * float_platforms_init — Place all three float-platform instances in the level.
 *
 * Three demonstrations, one per screen:
 *
 *   Platform 0 — STATIC, screen 1 (x 0–400).
 *     Hovers at (140, 200), 4 tiles wide (64 px).
 *     Placed between pillar 1 (x=80) and pillar 2 (x=256), within jumping
 *     reach from the floor — the player passes through from below on the
 *     way up and lands on the way down.
 *
 *   Platform 1 — CRUMBLE, screen 2 (x 400–800).
 *     Hovers at (540, 190), 3 tiles wide (48 px).
 *     Falls CRUMBLE_STAND_LIMIT seconds after the player lands on it.
 *     Placed between pillars 3 and 4, also within floor-jump reach.
 *
 *   Platform 2 — RAIL, screen 3 (x 800–1200).
 *     Attached to Rail 1 (closed 8×5 loop at world (852, 62)), starting at
 *     tile 14 — near the middle of the bottom row (world ≈ x=916, y=126).
 *     Speed 2.0 tiles/s so it orbits slower than the spike block (3.0 tiles/s)
 *     and the two stay separated.  Reachable by jumping from the medium
 *     pillar at x=880 (pillar 5).
 */
void float_platforms_init(FloatPlatform *fps, int *count, const Rail *rails) {
    /* Platform 0 — static hover, screen 1 */
    float_platform_init(&fps[0], FLOAT_PLATFORM_STATIC,
                        172.0f, 200.0f, 4,
                        0.0f,           /* stand_limit: unused for STATIC */
                        NULL, 0.0f, 0.0f);

    /* Platform 1 — crumble, screen 2, right of the spike block rail */
    float_platform_init(&fps[1], FLOAT_PLATFORM_CRUMBLE,
                        570.0f, 190.0f, 3,
                        CRUMBLE_STAND_LIMIT,
                        NULL, 0.0f, 0.0f);

    /* Platform 2 — rail-attached, screen 3, Rail 1, start at bottom row */
    float_platform_init(&fps[2], FLOAT_PLATFORM_RAIL,
                        0.0f, 0.0f, 3,  /* x/y overridden by rail position */
                        0.0f,
                        &rails[1], 14.0f, 2.0f);

    *count = 3;
}

/* ------------------------------------------------------------------ */

/*
 * float_platform_update — Advance one platform for this frame.
 *
 * Three independent state machines, one per mode:
 *
 *   STATIC   — no-op; position never changes.
 *
 *   CRUMBLE  — accumulate stand_timer while the player is on top.
 *              When the timer exceeds stand_limit, set falling=1.
 *              While falling, apply GRAVITY to fall_vy and integrate y each
 *              frame until the platform exits the bottom of the canvas.
 *              If the player steps off before the limit, the timer resets —
 *              the platform only falls after continuous standing.
 *
 *   RAIL     — save prev_x for the nudge calculation in game_loop, then
 *              advance t along the rail.  Closed loops use rail_advance()
 *              (which wraps t).  Open rails advance manually with bounce
 *              at both endpoints (no fall-off — platforms never detach).
 */
void float_platform_update(FloatPlatform *fp, float dt, int player_on_top) {
    if (!fp->active) return;

    switch (fp->mode) {

        /* ---- STATIC: nothing to do --------------------------------- */
        case FLOAT_PLATFORM_STATIC:
            break;

        /* ---- CRUMBLE: stand-timer then free-fall ------------------- */
        case FLOAT_PLATFORM_CRUMBLE:
            if (!fp->falling) {
                if (player_on_top) {
                    fp->stand_timer += dt;
                    if (fp->stand_timer >= fp->stand_limit) {
                        fp->falling  = 1;
                        fp->fall_vy  = CRUMBLE_FALL_INITIAL_VY;
                    }
                } else {
                    /*
                     * Player stepped off before the limit — reset the timer.
                     * This gives the player a chance to jump off a crumbling
                     * platform and use it again (resets only while not falling).
                     */
                    fp->stand_timer = 0.0f;
                }
            } else {
                /*
                 * Smooth fall: accelerate at CRUMBLE_FALL_GRAVITY (much
                 * gentler than player GRAVITY) so the platform sinks
                 * gradually before picking up speed.
                 */
                fp->fall_vy += CRUMBLE_FALL_GRAVITY * dt;
                fp->y       += fp->fall_vy * dt;
                if (fp->y > (float)(GAME_H + 64)) {
                    fp->active = 0;
                }
            }
            break;

        /* ---- RAIL: ride the track ---------------------------------- */
        case FLOAT_PLATFORM_RAIL: {
            /*
             * Save the current x before advancing so game_loop can compute
             * the horizontal delta (fp->x − fp->prev_x) and push the player
             * sideways when they are riding this platform.
             */
            fp->prev_x = fp->x;

            if (fp->rail->closed) {
                /* Closed loop: rail_advance wraps t continuously in [0, count) */
                fp->t = rail_advance(fp->rail, fp->t, fp->speed, dt);
            } else {
                /*
                 * Open rail: advance by speed × direction × dt.
                 * Bounce at both endpoints — platforms never fall off.
                 */
                fp->t += fp->speed * (float)fp->direction * dt;
                if (fp->t >= (float)(fp->rail->count - 1)) {
                    fp->t         = (float)(fp->rail->count - 1);
                    fp->direction = -1;
                } else if (fp->t <= 0.0f) {
                    fp->t         = 0.0f;
                    fp->direction = 1;
                }
            }

            /*
             * Clamp t just below (count − 1) for open rails before the
             * position lookup so the next-tile index j never wraps to tile[0].
             */
            float t_safe = fp->t;
            if (!fp->rail->closed && t_safe >= (float)(fp->rail->count - 1))
                t_safe = (float)(fp->rail->count - 1) - 0.0001f;

            /*
             * Recompute world-space position from the interpolated rail centre.
             * Subtract half the platform size to place the top-left corner.
             */
            float cx, cy;
            rail_get_world_pos(fp->rail, t_safe, &cx, &cy);
            fp->x = cx - fp->w * 0.5f;
            fp->y = cy - fp->h * 0.5f;
            break;
        }
    }
}

/* ------------------------------------------------------------------ */

void float_platforms_update(FloatPlatform *fps, int count,
                            float dt, int landed_idx) {
    for (int i = 0; i < count; i++) {
        int on_top = (i == landed_idx) ? 1 : 0;
        float_platform_update(&fps[i], dt, on_top);
    }
}

/* ------------------------------------------------------------------ */

/*
 * float_platform_render — Draw one platform using 3-slice horizontal rendering.
 *
 * Platform.png is 48×16 px divided into three 16×16 pieces:
 *   piece 0 (src.x = 0):  left  end cap
 *   piece 1 (src.x = 16): centre fill — repeated for platforms wider than 2 tiles
 *   piece 2 (src.x = 32): right end cap
 *
 * We iterate over w / PIECE_W pieces, selecting the correct source slice for
 * the leftmost, rightmost, and all interior tiles.
 */
void float_platform_render(const FloatPlatform *fp,
                           SDL_Renderer *renderer, SDL_Texture *tex, int cam_x) {
    if (!fp->active || !tex) return;

    int n_pieces = fp->w / FLOAT_PLATFORM_PIECE_W;   /* total horizontal pieces */
    int screen_x = (int)fp->x - cam_x;               /* left edge in screen space */

    for (int i = 0; i < n_pieces; i++) {
        /* Select source slice: 0 = left cap, 2 = right cap, 1 = center fill */
        int piece;
        if      (i == 0)          piece = 0;
        else if (i == n_pieces-1) piece = 2;
        else                      piece = 1;

        /*
         * src — the 16×16 region to cut from Platform.png.
         * dst — destination on screen: left edge + piece offset, at the
         *       platform's screen-space y (world y − no cam_y since camera
         *       is horizontal-only).
         */
        SDL_Rect src = { piece * FLOAT_PLATFORM_PIECE_W, 0,
                         FLOAT_PLATFORM_PIECE_W, FLOAT_PLATFORM_H };
        SDL_Rect dst = { screen_x + i * FLOAT_PLATFORM_PIECE_W, (int)fp->y,
                         FLOAT_PLATFORM_PIECE_W, FLOAT_PLATFORM_H };
        SDL_RenderCopy(renderer, tex, &src, &dst);
    }
}

/* ------------------------------------------------------------------ */

void float_platforms_render(const FloatPlatform *fps, int count,
                            SDL_Renderer *renderer, SDL_Texture *tex, int cam_x) {
    for (int i = 0; i < count; i++)
        float_platform_render(&fps[i], renderer, tex, cam_x);
}

/* ------------------------------------------------------------------ */

/*
 * float_platform_get_rect — Return the world-space bounding rectangle.
 * Used by the debug overlay to draw collision boxes.
 */
SDL_Rect float_platform_get_rect(const FloatPlatform *fp) {
    SDL_Rect r = { (int)fp->x, (int)fp->y, fp->w, fp->h };
    return r;
}
