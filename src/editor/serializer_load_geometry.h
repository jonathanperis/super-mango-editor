/*
 * serializer_load_geometry.h — TOML rail and platform parsing helpers.
 */
#pragma once

#include "../../vendor/tomlc17/tomlc17.h" /* toml_datum_t */
#include "../levels/level.h"             /* LevelDef */

int serializer_load_geometry(toml_datum_t top, LevelDef *def);
