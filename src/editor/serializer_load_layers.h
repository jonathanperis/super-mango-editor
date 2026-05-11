/*
 * serializer_load_layers.h — TOML layer parsing helpers.
 */
#pragma once

#include "../../vendor/tomlc17/tomlc17.h" /* toml_datum_t */
#include "../levels/level.h"             /* LevelDef */

int serializer_load_layers(toml_datum_t top, LevelDef *def);
