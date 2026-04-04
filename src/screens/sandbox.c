/*
 * sandbox.c — Sandbox phase: loads a level from JSON and wires it into the engine.
 *
 * Instead of referencing a compiled const LevelDef, this module reads
 * levels/sandbox_00.json at runtime using the editor's JSON serializer.
 * This means the game always plays whatever the editor last saved —
 * no recompilation needed after editing a level.
 *
 * The LevelDef is stored in a file-scoped static so level_reset can
 * reference it on player death without reloading from disk.
 */

#include <stdio.h>      /* fprintf, stderr */
#include <string.h>     /* memset */

#include "sandbox.h"
#include "../levels/level_loader.h"  /* level_load, level_reset */
#include "../levels/level.h"        /* LevelDef struct          */
#include "../editor/serializer.h"   /* level_load_json          */

/* ------------------------------------------------------------------ */

/*
 * File-scoped level definition — populated once from JSON in
 * sandbox_load_level, then referenced again in sandbox_reset_level.
 *
 * Using static storage avoids heap allocation and keeps the data alive
 * for the entire duration of the game session.
 */
static LevelDef s_level;

/*
 * Default JSON path — the editor saves sandbox levels here, and the
 * Play button exports to this same path before launching the game.
 */
#define SANDBOX_JSON_PATH  "levels/sandbox_00.json"

/* ------------------------------------------------------------------ */

/*
 * sandbox_load_level — Load the sandbox level from JSON and initialise.
 *
 * Called once from game_init after all textures have been loaded.
 * Reads the JSON file, deserializes it into s_level, then delegates
 * to level_load which populates all GameState entity arrays.
 *
 * If the JSON file is missing or corrupt, the game starts with an
 * empty level (all counts zero) — the designer can then open the
 * editor and create content.
 */
void sandbox_load_level(GameState *gs) {
    memset(&s_level, 0, sizeof(s_level));

    if (level_load_json(SANDBOX_JSON_PATH, &s_level) != 0) {
        fprintf(stderr, "Warning: could not load %s — starting empty level\n",
                SANDBOX_JSON_PATH);
        s_level.name = "Untitled";
    }

    level_load(gs, &s_level);
}

/* ------------------------------------------------------------------ */

/*
 * sandbox_reset_level — Reinitialise all mutable entities after player death.
 *
 * Delegates to level_reset using the same LevelDef that was loaded from
 * JSON at startup.  Also clears fp_prev_riding so the float-platform
 * ride index does not carry stale state from the previous life.
 */
void sandbox_reset_level(GameState *gs, int *fp_prev_riding) {
    level_reset(gs, &s_level);
    *fp_prev_riding = -1;
}
