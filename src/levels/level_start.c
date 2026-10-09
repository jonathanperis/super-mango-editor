/*
 * level_start.c — Resolve a playtest start point (see level_start.h).
 */

#include "level_start.h"

#include <math.h>    /* isfinite */
#include <stdio.h>   /* snprintf */

#include "../game_constants.h"        /* FLOOR_Y, FLOOR_GAP_W, TILE_SIZE, GAME_W */
#include "../surfaces/float_platform.h" /* FLOAT_PLATFORM_PIECE_W */

/* Bridges are drawn and collided as 16-px bricks (level_validate.c checks
 * the same width). */
#define BRIDGE_BRICK_W 16

/* Is x inside [left, left + width)? */
static int spans(float x, float left, float width)
{
    return x >= left && x < left + width;
}

/* Keep the higher of two surfaces: a smaller y is higher on screen. */
static void keep_highest(int *found, float *top, float candidate)
{
    if (!*found || candidate < *top) *top = candidate;
    *found = 1;
}

int level_ground_top_at(const LevelDef *def, float column_x, float *top)
{
    /* The player lands on what is under the middle of its column. */
    float centre = column_x + (float)TILE_SIZE / 2.0f;
    int found = 0;
    int over_gap = 0;
    float best = 0.0f;

    if (!def || !top) return 0;

    for (int i = 0; i < def->floor_gap_count; i++)
        if (spans(centre, (float)def->floor_gaps[i], (float)FLOOR_GAP_W)) over_gap = 1;
    if (!over_gap) keep_highest(&found, &best, (float)FLOOR_Y);

    for (int i = 0; i < def->platform_count; i++) {
        const PlatformPlacement *p = &def->platforms[i];
        int tiles = p->tile_width > 0 ? p->tile_width : 1;
        if (spans(centre, p->x, (float)(tiles * TILE_SIZE)))
            keep_highest(&found, &best, level_platform_top_y(p->tile_height));
    }
    for (int i = 0; i < def->bridge_count; i++) {
        const BridgePlacement *b = &def->bridges[i];
        if (spans(centre, b->x, (float)(b->brick_count * BRIDGE_BRICK_W)))
            keep_highest(&found, &best, b->y);
    }
    for (int i = 0; i < def->float_platform_count; i++) {
        const FloatPlatformPlacement *fp = &def->float_platforms[i];
        if (fp->mode == FLOAT_PLATFORM_RAIL) continue;   /* it moves */
        if (spans(centre, fp->x, (float)(fp->tile_count * FLOAT_PLATFORM_PIECE_W)))
            keep_highest(&found, &best, fp->y);
    }

    if (found) *top = best;
    return found;
}

/* The authored checkpoint furthest right that is not after x, or -1. */
static int checkpoint_behind(const LevelDef *def, float x)
{
    int best = -1;
    for (int i = 0; i < def->checkpoint_count; i++) {
        if (def->checkpoints[i].x > x) continue;
        if (best < 0 || def->checkpoints[i].x > def->checkpoints[best].x) best = i;
    }
    return best;
}

int level_start_resolve(const LevelDef *def, const LevelStart *start,
                        LevelStartPoint *out, char *err, size_t err_size)
{
    int screens;
    float world_w;

    if (err && err_size > 0) err[0] = '\0';
    if (!def || !start || !out) {
        if (err && err_size > 0) snprintf(err, err_size, "no level to start in");
        return -1;
    }
    screens = def->screen_count > 0 ? def->screen_count : 4;
    world_w = (float)screens * (float)GAME_W;

    switch (start->kind) {
    case LEVEL_START_AT_CHECKPOINT: {
        int i = start->checkpoint;
        if (i < 0 || i >= def->checkpoint_count) {
            if (err && err_size > 0)
                snprintf(err, err_size,
                         "checkpoint %d does not exist (the level has %d)",
                         i, def->checkpoint_count);
            return -1;
        }
        /* Exactly where a respawn at that checkpoint puts the player. */
        out->spawn_x = def->checkpoints[i].x;
        out->spawn_y = def->checkpoints[i].y;
        out->checkpoint_index = i;
        return 0;
    }
    case LEVEL_START_AT_X: {
        float column;
        float top;
        if (!isfinite(start->x) || start->x < 0.0f || start->x > world_w) {
            if (err && err_size > 0)
                snprintf(err, err_size, "x %.0f is outside the level (0..%.0f)",
                         start->x, world_w);
            return -1;
        }
        /* Centre the player's column on x, kept inside the world. */
        column = start->x - (float)TILE_SIZE / 2.0f;
        if (column < 0.0f) column = 0.0f;
        if (column > world_w - (float)TILE_SIZE) column = world_w - (float)TILE_SIZE;
        if (!level_ground_top_at(def, column, &top)) {
            if (err && err_size > 0)
                snprintf(err, err_size,
                         "nothing to stand on at x %.0f (a floor gap)", start->x);
            return -1;
        }
        out->spawn_x = column;
        out->spawn_y = top;
        out->checkpoint_index = checkpoint_behind(def, column);
        return 0;
    }
    case LEVEL_START_DEFAULT:
    default:
        level_effective_spawn(def, &out->spawn_x, &out->spawn_y);
        out->checkpoint_index = -1;
        return 0;
    }
}
