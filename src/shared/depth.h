/*
 * depth.h — The 2.5D view: flat pixel-art sprites drawn as solid slabs.
 *
 * The game is still played on one flat plane. Only the picture changes:
 * while a depth view is open, sprite_draw (graphics.h) hands each sprite to
 * depth_sprite instead of drawing a flat quad. depth_sprite keeps the front
 * of the sprite exactly where the 2D game drew it, and pushes its outline
 * back into the screen as walls, so every opaque pixel becomes the front of
 * a small box. A perspective camera then shows the tops, bottoms and sides
 * of those boxes as they move away from the point the eye looks at.
 *
 * Nothing new is stored on disk. The walls are worked out from the alpha
 * channel of the same PNG the 2D game used, once per sprite frame, the
 * first time that frame is drawn.
 *
 *   depth_begin(view)        open the view (after the backdrop is drawn)
 *   depth_solid(thickness)   the sprites that follow are slabs this deep
 *   depth_flat()             the sprites that follow stay flat (water, ghost)
 *   ...sprite_draw calls...
 *   depth_end()              back to plain 2D for the HUD and overlays
 */
#pragma once

#include <raylib.h>

/* Where the eye is, in the units of the render target being drawn into. */
typedef struct {
    float eye_x;     /* the column the eye looks straight at                  */
    float eye_y;     /* the row it looks straight at: tops show below it      */
    float eye_dist;  /* how far in front of the picture the eye is, in world
                      * pixels; smaller is a stronger perspective             */
    float unit;      /* target pixels per world pixel (the editor's zoom)     */
    int   target_w;  /* size of the render target                             */
    int   target_h;
} DepthView;

/* The game's own eye: above the middle of the 400x300 screen, so the floor
 * and every platform below it show their top face. */
#define DEPTH_EYE_Y     96.0f
#define DEPTH_EYE_DIST  420.0f
/* The game's eye also leans a little toward the player: this share of how
 * far they are from the middle of the screen. Fronts do not move (they lie
 * on the picture plane); only the walls swing, as they do when you move
 * your head in front of a diorama. */
#define DEPTH_EYE_FOLLOW_X 0.35f
#define DEPTH_EYE_FOLLOW_Y 0.20f

/*
 * How deep each kind of thing is, in world pixels. The game's layer table
 * (render/game_render.c) and the editor's canvas (editor/canvas.c) both read
 * these, so the editor shows the level as the game will.
 *
 * The floor is the deepest thing in a level. What stands on it is thinner,
 * so it sits on the front of the floor's top face.
 */
#define DEPTH_FLOOR           28.0f
#define DEPTH_WATER           26.0f   /* fills the floor gaps, just short
                                       * of the floor's own back edge      */
#define DEPTH_PLATFORM        20.0f   /* the pillars                       */
#define DEPTH_FLOAT_PLATFORM  18.0f
#define DEPTH_SPIKE_PLATFORM  18.0f
#define DEPTH_BRIDGE          16.0f
#define DEPTH_SPIKE_BLOCK     14.0f
#define DEPTH_BOUNCEPAD       12.0f
#define DEPTH_SPIKE_ROW        8.0f
#define DEPTH_CREATURE         6.0f   /* the player, spiders               */
#define DEPTH_FLYER            5.0f   /* birds and fish                    */
#define DEPTH_AXE_TRAP         4.0f
#define DEPTH_LAST_STAR        4.0f
#define DEPTH_PICKUP           3.0f   /* coins and health stars            */
#define DEPTH_SAW              3.0f
#define DEPTH_LADDER           3.0f
#define DEPTH_THIN             2.0f   /* rails, vines, ropes, flames       */

/* Open the view. Everything drawn until depth_end is depth-tested, so call
 * it only inside a render target that was cleared this frame. */
void depth_begin(DepthView view);
void depth_end(void);

/* Leave the view for a moment to draw plain 2D shapes (editor markers),
 * then pick it up where it was. */
void depth_suspend(void);
void depth_resume(void);

/* Whether sprite_draw is currently handing sprites to depth_sprite. */
int depth_active(void);

/* How the sprites that follow are drawn. Each call also moves the fronts a
 * hair toward the eye, so a later layer still covers an earlier one. */
void depth_solid(float thickness);
void depth_flat(void);

/* One sprite. source is the unflipped region of the texture; dest is its
 * top-left based rectangle; flip is SPRITE_FLIP_X / SPRITE_FLIP_Y bits. */
void depth_sprite(const Texture2D *texture, Rectangle source, Rectangle dest,
                  Vector2 pivot, float angle, int flip, Color tint);

/* texture_load gives every texture's pixels here once, so the walls can
 * follow its outline; texture_unload takes them away again. */
void depth_mask_set(unsigned int texture_id, const Image *image);
void depth_mask_forget(unsigned int texture_id);

/* The outline of one sprite frame, as the pieces depth_sprite draws: front
 * rectangles that cover exactly the opaque pixels, and the runs of edge
 * between an opaque pixel and an empty one. Exposed for the tests. */
typedef struct { short x0, y0, x1, y1; } DepthRect;
typedef enum { DEPTH_EDGE_TOP, DEPTH_EDGE_BOTTOM, DEPTH_EDGE_LEFT, DEPTH_EDGE_RIGHT } DepthEdgeSide;
typedef struct {
    short line;   /* the pixel row (top/bottom) or column (left/right)        */
    short from;   /* first pixel of the run along that line                   */
    short to;     /* one past its last pixel                                  */
    short paint;  /* the row or column whose colours the wall is painted
                   * with: a little inside the edge, past the dark outline
                   * most of the art is drawn with                            */
    unsigned char side;
} DepthEdge;
typedef struct {
    DepthRect *rects;
    DepthEdge *edges;
    int rect_count, edge_count;
} DepthShape;

/* Build the shape of the w x h pixels at `alpha` (one byte per pixel, row
 * by row, `stride` bytes per row; non-zero is opaque). Returns 0 when memory
 * runs out. Free with depth_shape_free. */
int depth_shape_build(DepthShape *shape, const unsigned char *alpha, int stride, int w, int h);
void depth_shape_free(DepthShape *shape);
