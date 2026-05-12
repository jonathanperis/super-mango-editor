/*
 * level_physics.h — Apply LevelDef physics overrides safely.
 */
#pragma once

#include "level.h"
#include "../player/player.h"

/* Reset player physics to engine defaults, then apply non-negative overrides. */
void level_apply_player_physics(Player *player, const LevelDef *def);

/* Camera physics accessors: negative values mean "use engine default". */
float level_camera_lookahead_vx_factor(const LevelDef *def);
float level_camera_lookahead_max(const LevelDef *def);
