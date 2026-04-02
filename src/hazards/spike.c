/*
 * spike.c — Spike rows: static ground hazards rendered as tiled blocks.
 *
 * Each spike row is a horizontal strip of 16×16 Spike.png tiles.  They
 * sit on the ground and damage the player on contact.  Unlike bridges,
 * they never fall or crumble — they are purely static hazards.
 */

#include <SDL.h>
#include <stdio.h>

#include "spike.h"
#include "../game.h"   /* FLOOR_Y, TILE_SIZE, GAME_W */

/* ------------------------------------------------------------------ */

/*
 * spike_rows_init — Place spike rows at their level positions.
 *
 * Spike Row 0 — on the ground in the middle phase (screen 2–3 boundary),
 * placed in a gap between platforms where the player must navigate.
 *
 * y is set so the spikes sit ON TOP of the floor surface:
 * FLOOR_Y − SPIKE_TILE_H = 252 − 16 = 236.
 */
void spike_rows_init(SpikeRow *rows, int *count) {
    /*
     * Row 0 — ground spikes at x=580, screen 2.
     *
     * 4 tiles × 16 px = 64 px wide.  Positioned between the sea gap at
     * x=560 (right edge 592) and pillar 4 at x=680.  The spikes start at
     * x=600 to leave a safe landing zone after the gap.
     */
    rows[0].x      = 780.0f;
    rows[0].y      = (float)(FLOOR_Y - SPIKE_TILE_H);  /* 236 */
    rows[0].count  = 4;
    rows[0].active = 1;

    *count = 1;
}

/* ------------------------------------------------------------------ */

/*
 * spike_rows_render — Draw each active spike row as tiled 16×16 blocks.
 *
 * Spike.png is a single 16×16 image.  Each tile in the row is rendered
 * independently at sequential x positions, sharing the same texture.
 */
void spike_rows_render(const SpikeRow *rows, int count,
                       SDL_Renderer *renderer, SDL_Texture *tex, int cam_x) {
    if (!tex) return;

    for (int i = 0; i < count; i++) {
        const SpikeRow *row = &rows[i];
        if (!row->active) continue;

        for (int t = 0; t < row->count; t++) {
            int tx = (int)row->x + t * SPIKE_TILE_W;
            int ty = (int)row->y;

            /* Off-screen culling per tile */
            int screen_x = tx - cam_x;
            if (screen_x + SPIKE_TILE_W < 0 || screen_x > GAME_W) continue;

            /*
             * dst — destination on screen.
             * NULL source rect = use the entire 16×16 texture.
             */
            SDL_Rect dst = {
                .x = screen_x,
                .y = ty,
                .w = SPIKE_TILE_W,
                .h = SPIKE_TILE_H,
            };
            SDL_RenderCopy(renderer, tex, NULL, &dst);
        }
    }
}

/* ------------------------------------------------------------------ */

/*
 * spike_row_get_rect — Return the full bounding rect of the row.
 */
SDL_Rect spike_row_get_rect(const SpikeRow *row) {
    SDL_Rect r = {
        .x = (int)row->x,
        .y = (int)row->y,
        .w = row->count * SPIKE_TILE_W,
        .h = SPIKE_TILE_H,
    };
    return r;
}

/* ------------------------------------------------------------------ */

/*
 * spike_row_hit_test — Check if a player rect overlaps any tile in the row.
 *
 * Uses a simple AABB overlap against the full row bounding rectangle.
 */
int spike_row_hit_test(const SpikeRow *row, const SDL_Rect *prect) {
    if (!row->active) return 0;
    SDL_Rect rr = spike_row_get_rect(row);
    return SDL_HasIntersection(prect, &rr);
}
