#pragma once

#include <stdint.h>

/* World hitboxes retain integer coordinates and half-open edges. Rendering
 * converts these values to floating-point rectangles only at the GPU boundary. */
typedef struct { int x, y, w, h; } IntRect;

static inline int rect_intersects(const IntRect *a, const IntRect *b)
{
    return a && b && a->w > 0 && a->h > 0 && b->w > 0 && b->h > 0 &&
        (int64_t)a->x < (int64_t)b->x + b->w &&
        (int64_t)b->x < (int64_t)a->x + a->w &&
        (int64_t)a->y < (int64_t)b->y + b->h &&
        (int64_t)b->y < (int64_t)a->y + a->h;
}
