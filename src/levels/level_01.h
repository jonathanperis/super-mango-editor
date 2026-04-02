/*
 * level_01.h — Declaration for the Sandbox level definition.
 *
 * Include this header wherever you need to pass &level_01_def to
 * level_load() or level_reset().  All placement data lives in level_01.c.
 */
#pragma once

#include "level.h"   /* LevelDef */

/*
 * level_01_def — The Sandbox level: four screens, all enemy types, every
 * hazard variant.  This is the original hand-crafted level.
 */
extern const LevelDef level_01_def;
