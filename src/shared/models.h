/*
 * models.h — The game's 3D models, and how each kind of thing is built
 * from them.
 *
 * Every function here draws one thing of the level out of the model files
 * in assets/models/ (made by tools/models/build.py), inside an open depth
 * view (shared/depth.h). Positions are in the units sprites are drawn with:
 * logical pixels in the game (scale 1), screen pixels in the editor (scale
 * = its zoom). The game and the editor call the same functions, so the
 * editor shows what the game will.
 *
 * Each returns 1 when it drew, and 0 when a model file it needs is missing;
 * the caller then draws the old sprite instead, so a level never goes
 * blank over a missing file.
 */
#pragma once

#include <raylib.h>

/* Free every model loaded so far. Call before the window closes. */
void models_unload_all(void);

/* Tell the models which level is being drawn, by two of its file paths:
 * the floor tileset and the water/lava strip (either may be NULL). Ground
 * and pillars have a 3D look only for the grass tileset so far; in other
 * levels models_draw_floor and models_draw_pillar return 0 and the sprites
 * are drawn. The liquid's path decides between water and lava colours. */
void models_set_level(const char *floor_tile_path, const char *liquid_path);

/* A stretch of ground from x0 to x1 with its grass top at top_y. left_end
 * and right_end round off that end (a floor gap or the level's edge);
 * otherwise the ground just carries on past it. Only tiles that touch
 * [clip_x0, clip_x1] are drawn. */
int models_draw_floor(float x0, float x1, float top_y, int left_end, int right_end,
                      float clip_x0, float clip_x1, float scale);

/* A pillar: a grass cap to stand on at (x, top_y), w wide, over a trunk
 * that runs h down. */
int models_draw_pillar(float x, float top_y, float w, float h, float scale);

/* A hovering platform, w wide, its top at top_y. */
int models_draw_float_platform(float x, float top_y, float w, float scale);

/* A coin centred on (cx, cy), turned spin degrees about its vertical axis. */
int models_draw_coin(float cx, float cy, float spin, float scale);

/* A spider standing at (cx, feet_y). step is its animation frame. */
int models_draw_spider(float cx, float feet_y, int facing_left, int step, float scale);

/* The player standing at (cx, feet_y).
 *   stride   0..1 through one walking step, or a negative number to stand
 *   rise     vertical speed in pixels per second, negative going up; the
 *            body stretches a little with it
 *   tint     WHITE normally; its alpha fades the whole character */
int models_draw_mango(float cx, float feet_y, int facing_left, float stride, float rise,
                      Color tint, float scale);

/* A soft round shadow lying on a surface at height y, under (cx). strength
 * is 0..1. Needs no model file. */
void models_draw_shadow(float cx, float y, float radius, float strength, float scale);

/* Water (or lava) from x0 to x1: a rippling surface at surface_y that runs
 * back into the screen, and a front face down to bottom_y. seconds moves
 * the ripples. Needs no model file; returns 0 outside a depth view. */
int models_draw_water(float x0, float x1, float surface_y, float bottom_y,
                      float seconds, float scale);
