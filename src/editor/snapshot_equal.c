/*
 * snapshot_equal.c — Compare undo snapshots by their fields.
 *
 * memcmp on PlacementData also compares the union bytes past the active
 * member, and memcmp on either snapshot compares padding and treats two
 * equal floats as different when their bits differ. Callers use the result
 * to decide whether an edit is worth an undo step, so only the fields the
 * editor wrote should count. Floats are compared by their bits: the panel
 * rejects NaN and infinity, and a typed -0 is a real change from 0.
 */

#include "snapshot_equal.h"

#include <stdint.h>
#include <string.h>

static int f32_same(float a, float b)
{
    uint32_t left, right;
    memcpy(&left, &a, sizeof(left));
    memcpy(&right, &b, sizeof(right));
    return left == right;
}

static int text_same(const char *a, const char *b, size_t n)
{
    return memcmp(a, b, n) == 0;
}

static int layer_same(const LevelLayer *a, const LevelLayer *b)
{
    return text_same(a->path, b->path, sizeof(a->path)) && f32_same(a->speed, b->speed);
}

static int layers_same(const LevelLayer *a, const LevelLayer *b, int count)
{
    for (int i = 0; i < count; i++)
        if (!layer_same(&a[i], &b[i])) return 0;
    return 1;
}

static int physics_same(const LevelPhysics *a, const LevelPhysics *b)
{
    /* A new field has to be compared here; the size check fails the build
     * until it is, instead of letting that field skip undo. */
    _Static_assert(sizeof(LevelPhysics) == 11 * sizeof(float),
                   "compare every LevelPhysics field");
    return f32_same(a->walk_max_speed, b->walk_max_speed) &&
           f32_same(a->run_max_speed, b->run_max_speed) &&
           f32_same(a->walk_ground_accel, b->walk_ground_accel) &&
           f32_same(a->run_ground_accel, b->run_ground_accel) &&
           f32_same(a->ground_friction, b->ground_friction) &&
           f32_same(a->ground_counter_accel, b->ground_counter_accel) &&
           f32_same(a->air_accel_walk, b->air_accel_walk) &&
           f32_same(a->air_accel_run, b->air_accel_run) &&
           f32_same(a->air_friction, b->air_friction) &&
           f32_same(a->cam_lookahead_vx_factor, b->cam_lookahead_vx_factor) &&
           f32_same(a->cam_lookahead_max, b->cam_lookahead_max);
}

static int xy_same(float ax, float ay, float bx, float by)
{
    return f32_same(ax, bx) && f32_same(ay, by);
}

/*
 * placement_same — Compare the fields of one placed entity.
 *
 * Returns 1 when the active member for `type` matches, including float
 * bits. A null pair matches; one null side does not. ENT_COUNT and any
 * other type this switch does not name do not match.
 */
int placement_same(EntityType type, const PlacementData *a, const PlacementData *b)
{
    if (!a || !b) return a == b;

    switch (type) {
    case ENT_FLOOR_GAP:
        return a->floor_gap == b->floor_gap;
    case ENT_CHECKPOINT:
        return xy_same(a->checkpoint.x, a->checkpoint.y, b->checkpoint.x, b->checkpoint.y);
    case ENT_RAIL:
        return a->rail.layout == b->rail.layout && a->rail.x == b->rail.x &&
               a->rail.y == b->rail.y && a->rail.w == b->rail.w &&
               a->rail.h == b->rail.h && a->rail.end_cap == b->rail.end_cap;
    case ENT_PLATFORM:
        return f32_same(a->platform.x, b->platform.x) &&
               a->platform.tile_height == b->platform.tile_height &&
               a->platform.tile_width == b->platform.tile_width &&
               text_same(a->platform.tile_path, b->platform.tile_path,
                         sizeof(a->platform.tile_path));
    case ENT_COIN:
        return xy_same(a->coin.x, a->coin.y, b->coin.x, b->coin.y);
    case ENT_STAR_YELLOW:
        return xy_same(a->star_yellow.x, a->star_yellow.y, b->star_yellow.x, b->star_yellow.y);
    case ENT_STAR_GREEN:
        return xy_same(a->star_green.x, a->star_green.y, b->star_green.x, b->star_green.y);
    case ENT_STAR_RED:
        return xy_same(a->star_red.x, a->star_red.y, b->star_red.x, b->star_red.y);
    case ENT_LAST_STAR:
    case ENT_PLAYER_SPAWN:
        /* Player spawn reuses the {x, y} last_star member; see
         * editor_snapshot_entity. */
        return xy_same(a->last_star.x, a->last_star.y, b->last_star.x, b->last_star.y);
    case ENT_SPIDER:
        return f32_same(a->spider.x, b->spider.x) && f32_same(a->spider.vx, b->spider.vx) &&
               f32_same(a->spider.patrol_x0, b->spider.patrol_x0) &&
               f32_same(a->spider.patrol_x1, b->spider.patrol_x1) &&
               a->spider.frame_index == b->spider.frame_index;
    case ENT_JUMPING_SPIDER:
        return f32_same(a->jumping_spider.x, b->jumping_spider.x) &&
               f32_same(a->jumping_spider.vx, b->jumping_spider.vx) &&
               f32_same(a->jumping_spider.patrol_x0, b->jumping_spider.patrol_x0) &&
               f32_same(a->jumping_spider.patrol_x1, b->jumping_spider.patrol_x1);
    case ENT_BIRD:
    case ENT_FASTER_BIRD:
        return f32_same(a->bird.x, b->bird.x) && f32_same(a->bird.base_y, b->bird.base_y) &&
               f32_same(a->bird.vx, b->bird.vx) &&
               f32_same(a->bird.patrol_x0, b->bird.patrol_x0) &&
               f32_same(a->bird.patrol_x1, b->bird.patrol_x1) &&
               a->bird.frame_index == b->bird.frame_index;
    case ENT_FISH:
    case ENT_FASTER_FISH:
        return f32_same(a->fish.x, b->fish.x) && f32_same(a->fish.vx, b->fish.vx) &&
               f32_same(a->fish.patrol_x0, b->fish.patrol_x0) &&
               f32_same(a->fish.patrol_x1, b->fish.patrol_x1);
    case ENT_AXE_TRAP:
        return f32_same(a->axe_trap.pillar_x, b->axe_trap.pillar_x) &&
               f32_same(a->axe_trap.y, b->axe_trap.y) &&
               a->axe_trap.mode == b->axe_trap.mode;
    case ENT_CIRCULAR_SAW:
        return f32_same(a->circular_saw.x, b->circular_saw.x) &&
               f32_same(a->circular_saw.y, b->circular_saw.y) &&
               f32_same(a->circular_saw.patrol_x0, b->circular_saw.patrol_x0) &&
               f32_same(a->circular_saw.patrol_x1, b->circular_saw.patrol_x1) &&
               a->circular_saw.direction == b->circular_saw.direction;
    case ENT_SPIKE_ROW:
        return f32_same(a->spike_row.x, b->spike_row.x) &&
               a->spike_row.count == b->spike_row.count;
    case ENT_SPIKE_PLATFORM:
        return f32_same(a->spike_platform.x, b->spike_platform.x) &&
               f32_same(a->spike_platform.y, b->spike_platform.y) &&
               a->spike_platform.tile_count == b->spike_platform.tile_count;
    case ENT_SPIKE_BLOCK:
        return a->spike_block.rail_index == b->spike_block.rail_index &&
               f32_same(a->spike_block.t_offset, b->spike_block.t_offset) &&
               f32_same(a->spike_block.speed, b->spike_block.speed);
    case ENT_BLUE_FLAME:
        return f32_same(a->blue_flame.x, b->blue_flame.x);
    case ENT_FIRE_FLAME:
        return f32_same(a->fire_flame.x, b->fire_flame.x);
    case ENT_FLOAT_PLATFORM:
        return a->float_platform.mode == b->float_platform.mode &&
               f32_same(a->float_platform.x, b->float_platform.x) &&
               f32_same(a->float_platform.y, b->float_platform.y) &&
               a->float_platform.tile_count == b->float_platform.tile_count &&
               a->float_platform.rail_index == b->float_platform.rail_index &&
               f32_same(a->float_platform.t_offset, b->float_platform.t_offset) &&
               f32_same(a->float_platform.speed, b->float_platform.speed);
    case ENT_BRIDGE:
        return f32_same(a->bridge.x, b->bridge.x) && f32_same(a->bridge.y, b->bridge.y) &&
               a->bridge.brick_count == b->bridge.brick_count;
    case ENT_BOUNCEPAD_SMALL:
    case ENT_BOUNCEPAD_MEDIUM:
    case ENT_BOUNCEPAD_HIGH:
        return f32_same(a->bouncepad.x, b->bouncepad.x) &&
               f32_same(a->bouncepad.launch_vy, b->bouncepad.launch_vy) &&
               a->bouncepad.pad_type == b->bouncepad.pad_type;
    case ENT_VINE:
        return f32_same(a->vine.x, b->vine.x) && f32_same(a->vine.y, b->vine.y) &&
               a->vine.tile_count == b->vine.tile_count &&
               a->vine.vine_type == b->vine.vine_type;
    case ENT_LADDER:
        return f32_same(a->ladder.x, b->ladder.x) && f32_same(a->ladder.y, b->ladder.y) &&
               a->ladder.tile_count == b->ladder.tile_count;
    case ENT_ROPE:
        return f32_same(a->rope.x, b->rope.x) && f32_same(a->rope.y, b->rope.y) &&
               a->rope.tile_count == b->rope.tile_count;
    case ENT_COUNT:
        break;
    }
    return 0;
}

int config_snapshot_same(const LevelConfigSnapshot *a, const LevelConfigSnapshot *b)
{
    if (!a || !b) return a == b;
    _Static_assert(sizeof(LevelLayer) == 64 + sizeof(float),
                   "LevelLayer gained padding; compare the new bytes");
    return text_same(a->name, b->name, sizeof(a->name)) &&
           text_same(a->description, b->description, sizeof(a->description)) &&
           text_same(a->generated_by, b->generated_by, sizeof(a->generated_by)) &&
           a->screen_count == b->screen_count &&
           text_same(a->next_phase, b->next_phase, sizeof(a->next_phase)) &&
           layers_same(a->background_layers, b->background_layers, MAX_BACKGROUND_LAYERS) &&
           a->background_layer_count == b->background_layer_count &&
           layers_same(a->foreground_layers, b->foreground_layers, MAX_BACKGROUND_LAYERS) &&
           a->foreground_layer_count == b->foreground_layer_count &&
           layers_same(a->fog_layers, b->fog_layers, MAX_FOG_TEXTURES) &&
           a->fog_layer_count == b->fog_layer_count &&
           text_same(a->music_path, b->music_path, sizeof(a->music_path)) &&
           a->music_volume == b->music_volume &&
           text_same(a->floor_tile_path, b->floor_tile_path, sizeof(a->floor_tile_path)) &&
           a->initial_hearts == b->initial_hearts &&
           a->initial_lives == b->initial_lives &&
           a->score_per_life == b->score_per_life &&
           a->coin_score == b->coin_score &&
           physics_same(&a->physics, &b->physics);
}
