/*
 * level_session.h — Active level load and phase transition helpers.
 */

#pragma once

#include <stddef.h>

#include "../game.h"
#include "level.h"

#define CAMPAIGN_MANIFEST_VERSION 1
#define CAMPAIGN_MANIFEST_PATH "levels/campaigns/main.toml"
#define CAMPAIGN_LEVEL_PATH_SIZE 256
#define CAMPAIGN_DISPLAY_NAME_SIZE 64

typedef struct {
    char path[CAMPAIGN_LEVEL_PATH_SIZE];
    char display_name[CAMPAIGN_DISPLAY_NAME_SIZE];
    LevelDef level;
} CampaignLevel;

typedef struct {
    CampaignLevel *levels;
    size_t count;
} CampaignCatalog;

/* Load and validate the ordered campaign manifest transactionally. */
int campaign_catalog_load(const char *manifest_path, CampaignCatalog *catalog);

/* Release catalog-owned LevelDef storage. */
void campaign_catalog_cleanup(CampaignCatalog *catalog);

/* Load the required startup level from GameState::level_path. */
int game_level_load_initial(GameState *gs);

/* Free active level storage owned by GameState. */
void game_level_session_cleanup(GameState *gs);
