/*
 * serializer_load_hazards.h — TOML hazard parsing helpers.
 */
#pragma once

#include "../../vendor/tomlc17/tomlc17.h" /* toml_datum_t */
#include "../levels/level.h"             /* LevelDef */

int serializer_load_hazards(toml_datum_t top, LevelDef *def);
