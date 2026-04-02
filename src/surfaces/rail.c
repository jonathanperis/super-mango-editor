/*
 * rail.c — Rail path building, rendering, and traversal helpers.
 */

#include <SDL.h>
#include <SDL_image.h>
#include <stdio.h>

#include "rail.h"
#include "game.h"   /* WORLD_W, GAME_H — used for placement bounds */
#include "../levels/level.h"  /* RailPlacement — for rail_init_from_placements */

/* ------------------------------------------------------------------ */

/*
 * connection_to_src — Map a connection bitmask to a source rect in Rails.png.
 *
 * Rails.png (64×64) is a 4×4 grid of 16×16-px tiles.
 * Positions below use 1-based (row, col) notation matching the sprite sheet:
 *
 *   Two-direction tiles (path interiors and loop corners):
 *   (1,2) = col 1, row 0  →  EAST + SOUTH  (top-left  loop corner)
 *   (1,4) = col 3, row 0  →  WEST + SOUTH  (top-right loop corner)
 *   (3,2) = col 1, row 2  →  EAST + NORTH  (bot-left  loop corner)
 *   (3,4) = col 3, row 2  →  WEST + NORTH  (bot-right loop corner)
 *   (1,3) = col 2, row 0  →  EAST + WEST   (horizontal straight)
 *   (2,1) = col 0, row 1  →  NORTH + SOUTH (vertical straight)
 *
 *   Single-direction tiles (end caps — terminate an open rail):
 *   (4,2) = col 1, row 3  →  EAST  only    (horizontal left  end cap)
 *   (4,4) = col 3, row 3  →  WEST  only    (horizontal right end cap)
 *   (1,1) = col 0, row 0  →  SOUTH only    (vertical   top   end cap)
 *   (1,3) = col 2, row 0  →  NORTH only    (vertical   bot   end cap)
 */
static SDL_Rect connection_to_src(int conn) {
    int col, row;
    switch (conn & (RAIL_N | RAIL_E | RAIL_S | RAIL_W)) {
        /* ---- Two-direction: loop corners and straights ---- */
        case RAIL_E | RAIL_S:  col = 1; row = 0; break;  /* top-left  corner (1,2) */
        case RAIL_W | RAIL_S:  col = 3; row = 0; break;  /* top-right corner (1,4) */
        case RAIL_E | RAIL_N:  col = 1; row = 2; break;  /* bot-left  corner (3,2) */
        case RAIL_W | RAIL_N:  col = 3; row = 2; break;  /* bot-right corner (3,4) */
        case RAIL_E | RAIL_W:  col = 2; row = 0; break;  /* H-straight       (1,3) */
        case RAIL_N | RAIL_S:  col = 0; row = 1; break;  /* V-straight       (2,1) */
        /* ---- Single-direction: end caps ---- */
        case RAIL_E:           col = 1; row = 3; break;  /* H left  end cap  (4,2) */
        case RAIL_W:           col = 3; row = 3; break;  /* H right end cap  (4,4) */
        case RAIL_S:           col = 0; row = 0; break;  /* V top   end cap  (1,1) */
        case RAIL_N:           col = 2; row = 0; break;  /* V bot   end cap  (1,3) */
        default:               col = 1; row = 3; break;  /* fallback: H left cap   */
    }
    SDL_Rect src = { col * RAIL_TILE_W, row * RAIL_TILE_H, RAIL_TILE_W, RAIL_TILE_H };
    return src;
}

/* ------------------------------------------------------------------ */

/*
 * build_rect_rail — Populate a Rail with a clockwise rectangular loop.
 *
 * x0, y0     : world-space top-left corner of the loop (logical pixels).
 * w_tiles    : number of tiles along the top and bottom edges (≥ 2).
 * h_tiles    : number of tiles along the left and right edges (≥ 2).
 *
 * Traversal order (clockwise, starts at top-left going right):
 *   1. Top row    — left  to right, w_tiles tiles (includes both corners)
 *   2. Right col  — top   to bottom, (h_tiles − 2) inner tiles
 *   3. Bottom row — right to left,   w_tiles tiles (includes both corners)
 *   4. Left col   — bottom to top,   (h_tiles − 2) inner tiles
 *
 * Each corner tile receives the bitmask matching its two open directions:
 *   top-left  → RAIL_E|RAIL_S   top-right → RAIL_W|RAIL_S
 *   bot-right → RAIL_W|RAIL_N   bot-left  → RAIL_E|RAIL_N
 *
 * Inner edge tiles receive the straight-line bitmask for their axis.
 */
static void build_rect_rail(Rail *r, int x0, int y0, int w_tiles, int h_tiles) {
    int idx = 0;
    const int S = RAIL_TILE_W;   /* grid step: one tile side = 16 px */

    /* ── Top edge: left → right ──────────────────────────────────── */
    for (int i = 0; i < w_tiles; i++) {
        int conn;
        if      (i == 0)           conn = RAIL_E | RAIL_S;  /* top-left corner  */
        else if (i == w_tiles - 1) conn = RAIL_W | RAIL_S;  /* top-right corner */
        else                       conn = RAIL_E | RAIL_W;  /* H-straight       */
        r->tiles[idx].x           = x0 + i * S;
        r->tiles[idx].y           = y0;
        r->tiles[idx].connections = conn;
        idx++;
    }

    /* ── Right edge: top → bottom (inner tiles only) ─────────────── */
    for (int j = 1; j < h_tiles - 1; j++) {
        r->tiles[idx].x           = x0 + (w_tiles - 1) * S;
        r->tiles[idx].y           = y0 + j * S;
        r->tiles[idx].connections = RAIL_N | RAIL_S;
        idx++;
    }

    /* ── Bottom edge: right → left ───────────────────────────────── */
    for (int i = w_tiles - 1; i >= 0; i--) {
        int conn;
        if      (i == w_tiles - 1) conn = RAIL_W | RAIL_N;  /* bot-right corner */
        else if (i == 0)           conn = RAIL_E | RAIL_N;  /* bot-left corner  */
        else                       conn = RAIL_E | RAIL_W;  /* H-straight       */
        r->tiles[idx].x           = x0 + i * S;
        r->tiles[idx].y           = y0 + (h_tiles - 1) * S;
        r->tiles[idx].connections = conn;
        idx++;
    }

    /* ── Left edge: bottom → top (inner tiles only) ──────────────── */
    for (int j = h_tiles - 2; j >= 1; j--) {
        r->tiles[idx].x           = x0;
        r->tiles[idx].y           = y0 + j * S;
        r->tiles[idx].connections = RAIL_N | RAIL_S;
        idx++;
    }

    r->count  = idx;
    r->closed = 1;
}

/* ------------------------------------------------------------------ */

/*
 * build_horiz_rail — Populate a Rail with a single horizontal line of tiles.
 *
 * x0, y0   : world-space left edge of the line (logical pixels).
 * w_tiles  : number of tiles (≥ 2).
 * end_cap  : 1 = place a right end-cap tile at the far end; the riding object
 *                bounces back when it reaches it.
 *            0 = no end-cap; the riding object detaches at the far tile and
 *                enters free-fall, inheriting the rail speed as initial vx.
 *
 * Tile layout:
 *   tile[0]          : RAIL_E only      → left  end-cap  (always present)
 *   tile[1..n-2]     : RAIL_E | RAIL_W  → horizontal straight
 *   tile[n-1] if end_cap=1 : RAIL_W only → right end-cap
 *   tile[n-1] if end_cap=0 : RAIL_E | RAIL_W → straight (no visual cap)
 *
 * Physical width: w_tiles × RAIL_TILE_W logical pixels.
 */
static void build_horiz_rail(Rail *r, int x0, int y0, int w_tiles, int end_cap) {
    /* First tile: left end-cap, opens rightward only */
    r->tiles[0].x           = x0;
    r->tiles[0].y           = y0;
    r->tiles[0].connections = RAIL_E;

    /* Middle tiles: horizontal straight, opens both ways */
    for (int i = 1; i < w_tiles - 1; i++) {
        r->tiles[i].x           = x0 + i * RAIL_TILE_W;
        r->tiles[i].y           = y0;
        r->tiles[i].connections = RAIL_E | RAIL_W;
    }

    /*
     * Last tile: right end-cap if end_cap=1 (opens leftward only, block
     * bounces), or straight if end_cap=0 (no visual stop, block detaches
     * and falls off the far end of the track).
     */
    int last = w_tiles - 1;
    r->tiles[last].x           = x0 + last * RAIL_TILE_W;
    r->tiles[last].y           = y0;
    r->tiles[last].connections = end_cap ? RAIL_W : (RAIL_E | RAIL_W);

    r->count   = w_tiles;
    r->closed  = 0;
    r->end_cap = end_cap;
}

/* ------------------------------------------------------------------ */

/*
 * rail_init — Define all rails in the level.
 *
 *   Rail 0 — Screen 2 (world x ≈ 400–800), closed rectangular loop
 *     10 tiles wide × 6 tiles tall = 160 × 96 logical px
 *     World origin (444, 72).  28 tiles total.
 *     Spike block speed: SLOW (1.5 tiles/s, ~18.7 s per loop).
 *
 *   Rail 1 — Screen 3 (world x ≈ 800–1200), closed rectangular loop
 *     8 tiles wide × 5 tiles tall = 128 × 80 logical px
 *     World origin (852, 80).  22 tiles total.
 *     Spike block speed: NORMAL (3.0 tiles/s, ~7.3 s per loop).
 *
 *   Rail 2 — Screen 4 (world x ≈ 1200–1600), open horizontal line
 *     14 tiles wide = 224 logical px
 *     World origin (1248, 112).  14 tiles total.
 *     Spike block speed: FAST (6.0 tiles/s, ~2.3 s per pass).
 *     The block bounces left↔right continuously.
 */
void rail_init(Rail *rails, int *count) {
    /*
     * Rail 0 — closed loop, screen 2.
     * end_cap is irrelevant for closed rails (the loop never terminates),
     * but we set it to 1 as a safe default.
     */
    build_rect_rail(&rails[0], 444, 35, 10, 6);  /* bottom edge at y=131, clears 2-tile platform coins at y=140 */
    rails[0].end_cap = 1;

    /* Rail 1 — closed loop, screen 3. */
    build_rect_rail(&rails[1], 852, 50, 8, 5);  /* bottom edge at y=130, clears 2-tile platform coins at y=140 */
    rails[1].end_cap = 1;

    /*
     * Rail 2 — open horizontal line, screen 4.  end_cap = 0.
     * 14 tiles × 16 px = 224 px wide, at world (1200, 112).
     *
     * With no right end-cap, the FAST spike block detaches when it reaches
     * tile 13 and enters free-fall with its rail speed as initial vx.
     * This demonstrates the "no end cap → fall off" mechanic.
     *
     * y=112 sits above the 2-tile pillar tops (y=172) and within jumping
     * reach from the 3-tile pillar nearby (top at y=124).
     */
    build_horiz_rail(&rails[2], 1200, 112, 14, 0);

    *count = 3;
}

/* ------------------------------------------------------------------ */

/*
 * rail_init_from_placements — Build rails from a level-data array.
 *
 * Interprets each RailPlacement and delegates to the appropriate
 * static builder (build_rect_rail or build_horiz_rail).
 * level.h must be included before this function is called.
 */
void rail_init_from_placements(Rail *rails, int *count,
                               const RailPlacement *placements, int n)
{
    for (int i = 0; i < n; i++) {
        const RailPlacement *p = &placements[i];
        if (p->layout == RAIL_LAYOUT_RECT) {
            build_rect_rail(&rails[i], p->x, p->y, p->w, p->h);
            rails[i].end_cap = 1;  /* closed loops always have end_cap = 1 */
        } else {
            build_horiz_rail(&rails[i], p->x, p->y, p->w, p->end_cap);
        }
    }
    *count = n;
}

/* ------------------------------------------------------------------ */

/*
 * rail_render — Draw every tile of every rail loop.
 *
 * For each tile we call connection_to_src() to pick the correct 16×16 cell
 * from Rails.png, then SDL_RenderCopy to blit it at its world position
 * adjusted by the camera offset (world → screen: dst.x = tile.x − cam_x).
 */
void rail_render(const Rail *rails, int count,
                 SDL_Renderer *renderer, SDL_Texture *tex, int cam_x) {
    for (int r = 0; r < count; r++) {
        const Rail *rail = &rails[r];
        for (int i = 0; i < rail->count; i++) {
            const RailTile *t = &rail->tiles[i];

            /*
             * src — the 16×16 region to cut from Rails.png.
             * Determined by the tile's connection bitmask.
             */
            SDL_Rect src = connection_to_src(t->connections);

            /*
             * dst — where to draw on screen.
             * Subtract cam_x so the rail scrolls with the camera.
             * Tile positions are already in logical pixels; no scaling needed.
             */
            SDL_Rect dst = { t->x - cam_x, t->y, RAIL_TILE_W, RAIL_TILE_H };

            SDL_RenderCopy(renderer, tex, &src, &dst);
        }
    }
}

/* ------------------------------------------------------------------ */

/*
 * rail_get_world_pos — Interpolate the world-space centre at position t.
 *
 * t lives in [0, rail->count).  The integer part i selects the current
 * tile; j = (i+1) % count is the next tile.  The fractional part lerps
 * linearly between the two centres, giving smooth sub-tile movement.
 *
 * Centre of tile k: (tiles[k].x + RAIL_TILE_W/2, tiles[k].y + RAIL_TILE_H/2)
 */
void rail_get_world_pos(const Rail *r, float t, float *out_x, float *out_y) {
    int   i    = (int)t % r->count;
    int   j    = (i + 1) % r->count;
    float frac = t - (float)(int)t;     /* sub-tile interpolation factor */

    float cx_i = (float)(r->tiles[i].x + RAIL_TILE_W / 2);
    float cy_i = (float)(r->tiles[i].y + RAIL_TILE_H / 2);
    float cx_j = (float)(r->tiles[j].x + RAIL_TILE_W / 2);
    float cy_j = (float)(r->tiles[j].y + RAIL_TILE_H / 2);

    /*
     * Linear interpolation (lerp): result = a + (b − a) × frac.
     * When frac=0 the position is exactly at tile i's centre;
     * when frac=1 it is exactly at tile j's centre.
     */
    *out_x = cx_i + (cx_j - cx_i) * frac;
    *out_y = cy_i + (cy_j - cy_i) * frac;
}

/* ------------------------------------------------------------------ */

/*
 * rail_advance — Advance t by speed × dt, with wrapping for closed rails.
 *
 * speed is in tiles per second (e.g. 2.0f = two tiles per second).
 * For a 28-tile loop at 2 tiles/s the block completes one circuit in 14 s.
 * Returns the new t value, wrapped to [0, count) for closed rails.
 */
float rail_advance(const Rail *r, float t, float speed, float dt) {
    t += speed * dt;
    if (r->closed) {
        float n = (float)r->count;
        /*
         * fmodf keeps t in [0, n).  The extra while-loop handles the rare
         * case where fmodf returns n due to floating-point rounding.
         */
        t = t - n * (float)(int)(t / n);
        if (t < 0.0f) t += n;
        if (t >= n)   t  = 0.0f;
    }
    return t;
}
