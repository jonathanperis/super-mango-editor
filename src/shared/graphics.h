#pragma once

#include <raylib.h>
#include <rlgl.h>
#include <stdlib.h>
#include "geometry.h"

int display_open(int width, int height, const char *title, int hidden);
void display_present(RenderTexture2D target);

/* Asset slots own heap-stable raylib values. Borrowers never unload a slot. */
static inline Texture2D *texture_load(const char *path)
{
    Texture2D value = LoadTexture(path);
    if (!IsTextureValid(value)) return NULL;
    Texture2D *texture = malloc(sizeof(*texture));
    if (!texture) { UnloadTexture(value); return NULL; }
    *texture = value;
    SetTextureFilter(value, TEXTURE_FILTER_POINT);
    return texture;
}

static inline void texture_unload(Texture2D *texture)
{
    if (!texture) return;
    /* A UI cache eviction can occur before EndDrawing flushes queued draws. */
    rlDrawRenderBatchActive();
    UnloadTexture(*texture);
    free(texture);
}

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
    if (flip & SPRITE_FLIP_X) src.width = -src.width;
    if (flip & SPRITE_FLIP_Y) src.height = -src.height;
    Rectangle dst = rect_to_raylib(*dest);
    dst.x += pivot.x;
    dst.y += pivot.y;
    DrawTexturePro(*texture, src, dst, pivot, angle, tint);
}

static inline void sprite_draw(const Texture2D *texture, const IntRect *source,
                               const IntRect *dest, float angle, int flip, Color tint)
{
    if (!dest) return;
    Vector2 pivot = {dest->w / 2.0f, dest->h / 2.0f};
    sprite_draw_pivot(texture, source, dest, angle, flip, pivot, tint);
}
