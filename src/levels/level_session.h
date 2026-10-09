/*
 * level_session.h — Active level load and phase transition helpers.
 */

#pragma once

#include <stddef.h>

#include "../game_fwd.h"  /* GameState, used by pointer only */
#include "level.h"

/* The campaign manifest and its catalog (CampaignCatalog, campaign_*) live
 * in campaign_catalog.h, which the editor shares; included here so callers
 * of the active-level API keep one include. */
#include "campaign_catalog.h"

/* Load the required startup level from GameState.world.level_path. */
int game_level_load_initial(GameState *gs);

/*
 * After an automatic screen checkpoint moved respawn_x on a start-point
 * run, put respawn_y on the highest surface under the new respawn column
 * (level_ground_top_at). Normal runs keep the level start's y, as before.
 * game_update_active calls it whenever the legacy checkpoint moved.
 */
void game_level_start_point_respawn_y(GameState *gs);

/* Free active level storage owned by GameState. */
void game_level_session_cleanup(GameState *gs);
