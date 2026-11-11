/*
 * model.c — .smm loading and drawing (the format is described in model.h).
 */
#include "model.h"
#include "depth.h"

#include <math.h>
#include <rlgl.h>
#include <stdlib.h>
#include <string.h>

#define SMM_HEADER_BYTES 12
#define SMM_VERTEX_BYTES 16
#define SMM_MAX_VERTICES 65535
#define SMM_MAX_INDICES  (SMM_MAX_VERTICES * 6)
/* No model part is further than this from its origin; a larger number is a
 * damaged file, and would send triangles across the whole screen. */
#define SMM_MAX_COORD    4096.0f

/* Four bytes, least significant first, whatever the host's byte order. */
static unsigned int read_u32(const unsigned char *at)
{
    return (unsigned int)at[0] | ((unsigned int)at[1] << 8) |
           ((unsigned int)at[2] << 16) | ((unsigned int)at[3] << 24);
}

static float read_f32(const unsigned char *at)
{
    unsigned int bits = read_u32(at);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

Model3D *model_parse(const unsigned char *bytes, int size)
{
    if (!bytes || size < SMM_HEADER_BYTES || memcmp(bytes, "SMM1", 4) != 0) return NULL;
    unsigned int vertex_count = read_u32(bytes + 4), index_count = read_u32(bytes + 8);
    if (vertex_count == 0 || vertex_count > SMM_MAX_VERTICES ||
        index_count == 0 || index_count > SMM_MAX_INDICES || index_count % 3 != 0) return NULL;
    /* The counts are small enough here that this sum cannot overflow. */
    unsigned int expected = SMM_HEADER_BYTES + vertex_count * SMM_VERTEX_BYTES + index_count * 2;
    if ((unsigned int)size != expected) return NULL;

    Model3D *model = calloc(1, sizeof(*model));
    if (!model) return NULL;
    model->vertices = malloc((size_t)vertex_count * sizeof(*model->vertices));
    model->indices = malloc((size_t)index_count * sizeof(*model->indices));
    if (!model->vertices || !model->indices) { model_unload(model); return NULL; }
    model->vertex_count = (int)vertex_count;
    model->index_count = (int)index_count;

    const unsigned char *at = bytes + SMM_HEADER_BYTES;
    for (unsigned int i = 0; i < vertex_count; i++, at += SMM_VERTEX_BYTES) {
        ModelVertex *v = &model->vertices[i];
        v->x = read_f32(at); v->y = read_f32(at + 4); v->z = read_f32(at + 8);
        v->r = at[12]; v->g = at[13]; v->b = at[14]; v->a = at[15];
        /* The comparisons are false for NaN, so NaN is rejected too. */
        if (!(fabsf(v->x) <= SMM_MAX_COORD && fabsf(v->y) <= SMM_MAX_COORD &&
              fabsf(v->z) <= SMM_MAX_COORD)) { model_unload(model); return NULL; }
    }
    for (unsigned int i = 0; i < index_count; i++, at += 2) {
        unsigned int index = (unsigned int)at[0] | ((unsigned int)at[1] << 8);
        if (index >= vertex_count) { model_unload(model); return NULL; }
        model->indices[i] = (unsigned short)index;
    }
    return model;
}

Model3D *model_load(const char *path)
{
    if (!path || !FileExists(path)) return NULL;
    int size = 0;
    unsigned char *bytes = LoadFileData(path, &size);
    if (!bytes) return NULL;
    Model3D *model = model_parse(bytes, size);
    UnloadFileData(bytes);
    return model;
}

void model_unload(Model3D *model)
{
    if (!model) return;
    free(model->vertices);
    free(model->indices);
    free(model);
}

void model_draw(const Model3D *model, ModelPose pose)
{
    if (!model || pose.tint.a == 0 || !(pose.scale > 0.0f)) return;
    float yaw = pose.yaw * DEG2RAD, roll = pose.roll * DEG2RAD;
    float cy = cosf(yaw), sy = sinf(yaw), cr = cosf(roll), sr = sinf(roll);
    /* Depth is measured in the render target's pixels, positions in the
     * units sprites use (see DepthView.coords). Outside a view there is no
     * depth: the model is drawn flat, as a silhouette. */
    float z_scale = depth_active() ? pose.scale * depth_coords() : 0.0f;

    rlBegin(RL_TRIANGLES);
    for (int i = 0; i < model->index_count; i++) {
        const ModelVertex *v = &model->vertices[model->indices[i]];
        float x = v->x * pose.stretch_x, y = v->y * pose.stretch_y, z = v->z * pose.stretch_x;
        /* Turn about the vertical axis, then lean about the viewing axis. */
        float tx = x * cy + z * sy, tz = -x * sy + z * cy;
        float rx = tx * cr - y * sr, ry = tx * sr + y * cr;
        rlColor4ub((unsigned char)(v->r * pose.tint.r / 255), (unsigned char)(v->g * pose.tint.g / 255),
                   (unsigned char)(v->b * pose.tint.b / 255), (unsigned char)(v->a * pose.tint.a / 255));
        /* Model y points up; the screen's y points down. */
        rlVertex3f(pose.x + rx * pose.scale, pose.y - ry * pose.scale, (tz + pose.z) * z_scale);
    }
    rlEnd();
}
