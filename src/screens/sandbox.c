/*
 * sandbox.c — Sandbox phase: wires the Level 01 data into the game engine.
 *
 * This file is the entry point for the sandbox level.  All actual placement
 * data lives in levels/level_01.c as a const LevelDef.  The engine glue
 * (populating GameState from that data) lives in level_loader.c.
 *
 * To add a second level, create levels/level_02.h/.c, then call
 * level_load(gs, &level_02_def) from a new phase entry point.
 *
 * Separating content (levels/) from engine (level_loader.c) means new
 * levels require zero changes to engine or entity code.
 */

#include "sandbox.h"
#include "../levels/level_loader.h"  /* level_load, level_reset */
#include "../levels/level_01.h"      /* level_01_def */

/* ------------------------------------------------------------------ */

/*
 * sandbox_load_level — Initialise all entities for the sandbox level.
 *
 * Called once from game_init after all textures have been loaded.
 * Delegates entirely to level_load with the Level 01 definition.
 */
void sandbox_load_level(GameState *gs) {
    level_load(gs, &level_01_def);
}

/* ------------------------------------------------------------------ */

/*
 * sandbox_reset_level — Reinitialise all mutable entities after player death.
 *
 * Delegates to level_reset (which calls player_reset internally).
 * Also clears fp_prev_riding so the float-platform ride index does not
 * carry stale state from the previous life into the next.
 */
void sandbox_reset_level(GameState *gs, int *fp_prev_riding) {
    level_reset(gs, &level_01_def);
    *fp_prev_riding = -1;
}
