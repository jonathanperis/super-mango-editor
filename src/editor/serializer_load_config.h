/*
 * serializer_load_config.h — TOML level-wide configuration parsing.
 */
#pragma once

#include "../../vendor/tomlc17/tomlc17.h" /* toml_datum_t */
#include "../levels/level.h"             /* LevelDef */

void serializer_load_config(toml_datum_t top, LevelDef *def);
