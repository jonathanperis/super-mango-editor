/*
 * game_state.c — Game state management implementation.
 *
 * Handles player death, respawn, checkpoint application, and level reset.
 */

#include "game_state.h"

#include "../levels/level.h"
#include "../levels/level_loader.h"

/*
 * External reference to the file-scoped level definition in game.c.
 * Populated once from TOML in game_init, then referenced here on death.
 */
extern LevelDef s_level;

void reset_current_level(GameState *gs, int *fp_prev_riding)
{
    /* Apply checkpoint offset to spawn position if set */
    if (gs->checkpoint_x > 0.0f) {
        gs->player.spawn_x = gs->checkpoint_x;
    }
    level_reset(gs, &s_level);
    *fp_prev_riding = -1;
}
