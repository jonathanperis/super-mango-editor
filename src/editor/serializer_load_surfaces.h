/*
 * serializer_load_surfaces.h — TOML surface parsing helpers.
 */
#pragma once

#include "../../vendor/tomlc17/tomlc17.h" /* toml_datum_t */
#include "../levels/level.h"             /* LevelDef */

int serializer_load_surfaces(toml_datum_t top, LevelDef *def);
