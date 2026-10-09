/*
 * level_session.h — Active level load and phase transition helpers.
 */

#pragma once

#include <stddef.h>

#include "../game.h"
#include "level.h"

/* The campaign manifest and its catalog (CampaignCatalog, campaign_*) live
 * in campaign_catalog.h, which the editor shares; included here so callers
 * of the active-level API keep one include. */
#include "campaign_catalog.h"

/* Load the required startup level from GameState.world.level_path. */
int game_level_load_initial(GameState *gs);

/* Free active level storage owned by GameState. */
void game_level_session_cleanup(GameState *gs);
