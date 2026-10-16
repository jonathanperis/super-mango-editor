/*
 * serializer_load_checkpoints.h — Authored checkpoint TOML parsing.
 */
#pragma once

#include "../../vendor/tomlc17/tomlc17.h"
#include "../levels/level.h"

int serializer_load_checkpoints(toml_datum_t top, LevelDef *def);
