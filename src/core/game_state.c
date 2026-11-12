/*
 * game_state.c — Game state management implementation.
 *
 * Handles player death, respawn, checkpoint application, and level reset.
 */

#include "game_state.h"
#include "../game.h"  /* GameState: this file reads its fields */
#include "game_camera.h"

#include "../levels/level.h"
#include "../levels/level_loader.h"

void reset_current_level(GameState *gs, int *fp_prev_riding)
{
    const LevelDef *def = (const LevelDef *)gs->world.runtime.current_level;

    *fp_prev_riding = -1;

    if (!def) return;

    /* Runtime owns resolved respawn coordinates; LevelDef stays immutable. */
    gs->world.player.spawn_x = gs->world.respawn_x;
    gs->world.player.spawn_y = gs->world.respawn_y;
    level_reset(gs, def);

    /* A jump key still down from the life that just ended must not jump
     * the instant the player is back on the ground: count it as held, so
     * only a fresh press jumps (the first step with jump up clears it).
     * jump_cut_armed stays 0: there is no jump in progress to shorten.
     * player_reset itself leaves jump_held at 0, because a level start has
     * no earlier life; a key held through a level change is the input
     * release latch's job (game_input.c). */
    gs->world.player.jump_held = 1;

    /* The player is back at the respawn point, possibly a whole level away
     * from where they died. Show it at once instead of panning back. */
    game_camera_snap(gs);
}
