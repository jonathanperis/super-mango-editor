/*
 * game_state.c — Game state management implementation.
 *
 * Handles player death, respawn, checkpoint application, and level reset.
 */

#include "game_state.h"
#include "game_camera.h"

#include "../levels/level.h"
#include "../levels/level_loader.h"

void reset_current_level(GameState *gs, int *fp_prev_riding)
{
    const LevelDef *def = (const LevelDef *)gs->runtime.current_level;

    *fp_prev_riding = -1;

    if (!def) return;

    /* Runtime owns resolved respawn coordinates; LevelDef stays immutable. */
    gs->player.spawn_x = gs->respawn_x;
    gs->player.spawn_y = gs->respawn_y;
    level_reset(gs, def);

    /* The player is back at the respawn point, possibly a whole level away
     * from where they died. Show it at once instead of panning back. */
    game_camera_snap(gs);
}
