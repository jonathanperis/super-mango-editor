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
#define CAMPAIGN_PROBLEM_SIZE 48

/*
 * One manifest entry. A level file that is missing, invalid or out of the
 * campaign order does not take the whole menu down: its entry stays listed
 * with available == 0 and a short reason, and the menu shows it disabled.
 * level is only meaningful for an available entry.
 */
typedef struct {
    char path[CAMPAIGN_LEVEL_PATH_SIZE];
    char display_name[CAMPAIGN_DISPLAY_NAME_SIZE];
    int  available;                      /* 1 = loaded and playable         */
    char problem[CAMPAIGN_PROBLEM_SIZE]; /* why not, for an unavailable one */
    LevelDef level;
} CampaignLevel;

typedef struct {
    CampaignLevel *levels;
    size_t count;
} CampaignCatalog;

/*
 * Load and validate the ordered campaign manifest transactionally. A broken
 * manifest (syntax, version, unsafe or repeated paths) or one with no
 * playable level fails and keeps *catalog; a broken level file only marks
 * its own entry unavailable.
 */
int campaign_catalog_load(const char *manifest_path, CampaignCatalog *catalog);

/* Index of the first playable entry, or -1 when there is none. */
int campaign_first_available(const CampaignCatalog *catalog);

/* Release catalog-owned LevelDef storage. */
void campaign_catalog_cleanup(CampaignCatalog *catalog);

/* Load the required startup level from GameState::level_path. */
int game_level_load_initial(GameState *gs);

/* Free active level storage owned by GameState. */
void game_level_session_cleanup(GameState *gs);
