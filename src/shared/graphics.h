/* Small drawing helpers that keep integer gameplay geometry separate from
 * raylib's floating-point drawing rectangles. These are project functions. */
#pragma once

#include <raylib.h>
#include <rlgl.h>
#include <stdlib.h>
#include "geometry.h"

int display_open(int width, int height, const char *title, int hidden);
void display_present(RenderTexture2D target);
/* Where display_present draws a canvas_w x canvas_h canvas inside a
 * window_w x window_h window, in window units. At 1x or more it is the
 * largest whole-number scale that fits, centred on whole-pixel offsets;
 * a smaller window gets a fractional fit. All-zero for empty sizes.
 * input_pointer_to_logical inverts it, so clicks match what is drawn. */
Rectangle display_viewport(int window_w, int window_h, int canvas_w, int canvas_h);

/* Asset slots own heap-stable raylib values. Borrowers never unload a slot.
 * Texture2D itself is a small handle, not the image pixels. Allocating a slot
 * preserves explicit NULL-on-failure and shared-pointer ownership throughout
 * the entity code; it does not allocate another copy of the GPU image.
 * The three functions below are defined in texture.c. */

/* Decode an image file and upload it. Returns an owned handle, or NULL when
 * the file is missing or is not an image. */
Texture2D *texture_load(const char *path);

/* Put a valid raylib texture into an owned handle, with the nearest-neighbour
 * filter pixel art needs. Every handle is made here; texture_load passes the
 * file it decoded. When memory runs out it unloads value and returns NULL. */
Texture2D *texture_own(Texture2D value);

/* Release the GPU image and the handle. NULL is ignored. */
void texture_unload(Texture2D *texture);

#ifdef MANGO_TESTING
/* Test builds only. texture_test_loads: image files texture_load has decoded
 * so far. texture_test_live: handles that exist right now, so a test can
 * check that every one made during a session was unloaded exactly once. */
int texture_test_loads(void);
int texture_test_live(void);
#endif

/*
 * DESTROY_TEX — unload a texture slot and clear it, in one statement.
 *
 * texture_unload already ignores NULL; setting the slot to NULL afterwards
 * makes a second cleanup of the same slot a harmless no-op. The resource
 * tables in game_resources.c and the editor's texture cleanup use it.
 * Slot owners clear pointers after unloading; borrowers never call this.
 */
#define DESTROY_TEX(tex) \
    do { texture_unload(tex); (tex) = NULL; } while (0)

enum { SPRITE_NORMAL = 0, SPRITE_FLIP_X = 1, SPRITE_FLIP_Y = 2 };

static inline Rectangle rect_to_raylib(IntRect r)
{
    return (Rectangle){(float)r.x, (float)r.y, (float)r.w, (float)r.h};
}

/* The destination is top-left based; raylib positions a rotated sprite by its
 * pivot instead. Keep that conversion in one place for all atlas consumers. */
static inline void sprite_draw_pivot(const Texture2D *texture, const IntRect *source,
                                     const IntRect *dest, float angle, int flip,
                                     Vector2 pivot, Color tint)
{
    if (!texture || !dest || dest->w <= 0 || dest->h <= 0) return;
    Rectangle src = source ? rect_to_raylib(*source) :
        (Rectangle){0, 0, (float)texture->width, (float)texture->height};
    /* Negative source dimensions mirror the sampled image. WHITE leaves
     * its original color/alpha unchanged; another tint affects this draw only. */
    if (flip & SPRITE_FLIP_X) src.width = -src.width;
    if (flip & SPRITE_FLIP_Y) src.height = -src.height;
    Rectangle dst = rect_to_raylib(*dest);
    dst.x += pivot.x;
    dst.y += pivot.y;
    /* angle is in degrees. dst locates the pivot; pivot is an offset from the
     * sprite's top-left, so add it once to retain the caller's top-left layout. */
    DrawTexturePro(*texture, src, dst, pivot, angle, tint);
}

static inline void sprite_draw(const Texture2D *texture, const IntRect *source,
                               const IntRect *dest, float angle, int flip, Color tint)
{
    if (!dest) return;
    Vector2 pivot = {dest->w / 2.0f, dest->h / 2.0f};
    sprite_draw_pivot(texture, source, dest, angle, flip, pivot, tint);
}
