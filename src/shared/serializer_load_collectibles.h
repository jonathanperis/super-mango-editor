/*
 * serializer_load_collectibles.h — TOML collectible parsing helpers.
 */
#pragma once

#include "../../vendor/tomlc17/tomlc17.h" /* toml_datum_t */
#include "../levels/level.h"             /* LevelDef */

int serializer_load_collectibles(toml_datum_t top, LevelDef *def);
