/*
 * game_state.c — Game state management implementation.
 *
 * Handles player death, respawn, checkpoint application, and level reset.
 */

#include "game_state.h"

#include "../levels/level.h"
#include "../levels/level_loader.h"

void reset_current_level(GameState *gs, int *fp_prev_riding)
{
    const LevelDef *def = (const LevelDef *)gs->runtime.current_level;

    *fp_prev_riding = -1;

    if (!def) return;

    /* Apply checkpoint offset to spawn position if set */
    if (gs->checkpoint_x > 0.0f) {
        gs->player.spawn_x = gs->checkpoint_x;
    }
    level_reset(gs, def);
}
