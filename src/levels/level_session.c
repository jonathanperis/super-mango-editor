/*
 * level_session.c — Active level load and phase transition helpers.
 */

#include "level_session.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "level.h"
#include "level_loader.h"
#include "level_path.h"
#include "level_ref.h"
#include "level_resources.h"
#include "phase_transition.h"
#include "../core/game_completion.h"
#include "../core/game_resources.h"
#include "../core/game_experiment.h"
#include "../shared/platform.h"  /* str_copy */
#include "../shared/serializer.h"
#include "../shared/serializer_io.h" /* serializer_file_exists_utf8 */
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

/* Record why an entry cannot be played; the menu shows this text. */
static void campaign_mark_unavailable(CampaignLevel *entry, const char *problem)
{
    entry->available = 0;
    str_copy(entry->problem, problem, sizeof(entry->problem));
    fprintf(stderr, "campaign: '%s' unavailable: %s\n", entry->path, problem);
}

/*
 * campaign_load_entry — Load one listed level file into its catalog entry.
 *
 * entry->path is already a safe levels/NAME.toml string. Failing here makes
 * only this entry unavailable; its display name then comes from the file
 * name, because the level's own name could not be read.
 */
static void campaign_load_entry(CampaignLevel *entry)
{
    char canonical_path[4096];

    entry->available = 1;
    entry->problem[0] = '\0';
    level_def_init_defaults(&entry->level);
    /* realpath() fails for a missing file, but Windows (GetFullPathNameW)
     * and the browser build only tidy the path without looking at the disk,
     * so the file is checked separately to give the same reason everywhere. */
    if (campaign_canonical_path(entry->path, strlen(entry->path),
                                canonical_path, sizeof(canonical_path)) != 0 ||
        !serializer_file_exists_utf8(canonical_path))
        campaign_mark_unavailable(entry, "level file not found");
    else if (level_load_toml(canonical_path, &entry->level) != 0) {
        level_def_init_defaults(&entry->level);
        campaign_mark_unavailable(entry, "level file is invalid");
    }
    campaign_derive_display_name(entry);
    if (!campaign_has_visible_text(entry->display_name, sizeof(entry->display_name))) {
        str_copy(entry->display_name, entry->path, sizeof(entry->display_name));
        if (entry->available) campaign_mark_unavailable(entry, "level has no display name");
    }
}

/*
 * campaign_manifest_load_entries — Parse a campaign manifest into staged.
 *
 * The manifest is untrusted input, so every step checks before it trusts:
 * parse, root keys, version, the level list, then each level path and the
 * level file it names. Returns 0 with staged filled in, or -1 after printing
 * why; on failure the caller discards staged, so a bad manifest never
 * replaces a working campaign. A broken *level file* is not a broken
 * manifest: campaign_load_entry marks only that entry unavailable.
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
        campaign_load_entry(&staged->levels[i]);
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
 * unavailable entry stays playable; its Next Level reports the failure.
 */
static void campaign_check_chain(CampaignCatalog *catalog)
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
                fprintf(stderr, "campaign: '%s' next_phase must be '%s'\n",
                        entry->path, catalog->levels[i + 1].path);
                campaign_mark_unavailable(entry, "next_phase is out of campaign order");
            }
        } else if (level->next_phase[0] != '\0') {
            fprintf(stderr, "campaign: final level '%s' must not have next_phase\n",
                    entry->path);
            campaign_mark_unavailable(entry, "final level has a next_phase");
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

int campaign_catalog_load(const char *manifest_path, CampaignCatalog *catalog)
{
    CampaignCatalog staged = {0};

    if (!manifest_path || !catalog) return -1;
    if (campaign_manifest_load_entries(manifest_path, &staged) != 0) {
        campaign_catalog_cleanup(&staged);
        return -1;
    }
    campaign_check_chain(&staged);
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

/*
 * read_stable_level — Parse, validate and fingerprint a level into the heap.
 *
 * level_load_toml validates the definition, which is the only validation a
 * level load needs; the callers below never repeat it. The copy lives on the
 * heap because a LevelDef is about 16 KB (see LEVEL_DEF_SIZE_BUDGET in
 * level.h). The file is fingerprinted before and after the parse, so the
 * hash an experiment records is the hash of exactly these bytes. Returns an
 * owned LevelDef, or NULL after printing why.
 */
static LevelDef *read_stable_level(const char *path, uint64_t *hash)
{
    SerializerFileFingerprint before, after;
    LevelDef *level = malloc(sizeof(*level));
    if (!level) {
        fprintf(stderr, "Error: Failed to allocate level storage\n");
        return NULL;
    }
    level_def_init_defaults(level);
    if (serializer_fingerprint_utf8(path, &before) != 1 || level_load_toml(path, level) != 0 ||
        serializer_fingerprint_utf8(path, &after) != 1 || !serializer_fingerprint_equal(&before, &after)) {
        fprintf(stderr, "Error: cannot read stable level bytes: %s\n", path);
        free(level);
        return NULL;
    }
    *hash = after.content_hash;
    return level;
}

/*
 * game_level_commit — Make a checked, heap-staged level the active one.
 *
 * Every step that can fail (resolve, parse, validate, required sprites) has
 * already run, so this function only moves pointers and applies data: it
 * cannot fail, and a failed load before it leaves the current level, path
 * and hash untouched. gs takes ownership of staged.
 */
static void game_level_commit(GameState *gs, LevelDef *staged, uint64_t hash)
{
    free(gs->level_def);
    gs->level_def = staged;
    gs->runtime.current_level = staged;
    gs->source_level_hash = hash;
    level_apply(gs, staged);
    game_completion_reset_summary(gs);
    level_resources_apply(gs, staged);
}

int game_level_load_initial(GameState *gs)
{
    char safe_path[GAME_LEVEL_PATH_MAX] = {0};

    if (!gs || gs->level_path[0] == '\0') {
        fprintf(stderr, "Error: initial level path is missing\n");
        return -1;
    }

    if (level_resolve_path(gs->level_path, safe_path, sizeof(safe_path)) != 0) {
        fprintf(stderr, "Error: could not resolve initial level: %s\n",
                gs->level_path);
        return -1;
    }

    uint64_t source_hash;
    LevelDef *loaded = read_stable_level(safe_path, &source_hash);
    if (!loaded) {
        fprintf(stderr, "Error: could not load initial level: %s\n", safe_path);
        return -1;
    }

    /* Parse and required sprites are checked before replacing active storage. */
    if (game_resources_require_level_textures(gs, loaded) != 0) {
        free(loaded);
        return -1;
    }
    game_level_commit(gs, loaded, source_hash);
    return 0;
}

int game_load_next_phase(GameState *gs)
{
    const LevelDef *current = (const LevelDef *)gs->runtime.current_level;
    char next_path[256] = {0};
    if (phase_next_path(current, next_path, sizeof(next_path)) != 0) return -1;

    PhaseProgress saved_progress;
    phase_progress_save(gs, &saved_progress);

    char safe_path[GAME_LEVEL_PATH_MAX] = {0};
    if (level_resolve_path(next_path, safe_path, sizeof(safe_path)) != 0) {
        fprintf(stderr, "Error: Failed to resolve next phase path: %s\n", next_path);
        return -1;
    }

    uint64_t source_hash;
    LevelDef *next_level = read_stable_level(safe_path, &source_hash);
    if (!next_level) {
        fprintf(stderr, "Error: Failed to load next phase: %s\n", safe_path);
        return -1;
    }
    if (game_resources_require_level_textures(gs, next_level) != 0) {
        free(next_level);
        return -1;
    }

    /* Every failure point is behind us: from here the switch cannot fail,
     * so the current level is only given up once the next one is certain. */
    game_experiment_cleanup(gs);
    str_copy(gs->level_path, next_path, sizeof(gs->level_path));
    game_level_commit(gs, next_level, source_hash);

    /* Only campaign progress crosses a phase boundary. Old movement, climbing,
     * and support indices refer to the previous level and must not survive. */
    player_reset(&gs->player);
    gs->camera.x = 0.0f;
    gs->loop.fp_prev_riding = -1;

    phase_progress_restore(gs, &saved_progress);
    gs->level_score_start = gs->score;
    gs->profile_completion_recorded = 0;

    gs->completion.complete = 0;

    if (gs->debug_mode) {
        debug_log(&gs->debug, "PHASE TRANSITION to: %s", safe_path);
    }

    return 0;
}

void game_level_session_cleanup(GameState *gs)
{
    if (gs->level_def) {
        free(gs->level_def);
        gs->level_def = NULL;
    }
    gs->runtime.current_level = NULL;
}
