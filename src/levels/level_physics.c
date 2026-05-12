/*
 * level_physics.c — Shared physics override handling for levels.
 */

#include "level_physics.h"

static int has_override(float value)
{
    return value >= 0.0f;
}

void level_apply_player_physics(Player *player, const LevelDef *def)
{
    if (!player) return;

    player_apply_default_physics(player);
    if (!def) return;

#define PHYS_OVERRIDE(field) \
    do { \
        if (has_override(def->physics.field)) player->field = def->physics.field; \
    } while (0)

    PHYS_OVERRIDE(walk_max_speed);
    PHYS_OVERRIDE(run_max_speed);
    PHYS_OVERRIDE(walk_ground_accel);
    PHYS_OVERRIDE(run_ground_accel);
    PHYS_OVERRIDE(ground_friction);
    PHYS_OVERRIDE(ground_counter_accel);
    PHYS_OVERRIDE(air_accel_walk);
    PHYS_OVERRIDE(air_accel_run);
    PHYS_OVERRIDE(air_friction);

#undef PHYS_OVERRIDE
}

float level_camera_lookahead_vx_factor(const LevelDef *def)
{
    if (def && has_override(def->physics.cam_lookahead_vx_factor)) {
        return def->physics.cam_lookahead_vx_factor;
    }
    return CAM_LOOKAHEAD_VX_FACTOR;
}

float level_camera_lookahead_max(const LevelDef *def)
{
    if (def && has_override(def->physics.cam_lookahead_max)) {
        return def->physics.cam_lookahead_max;
    }
    return CAM_LOOKAHEAD_MAX;
}
