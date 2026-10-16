/*
 * serializer_load_header.h — TOML header and floor-gap parsing.
 */
#pragma once

#include "../../vendor/tomlc17/tomlc17.h" /* toml_datum_t */
#include "../levels/level.h"             /* LevelDef */

int serializer_load_header(toml_datum_t top, LevelDef *def);
