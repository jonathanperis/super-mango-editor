/*
 * campaign_catalog.h — The campaign manifest (levels/campaigns/main.toml)
 * and the catalog of levels it lists.
 *
 * The game's start menu loads it (campaign_catalog_load); the editor's
 * Campaign view edits it with the same rules (campaign_catalog_check) and
 * writes it back (campaign_manifest_save).  The rules only read level files,
 * so they live apart from the game's active-level code in level_session.c.
 */
#pragma once

#include <stddef.h>   /* size_t */

#include "level.h"    /* LevelDef */

#define CAMPAIGN_MANIFEST_VERSION 1
#define CAMPAIGN_MANIFEST_PATH "levels/campaigns/main.toml"
#define CAMPAIGN_LEVEL_PATH_SIZE 256
#define CAMPAIGN_DISPLAY_NAME_SIZE 64
#define CAMPAIGN_PROBLEM_SIZE 48

/*
 * One manifest entry. A level file that is missing, invalid or out of the
 * campaign order does not take the whole menu down: its entry stays listed
 * with available == 0 and a short reason, and the menu shows it disabled.
 * level is only meaningful when loaded is 1; available is loaded plus the
 * order and name rules (campaign_catalog_check).
 */
typedef struct {
    char path[CAMPAIGN_LEVEL_PATH_SIZE];
    char display_name[CAMPAIGN_DISPLAY_NAME_SIZE];
    int  available;                      /* 1 = loaded and playable         */
    char problem[CAMPAIGN_PROBLEM_SIZE]; /* why not, for an unavailable one */
    LevelDef level;
    int  loaded;                         /* 1 = the level file loaded       */
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

/*
 * campaign_catalog_read — The parse half of campaign_catalog_load: the
 * manifest itself must be valid (syntax, version, a nonempty list of
 * valid, unrepeated paths), and each listed level file is loaded, but the
 * order rules are not applied and no playable entry is required.  The
 * editor opens a manifest this way, so a campaign the menu would refuse
 * can still be repaired.  Keeps *catalog on failure.
 */
int campaign_catalog_read(const char *manifest_path, CampaignCatalog *catalog);

/* Index of the first playable entry, or -1 when there is none. */
int campaign_first_available(const CampaignCatalog *catalog);

/* Release catalog-owned LevelDef storage. */
void campaign_catalog_cleanup(CampaignCatalog *catalog);

/* 1 when path (a C string) may be listed in a manifest: the shared
 * levels/<name>.toml rule (level_ref.h) within CAMPAIGN_LEVEL_PATH_SIZE. */
int campaign_entry_path_valid(const char *path);

/*
 * campaign_entry_load — Read the level file entry->path names into the
 * entry (loaded, level, display name; available and problem as far as the
 * file alone can tell).  entry->path must already be valid.
 */
void campaign_entry_load(CampaignLevel *entry);

/*
 * campaign_catalog_check — Apply every manifest rule the game applies to
 * an in-memory catalog whose entries were loaded with campaign_entry_load
 * and may have been edited since (reordered, removed, a level's name or
 * next_phase changed): at least one entry, every path valid and listed
 * once, each level's display name, each level's next_phase naming the
 * next entry (none for the last), and at least one playable entry.
 * Updates every entry's available / problem / display_name.  Returns 0,
 * or -1 with the manifest-wide problem in err (an entry's own problem is
 * left in that entry, not in err).
 */
int campaign_catalog_check(CampaignCatalog *catalog, char *err, size_t err_size);

/*
 * campaign_manifest_save — Write the catalog's paths as a v1 manifest at
 * manifest_path, through the same temporary-file-and-replace path a level
 * save uses, so a failure leaves the old manifest whole.  Refuses (-1) a
 * catalog whose paths break the path rules.  Returns 0, -1, or
 * SERIALIZER_REPLACE_TEMP_KEPT (serializer.h).
 */
int campaign_manifest_save(const char *manifest_path, const CampaignCatalog *catalog);
