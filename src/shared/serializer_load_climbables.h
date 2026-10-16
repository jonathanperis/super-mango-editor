/*
 * serializer_load_climbables.h — TOML climbable/decor parsing helpers.
 */
#pragma once

#include "../../vendor/tomlc17/tomlc17.h" /* toml_datum_t */
#include "../levels/level.h"             /* LevelDef */

int serializer_load_climbables(toml_datum_t top, LevelDef *def);
