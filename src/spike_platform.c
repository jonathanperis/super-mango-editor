/*
 * spike_platform.c — SpikePlatform: static elevated hazard surface.
 *
 * Rendered using the same 3-slice approach as float platforms, but using
 * Spike_Platform.png.  The player can land on top (one-way collision)
 * but touching the spikes from sides or below deals damage.
 */

#include <SDL.h>
#include <stdio.h>

#include "spike_platform.h"
#include "game.h"   /* FLOOR_Y, TILE_SIZE, GAME_W */

/* ------------------------------------------------------------------ */

/*
 * spike_platforms_init — Place spike platforms at their level positions.
 *
 * Platform 0 — in the middle phase (screen 2), elevated between pillars.
 *
 * Placed at x=520, y=200 (above the floor, between pillar 3 at x=452
 * and the sea gap at x=560).  3 pieces × 16 px = 48 px wide.
 */
void spike_platforms_init(SpikePlatform *sps, int *count) {
    sps[0].x      = 370.0f;
    sps[0].y      = 200.0f;
    sps[0].w      = 3 * SPIKE_PLAT_PIECE_W;  /* 48 px */
    sps[0].active = 1;

    *count = 1;
}

/* ------------------------------------------------------------------ */

/*
 * spike_platforms_render — Draw each active platform using 3-slice rendering.
 *
 * Spike_Platform.png is 48×16 px divided into three 16×16 pieces:
 *   piece 0 (src.x = 0):  left  end cap
 *   piece 1 (src.x = 16): centre fill
 *   piece 2 (src.x = 32): right end cap
 */
void spike_platforms_render(const SpikePlatform *sps, int count,
                            SDL_Renderer *renderer, SDL_Texture *tex,
                            int cam_x) {
    if (!tex) return;

    for (int i = 0; i < count; i++) {
        const SpikePlatform *sp = &sps[i];
        if (!sp->active) continue;

        int n_pieces = sp->w / SPIKE_PLAT_PIECE_W;
        int screen_x = (int)sp->x - cam_x;

        for (int p = 0; p < n_pieces; p++) {
            /* Select source slice: 0 = left, 2 = right, 1 = centre */
            int piece;
            if      (p == 0)          piece = 0;
            else if (p == n_pieces-1) piece = 2;
            else                      piece = 1;

            SDL_Rect src = { piece * SPIKE_PLAT_PIECE_W, SPIKE_PLAT_SRC_Y,
                             SPIKE_PLAT_PIECE_W, SPIKE_PLAT_SRC_H };
            SDL_Rect dst = { screen_x + p * SPIKE_PLAT_PIECE_W, (int)sp->y,
                             SPIKE_PLAT_PIECE_W, SPIKE_PLAT_SRC_H };
            SDL_RenderCopy(renderer, tex, &src, &dst);
        }
    }
}

/* ------------------------------------------------------------------ */

/*
 * spike_platform_get_rect — Return the damage hitbox in world space.
 *
 * The hitbox extends 4 px above the visual platform so that a player
 * standing on top (whose physics bottom sits exactly at sp->y due to
 * FLOOR_SINK snapping) still registers an overlap.  Without this
 * extension, SDL_HasIntersection's strict less-than test would miss
 * the edge-aligned case and no damage would ever trigger.
 */
SDL_Rect spike_platform_get_rect(const SpikePlatform *sp) {
    /*
     * The hitbox matches the cropped content (11 px tall) plus a 2 px
     * upward extension so a player standing on top still registers overlap.
     */
    SDL_Rect r = {
        .x = (int)sp->x,
        .y = (int)sp->y - 2,
        .w = sp->w,
        .h = SPIKE_PLAT_SRC_H + 2,
    };
    return r;
}
