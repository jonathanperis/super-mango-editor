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
#include "level_resources.h"
#include "phase_transition.h"
#include "../core/game_completion.h"
#include "../core/game_resources.h"
#include "../core/game_experiment.h"
#include "../shared/serializer.h"
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

static int campaign_level_path_safe(const char *path, size_t length)
{
    static const char prefix[] = "levels/";
    static const char suffix[] = ".toml";
    const size_t prefix_len = sizeof(prefix) - 1;
    const size_t suffix_len = sizeof(suffix) - 1;
    const char *name;
    size_t name_len;

    if (!path || length == 0 || length >= CAMPAIGN_LEVEL_PATH_SIZE ||
        memchr(path, '\0', length) != NULL)
        return 0;
    if (length <= prefix_len + suffix_len ||
        memcmp(path, prefix, prefix_len) != 0 ||
        memcmp(path + length - suffix_len, suffix, suffix_len) != 0 ||
        memchr(path, '\\', length) != NULL ||
        memchr(path, ':', length) != NULL)
        return 0;

    name = path + prefix_len;
    name_len = length - prefix_len;
    if (memchr(name, '/', name_len) != NULL ||
        (name_len == suffix_len && memcmp(name, suffix, suffix_len) == 0) ||
        (name_len == sizeof("..toml") - 1 &&
         memcmp(name, "..toml", sizeof("..toml") - 1) == 0))
        return 0;
    return 1;
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

static int campaign_manifest_load_entries(const char *manifest_path,
                                          CampaignCatalog *staged)
{
    FILE *fp;
    toml_result_t parsed;
    toml_datum_t top;
    toml_datum_t version;
    toml_datum_t levels;

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

    top = parsed.toptab;
    if (top.type != TOML_TABLE) {
        fprintf(stderr, "campaign: manifest root must be a table\n");
        toml_free(parsed);
        return -1;
    }
    for (int i = 0; i < top.u.tab.size; i++) {
        if (!top.u.tab.key[i] || top.u.tab.len[i] < 0 ||
            memchr(top.u.tab.key[i], '\0', (size_t)top.u.tab.len[i]) != NULL) {
            fprintf(stderr, "campaign: manifest key contains an embedded NUL\n");
            toml_free(parsed);
            return -1;
        }
        if (!campaign_manifest_key_allowed(top.u.tab.key[i], top.u.tab.len[i])) {
            fprintf(stderr, "campaign: manifest contains an unsupported field\n");
            toml_free(parsed);
            return -1;
        }
    }

    version = campaign_manifest_get_exact(top, "format_version",
                                          sizeof("format_version") - 1);
    if (version.type != TOML_INT64 ||
        version.u.int64 != CAMPAIGN_MANIFEST_VERSION) {
        fprintf(stderr, "campaign: manifest format_version must be integer %d\n",
                CAMPAIGN_MANIFEST_VERSION);
        toml_free(parsed);
        return -1;
    }

    levels = campaign_manifest_get_exact(top, "levels", sizeof("levels") - 1);
    if (levels.type != TOML_ARRAY || levels.u.arr.size <= 0) {
        fprintf(stderr, "campaign: manifest levels must be a nonempty array\n");
        toml_free(parsed);
        return -1;
    }

    staged->levels = calloc((size_t)levels.u.arr.size, sizeof(*staged->levels));
    if (!staged->levels) {
        fprintf(stderr, "campaign: catalog allocation failed\n");
        toml_free(parsed);
        return -1;
    }
    staged->count = (size_t)levels.u.arr.size;

    for (size_t i = 0; i < staged->count; i++) {
        toml_datum_t item = levels.u.arr.elem[i];
        char canonical_path[4096];

        if (item.type != TOML_STRING || !item.u.str.ptr || item.u.str.len < 0 ||
            !campaign_level_path_safe(item.u.str.ptr, (size_t)item.u.str.len)) {
            fprintf(stderr, "campaign: levels[%zu] must be a safe levels/*.toml path\n", i);
            toml_free(parsed);
            return -1;
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
                toml_free(parsed);
                return -1;
            }
        }
        if (campaign_canonical_path(item.u.str.ptr, (size_t)item.u.str.len,
                                    canonical_path, sizeof(canonical_path)) != 0) {
            fprintf(stderr, "campaign: cannot resolve manifest level '%s'\n",
                    item.u.str.ptr);
            toml_free(parsed);
            return -1;
        }

        if (campaign_copy_exact(staged->levels[i].path,
                                sizeof(staged->levels[i].path),
                                item.u.str.ptr, (size_t)item.u.str.len) != 0) {
            fprintf(stderr, "campaign: manifest level path copy failed\n");
            toml_free(parsed);
            return -1;
        }
        level_def_init_defaults(&staged->levels[i].level);
        if (level_load_toml(canonical_path, &staged->levels[i].level) != 0) {
            fprintf(stderr, "campaign: invalid manifest level '%s'\n",
                    item.u.str.ptr);
            toml_free(parsed);
            return -1;
        }
        campaign_derive_display_name(&staged->levels[i]);
        if (!campaign_has_visible_text(staged->levels[i].display_name,
                                       sizeof(staged->levels[i].display_name))) {
            fprintf(stderr, "campaign: level '%s' has no display name\n",
                    item.u.str.ptr);
            toml_free(parsed);
            return -1;
        }
    }

    toml_free(parsed);
    return 0;
}

static int campaign_validate_chain(const CampaignCatalog *catalog)
{
    for (size_t i = 0; i < catalog->count; i++) {
        const LevelDef *level = &catalog->levels[i].level;

        if (i + 1 < catalog->count) {
            if (level->next_phase[0] == '\0' ||
                !campaign_paths_equal(level->next_phase,
                                      catalog->levels[i + 1].path)) {
                fprintf(stderr, "campaign: '%s' next_phase must be '%s'\n",
                        catalog->levels[i].path,
                        catalog->levels[i + 1].path);
                return -1;
            }
        } else if (level->next_phase[0] != '\0') {
            fprintf(stderr, "campaign: final level '%s' must not have next_phase\n",
                    catalog->levels[i].path);
            return -1;
        }
    }
    return 0;
}

int campaign_catalog_load(const char *manifest_path, CampaignCatalog *catalog)
{
    CampaignCatalog staged = {0};

    if (!manifest_path || !catalog) return -1;
    if (campaign_manifest_load_entries(manifest_path, &staged) != 0 ||
        campaign_validate_chain(&staged) != 0) {
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

static LevelDef *game_level_storage(GameState *gs)
{
    if (!gs->level_def) {
        gs->level_def = (LevelDef *)calloc(1, sizeof(LevelDef));
        if (!gs->level_def) {
            fprintf(stderr, "Error: Failed to allocate active level storage\n");
            return NULL;
        }
    }
    gs->runtime.current_level = gs->level_def;
    return (LevelDef *)gs->level_def;
}

static int read_stable_level(const char *path, LevelDef *level, uint64_t *hash)
{
    SerializerFileFingerprint before, after;
    if (serializer_fingerprint_utf8(path, &before) != 1 || level_load_toml(path, level) != 0 ||
        serializer_fingerprint_utf8(path, &after) != 1 || !serializer_fingerprint_equal(&before, &after)) {
        fprintf(stderr, "Error: cannot read stable level bytes: %s\n", path);
        return -1;
    }
    *hash = after.content_hash;
    return 0;
}

int game_level_load_initial(GameState *gs)
{
    LevelDef loaded;
    LevelDef *level;
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

    level_def_init_defaults(&loaded);
    uint64_t source_hash;
    if (read_stable_level(safe_path, &loaded, &source_hash) != 0) {
        fprintf(stderr, "Error: could not load initial level: %s\n", safe_path);
        return -1;
    }

    if (game_resources_require_level_textures(gs, &loaded) != 0) return -1;
    /* Parse and required sprites are checked before replacing active storage. */
    level = game_level_storage(gs);
    if (!level) return -1;
    *level = loaded;
    gs->source_level_hash = source_hash;

    if (level_load(gs, level) != 0) return -1;
    game_completion_reset_summary(gs);
    level_resources_apply(gs, (const LevelDef *)gs->runtime.current_level);
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

    LevelDef next_level;
    level_def_init_defaults(&next_level);

    uint64_t source_hash;
    if (read_stable_level(safe_path, &next_level, &source_hash) != 0) {
        fprintf(stderr, "Error: Failed to load next phase: %s\n", safe_path);
        return -1;
    }

    char err[128];
    if (level_validate_runtime(&next_level, err, sizeof(err)) != 0) {
        fprintf(stderr, "Error: Invalid next phase %s: %s\n", safe_path, err);
        return -1;
    }

    if (game_resources_require_level_textures(gs, &next_level) != 0) return -1;
    game_experiment_cleanup(gs);
    LevelDef *level = game_level_storage(gs);
    if (!level) return -1;
    *level = next_level;
    gs->source_level_hash = source_hash;

    strncpy(gs->level_path, next_path, sizeof(gs->level_path) - 1);
    gs->level_path[sizeof(gs->level_path) - 1] = '\0';

    if (level_load(gs, level) != 0) return -1;
    game_completion_reset_summary(gs);
    level_resources_apply(gs, level);

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
