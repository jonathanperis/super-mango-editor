/*
 * model.h — 3D models: load a .smm file, draw it in the 2.5D view.
 *
 * A model is a list of triangles whose corners each carry a colour. The
 * colours already contain the lighting: tools/models/ builds every model in
 * Blender, which works out the light and shade once and stores the result
 * (baking). Drawing is therefore just "put these coloured triangles here",
 * with no lighting maths and no shader, and looks the same on desktop, in
 * the browser and in the tests' software renderer.
 *
 * The .smm file ("Super Mango model"), all little-endian:
 *
 *   4 bytes   "SMM1"
 *   uint32    vertex count  (at most 65535)
 *   uint32    index count   (a multiple of 3: three per triangle)
 *   vertices  x, y, z as float32, then r, g, b, a as bytes (16 bytes each)
 *   indices   uint16 each, into the vertex list
 *
 * Positions are in game pixels: x to the right, y UP, z toward the viewer,
 * with the model's own origin at (0, 0, 0) — between the feet for a
 * creature, the top-left of the front for a terrain piece.
 */
#pragma once

#include <raylib.h>

typedef struct {
    float x, y, z;
    unsigned char r, g, b, a;
} ModelVertex;

typedef struct {
    ModelVertex    *vertices;
    unsigned short *indices;
    int vertex_count;
    int index_count;
} Model3D;

/* Read and check a .smm file. NULL when it is missing or malformed. */
Model3D *model_load(const char *path);
/* The same checks on bytes already in memory (the loader and its tests). */
Model3D *model_parse(const unsigned char *bytes, int size);
void model_unload(Model3D *model);

/* Where and how to draw a model. The zero value is not useful: start from
 * MODEL_POSE(x, y, scale). */
typedef struct {
    float x, y;      /* where the model's origin goes, in the units sprites
                      * are drawn with (logical pixels in the game)         */
    float z;         /* how far toward the viewer, in model pixels          */
    float scale;     /* drawn units per model pixel                         */
    float yaw;       /* degrees turned about the vertical axis; positive
                      * turns the front toward the right                    */
    float roll;      /* degrees turned about the axis pointing at the viewer */
    float stretch_x; /* squash and stretch: 1 leaves the shape alone        */
    float stretch_y;
    Color tint;      /* multiplied into every colour; alpha fades the model */
} ModelPose;

#define MODEL_POSE(px, py, s) \
    ((ModelPose){.x = (px), .y = (py), .scale = (s), .stretch_x = 1.0f, .stretch_y = 1.0f, .tint = WHITE})

/* Draw inside an open depth view (shared/depth.h). NULL is ignored. */
void model_draw(const Model3D *model, ModelPose pose);
