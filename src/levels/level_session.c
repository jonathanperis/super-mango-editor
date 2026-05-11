/*
 * level_session.c — Active level load and phase transition helpers.
 */

#include "level_session.h"

#include <stdio.h>
#include <string.h>

#include "level.h"
#include "level_loader.h"
#include "level_path.h"
#include "level_resources.h"
#include "phase_transition.h"
#include "../core/game_completion.h"
#include "../editor/serializer.h"

/* Cached LevelDef backing the active runtime level pointer. */
static LevelDef s_level;

void game_level_load_initial(GameState *gs)
{
    memset(&s_level, 0, sizeof(s_level));

    char safe_path[512] = {0};
    int path_valid = (level_resolve_path(gs->level_path, safe_path, sizeof(safe_path)) == 0);

    level_def_init_defaults(&s_level);

    if (path_valid &&
        level_load_toml(safe_path, &s_level) == 0) {
        /* Successfully loaded from the resolved path. */
    } else {
        if (gs->level_path[0] != '\0') {
            fprintf(stderr, "Warning: could not load %s — starting empty level\n",
                    gs->level_path);
        }
        strncpy(s_level.name, "Untitled", sizeof(s_level.name) - 1);
    }

    level_load(gs, &s_level);
    game_completion_reset_summary(gs);
    level_resources_apply(gs, (const LevelDef *)gs->runtime.current_level);
}

int game_load_next_phase(GameState *gs)
{
    const LevelDef *current = (const LevelDef *)gs->runtime.current_level;
    char next_path[256] = {0};
    if (phase_next_path(current, next_path, sizeof(next_path)) != 0) return -1;

    PhaseProgress saved_progress;
    phase_progress_save(gs, &saved_progress);

    char safe_path[512] = {0};
    if (level_resolve_path(next_path, safe_path, sizeof(safe_path)) != 0) {
        fprintf(stderr, "Error: Failed to resolve next phase path: %s\n", next_path);
        return -1;
    }

    level_def_init_defaults(&s_level);

    if (level_load_toml(safe_path, &s_level) != 0) {
        fprintf(stderr, "Error: Failed to load next phase: %s\n", safe_path);
        return -1;
    }

    strncpy(gs->level_path, next_path, sizeof(gs->level_path) - 1);
    gs->level_path[sizeof(gs->level_path) - 1] = '\0';

    level_load(gs, &s_level);
    game_completion_reset_summary(gs);
    level_resources_apply(gs, &s_level);

    phase_progress_restore(gs, &saved_progress);

    gs->checkpoint_x = 0.0f;
    gs->level_complete = 0;

    if (gs->debug_mode) {
        debug_log(&gs->debug, "PHASE TRANSITION to: %s", safe_path);
    }

    return 0;
}
