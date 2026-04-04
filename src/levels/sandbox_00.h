/*
 * sandbox_00.h — Declaration for the Sandbox level definition.
 *
 * Include this header wherever you need to pass &sandbox_00_def to
 * level_load() or level_reset().  All placement data lives in sandbox_00.c.
 */
#pragma once

#include "level.h"   /* LevelDef */

/*
 * sandbox_00_def — The Sandbox level: four screens, all enemy types, every
 * hazard variant.  This is the original hand-crafted test level.
 */
extern const LevelDef sandbox_00_def;
