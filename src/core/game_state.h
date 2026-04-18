/*
 * game_state.h — Game state management public interface.
 *
 * Handles player death, respawn, checkpoint application, and level reset.
 */
#pragma once

#include "../game.h"

/* ------------------------------------------------------------------ */
/* Level reset / respawn                                              */
/* ------------------------------------------------------------------ */

/*
 * reset_current_level — centralised "player died, restart level" handler.
 *
 * Resets every entity array and the player to their initial state.
 * Called from every hearts<=0 branch so all sources of death produce
 * an identical reset — no entity is accidentally left in a stale state.
 *
 * Applies checkpoint offset to spawn position if a checkpoint has been
 * reached (checkpoint_x > 0). This allows players to respawn at the
 * furthest screen they reached instead of always restarting at level start.
 *
 * fp_prev_riding is passed by pointer because it lives as a local
 * variable inside game_loop; resetting it here keeps the float-platform
 * stay-on logic from snapping the player to a platform that no longer
 * exists after the reset.
 */
void reset_current_level(GameState *gs, int *fp_prev_riding);
