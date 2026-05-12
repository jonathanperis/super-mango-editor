/*
 * entity_meta.c — Shared editor geometry helpers.
 */

#include "entity_meta.h"

int editor_rail_placement_tile_count(const RailPlacement *rp)
{
    if (!rp) return 0;
    if (rp->layout == RAIL_LAYOUT_RECT) {
        return rp->w * 2 + (rp->h - 2) * 2;
    }
    return rp->w;
}

static void rail_rect_tile_position(const RailPlacement *rp, int idx,
                                    float *x, float *y)
{
    int w = rp->w;
    int h = rp->h;

    if (idx < w) {
        *x = (float)(rp->x + idx * RAIL_TILE_W + RAIL_TILE_W / 2);
        *y = (float)(rp->y + RAIL_TILE_H / 2);
        return;
    }

    idx -= w;
    if (idx < h - 2) {
        *x = (float)(rp->x + (w - 1) * RAIL_TILE_W + RAIL_TILE_W / 2);
        *y = (float)(rp->y + (idx + 1) * RAIL_TILE_H + RAIL_TILE_H / 2);
        return;
    }

    idx -= h - 2;
    if (idx < w) {
        *x = (float)(rp->x + (w - 1 - idx) * RAIL_TILE_W + RAIL_TILE_W / 2);
        *y = (float)(rp->y + (h - 1) * RAIL_TILE_H + RAIL_TILE_H / 2);
        return;
    }

    idx -= w;
    *x = (float)(rp->x + RAIL_TILE_W / 2);
    *y = (float)(rp->y + (h - 2 - idx) * RAIL_TILE_H + RAIL_TILE_H / 2);
}

void editor_rail_placement_position_at(const RailPlacement *rp, float t,
                                       float *x, float *y)
{
    int count;

    if (!rp || !x || !y) return;

    count = editor_rail_placement_tile_count(rp);
    if (count <= 0) {
        *x = (float)rp->x;
        *y = (float)rp->y;
        return;
    }

    if (rp->layout == RAIL_LAYOUT_HORIZ) {
        *x = (float)rp->x + t * (float)RAIL_TILE_W + (float)RAIL_TILE_W / 2.0f;
        *y = (float)rp->y + (float)RAIL_TILE_H / 2.0f;
        return;
    }

    while (t < 0.0f) t += (float)count;
    while (t >= (float)count) t -= (float)count;

    {
        int tile = (int)t;
        int next = (tile + 1) % count;
        float frac = t - (float)tile;
        float ax, ay, bx, by;

        rail_rect_tile_position(rp, tile, &ax, &ay);
        rail_rect_tile_position(rp, next, &bx, &by);
        *x = ax + (bx - ax) * frac;
        *y = ay + (by - ay) * frac;
    }
}
