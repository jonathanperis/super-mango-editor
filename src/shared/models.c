/*
 * models.c — The catalogue of 3D models and the things built from them
 * (see models.h).
 */
#include "models.h"
#include "model.h"
#include "depth.h"

#include <math.h>
#include <rlgl.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* The catalogue                                                       */
/* ------------------------------------------------------------------ */

/* One row per file in assets/models/. X-macro: the list is written once
 * and expanded into both the enum and the table of file names. */
#define MODEL_LIST(X) \
    X(MANGO, "mango") X(MANGO_FOOT, "mango_foot") X(SPIDER, "spider") X(COIN, "coin") \
    X(FLOOR_LEFT, "grass_floor_left") X(FLOOR_MID, "grass_floor_mid") X(FLOOR_RIGHT, "grass_floor_right") \
    X(PILLAR_TOP_LEFT, "grass_pillar_top_left") X(PILLAR_TOP_MID, "grass_pillar_top_mid") \
    X(PILLAR_TOP_RIGHT, "grass_pillar_top_right") X(PILLAR_BODY_LEFT, "grass_pillar_body_left") \
    X(PILLAR_BODY_MID, "grass_pillar_body_mid") X(PILLAR_BODY_RIGHT, "grass_pillar_body_right") \
    X(PLATFORM_LEFT, "float_platform_left") X(PLATFORM_MID, "float_platform_mid") \
    X(PLATFORM_RIGHT, "float_platform_right")

typedef enum {
#define X(id, name) MODEL_##id,
    MODEL_LIST(X)
#undef X
    MODEL_COUNT
} ModelId;

static const char *const s_model_names[MODEL_COUNT] = {
#define X(id, name) name,
    MODEL_LIST(X)
#undef X
};

/* Loaded the first time each is asked for. `tried` stops a missing file
 * from being looked for again every frame. */
static struct { Model3D *model; int tried; } s_models[MODEL_COUNT];

static const Model3D *get(ModelId id)
{
    if (!s_models[id].tried) {
        s_models[id].tried = 1;
        s_models[id].model = model_load(TextFormat("assets/models/%s.smm", s_model_names[id]));
    }
    return s_models[id].model;
}

void models_unload_all(void)
{
    for (int i = 0; i < MODEL_COUNT; i++) {
        model_unload(s_models[i].model);
        s_models[i].model = NULL;
        s_models[i].tried = 0;
    }
}

static int s_grass_terrain;   /* the level uses the grass tileset */
static int s_lava;            /* its liquid is lava, not water    */

void models_set_level(const char *floor_tile_path, const char *liquid_path)
{
    s_grass_terrain = floor_tile_path && strstr(floor_tile_path, "grass_tileset") != NULL;
    s_lava = liquid_path && strstr(liquid_path, "lava") != NULL;
}

/* ------------------------------------------------------------------ */
/* Terrain: pieces one tile wide, laid side by side                    */
/* ------------------------------------------------------------------ */

#define TILE 16.0f   /* every terrain piece is this wide (and tall) */

/* Which of a row's three pieces sits at tile `i` of `count`. */
static const Model3D *row_piece(int i, int count, int left_end, int right_end,
                                const Model3D *left, const Model3D *mid, const Model3D *right)
{
    if (i == 0 && left_end) return left;
    if (i == count - 1 && right_end) return right;
    return mid;
}

int models_draw_floor(float x0, float x1, float top_y, int left_end, int right_end,
                      float clip_x0, float clip_x1, float scale)
{
    if (!s_grass_terrain || !depth_active()) return 0;
    const Model3D *left = get(MODEL_FLOOR_LEFT), *mid = get(MODEL_FLOOR_MID),
                  *right = get(MODEL_FLOOR_RIGHT);
    if (!left || !mid || !right) return 0;
    float step = TILE * scale;
    int count = (int)((x1 - x0) / step + 0.5f);
    for (int i = 0; i < count; i++) {
        float x = x0 + (float)i * step;
        if (x + step < clip_x0 || x > clip_x1) continue;
        model_draw(row_piece(i, count, left_end, right_end, left, mid, right),
                   MODEL_POSE(x, top_y, scale));
    }
    return 1;
}

int models_draw_pillar(float x, float top_y, float w, float h, float scale)
{
    if (!s_grass_terrain || !depth_active()) return 0;
    const Model3D *top[3] = {get(MODEL_PILLAR_TOP_LEFT), get(MODEL_PILLAR_TOP_MID),
                             get(MODEL_PILLAR_TOP_RIGHT)};
    const Model3D *body[3] = {get(MODEL_PILLAR_BODY_LEFT), get(MODEL_PILLAR_BODY_MID),
                              get(MODEL_PILLAR_BODY_RIGHT)};
    for (int i = 0; i < 3; i++) if (!top[i] || !body[i]) return 0;
    int columns = (int)(w / TILE + 0.5f), rows = (int)(h / TILE + 0.5f);
    if (columns < 1) columns = 1;
    float step = TILE * scale;
    for (int c = 0; c < columns; c++) {
        float px = x + (float)c * step;
        model_draw(row_piece(c, columns, 1, 1, top[0], top[1], top[2]), MODEL_POSE(px, top_y, scale));
        /* The top piece is one tile tall; trunk tiles carry on below it. */
        for (int r = 1; r < rows; r++)
            model_draw(row_piece(c, columns, 1, 1, body[0], body[1], body[2]),
                       MODEL_POSE(px, top_y + (float)r * step, scale));
    }
    return 1;
}

int models_draw_float_platform(float x, float top_y, float w, float scale)
{
    if (!depth_active()) return 0;
    const Model3D *left = get(MODEL_PLATFORM_LEFT), *mid = get(MODEL_PLATFORM_MID),
                  *right = get(MODEL_PLATFORM_RIGHT);
    if (!left || !mid || !right) return 0;
    int count = (int)(w / TILE + 0.5f);
    if (count < 1) count = 1;
    for (int i = 0; i < count; i++)
        model_draw(row_piece(i, count, 1, 1, left, mid, right),
                   MODEL_POSE(x + (float)i * TILE * scale, top_y, scale));
    return 1;
}

/* ------------------------------------------------------------------ */
/* Pickups and creatures                                               */
/* ------------------------------------------------------------------ */

/* How far a creature turns from facing the viewer toward where it walks.
 * Less than 90 so its face stays in view. */
#define FACING_YAW 42.0f

int models_draw_coin(float cx, float cy, float spin, float scale)
{
    const Model3D *coin = depth_active() ? get(MODEL_COIN) : NULL;
    if (!coin) return 0;
    ModelPose pose = MODEL_POSE(cx, cy, scale * 0.8f);
    pose.yaw = spin;
    model_draw(coin, pose);
    return 1;
}

int models_draw_spider(float cx, float feet_y, int facing_left, int step, float scale)
{
    const Model3D *spider = depth_active() ? get(MODEL_SPIDER) : NULL;
    if (!spider) return 0;
    ModelPose pose = MODEL_POSE(cx, feet_y, scale);
    pose.yaw = facing_left ? -FACING_YAW : FACING_YAW;
    /* A scuttle: the body rocks from side to side with each frame. */
    pose.roll = (step % 2) ? 5.0f : -5.0f;
    model_draw(spider, pose);
    return 1;
}

/* The sprite the model replaces is about 18 pixels wide; the model was
 * built larger so its face has room for detail. */
#define MANGO_SCALE   0.72f
#define MANGO_FOOT_DX 4.8f    /* each foot's distance from the middle, model pixels */

int models_draw_mango(float cx, float feet_y, int facing_left, float stride, float rise,
                      Color tint, float scale)
{
    if (!depth_active()) return 0;
    const Model3D *body = get(MODEL_MANGO), *foot = get(MODEL_MANGO_FOOT);
    if (!body || !foot) return 0;
    float s = scale * MANGO_SCALE;
    float yaw = facing_left ? -FACING_YAW : FACING_YAW;
    float turn = yaw * DEG2RAD;

    /* Rising or falling fast, the body stretches upward and thins. */
    float stretch = fabsf(rise) / 2400.0f;
    if (stretch > 0.14f) stretch = 0.14f;
    /* Walking, the body bobs twice per stride (once per footfall). */
    float bob = stride >= 0.0f ? fabsf(sinf(stride * 2.0f * PI)) * 1.1f : 0.0f;

    ModelPose pose = MODEL_POSE(cx, feet_y - bob * s, s);
    pose.yaw = yaw;
    pose.stretch_y = 1.0f + stretch;
    pose.stretch_x = 1.0f - stretch * 0.5f;
    pose.tint = tint;
    model_draw(body, pose);

    /* The feet swing forward and back in turn, lifting on the way forward.
     * In the air they hang together, tucked up a little. */
    for (int side = -1; side <= 1; side += 2) {
        float swing = 0.0f, lift = 0.0f;
        if (rise != 0.0f && stride < 0.0f) {
            lift = 1.6f;
        } else if (stride >= 0.0f) {
            float phase = stride * 2.0f * PI + (side > 0 ? PI : 0.0f);
            swing = sinf(phase) * 3.4f;
            lift = cosf(phase) > 0.0f ? cosf(phase) * 2.4f : 0.0f;
        }
        /* The foot's place in the body's own space (x across, z forward),
         * turned with the body. */
        float fx = (float)side * MANGO_FOOT_DX, fz = swing;
        ModelPose foot_pose = MODEL_POSE(cx + (fx * cosf(turn) + fz * sinf(turn)) * s,
                                         feet_y - lift * s, s);
        foot_pose.z = (-fx * sinf(turn) + fz * cosf(turn)) * MANGO_SCALE;
        foot_pose.yaw = yaw;
        foot_pose.tint = tint;
        model_draw(foot, foot_pose);
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Shapes made here rather than in Blender                             */
/* ------------------------------------------------------------------ */

/* Colour only, no depth: a shadow or a pane of water must not hide what is
 * drawn behind it later. */
static void depth_writes(int on)
{
    rlDrawRenderBatchActive();
    if (on) rlEnableDepthMask();
    else rlDisableDepthMask();
}

void models_draw_shadow(float cx, float y, float radius, float strength, float scale)
{
    if (!depth_active() || !(strength > 0.0f) || !(radius > 0.0f)) return;
    if (strength > 1.0f) strength = 1.0f;
    enum { SEGMENTS = 20 };
    float z_scale = scale * depth_coords();
    float lift = 0.9f * scale;   /* just above the surface, clear of it */
    unsigned char dark = (unsigned char)(110.0f * strength);

    depth_writes(0);
    rlBegin(RL_TRIANGLES);
    for (int i = 0; i < SEGMENTS; i++) {
        float a0 = (float)i * 2.0f * PI / SEGMENTS, a1 = (float)(i + 1) * 2.0f * PI / SEGMENTS;
        /* A fan lying flat: x across, z into the screen, dark in the
         * middle and clear at the rim. */
        rlColor4ub(0, 0, 0, dark);
        rlVertex3f(cx, y - lift, 0.0f);
        rlColor4ub(0, 0, 0, 0);
        rlVertex3f(cx + cosf(a0) * radius * scale, y - lift, sinf(a0) * radius * 0.8f * z_scale);
        rlVertex3f(cx + cosf(a1) * radius * scale, y - lift, sinf(a1) * radius * 0.8f * z_scale);
    }
    rlEnd();
    depth_writes(1);
}

/* How far the water reaches toward the viewer and into the screen, in
 * pixels: a little past the floor's own front and back (tools/models). */
#define WATER_FRONT  17.0f
#define WATER_BACK  (-47.0f)
#define WATER_STEP   8.0f    /* width of one strip of the surface */
#define WATER_ROWS   4       /* strips from front to back         */

/* The surface's height offset at (x, row), in pixels. */
static float ripple(float x, int row, float seconds)
{
    return sinf(x * 0.11f + seconds * 2.2f + (float)row * 0.9f) * 1.3f +
           sinf(x * 0.047f - seconds * 1.3f) * 0.9f;
}

int models_draw_water(float x0, float x1, float surface_y, float bottom_y,
                      float seconds, float scale)
{
    if (!depth_active()) return 0;
    int lava = s_lava;
    /* Bright where the surface catches the light, deeper down the front. */
    const Color crest = lava ? (Color){255, 214, 96, 255}  : (Color){150, 222, 255, 245};
    const Color top   = lava ? (Color){255, 132, 40, 255}  : (Color){30, 136, 232, 245};
    const Color deep  = lava ? (Color){196, 36, 30, 255}   : (Color){18, 84, 200, 250};
    float z_scale = scale * depth_coords();
    float step = WATER_STEP * scale;

    rlBegin(RL_TRIANGLES);
    for (float x = x0; x < x1; x += step) {
        float xa = x, xb = x + step;
        /* World-like x for the ripple, so it does not change with scale. */
        float wa = xa / scale, wb = xb / scale;
        for (int row = 0; row < WATER_ROWS; row++) {
            float za = WATER_FRONT + (WATER_BACK - WATER_FRONT) * (float)row / WATER_ROWS;
            float zb = WATER_FRONT + (WATER_BACK - WATER_FRONT) * (float)(row + 1) / WATER_ROWS;
            float y00 = surface_y + ripple(wa, row, seconds) * scale;
            float y10 = surface_y + ripple(wb, row, seconds) * scale;
            float y01 = surface_y + ripple(wa, row + 1, seconds) * scale;
            float y11 = surface_y + ripple(wb, row + 1, seconds) * scale;
            /* Lighter at the crests: blend by how high this corner is. */
            #define SURFACE(yy) do { \
                float t = (surface_y - (yy)) / (2.2f * scale) - 0.25f; \
                if (t < 0.0f) t = 0.0f; \
                if (t > 1.0f) t = 1.0f; \
                rlColor4ub((unsigned char)(top.r + (crest.r - top.r) * t), \
                           (unsigned char)(top.g + (crest.g - top.g) * t), \
                           (unsigned char)(top.b + (crest.b - top.b) * t), top.a); \
            } while (0)
            SURFACE(y00); rlVertex3f(xa, y00, za * z_scale);
            SURFACE(y01); rlVertex3f(xa, y01, zb * z_scale);
            SURFACE(y11); rlVertex3f(xb, y11, zb * z_scale);
            SURFACE(y00); rlVertex3f(xa, y00, za * z_scale);
            SURFACE(y11); rlVertex3f(xb, y11, zb * z_scale);
            SURFACE(y10); rlVertex3f(xb, y10, za * z_scale);
            #undef SURFACE
        }
        /* The front face, from the surface's front edge down. */
        float ya = surface_y + ripple(wa, 0, seconds) * scale;
        float yb = surface_y + ripple(wb, 0, seconds) * scale;
        float zf = WATER_FRONT * z_scale;
        rlColor4ub(top.r, top.g, top.b, top.a);    rlVertex3f(xa, ya, zf);
        rlColor4ub(deep.r, deep.g, deep.b, deep.a); rlVertex3f(xa, bottom_y, zf);
        rlVertex3f(xb, bottom_y, zf);
        rlColor4ub(top.r, top.g, top.b, top.a);    rlVertex3f(xa, ya, zf);
        rlColor4ub(deep.r, deep.g, deep.b, deep.a); rlVertex3f(xb, bottom_y, zf);
        rlColor4ub(top.r, top.g, top.b, top.a);    rlVertex3f(xb, yb, zf);
    }
    rlEnd();
    return 1;
}
