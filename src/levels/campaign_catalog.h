/*
 * campaign_catalog.h — The campaign manifest (levels/campaigns/main.toml)
 * and the catalog of levels it lists.
 *
 * The game's start menu loads it (campaign_catalog_load); the editor's
 * Campaign view edits it with the same rules (campaign_catalog_check) and
 * writes it back (campaign_manifest_write_temp).  The rules only read level files,
 * so they live apart from the game's active-level code in level_session.c.
 */
#pragma once

#include <stddef.h>   /* size_t */

#include "level.h"    /* LevelDef */

#define CAMPAIGN_MANIFEST_VERSION 1

/*
 * CAMPAIGN_MAX_LEVELS — the most levels one campaign may list.  It matches
 * PROFILE_LEVEL_COUNT (game_profile.h), the number of levels a profile can
 * hold results for, so every listed level can have a best time.
 * tools/validate_levels.py applies the same limit.
 */
#define CAMPAIGN_MAX_LEVELS 128

/* The largest manifest file read, in bytes (the file plus one NUL): 128
 * entries of a 255-byte path are about 33 KB with their punctuation. */
#define CAMPAIGN_MANIFEST_TEXT_MAX (64 * 1024)
#define CAMPAIGN_MANIFEST_PATH "levels/campaigns/main.toml"
#define CAMPAIGN_LEVEL_PATH_SIZE 256
#define CAMPAIGN_DISPLAY_NAME_SIZE 64
#define CAMPAIGN_PROBLEM_SIZE 48

/*
 * One manifest entry. A level file that is missing, invalid or out of the
 * campaign order does not take the whole menu down: its entry stays listed
 * with available == 0 and a short reason, and the menu shows it disabled.
 * The game's campaign flow (Play, Continue, Next Level, --continue) never
 * starts an unavailable entry; only --level, which bypasses the manifest,
 * can still load its file.
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
 * campaign_catalog_load_text — campaign_catalog_load for manifest bytes
 * already in memory (size of them at text; no NUL needed).  label stands in
 * for the file name in messages.  The listed level files are still read
 * from disk.  The fuzz harness and tests use it.
 */
int campaign_catalog_load_text(const char *text, size_t size, const char *label,
                               CampaignCatalog *catalog);

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
 * campaign_manifest_write_temp — Write the catalog's paths as a v1
 * manifest into a new temporary file next to manifest_path, whose name goes
 * in temp_path (see serializer_write_temp).  The caller installs it with
 * serializer_install_temp once every other file of the same save is
 * written too, so a failure leaves the old manifest whole.  Refuses (-1) a
 * catalog whose paths break the path rules.  Returns 0 or -1.
 */
int campaign_manifest_write_temp(const char *manifest_path, const CampaignCatalog *catalog,
                                 char *temp_path, size_t temp_path_size);
