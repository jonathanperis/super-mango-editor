/*
 * depth.c — The 2.5D view (see depth.h for the idea and the call order).
 *
 * How to read it:
 *   1. Masks: one byte per pixel of each loaded texture, "is it opaque".
 *   2. Shapes: for one sprite frame of a mask, the front rectangles and the
 *      edge runs (depth_shape_build), cached per texture and frame.
 *   3. The view: a perspective projection whose z = 0 plane lands on the
 *      render target pixel for pixel, so fronts match the 2D game.
 *   4. depth_sprite: emits the front, then one wall per edge run.
 */
#include "depth.h"
#include "graphics.h"   /* SPRITE_FLIP_X, SPRITE_FLIP_Y */

#include <math.h>
#include <rlgl.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Masks                                                               */
/* ------------------------------------------------------------------ */

/* A pixel this transparent or more is treated as empty space. */
#define MASK_ALPHA_MIN 24
/* Larger images (backgrounds, fog) are only ever drawn flat. */
#define MASK_MAX_PIXELS (1024 * 1024)

typedef struct {
    unsigned int   id;      /* raylib texture id; 0 marks a free slot */
    int            w, h;
    unsigned char *alpha;   /* w*h bytes, 1 = opaque */
} DepthMask;

static DepthMask *s_masks;
static int s_mask_count, s_mask_cap;

static DepthMask *mask_find(unsigned int id)
{
    for (int i = 0; i < s_mask_count; i++)
        if (s_masks[i].id == id) return &s_masks[i];
    return NULL;
}

static void shapes_forget(unsigned int texture_id);

void depth_mask_forget(unsigned int texture_id)
{
    DepthMask *mask = mask_find(texture_id);
    shapes_forget(texture_id);
    if (!mask) return;
    free(mask->alpha);
    *mask = s_masks[--s_mask_count];
}

void depth_mask_set(unsigned int texture_id, const Image *image)
{
    if (!texture_id || !image || !image->data) return;
    /* The id may be one a released texture used before. */
    depth_mask_forget(texture_id);
    if (image->width <= 0 || image->height <= 0 ||
        image->width > MASK_MAX_PIXELS / image->height) return;

    size_t pixels = (size_t)image->width * (size_t)image->height;
    unsigned char *alpha = malloc(pixels);
    if (!alpha) return;
    if (image->format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) {
        const unsigned char *rgba = image->data;
        for (size_t i = 0; i < pixels; i++) alpha[i] = rgba[i * 4 + 3] >= MASK_ALPHA_MIN;
    } else {
        /* Any other format: let raylib convert a copy. */
        Color *colors = LoadImageColors(*image);
        if (!colors) { free(alpha); return; }
        for (size_t i = 0; i < pixels; i++) alpha[i] = colors[i].a >= MASK_ALPHA_MIN;
        UnloadImageColors(colors);
    }

    if (s_mask_count == s_mask_cap) {
        int cap = s_mask_cap ? s_mask_cap * 2 : 32;
        DepthMask *grown = realloc(s_masks, (size_t)cap * sizeof(*grown));
        if (!grown) { free(alpha); return; }
        s_masks = grown;
        s_mask_cap = cap;
    }
    s_masks[s_mask_count++] = (DepthMask){texture_id, image->width, image->height, alpha};
}

/* ------------------------------------------------------------------ */
/* Shapes                                                              */
/* ------------------------------------------------------------------ */

static int push_rect(DepthShape *shape, int *cap, DepthRect rect)
{
    if (shape->rect_count == *cap) {
        int grown_cap = *cap ? *cap * 2 : 16;
        DepthRect *grown = realloc(shape->rects, (size_t)grown_cap * sizeof(*grown));
        if (!grown) return 0;
        shape->rects = grown;
        *cap = grown_cap;
    }
    shape->rects[shape->rect_count++] = rect;
    return 1;
}

static int push_edge(DepthShape *shape, int *cap, DepthEdge edge)
{
    if (shape->edge_count == *cap) {
        int grown_cap = *cap ? *cap * 2 : 32;
        DepthEdge *grown = realloc(shape->edges, (size_t)grown_cap * sizeof(*grown));
        if (!grown) return 0;
        shape->edges = grown;
        *cap = grown_cap;
    }
    shape->edges[shape->edge_count++] = edge;
    return 1;
}

/* Opaque, with everything outside the frame counted as empty. */
static int solid(const unsigned char *alpha, int stride, int w, int h, int x, int y)
{
    return x >= 0 && y >= 0 && x < w && y < h && alpha[y * stride + x];
}

/* Pixel-art outlines are a pixel or two of near-black. A wall painted from
 * the edge pixels themselves would be black too, so it takes its colours
 * from up to this many pixels further in. */
#define PAINT_INSET 2

/* The line a wall is painted from: as far inside the edge as PAINT_INSET
 * allows while every pixel of the run is still opaque there. `along_x` is
 * true for top/bottom edges (the run lies along x on row `line`). */
static short paint_line(const unsigned char *alpha, int stride, int w, int h,
                        int along_x, int line, int from, int to, int inward)
{
    int best = line;
    for (int step = 1; step <= PAINT_INSET; step++) {
        int candidate = line + inward * step;
        for (int i = from; i < to; i++) {
            int x = along_x ? i : candidate, y = along_x ? candidate : i;
            if (!solid(alpha, stride, w, h, x, y)) return (short)best;
        }
        best = candidate;
    }
    return (short)best;
}

void depth_shape_free(DepthShape *shape)
{
    if (!shape) return;
    free(shape->rects);
    free(shape->edges);
    *shape = (DepthShape){0};
}

int depth_shape_build(DepthShape *shape, const unsigned char *alpha, int stride, int w, int h)
{
    int rect_cap = 0, edge_cap = 0;
    *shape = (DepthShape){0};
    if (!alpha || w <= 0 || h <= 0 || w > 32767 || h > 32767) return 1;

    /* Front: each row's runs of opaque pixels. A run that repeats the one
     * directly above it grows that rectangle instead of starting another,
     * so a solid 16x16 tile is one rectangle, not sixteen. */
    int row_first = 0;   /* index of the first rectangle the previous row touched */
    for (int y = 0; y < h; y++) {
        int row_start = shape->rect_count;
        for (int x = 0; x < w; ) {
            if (!solid(alpha, stride, w, h, x, y)) { x++; continue; }
            int x0 = x;
            while (x < w && solid(alpha, stride, w, h, x, y)) x++;
            int grown = 0;
            for (int i = row_first; i < row_start && !grown; i++) {
                DepthRect *above = &shape->rects[i];
                if (above->y1 == y && above->x0 == x0 && above->x1 == x) {
                    above->y1 = (short)(y + 1);
                    grown = 1;
                }
            }
            if (!grown && !push_rect(shape, &rect_cap,
                    (DepthRect){(short)x0, (short)y, (short)x, (short)(y + 1)})) goto fail;
        }
        /* Rectangles that stopped growing can never grow again; skip the
         * ones before the oldest still reaching this row. */
        while (row_first < shape->rect_count && shape->rects[row_first].y1 <= y) row_first++;
    }

    /* Top and bottom edges: runs along each row. */
    for (int y = 0; y < h; y++) {
        for (int dir = 0; dir < 2; dir++) {
            int ny = dir == 0 ? y - 1 : y + 1;
            for (int x = 0; x < w; ) {
                if (!(solid(alpha, stride, w, h, x, y) && !solid(alpha, stride, w, h, x, ny))) { x++; continue; }
                int x0 = x;
                while (x < w && solid(alpha, stride, w, h, x, y) && !solid(alpha, stride, w, h, x, ny)) x++;
                if (!push_edge(shape, &edge_cap, (DepthEdge){(short)y, (short)x0, (short)x,
                        paint_line(alpha, stride, w, h, 1, y, x0, x, dir == 0 ? 1 : -1),
                        (unsigned char)(dir == 0 ? DEPTH_EDGE_TOP : DEPTH_EDGE_BOTTOM)})) goto fail;
            }
        }
    }
    /* Left and right edges: runs down each column. */
    for (int x = 0; x < w; x++) {
        for (int dir = 0; dir < 2; dir++) {
            int nx = dir == 0 ? x - 1 : x + 1;
            for (int y = 0; y < h; ) {
                if (!(solid(alpha, stride, w, h, x, y) && !solid(alpha, stride, w, h, nx, y))) { y++; continue; }
                int y0 = y;
                while (y < h && solid(alpha, stride, w, h, x, y) && !solid(alpha, stride, w, h, nx, y)) y++;
                if (!push_edge(shape, &edge_cap, (DepthEdge){(short)x, (short)y0, (short)y,
                        paint_line(alpha, stride, w, h, 0, x, y0, y, dir == 0 ? 1 : -1),
                        (unsigned char)(dir == 0 ? DEPTH_EDGE_LEFT : DEPTH_EDGE_RIGHT)})) goto fail;
            }
        }
    }
    return 1;

fail:
    depth_shape_free(shape);
    return 0;
}

/* The shape cache: one entry per (texture, frame) drawn so far. */
#define SHAPE_SLOTS 1024   /* a power of two */

typedef struct {
    unsigned int id;        /* 0 = empty slot */
    int x, y, w, h;
    DepthShape shape;
} ShapeEntry;

static ShapeEntry s_shapes[SHAPE_SLOTS];
static int s_shape_used;

static void shapes_clear(void)
{
    for (int i = 0; i < SHAPE_SLOTS; i++) {
        depth_shape_free(&s_shapes[i].shape);
        s_shapes[i].id = 0;
    }
    s_shape_used = 0;
}

static void shapes_forget(unsigned int texture_id)
{
    /* Open addressing cannot remove single entries without breaking the
     * probe chains of others, and textures are released together when a
     * level closes, so drop everything and let the next frames rebuild. */
    for (int i = 0; i < SHAPE_SLOTS; i++) {
        if (s_shapes[i].id == texture_id) { shapes_clear(); return; }
    }
}

static const DepthShape *shape_for(const DepthMask *mask, int x, int y, int w, int h)
{
    unsigned int hash = mask->id * 2654435761u ^ (unsigned int)x * 40503u ^
                        (unsigned int)y * 9176u ^ (unsigned int)w * 131u ^ (unsigned int)h;
    /* Keep a quarter of the slots empty so a probe always ends. */
    if (s_shape_used >= SHAPE_SLOTS - SHAPE_SLOTS / 4) shapes_clear();
    for (unsigned int probe = 0; probe < SHAPE_SLOTS; probe++) {
        ShapeEntry *entry = &s_shapes[(hash + probe) & (SHAPE_SLOTS - 1)];
        if (entry->id == mask->id && entry->x == x && entry->y == y &&
            entry->w == w && entry->h == h) return &entry->shape;
        if (entry->id) continue;
        if (!depth_shape_build(&entry->shape, mask->alpha + (size_t)y * (size_t)mask->w + x,
                               mask->w, w, h)) return NULL;
        entry->id = mask->id;
        entry->x = x; entry->y = y; entry->w = w; entry->h = h;
        s_shape_used++;
        return &entry->shape;
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* The view                                                            */
/* ------------------------------------------------------------------ */

/* How far each depth_solid/depth_flat call moves the fronts toward the
 * eye, in world pixels. Small enough that thirty layers shift a front by
 * less than half a pixel at the screen's edge, so fronts stay pixel-exact. */
#define LAYER_STEP 0.012f

static struct {
    DepthView view;
    int   open;        /* between depth_begin and depth_end                 */
    int   active;      /* open and not suspended                            */
    int   flat;        /* current style: no walls, no depth writes          */
    float thickness;   /* current slab depth, in target pixels              */
    float front;       /* z of the current fronts, in target pixels         */
} s_depth;

static void view_enter(void)
{
    const DepthView *v = &s_depth.view;
    double dist = (double)v->eye_dist * v->unit;
    double near_plane = dist * 0.5, far_plane = dist * 2.0;
    double k = near_plane / dist;

    rlDrawRenderBatchActive();
    /* An off-centre frustum: the z = 0 plane fills the target exactly, x to
     * the right and y down, wherever the eye point is. Row 0 maps to the
     * top of the frustum, the same way BeginTextureMode's own projection
     * lays out a render target. */
    rlMatrixMode(RL_PROJECTION);
    rlPushMatrix();
    rlLoadIdentity();
    rlFrustum(-(double)v->eye_x * k, ((double)v->target_w - v->eye_x) * k,
              -((double)v->target_h - v->eye_y) * k, (double)v->eye_y * k,
              near_plane, far_plane);
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
    rlScalef(1.0f, -1.0f, 1.0f);
    rlTranslatef(-v->eye_x, -v->eye_y, -(float)dist);
    rlEnableDepthTest();
    /* The y flip above reverses every triangle's winding, and walls are
     * seen from whichever side faces the eye. */
    rlDisableBackfaceCulling();
    if (s_depth.flat) rlDisableDepthMask();
    s_depth.active = 1;
}

static void view_leave(void)
{
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    rlEnableBackfaceCulling();
    rlDisableDepthTest();
    rlMatrixMode(RL_PROJECTION);
    rlPopMatrix();
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
    s_depth.active = 0;
}

void depth_begin(DepthView view)
{
    if (s_depth.open || view.target_w <= 0 || view.target_h <= 0 ||
        !(view.eye_dist > 0.0f) || !(view.unit > 0.0f)) return;
    s_depth.view = view;
    s_depth.open = 1;
    s_depth.flat = 0;
    s_depth.thickness = 0.0f;
    s_depth.front = 0.0f;
    view_enter();
}

void depth_end(void)
{
    if (!s_depth.open) return;
    if (s_depth.active) view_leave();
    s_depth.open = 0;
}

void depth_suspend(void)
{
    if (s_depth.open && s_depth.active) view_leave();
}

void depth_resume(void)
{
    if (s_depth.open && !s_depth.active) view_enter();
}

int depth_active(void)
{
    return s_depth.active;
}

static void set_style(int flat, float thickness)
{
    if (!s_depth.open) return;
    s_depth.front += LAYER_STEP * s_depth.view.unit;
    s_depth.thickness = thickness * s_depth.view.unit;
    if (flat == s_depth.flat) return;
    s_depth.flat = flat;
    if (!s_depth.active) return;
    /* Depth writes are GL state: draw what is queued before changing it. */
    rlDrawRenderBatchActive();
    if (flat) rlDisableDepthMask();
    else rlEnableDepthMask();
}

void depth_solid(float thickness)
{
    set_style(!(thickness > 0.0f), thickness > 0.0f ? thickness : 0.0f);
}

void depth_flat(void)
{
    set_style(1, 0.0f);
}

/* ------------------------------------------------------------------ */
/* Drawing one sprite                                                  */
/* ------------------------------------------------------------------ */

/* How bright each face is next to the front (which keeps the art's own
 * colours): light comes from above, so tops stay bright and undersides go
 * dark. */
#define SHADE_TOP    0.86f
#define SHADE_SIDE   0.64f
#define SHADE_BOTTOM 0.46f
/* A wall also darkens toward its back edge, which reads as distance and
 * keeps a long top face from looking like a flat band of colour. */
#define SHADE_BACK   0.70f

/* Everything depth_sprite worked out once and each quad needs. */
typedef struct {
    Rectangle source, dest;
    Vector2 pivot;
    float sin_a, cos_a;
    float inv_tex_w, inv_tex_h;
    int flip;
} SpritePlace;

/* A point of the frame, in source pixels from its top-left, to the target. */
static Vector2 place_point(const SpritePlace *p, float lx, float ly)
{
    if (p->flip & SPRITE_FLIP_X) lx = p->source.width - lx;
    if (p->flip & SPRITE_FLIP_Y) ly = p->source.height - ly;
    float x = lx * p->dest.width / p->source.width - p->pivot.x;
    float y = ly * p->dest.height / p->source.height - p->pivot.y;
    return (Vector2){p->dest.x + p->pivot.x + x * p->cos_a - y * p->sin_a,
                     p->dest.y + p->pivot.y + x * p->sin_a + y * p->cos_a};
}

static void emit_vertex(const SpritePlace *p, float lx, float ly, float z, float u, float v)
{
    Vector2 at = place_point(p, lx, ly);
    rlTexCoord2f((p->source.x + u) * p->inv_tex_w, (p->source.y + v) * p->inv_tex_h);
    rlVertex3f(at.x, at.y, z);
}

static Color shade(Color tint, float amount)
{
    return (Color){(unsigned char)((float)tint.r * amount), (unsigned char)((float)tint.g * amount),
                   (unsigned char)((float)tint.b * amount), tint.a};
}

/* A piece of the front, from (x0, y0) to (x1, y1) in frame pixels. */
static void emit_front(const SpritePlace *p, float x0, float y0, float x1, float y1,
                       float z, Color tint)
{
    rlColor4ub(tint.r, tint.g, tint.b, tint.a);
    emit_vertex(p, x0, y0, z, x0, y0);
    emit_vertex(p, x0, y1, z, x0, y1);
    emit_vertex(p, x1, y1, z, x1, y1);
    emit_vertex(p, x1, y0, z, x1, y0);
}

/* One wall: the run's edge at the front, the same edge at the back, and in
 * between the colours of the pixels along it. */
static void emit_wall(const SpritePlace *p, DepthEdge e, float front, float back, Color tint)
{
    float ax, ay, bx, by;      /* the run's two ends, in frame pixels      */
    float au, av, bu, bv;      /* where each end samples the texture       */
    float amount = SHADE_SIDE;
    int side = e.side;
    /* A mirrored sprite shows its underside where its top was. */
    if (p->flip & SPRITE_FLIP_Y) {
        if (side == DEPTH_EDGE_TOP) side = DEPTH_EDGE_BOTTOM;
        else if (side == DEPTH_EDGE_BOTTOM) side = DEPTH_EDGE_TOP;
    }
    if (side == DEPTH_EDGE_TOP) amount = SHADE_TOP;
    else if (side == DEPTH_EDGE_BOTTOM) amount = SHADE_BOTTOM;

    if (e.side == DEPTH_EDGE_TOP || e.side == DEPTH_EDGE_BOTTOM) {
        ay = by = (float)e.line + (e.side == DEPTH_EDGE_BOTTOM ? 1.0f : 0.0f);
        ax = e.from; bx = e.to;
        au = ax; bu = bx;
        av = bv = (float)e.paint + 0.5f;   /* the middle of the painted row */
    } else {
        ax = bx = (float)e.line + (e.side == DEPTH_EDGE_RIGHT ? 1.0f : 0.0f);
        ay = e.from; by = e.to;
        av = ay; bv = by;
        au = bu = (float)e.paint + 0.5f;
    }
    Color near_c = shade(tint, amount), far_c = shade(tint, amount * SHADE_BACK);
    rlColor4ub(near_c.r, near_c.g, near_c.b, near_c.a);
    emit_vertex(p, ax, ay, front, au, av);
    rlColor4ub(far_c.r, far_c.g, far_c.b, far_c.a);
    emit_vertex(p, ax, ay, back,  au, av);
    emit_vertex(p, bx, by, back,  bu, bv);
    rlColor4ub(near_c.r, near_c.g, near_c.b, near_c.a);
    emit_vertex(p, bx, by, front, bu, bv);
}

void depth_sprite(const Texture2D *texture, Rectangle source, Rectangle dest,
                  Vector2 pivot, float angle, int flip, Color tint)
{
    if (!texture || texture->id == 0 || texture->width <= 0 || texture->height <= 0 ||
        !(source.width > 0.0f) || !(source.height > 0.0f)) return;

    float radians = angle * DEG2RAD;
    SpritePlace place = {
        .source = source, .dest = dest, .pivot = pivot,
        .sin_a = sinf(radians), .cos_a = cosf(radians),
        .inv_tex_w = 1.0f / (float)texture->width, .inv_tex_h = 1.0f / (float)texture->height,
        .flip = flip,
    };
    float front = s_depth.front;

    /* The outline, when this is a slab and the frame lies on whole pixels
     * inside a texture whose pixels we were given. */
    const DepthShape *shape = NULL;
    int sx = (int)source.x, sy = (int)source.y, sw = (int)source.width, sh = (int)source.height;
    if (!s_depth.flat && s_depth.thickness > 0.0f && tint.a > 0) {
        const DepthMask *mask = mask_find(texture->id);
        if (mask && mask->w == texture->width && mask->h == texture->height &&
            (float)sx == source.x && (float)sy == source.y &&
            (float)sw == source.width && (float)sh == source.height &&
            sx >= 0 && sy >= 0 && sx + sw <= mask->w && sy + sh <= mask->h) {
            shape = shape_for(mask, sx, sy, sw, sh);
        }
    }

    rlSetTexture(texture->id);
    rlBegin(RL_QUADS);
    if (!shape) {
        /* Flat: the whole frame as one quad, as the 2D game draws it. */
        emit_front(&place, 0.0f, 0.0f, source.width, source.height, front, tint);
    } else {
        float back = front - s_depth.thickness;
        for (int i = 0; i < shape->edge_count; i++)
            emit_wall(&place, shape->edges[i], front, back, tint);
        for (int i = 0; i < shape->rect_count; i++) {
            DepthRect r = shape->rects[i];
            emit_front(&place, r.x0, r.y0, r.x1, r.y1, front, tint);
        }
    }
    rlEnd();
    rlSetTexture(0);
}
