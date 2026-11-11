#include <stdio.h>

#include "core/entity_utils.h"
#include "shared/depth.h"
#include "shared/model.h"
#include <string.h>

static int expect_int(const char *name, int actual, int expected)
{
    if (actual != expected) {
        fprintf(stderr, "entity_utils_test: %s got %d expected %d\n", name, actual, expected);
        return 1;
    }
    return 0;
}

static int expect_float(const char *name, float actual, float expected)
{
    float diff = actual - expected;
    if (diff < 0.0f) diff = -diff;
    if (diff > 0.001f) {
        fprintf(stderr, "entity_utils_test: %s got %.3f expected %.3f\n", name, actual, expected);
        return 1;
    }
    return 0;
}

static int advances_animation_and_preserves_leftover_time(void)
{
    int frame = 0;
    float timer = 0.0f;

    if (expect_int("first advance wrap", animate_frame_ms(&frame, &timer, 0.120f, 100, 3), 0) != 0)
        return 1;
    if (expect_int("first frame", frame, 1) != 0) return 1;
    if (expect_float("leftover ms", timer, 20.0f) != 0) return 1;

    if (expect_int("second advance wrap", animate_frame_ms(&frame, &timer, 0.100f, 100, 3), 0) != 0)
        return 1;
    if (expect_int("second frame", frame, 2) != 0) return 1;

    if (expect_int("third advance wrap", animate_frame_ms(&frame, &timer, 0.100f, 100, 3), 1) != 0)
        return 1;
    if (expect_int("wrapped frame", frame, 0) != 0) return 1;

    return 0;
}

/* 61 fixed 1/60 s steps are 1016.7 ms: ten 100 ms frames. Truncating each
 * step to 16 ms (976 ms total) would advance only nine, ~4% slow. */
static int fixed_steps_keep_fractional_milliseconds(void)
{
    int frame = 0;
    float timer = 0.0f;
    for (int step = 0; step < 61; step++)
        animate_frame_ms(&frame, &timer, 1.0f / 60.0f, 100, 1000);
    if (expect_int("advances after 61 steps", frame, 10) != 0) return 1;
    if (expect_float("carried remainder ms", timer, 16.667f) != 0) return 1;
    return 0;
}

static int reverses_patrol_at_boundaries(void)
{
    float x = 88.0f;
    float vx = 50.0f;

    patrol_update(&x, &vx, 16.0f, 20.0f, 100.0f, 0.25f);
    if (expect_float("right snap x", x, 84.0f) != 0) return 1;
    if (expect_float("right reverse vx", vx, -50.0f) != 0) return 1;

    x = 22.0f;
    vx = -50.0f;
    patrol_update(&x, &vx, 16.0f, 20.0f, 100.0f, 0.25f);
    if (expect_float("left snap x", x, 20.0f) != 0) return 1;
    if (expect_float("left reverse vx", vx, 50.0f) != 0) return 1;

    return 0;
}

static int reverses_at_floor_gaps(void)
{
    int gaps[] = {100};
    float x = 80.0f;
    float vx = 40.0f;

    patrol_gap_reverse(&x, &vx, 0.0f, 48.0f, gaps, 1, 32);
    if (expect_float("right gap snap x", x, 52.0f) != 0) return 1;
    if (expect_float("right gap reverse vx", vx, -40.0f) != 0) return 1;

    x = 120.0f;
    vx = -40.0f;
    patrol_gap_reverse(&x, &vx, 0.0f, 16.0f, gaps, 1, 32);
    if (expect_float("left gap snap x", x, 132.0f) != 0) return 1;
    if (expect_float("left gap reverse vx", vx, 40.0f) != 0) return 1;

    return 0;
}

/* A turn flips the direction only: the level's speed (here 130 px/s, not a
 * type's built-in speed) survives both kinds of turn. */
static int turns_keep_the_authored_speed(void)
{
    int gaps[] = {100};
    float x = 88.0f;
    float vx = 130.0f;

    patrol_update(&x, &vx, 16.0f, 20.0f, 100.0f, 0.25f);
    if (expect_float("patrol turn keeps speed", vx, -130.0f) != 0) return 1;
    patrol_update(&x, &vx, 16.0f, 20.0f, 100.0f, 1.0f);
    if (expect_float("second patrol turn keeps speed", vx, 130.0f) != 0) return 1;

    x = 80.0f;
    vx = 130.0f;
    patrol_gap_reverse(&x, &vx, 0.0f, 48.0f, gaps, 1, 32);
    if (expect_float("gap turn keeps speed", vx, -130.0f) != 0) return 1;

    return 0;
}

static int computes_sound_falloff(void)
{
    if (expect_int("near volume", sound_volume_for_distance(0.0f, 400.0f, 128), 128) != 0)
        return 1;
    if (expect_int("mid volume", sound_volume_for_distance(200.0f, 400.0f, 128), 64) != 0)
        return 1;
    if (expect_int("far volume", sound_volume_for_distance(400.0f, 400.0f, 128), 0) != 0)
        return 1;

    return 0;
}

/* A sound pans toward the side of the screen its source is drawn on,
 * never past SOUND_PAN_MAX; raylib 6 puts -1 on the left. */
static int pans_with_screen_position(void)
{
    if (expect_float("centre pan", sound_pan_for_screen_x(1200.0f, 1000, 400.0f), 0.0f) != 0) return 1;
    if (expect_float("left edge pan", sound_pan_for_screen_x(1000.0f, 1000, 400.0f), -SOUND_PAN_MAX) != 0) return 1;
    if (expect_float("right edge pan", sound_pan_for_screen_x(1400.0f, 1000, 400.0f), SOUND_PAN_MAX) != 0) return 1;
    if (expect_float("quarter pan", sound_pan_for_screen_x(1100.0f, 1000, 400.0f), -SOUND_PAN_MAX / 2.0f) != 0) return 1;
    if (expect_float("off-screen left clamps", sound_pan_for_screen_x(900.0f, 1000, 400.0f), -SOUND_PAN_MAX) != 0) return 1;
    if (expect_float("off-screen right clamps", sound_pan_for_screen_x(1500.0f, 1000, 400.0f), SOUND_PAN_MAX) != 0) return 1;
    return 0;
}

/* The render cull keeps a margin, so a slab just off screen still draws
 * the side that reaches back into view (2.5D view, shared/depth.h). */
static int cull_keeps_a_margin_for_slab_sides(void)
{
    if (expect_int("on screen", on_screen_x(10, 16), 1) != 0) return 1;
    if (expect_int("just off left", on_screen_x(-16 - SCREEN_CULL_MARGIN + 1, 16), 1) != 0) return 1;
    if (expect_int("past left margin", on_screen_x(-16 - SCREEN_CULL_MARGIN, 16), 0) != 0) return 1;
    if (expect_int("just off right", on_screen_x(GAME_W + SCREEN_CULL_MARGIN - 1, 16), 1) != 0) return 1;
    if (expect_int("past right margin", on_screen_x(GAME_W + SCREEN_CULL_MARGIN, 16), 0) != 0) return 1;
    return 0;
}

static int count_edges(const DepthShape *shape, int side)
{
    int count = 0;
    for (int i = 0; i < shape->edge_count; i++) count += shape->edges[i].side == side;
    return count;
}

/* A fully opaque frame is one front rectangle and one wall per side, and
 * each wall is painted from two pixels inside the edge. */
static int solid_frame_is_one_box(void)
{
    unsigned char alpha[8 * 8];
    for (int i = 0; i < 64; i++) alpha[i] = 1;
    DepthShape shape;
    if (expect_int("build", depth_shape_build(&shape, alpha, 8, 8, 8), 1) != 0) return 1;
    int failed = expect_int("rects", shape.rect_count, 1) ||
                 expect_int("edges", shape.edge_count, 4);
    if (!failed) {
        failed = expect_int("rect x1", shape.rects[0].x1, 8) || expect_int("rect y1", shape.rects[0].y1, 8);
        for (int i = 0; i < shape.edge_count && !failed; i++) {
            const DepthEdge *e = &shape.edges[i];
            int inward = (e->side == DEPTH_EDGE_TOP || e->side == DEPTH_EDGE_LEFT) ? 2 : 5;
            failed = expect_int("run from", e->from, 0) || expect_int("run to", e->to, 8) ||
                     expect_int("paint line", e->paint, inward);
        }
    }
    depth_shape_free(&shape);
    return failed;
}

/* An L of pixels inside a wider row stride: the fronts cover exactly the
 * opaque pixels, and an edge appears wherever opaque meets empty. */
static int outline_follows_the_opaque_pixels(void)
{
    /* 4 wide, 3 tall, stride 6 (two unused bytes per row):
     *   X . . .
     *   X . . .
     *   X X X .                                                        */
    const unsigned char alpha[] = {
        1,0,0,0, 9,9,
        1,0,0,0, 9,9,
        1,1,1,0, 9,9,
    };
    DepthShape shape;
    if (expect_int("build", depth_shape_build(&shape, alpha, 6, 4, 3), 1) != 0) return 1;
    int area = 0;
    for (int i = 0; i < shape.rect_count; i++)
        area += (shape.rects[i].x1 - shape.rects[i].x0) * (shape.rects[i].y1 - shape.rects[i].y0);
    int failed = expect_int("front area", area, 5) ||
                 expect_int("front pieces", shape.rect_count, 2) ||
                 expect_int("top edges", count_edges(&shape, DEPTH_EDGE_TOP), 2) ||
                 expect_int("bottom edges", count_edges(&shape, DEPTH_EDGE_BOTTOM), 1) ||
                 expect_int("left edges", count_edges(&shape, DEPTH_EDGE_LEFT), 1) ||
                 expect_int("right edges", count_edges(&shape, DEPTH_EDGE_RIGHT), 2);
    depth_shape_free(&shape);
    if (failed) return 1;

    /* Nothing opaque: nothing to draw. */
    const unsigned char empty[4] = {0};
    if (expect_int("empty build", depth_shape_build(&shape, empty, 2, 2, 2), 1) != 0) return 1;
    failed = expect_int("empty rects", shape.rect_count, 0) || expect_int("empty edges", shape.edge_count, 0);
    depth_shape_free(&shape);
    return failed;
}

/* One triangle, written byte by byte as tools/models/kit.py writes it. */
static int build_triangle_file(unsigned char *out)
{
    static const float corners[3][3] = {{0, 0, 0}, {16, 0, 0}, {0, 8, -4}};
    int at = 0;
    memcpy(out, "SMM1", 4); at = 4;
    out[at++] = 3; out[at++] = 0; out[at++] = 0; out[at++] = 0;   /* vertices */
    out[at++] = 3; out[at++] = 0; out[at++] = 0; out[at++] = 0;   /* indices  */
    for (int i = 0; i < 3; i++) {
        memcpy(out + at, corners[i], 12); at += 12;   /* tests run little-endian */
        out[at++] = 200; out[at++] = 100; out[at++] = 50; out[at++] = 255;
    }
    for (int i = 0; i < 3; i++) { out[at++] = (unsigned char)i; out[at++] = 0; }
    return at;
}

/* The model loader accepts a well-formed file and refuses damaged ones. */
static int model_loader_checks_its_input(void)
{
    unsigned char file[128];
    int size = build_triangle_file(file);
    Model3D *model = model_parse(file, size);
    if (!model) { fprintf(stderr, "entity_utils_test: valid model rejected\n"); return 1; }
    int failed = expect_int("vertices", model->vertex_count, 3) ||
                 expect_int("indices", model->index_count, 3) ||
                 expect_float("x", model->vertices[1].x, 16.0f) ||
                 expect_float("z", model->vertices[2].z, -4.0f) ||
                 expect_int("green", model->vertices[0].g, 100);
    model_unload(model);
    if (failed) return 1;

    unsigned char bad[128];
    memcpy(bad, file, sizeof(bad)); bad[0] = 'X';              /* wrong magic      */
    if (model_parse(bad, size)) { fprintf(stderr, "entity_utils_test: bad magic accepted\n"); return 1; }
    if (model_parse(file, size - 1)) { fprintf(stderr, "entity_utils_test: short file accepted\n"); return 1; }
    memcpy(bad, file, sizeof(bad)); bad[8] = 4;                /* not whole triangles */
    if (model_parse(bad, size)) { fprintf(stderr, "entity_utils_test: stray index accepted\n"); return 1; }
    memcpy(bad, file, sizeof(bad)); bad[size - 2] = 9;         /* index past the end */
    if (model_parse(bad, size)) { fprintf(stderr, "entity_utils_test: wild index accepted\n"); return 1; }
    memcpy(bad, file, sizeof(bad)); memset(bad + 12, 0xFF, 4); /* x is NaN          */
    if (model_parse(bad, size)) { fprintf(stderr, "entity_utils_test: NaN accepted\n"); return 1; }
    if (model_parse(NULL, 0)) return 1;
    return 0;
}

int main(void)
{
    if (model_loader_checks_its_input() != 0) return 1;
    if (cull_keeps_a_margin_for_slab_sides() != 0) return 1;
    if (solid_frame_is_one_box() != 0) return 1;
    if (outline_follows_the_opaque_pixels() != 0) return 1;
    if (advances_animation_and_preserves_leftover_time() != 0) return 1;
    if (pans_with_screen_position() != 0) return 1;
    if (fixed_steps_keep_fractional_milliseconds() != 0) return 1;
    if (reverses_patrol_at_boundaries() != 0) return 1;
    if (reverses_at_floor_gaps() != 0) return 1;
    if (turns_keep_the_authored_speed() != 0) return 1;
    if (computes_sound_falloff() != 0) return 1;

    puts("entity_utils_test: ok");
    return 0;
}
