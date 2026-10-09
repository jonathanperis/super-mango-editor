/*
 * campaign_catalog.c — The campaign manifest and its level catalog (see
 * campaign_catalog.h).  The start menu loads it; the editor's Campaign
 * view checks and saves it with the same rules.
 */

#include "campaign_catalog.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "level_path.h"                   /* level_resolve_path */
#include "level_ref.h"                    /* level_ref_valid */
#include "../shared/platform.h"           /* str_copy */
#include "../shared/serializer.h"         /* level_load_toml, serializer_save_file */
#include "../shared/serializer_io.h"      /* serializer_file_exists_utf8 */
#include "../shared/serializer_emit.h"    /* write_toml_string */
#include "../../vendor/tomlc17/tomlc17.h"

static int campaign_key_matches(const char *supplied, int supplied_len,
                                const char *expected, size_t expected_len)
{
    if (!supplied || supplied_len < 0 || !expected ||
        (size_t)supplied_len != expected_len)
        return 0;
    if (memchr(supplied, '\0', (size_t)supplied_len) != NULL) return 0;
    return memcmp(supplied, expected, expected_len) == 0;
}

static int campaign_manifest_key_allowed(const char *key, int key_len)
{
    return campaign_key_matches(key, key_len, "format_version",
                                 sizeof("format_version") - 1) ||
           campaign_key_matches(key, key_len, "levels", sizeof("levels") - 1);
}

/*
 * Manifest entries use the same levels/<name>.toml rule as next_phase and
 * profile keys (level_ref.h); this wrapper only adds the storage limit.
 * TOML strings carry an explicit length, so an embedded NUL is also rejected
 * by level_ref_valid as a control byte.
 */
static int campaign_level_path_safe(const char *path, size_t length)
{
    if (!path || length == 0 || length >= CAMPAIGN_LEVEL_PATH_SIZE) return 0;
    return level_ref_valid(path, length);
}

static int campaign_cstring_length(const char *text, size_t capacity,
                                   size_t *length)
{
    if (!text || !length) return -1;
    for (size_t i = 0; i < capacity; i++) {
        if (text[i] == '\0') {
            *length = i;
            return 0;
        }
    }
    return -1;
}

static int campaign_copy_exact(char *dst, size_t dst_size,
                               const char *src, size_t src_len)
{
    if (!dst || dst_size == 0 || !src || src_len >= dst_size ||
        memchr(src, '\0', src_len) != NULL)
        return -1;
    memcpy(dst, src, src_len);
    dst[src_len] = '\0';
    return 0;
}

static int campaign_has_visible_text(const char *text, size_t capacity)
{
    size_t length;

    if (campaign_cstring_length(text, capacity, &length) != 0) return 0;
    for (size_t i = 0; i < length; i++) {
        if (!isspace((unsigned char)text[i])) return 1;
    }
    return 0;
}

static void campaign_derive_display_name(CampaignLevel *entry)
{
    size_t path_len;
    size_t name_len;
    const char *filename;
    size_t filename_len;

    if (!entry) return;
    if (campaign_has_visible_text(entry->level.name,
                                 sizeof(entry->level.name))) {
        if (campaign_cstring_length(entry->level.name,
                                    sizeof(entry->level.name), &name_len) == 0)
            (void)campaign_copy_exact(entry->display_name,
                                       sizeof(entry->display_name),
                                       entry->level.name, name_len);
        return;
    }

    if (campaign_cstring_length(entry->path, sizeof(entry->path), &path_len) != 0)
        return;
    filename = entry->path;
    for (size_t i = 0; i < path_len; i++) {
        if (entry->path[i] == '/') filename = entry->path + i + 1;
    }
    filename_len = path_len - (size_t)(filename - entry->path);
    if (filename_len >= sizeof(".toml") - 1 &&
        memcmp(filename + filename_len - (sizeof(".toml") - 1),
               ".toml", sizeof(".toml") - 1) == 0)
        filename_len -= sizeof(".toml") - 1;
    if (filename_len >= sizeof(entry->display_name))
        filename_len = sizeof(entry->display_name) - 1;
    memcpy(entry->display_name, filename, filename_len);
    entry->display_name[filename_len] = '\0';
}

static int campaign_canonical_path(const char *path, size_t path_len,
                                   char *out, size_t out_size)
{
    if (!campaign_level_path_safe(path, path_len)) return -1;
    return level_resolve_path(path, out, out_size);
}

static int campaign_paths_equal(const char *left, const char *right)
{
    char left_canonical[4096];
    char right_canonical[4096];
    size_t left_len;
    size_t right_len;
    size_t left_canonical_len;
    size_t right_canonical_len;

    if (campaign_cstring_length(left, CAMPAIGN_LEVEL_PATH_SIZE, &left_len) != 0 ||
        campaign_cstring_length(right, CAMPAIGN_LEVEL_PATH_SIZE, &right_len) != 0 ||
        campaign_canonical_path(left, left_len, left_canonical,
                                sizeof(left_canonical)) != 0 ||
        campaign_canonical_path(right, right_len, right_canonical,
                                sizeof(right_canonical)) != 0 ||
        campaign_cstring_length(left_canonical, sizeof(left_canonical),
                                &left_canonical_len) != 0 ||
        campaign_cstring_length(right_canonical, sizeof(right_canonical),
                                &right_canonical_len) != 0)
        return 0;
    return left_canonical_len == right_canonical_len &&
           memcmp(left_canonical, right_canonical, left_canonical_len) == 0;
}

static toml_datum_t campaign_manifest_get_exact(toml_datum_t table,
                                                 const char *expected,
                                                 size_t expected_len)
{
    toml_datum_t missing = {0};

    if (table.type != TOML_TABLE || !expected) return missing;
    for (int i = 0; i < table.u.tab.size; i++) {
        if (campaign_key_matches(table.u.tab.key[i], table.u.tab.len[i],
                                 expected, expected_len))
            return table.u.tab.value[i];
    }
    return missing;
}

/* Record why an entry cannot be played; the menu shows this text.  The
 * game also logs it (report); the editor rechecks after every edit and
 * shows it in its Campaign view instead. */
static void campaign_mark_unavailable(CampaignLevel *entry, const char *problem,
                                      int report)
{
    entry->available = 0;
    str_copy(entry->problem, problem, sizeof(entry->problem));
    if (report)
        fprintf(stderr, "campaign: '%s' unavailable: %s\n", entry->path, problem);
}

/*
 * campaign_entry_refresh — What an entry's level, as it is now, says about
 * the entry: playable when its file loaded, named by the level's name (or
 * the file name), and unavailable when that name would be empty.
 */
static void campaign_entry_refresh(CampaignLevel *entry, int report)
{
    entry->available = entry->loaded;
    if (entry->loaded) entry->problem[0] = '\0';
    campaign_derive_display_name(entry);
    if (!campaign_has_visible_text(entry->display_name, sizeof(entry->display_name))) {
        str_copy(entry->display_name, entry->path, sizeof(entry->display_name));
        if (entry->available) campaign_mark_unavailable(entry, "level has no display name", report);
    }
}

/*
 * campaign_entry_load — Load one listed level file into its catalog entry.
 *
 * entry->path is already a safe levels/NAME.toml string. Failing here makes
 * only this entry unavailable; its display name then comes from the file
 * name, because the level's own name could not be read.
 */
void campaign_entry_load(CampaignLevel *entry)
{
    char canonical_path[4096];

    entry->loaded = 1;
    entry->problem[0] = '\0';
    level_def_init_defaults(&entry->level);
    /* realpath() fails for a missing file, but Windows (GetFullPathNameW)
     * and the browser build only tidy the path without looking at the disk,
     * so the file is checked separately to give the same reason everywhere. */
    if (campaign_canonical_path(entry->path, strlen(entry->path),
                                canonical_path, sizeof(canonical_path)) != 0 ||
        !serializer_file_exists_utf8(canonical_path)) {
        entry->loaded = 0;
        campaign_mark_unavailable(entry, "level file not found", 1);
    } else if (level_load_toml(canonical_path, &entry->level) != 0) {
        level_def_init_defaults(&entry->level);
        entry->loaded = 0;
        campaign_mark_unavailable(entry, "level file is invalid", 1);
    }
    campaign_entry_refresh(entry, 1);
}

/*
 * campaign_manifest_load_entries — Parse a campaign manifest into staged.
 *
 * The manifest is untrusted input, so every step checks before it trusts:
 * parse, root keys, version, the level list, then each level path and the
 * level file it names. Returns 0 with staged filled in, or -1 after printing
 * why; on failure the caller discards staged, so a bad manifest never
 * replaces a working campaign. A broken *level file* is not a broken
 * manifest: campaign_entry_load marks only that entry unavailable.
 */
static int campaign_manifest_load_entries(const char *manifest_path,
                                          CampaignCatalog *staged)
{
    FILE *fp;
    toml_result_t parsed;
    toml_datum_t top;
    toml_datum_t version;
    toml_datum_t levels;
    int result = -1;

    /* 1. Read and parse the manifest file. */
    fp = serializer_fopen_utf8(manifest_path, "rb");
    if (!fp) {
        fprintf(stderr, "campaign: cannot open manifest '%s'\n", manifest_path);
        return -1;
    }
    parsed = toml_parse_file(fp);
    fclose(fp);
    if (!parsed.ok) {
        fprintf(stderr, "campaign: TOML parse error in '%s': %s\n",
                manifest_path, parsed.errmsg);
        return -1;
    }

    /* 2. The root must be a table holding only the known keys. */
    top = parsed.toptab;
    if (top.type != TOML_TABLE) {
        fprintf(stderr, "campaign: manifest root must be a table\n");
        goto done;
    }
    for (int i = 0; i < top.u.tab.size; i++) {
        if (!top.u.tab.key[i] || top.u.tab.len[i] < 0 ||
            memchr(top.u.tab.key[i], '\0', (size_t)top.u.tab.len[i]) != NULL) {
            fprintf(stderr, "campaign: manifest key contains an embedded NUL\n");
            goto done;
        }
        if (!campaign_manifest_key_allowed(top.u.tab.key[i], top.u.tab.len[i])) {
            fprintf(stderr, "campaign: manifest contains an unsupported field\n");
            goto done;
        }
    }

    /* 3. Exactly the supported format version. */
    version = campaign_manifest_get_exact(top, "format_version",
                                          sizeof("format_version") - 1);
    if (version.type != TOML_INT64 ||
        version.u.int64 != CAMPAIGN_MANIFEST_VERSION) {
        fprintf(stderr, "campaign: manifest format_version must be integer %d\n",
                CAMPAIGN_MANIFEST_VERSION);
        goto done;
    }

    /* 4. A nonempty ordered list of levels; stage one entry per level. */
    levels = campaign_manifest_get_exact(top, "levels", sizeof("levels") - 1);
    if (levels.type != TOML_ARRAY || levels.u.arr.size <= 0) {
        fprintf(stderr, "campaign: manifest levels must be a nonempty array\n");
        goto done;
    }

    staged->levels = calloc((size_t)levels.u.arr.size, sizeof(*staged->levels));
    if (!staged->levels) {
        fprintf(stderr, "campaign: catalog allocation failed\n");
        goto done;
    }
    staged->count = (size_t)levels.u.arr.size;

    for (size_t i = 0; i < staged->count; i++) {
        toml_datum_t item = levels.u.arr.elem[i];

        /* 5a. Each entry is a safe levels/NAME.toml path, listed once. */
        if (item.type != TOML_STRING || !item.u.str.ptr || item.u.str.len < 0 ||
            !campaign_level_path_safe(item.u.str.ptr, (size_t)item.u.str.len)) {
            fprintf(stderr, "campaign: levels[%zu] must be a safe levels/*.toml path\n", i);
            goto done;
        }
        for (size_t previous = 0; previous < i; previous++) {
            size_t previous_len;
            if (campaign_cstring_length(staged->levels[previous].path,
                                        sizeof(staged->levels[previous].path),
                                        &previous_len) != 0)
                continue;
            if (previous_len == (size_t)item.u.str.len &&
                memcmp(staged->levels[previous].path, item.u.str.ptr,
                       previous_len) == 0) {
                fprintf(stderr, "campaign: duplicate manifest level path '%s'\n",
                        item.u.str.ptr);
                goto done;
            }
        }
        /* 5b. Keep the manifest spelling, then resolve and load the level
         *     file. A file that is missing or invalid is the entry's own
         *     problem: it stays listed, unavailable, and the rest of the
         *     campaign remains playable. */
        if (campaign_copy_exact(staged->levels[i].path,
                                sizeof(staged->levels[i].path),
                                item.u.str.ptr, (size_t)item.u.str.len) != 0) {
            fprintf(stderr, "campaign: manifest level path copy failed\n");
            goto done;
        }
        campaign_entry_load(&staged->levels[i]);
    }

    result = 0;

done:
    /* Single cleanup point: every failure after parsing jumps here, so the
     * parsed TOML tree is freed exactly once on every path. */
    toml_free(parsed);
    return result;
}

/*
 * campaign_check_chain — Each playable level must lead to the next entry.
 *
 * Completing level i offers "Next Level", which loads its next_phase, so
 * that must name entry i+1 (and the last entry must have none). A level
 * that breaks the order is marked unavailable rather than rejecting the
 * whole campaign. The comparison uses paths only, so a level before an
 * unavailable entry stays playable, but the game's Next Level refuses to
 * open that entry and says so, as the menu refuses to start it; so does
 * --continue (session_campaign_refusal in app_session.c). Only --level,
 * which loads one file directly, bypasses the manifest.
 */
static void campaign_check_chain(CampaignCatalog *catalog, int report)
{
    for (size_t i = 0; i < catalog->count; i++) {
        CampaignLevel *entry = &catalog->levels[i];
        const LevelDef *level = &entry->level;

        if (!entry->available) continue;
        if (i + 1 < catalog->count) {
            /* Same spelling also counts: the next file may be missing, and
             * then it cannot be resolved for the on-disk comparison. */
            const char *next = catalog->levels[i + 1].path;
            if (level->next_phase[0] == '\0' ||
                (strcmp(level->next_phase, next) != 0 &&
                 !campaign_paths_equal(level->next_phase, next))) {
                if (report)
                    fprintf(stderr, "campaign: '%s' next_phase must be '%s'\n",
                            entry->path, catalog->levels[i + 1].path);
                campaign_mark_unavailable(entry, "next_phase is out of campaign order",
                                          report);
            }
        } else if (level->next_phase[0] != '\0') {
            if (report)
                fprintf(stderr, "campaign: final level '%s' must not have next_phase\n",
                        entry->path);
            campaign_mark_unavailable(entry, "final level has a next_phase", report);
        }
    }
}

int campaign_first_available(const CampaignCatalog *catalog)
{
    if (!catalog) return -1;
    for (size_t i = 0; i < catalog->count; i++)
        if (catalog->levels[i].available) return (int)i;
    return -1;
}

int campaign_catalog_read(const char *manifest_path, CampaignCatalog *catalog)
{
    CampaignCatalog staged = {0};

    if (!manifest_path || !catalog) return -1;
    if (campaign_manifest_load_entries(manifest_path, &staged) != 0) {
        campaign_catalog_cleanup(&staged);
        return -1;
    }
    campaign_catalog_cleanup(catalog);
    *catalog = staged;
    return 0;
}

int campaign_catalog_load(const char *manifest_path, CampaignCatalog *catalog)
{
    CampaignCatalog staged = {0};

    if (!manifest_path || !catalog) return -1;
    if (campaign_catalog_read(manifest_path, &staged) != 0) return -1;
    campaign_check_chain(&staged, 1);
    /* A menu with nothing to play is not a usable campaign. */
    if (campaign_first_available(&staged) < 0) {
        fprintf(stderr, "campaign: no playable level in '%s'\n", manifest_path);
        campaign_catalog_cleanup(&staged);
        return -1;
    }

    campaign_catalog_cleanup(catalog);
    *catalog = staged;
    return 0;
}

void campaign_catalog_cleanup(CampaignCatalog *catalog)
{
    if (!catalog) return;
    free(catalog->levels);
    catalog->levels = NULL;
    catalog->count = 0;
}

/* ------------------------------------------------------------------ */
/* Editing a catalog in memory (the editor's Campaign view)            */
/* ------------------------------------------------------------------ */

int campaign_entry_path_valid(const char *path)
{
    size_t length;

    if (campaign_cstring_length(path, CAMPAIGN_LEVEL_PATH_SIZE, &length) != 0) return 0;
    return campaign_level_path_safe(path, length);
}

/*
 * campaign_paths_problem — The rules campaign_manifest_load_entries applies
 * to the levels array (5a): at least one entry, each a valid path, none
 * listed twice.  Returns 0, or -1 with why in err.
 */
static int campaign_paths_problem(const CampaignCatalog *catalog,
                                  char *err, size_t err_size)
{
    if (!catalog || catalog->count == 0) {
        if (err && err_size > 0) snprintf(err, err_size, "the campaign lists no levels");
        return -1;
    }
    for (size_t i = 0; i < catalog->count; i++) {
        if (!campaign_entry_path_valid(catalog->levels[i].path)) {
            if (err && err_size > 0)
                snprintf(err, err_size, "levels[%zu] must be a levels/<name>.toml path", i);
            return -1;
        }
        for (size_t previous = 0; previous < i; previous++) {
            if (strcmp(catalog->levels[previous].path, catalog->levels[i].path) == 0) {
                if (err && err_size > 0)
                    snprintf(err, err_size, "levels[%zu] repeats levels[%zu] (%s)",
                             i, previous, catalog->levels[i].path);
                return -1;
            }
        }
    }
    return 0;
}

int campaign_catalog_check(CampaignCatalog *catalog, char *err, size_t err_size)
{
    if (err && err_size > 0) err[0] = '\0';
    if (campaign_paths_problem(catalog, err, err_size) != 0) return -1;
    /* The same per-entry rules as a load, on the levels as edited. */
    for (size_t i = 0; i < catalog->count; i++)
        campaign_entry_refresh(&catalog->levels[i], 0);
    campaign_check_chain(catalog, 0);
    if (campaign_first_available(catalog) < 0) {
        if (err && err_size > 0)
            snprintf(err, err_size, "no level in the campaign is playable");
        return -1;
    }
    return 0;
}

/* The manifest text: the version, then one path per line in order. */
static void emit_manifest(FILE *fp, const void *context)
{
    const CampaignCatalog *catalog = context;

    fprintf(fp, "format_version = %d\n", CAMPAIGN_MANIFEST_VERSION);
    fputs("levels = [\n", fp);
    for (size_t i = 0; i < catalog->count; i++) {
        fputs("    ", fp);
        write_toml_string(fp, catalog->levels[i].path);
        fputs(",\n", fp);
    }
    fputs("]\n", fp);
}

int campaign_manifest_save(const char *manifest_path, const CampaignCatalog *catalog)
{
    char err[160];

    if (!manifest_path) return -1;
    if (campaign_paths_problem(catalog, err, sizeof(err)) != 0) {
        fprintf(stderr, "campaign: not saving '%s': %s\n", manifest_path, err);
        return -1;
    }
    return serializer_save_file(manifest_path, emit_manifest, catalog);
}
