#pragma once

#include <stdint.h>
#include <string.h>  /* memcpy */

/* World hitboxes retain integer coordinates and half-open edges. Rendering
 * converts these values to floating-point rectangles only at the GPU boundary. */
typedef struct {
    int x, y, w, h;
} IntRect;

static inline int rect_intersects(const IntRect *a, const IntRect *b)
{
    /* Half-open means [x, x+w): merely touching an edge is not overlap.
     * Widen BEFORE adding widths/heights so signed-int addition cannot wrap. */
    return a && b && a->w > 0 && a->h > 0 && b->w > 0 && b->h > 0 &&
        (int64_t)a->x < (int64_t)b->x + b->w &&
        (int64_t)b->x < (int64_t)a->x + a->w &&
        (int64_t)a->y < (int64_t)b->y + b->h &&
        (int64_t)b->y < (int64_t)a->y + a->h;
}

/*
 * float_same_value — Whether two floats hold exactly the same value.
 *
 * Most float comparisons want "close enough", because arithmetic rounds. A
 * few want "is this the very number I stored": a respawn point copied from a
 * checkpoint, a zoom picked from a fixed list. Those are compared bit for bit
 * here, which says so in the code and never mistakes two different values
 * for equal. Adding 0.0f first turns -0.0 into +0.0, so the two zeros (equal
 * as numbers, different as bits) still match.
 */
static inline int float_same_value(float a, float b)
{
    uint32_t bits_a, bits_b;
    a += 0.0f;
    b += 0.0f;
    memcpy(&bits_a, &a, sizeof(bits_a));
    memcpy(&bits_b, &b, sizeof(bits_b));
    return bits_a == bits_b;
}
