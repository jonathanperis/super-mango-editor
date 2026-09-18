/*
 * editor_files.c — Editor file, autosave, and recent-file helpers.
 */

#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "editor_files.h"

#include <stdio.h>      /* FILE, fopen, fprintf, stderr */
#include <stdint.h>     /* uint64_t */
#include <time.h>       /* time */
#include <errno.h>      /* errno, ERANGE */
#include <stdlib.h>     /* strtoull */
#include <string.h>     /* memset, strcmp, strlen, strncpy, strrchr */

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#include <windows.h>    /* GetCurrentProcessId */
#include <bcrypt.h>
#else
#include <dirent.h>     /* directory discovery */
#include <unistd.h>     /* getpid */
#endif

#include "editor_session.h"    /* editor status/title/persist helpers */
#include "editor_validation.h" /* editor_validate_level */
#include "file_dialog.h"       /* file_dialog_open */
#include "../shared/serializer.h"    /* shared level format */
#include "../shared/serializer_io.h" /* UTF-8 file I/O */
#include "undo.h"              /* undo_clear */

#define EDITOR_RECENT_MAX    5
#define EDITOR_AUTOSAVE_MS   30000u
#define EDITOR_PREF_ORG      "Super Mango"
#define EDITOR_PREF_APP      "Editor"
#define EDITOR_RECENT_NAME   "editor_recent.txt"
#define EDITOR_RECOVERY_PREFIX "editor_recovery_"

static unsigned long editor_playtest_sequence;
static uint64_t editor_recovery_sequence;
static int editor_recovery_seeded;
static int editor_test_recovery_choice = -1;

void editor_test_set_recovery_choice(int button_id)
{
    editor_test_recovery_choice = button_id;
}

static void editor_save_recent_files(const EditorState *es);
static void editor_add_recent_file(EditorState *es, const char *path);
static void editor_replace_texture(Texture2D **slot,
                                   const char *path);
static int editor_preference_file_path(const EditorState *es, const char *name,
                                       char *buf,
                                       size_t buf_size);
static int editor_preference_root_path(const EditorState *es, char *buf,
                                       size_t buf_size);
static int editor_set_recovery_path(EditorState *es, const char *document_path);
static int editor_make_playtest_path(EditorState *es);
static int editor_path_is_recovery(const char *path);
static int editor_path_is_recovery_metadata(const char *path);
static int editor_path_is_private(const EditorState *es, const char *path);
static int editor_add_recovery_entry(EditorState *es, const char *source_path);
static int editor_remove_recovery_entry(EditorState *es, int entry_index);
static int editor_recovery_entry_valid(const EditorRecoveryEntry *entry);
static uint64_t editor_new_recovery_id(const EditorState *es);
static int editor_recovery_metadata_path(const EditorState *es, uint64_t id,
                                         char *path, size_t path_size);
static int editor_recovery_snapshot_path(const EditorState *es, uint64_t id,
                                         char *path, size_t path_size);
static int editor_write_recovery_metadata(const EditorRecoveryEntry *entry);
static void editor_apply_loaded_level(EditorState *es, const LevelDef *level,
                                      const char *path, int modified,
                                      int add_recent);
static int editor_save_current_level_as_validated(EditorState *es);
static int editor_source_changed(EditorState *es);
static int editor_finalize_save(EditorState *es, const char *path,
                                const char *status_prefix);

int editor_path_fits(const char *path)
{
    return path && strlen(path) < EDITOR_PATH_MAX;
}

int editor_set_preference_root(EditorState *es, const char *root)
{
    if (!es || !root || !editor_path_fits(root) || root[0] == '\0') return -1;
    memcpy(es->preference_root, root, strlen(root) + 1);
    return 0;
}

int editor_init_persistence_paths(EditorState *es)
{
    if (!es) return -1;

    es->autosave_path[0] = '\0';
    es->playtest_path[0] = '\0';
    es->recent_path[0] = '\0';
    es->recovery_root_path[0] = '\0';
    es->recovery_entry_count = 0;
    es->pending_recovery_id = 0;

    if (editor_preference_root_path(es, es->recovery_root_path,
                                    sizeof(es->recovery_root_path)) != 0 ||
        editor_discover_recoveries(es) < 0 ||
        editor_set_recovery_path(es, es->file_path) != 0 ||
        editor_make_playtest_path(es) != 0 ||
        editor_preference_file_path(es, EDITOR_RECENT_NAME,
                                    es->recent_path,
                                    sizeof(es->recent_path)) != 0) {
        es->autosave_path[0] = '\0';
        es->playtest_path[0] = '\0';
        es->recent_path[0] = '\0';
        return -1;
    }

    return 0;
}

int editor_set_recovery_document(EditorState *es, const char *document_path)
{
    if (document_path && !editor_path_fits(document_path)) return -1;
    return editor_set_recovery_path(es, document_path);
}

int editor_load_level(EditorState *es, const char *path)
{
    LevelDef new_level;
    SerializerFileFingerprint fingerprint;
    memset(&new_level, 0, sizeof(new_level));

    if (!es || !editor_path_fits(path)) {
        if (es) editor_set_status(es, "Load failed: path too long");
        return -1;
    }

    if (level_load_toml(path, &new_level) != 0) {
        fprintf(stderr, "Error: failed to load %s\n", path);
        editor_set_status(es, "Load failed: %s", path);
        return -1;
    }

    if (serializer_fingerprint_utf8(path, &fingerprint) != 1) {
        editor_set_status(es, "Load failed: cannot fingerprint %s", path);
        return -1;
    }

    /* The load succeeded; only now retire the old session's recovery. */
    editor_retire_current_recovery(es);
    editor_apply_loaded_level(es, &new_level, path, 0, 1);
    if (editor_set_recovery_path(es, path) != 0) {
        editor_set_status(es, "Load failed: recovery path unavailable");
        return -1;
    }
    es->source_fingerprint = fingerprint;
    es->source_state = EDITOR_SOURCE_EXPECTED_EXISTING;

    fprintf(stderr, "Loaded %s (%d entities)\n", path,
            es->level.coin_count + es->level.spider_count +
            es->level.platform_count + es->level.rail_count +
            es->level.bird_count + es->level.fish_count);
    editor_set_status(es, "Loaded %s", path);
    return 0;
}

static void editor_apply_loaded_level(EditorState *es, const LevelDef *level,
                                      const char *path, int modified,
                                      int add_recent)
{
    es->level = *level;
    if (path) {
        if (!editor_path_fits(path)) return;
        memcpy(es->file_path, path, strlen(path) + 1);
    } else {
        es->file_path[0] = '\0';
    }
    memset(&es->source_fingerprint, 0, sizeof(es->source_fingerprint));
    es->source_state = EDITOR_SOURCE_UNKNOWN;
    undo_clear(es->undo);
    es->selection.index = -1;
    if (modified) {
        editor_set_recovered_dirty(es);
    } else {
        editor_set_document_save_point(es);
    }
    if (add_recent && path && path[0] != '\0') editor_add_recent_file(es, path);

    editor_sync_config_resources(es);

    editor_update_window_title(es);
}

static void editor_replace_texture(Texture2D **slot,
                                   const char *path)
{
    if (!slot) return;
    if (!path || path[0] == '\0') {
        texture_unload(*slot);
        *slot = NULL;
        return;
    }
    if (!IsWindowReady()) return;

    {
        Texture2D *replacement = texture_load(path);
        if (replacement) {
            texture_unload(*slot);
            *slot = replacement;
        } else {
            fprintf(stderr, "Warning: keeping preview texture; cannot load %s\n", path);
        }
    }
}

void editor_sync_config_resources(EditorState *es)
{
    if (!es) return;
    editor_replace_texture(&es->textures.sky,
                           es->level.background_layer_count > 0
                           ? es->level.background_layers[0].path : NULL);
    editor_replace_texture(&es->textures.floor_tile,
                           es->level.floor_tile_path);
    editor_replace_texture(&es->textures.water,
                           es->level.foreground_layer_count > 0
                           ? es->level.foreground_layers[
                                 es->level.foreground_layer_count - 1].path
                           : NULL);
}

void editor_open_level_file(EditorState *es)
{
    char path[EDITOR_PATH_MAX];
    int dialog_result;

    if (!es || !editor_finish_field_edit(es)) return;
    dialog_result = file_dialog_open(path, (int)sizeof(path));
    if (dialog_result == FILE_DIALOG_SELECTED) {
        if (!editor_path_fits(path)) {
            editor_set_status(es, "Open failed: path too long");
        } else {
            (void)editor_load_level(es, path);
        }
    } else if (dialog_result == FILE_DIALOG_CANCELLED) {
        editor_set_status(es, "Open cancelled");
    } else {
        editor_set_status(es, "Open failed: dialog error");
    }
}

int editor_save_current_level(EditorState *es)
{
    int source_status;
    EditorExternalChoice choice;

    if (!es || !editor_finish_field_edit(es)) return -1;
    if (!editor_can_persist(es, "Save")) return -1;

    if (es->file_path[0] == '\0' || editor_path_is_private(es, es->file_path)) {
        return editor_save_current_level_as_validated(es);
    }

    source_status = editor_source_changed(es);
    if (source_status < 0) {
        editor_set_status(es, "Save failed: cannot inspect source file");
        return -1;
    }
    if (source_status > 0) {
        choice = editor_confirm_external_change(es);
        if (choice == EDITOR_EXTERNAL_SAVE_AS) {
            return editor_save_current_level_as_validated(es);
        }
        if (choice != EDITOR_EXTERNAL_REPLACE) return -1;
        {
            SerializerFileFingerprint replace_expected;
            if (serializer_fingerprint_utf8(es->file_path,
                                            &replace_expected) != 1) {
                editor_set_status(es, "Save cancelled: source unavailable");
                return -1;
            }
            /* Recheck occurs immediately before replacement. */
            if (level_save_toml_checked(&es->level, es->file_path,
                                        SERIALIZER_SAVE_REPLACE,
                                        &replace_expected) != 0) {
                editor_set_status(es, "Save failed: %s", es->file_path);
                return -1;
            }
        }
        return editor_finalize_save(es, es->file_path, "Saved");
    }

    if (es->source_state == EDITOR_SOURCE_EXPECTED_MISSING) {
        if (level_save_toml_with_policy(&es->level, es->file_path,
                                        SERIALIZER_SAVE_CREATE_ONLY) != 0) {
            editor_set_status(es, "Save failed: %s", es->file_path);
            return -1;
        }
        return editor_finalize_save(es, es->file_path, "Saved");
    }
    if (es->source_state != EDITOR_SOURCE_EXPECTED_EXISTING ||
        !es->source_fingerprint.valid) {
        editor_set_status(es, "Save failed: source baseline unavailable");
        return -1;
    }
    if (level_save_toml_checked(&es->level, es->file_path,
                                SERIALIZER_SAVE_REPLACE,
                                &es->source_fingerprint) != 0) {
        editor_set_status(es, "Save failed: %s", es->file_path);
        return -1;
    }
    return editor_finalize_save(es, es->file_path, "Saved");
}

static int editor_save_current_level_as_validated(EditorState *es)
{
    char path[EDITOR_PATH_MAX];
    int dialog_result;
    SerializerPathStatus target_status;

    dialog_result = file_dialog_save(path, (int)sizeof(path));
    if (dialog_result == FILE_DIALOG_CANCELLED) {
        editor_set_status(es, "Save cancelled");
        return -1;
    }
    if (dialog_result == FILE_DIALOG_ERROR) {
        editor_set_status(es, "Save failed: dialog error");
        return -1;
    }
    if (editor_path_is_private(es, path)) {
        editor_set_status(es, "Save failed: private editor path");
        return -1;
    }
    if (!editor_path_fits(path)) {
        editor_set_status(es, "Save failed: path too long");
        return -1;
    }
    target_status = serializer_probe_path_utf8(path);
    if (target_status == SERIALIZER_PATH_ERROR) {
        editor_set_status(es, "Save failed: cannot inspect destination");
        return -1;
    }
    if (target_status == SERIALIZER_PATH_EXISTING &&
        !editor_confirm_overwrite(es, path)) {
        editor_set_status(es, "Save cancelled");
        return -1;
    }

    if (target_status == SERIALIZER_PATH_EXISTING) {
        SerializerFileFingerprint baseline;
        if (serializer_fingerprint_utf8(path, &baseline) != 1 ||
            level_save_toml_checked(&es->level, path,
                                    SERIALIZER_SAVE_REPLACE, &baseline) != 0) {
            editor_set_status(es, "Save failed: %s", path);
            return -1;
        }
    } else if (level_save_toml_with_policy(&es->level, path,
                                           SERIALIZER_SAVE_CREATE_ONLY) != 0) {
        fprintf(stderr, "Error: failed to save %s\n", path);
        editor_set_status(es, "Save failed: %s", path);
        return -1;
    }

    memcpy(es->file_path, path, strlen(path) + 1);
    editor_retire_current_recovery(es);
    if (editor_set_recovery_path(es, es->file_path) != 0) {
        editor_set_status(es, "Saved but recovery path unavailable");
        return -1;
    }
    if (serializer_fingerprint_utf8(es->file_path, &es->source_fingerprint) != 1) {
        memset(&es->source_fingerprint, 0, sizeof(es->source_fingerprint));
        es->source_state = EDITOR_SOURCE_UNKNOWN;
    } else {
        es->source_state = EDITOR_SOURCE_EXPECTED_EXISTING;
    }
    editor_set_document_save_point(es);
    editor_add_recent_file(es, es->file_path);
    editor_set_status(es, "Saved as %s", es->file_path);
    return 0;
}

int editor_save_current_level_as(EditorState *es)
{
    if (!es || !editor_finish_field_edit(es)) return -1;
    if (!editor_can_persist(es, "Save")) return -1;
    return editor_save_current_level_as_validated(es);
}

static int editor_source_changed(EditorState *es)
{
    SerializerFileFingerprint actual;
    int result;

    if (!es || es->file_path[0] == '\0') return -1;
    if (es->source_state == EDITOR_SOURCE_UNKNOWN) return 1;
    if (es->source_state == EDITOR_SOURCE_EXPECTED_MISSING) {
        result = serializer_probe_path_utf8(es->file_path);
        if (result == SERIALIZER_PATH_ERROR) return -1;
        return result == SERIALIZER_PATH_EXISTING ? 1 : 0;
    }
    if (!es->source_fingerprint.valid) return -1;
    result = serializer_fingerprint_utf8(es->file_path, &actual);
    if (result < 0) return -1;
    if (result == 0 || !serializer_fingerprint_equal(&es->source_fingerprint,
                                                     &actual)) return 1;
    return 0;
}

static int editor_finalize_save(EditorState *es, const char *path,
                                const char *status_prefix)
{
    if (!es || !path || !editor_path_fits(path)) return -1;
    editor_retire_current_recovery(es);
    if (editor_set_recovery_path(es, path) != 0) {
        editor_set_status(es, "Saved but recovery path unavailable");
        return -1;
    }
    if (serializer_fingerprint_utf8(path, &es->source_fingerprint) != 1) {
        memset(&es->source_fingerprint, 0, sizeof(es->source_fingerprint));
        es->source_state = EDITOR_SOURCE_UNKNOWN;
    } else {
        es->source_state = EDITOR_SOURCE_EXPECTED_EXISTING;
    }
    editor_set_document_save_point(es);
    editor_add_recent_file(es, path);
    editor_set_status(es, "%s %s", status_prefix, path);
    return 0;
}

int editor_file_exists(const char *path)
{
    return serializer_file_exists_utf8(path);
}

void editor_retire_matching_recovery(EditorState *es, const char *destination)
{
    if (!es || !destination || !editor_path_fits(destination)) return;
    if (es->recovery_root_path[0] != '\0' &&
        editor_discover_recoveries(es) != 0) return;
    for (int i = 0; i < es->recovery_entry_count; i++) {
        EditorRecoveryEntry *entry = &es->recovery_entries[i];
        if (entry->id == es->recovery_document_id &&
            strcmp(entry->source_path, destination) == 0) {
            if (editor_remove_recovery_entry(es, i) == 0)
                es->recovery_original_path[0] = '\0';
            return;
        }
    }

    /* Legacy direct recovery paths are only used by isolated callers. */
    if (es->recovery_root_path[0] == '\0' && es->autosave_path[0] != '\0' &&
        editor_file_exists(es->autosave_path) &&
        level_read_recovery_path(es->autosave_path, es->recovery_original_path,
                                 sizeof(es->recovery_original_path)) == 1 &&
        strcmp(es->recovery_original_path, destination) == 0) {
        (void)serializer_remove_utf8(es->autosave_path);
        es->recovery_original_path[0] = '\0';
    }
}

void editor_retire_current_recovery(EditorState *es)
{
    if (!es || es->autosave_path[0] == '\0') return;
    if (es->recovery_root_path[0] != '\0' &&
        editor_discover_recoveries(es) != 0) return;
    for (int i = 0; i < es->recovery_entry_count; i++) {
        if (es->recovery_entries[i].id == es->recovery_document_id ||
            strcmp(es->recovery_entries[i].snapshot_path,
                   es->autosave_path) == 0) {
            if (editor_remove_recovery_entry(es, i) == 0)
                es->recovery_original_path[0] = '\0';
            return;
        }
    }
    if (es->recovery_root_path[0] == '\0') {
        (void)serializer_remove_utf8(es->autosave_path);
        es->recovery_original_path[0] = '\0';
    }
}

void editor_maybe_autosave(EditorState *es)
{
    uint32_t now;

    if (!es || !es->modified) return;
    now = (uint32_t)clock_millis();
    if (now - es->last_autosave_ms < EDITOR_AUTOSAVE_MS) return;

    editor_validate_level(&es->level, &es->validation_report);
    if (es->validation_report.error_count > 0) {
        editor_set_status(es, "Autosave skipped: level has validation errors");
        return;
    }

    if (es->autosave_path[0] != '\0' &&
        level_save_toml_recovery(&es->level, es->autosave_path,
                                 es->file_path) == 0 &&
        editor_add_recovery_entry(es, es->file_path) == 0) {
        es->last_autosave_ms = now;
        memcpy(es->recovery_original_path, es->file_path,
               strlen(es->file_path) + 1);
        editor_set_status(es, "Autosaved recovery copy");
    } else {
        editor_set_status(es, "Autosave failed");
    }
}

int editor_recover_autosave(EditorState *es)
{
    if (es && es->recovery_root_path[0] != '\0' &&
        editor_discover_recoveries(es) != 0) {
        editor_set_status(es, "Recovery discovery failed");
        return -1;
    }
    if (!es || es->autosave_path[0] == '\0' ||
        !editor_file_exists(es->autosave_path)) {
        editor_set_status(es, "Recovery copy not found");
        return -1;
    }
    for (int i = 0; i < es->recovery_entry_count; i++) {
        if (strcmp(es->recovery_entries[i].snapshot_path,
                   es->autosave_path) == 0)
            return editor_recover_entry_by_id(es, es->recovery_entries[i].id);
    }

    /* Legacy recovery file without an index entry. */
    {
        LevelDef recovered;
        char destination[EDITOR_PATH_MAX];
        memset(&recovered, 0, sizeof(recovered));
        if (level_load_toml(es->autosave_path, &recovered) != 0) {
            editor_set_status(es, "Recovery failed");
            return -1;
        }
        destination[0] = '\0';
        if (level_read_recovery_path(es->autosave_path, destination,
                                     sizeof(destination)) != 1 ||
            !editor_path_fits(destination)) destination[0] = '\0';
        editor_apply_loaded_level(es, &recovered,
                                  destination[0] != '\0' ? destination : NULL,
                                  1, 0);
        memcpy(es->recovery_original_path, destination,
               strlen(destination) + 1);
        editor_set_recovered_dirty(es);
        editor_set_status(es, "Recovered unsaved changes");
        return 0;
    }
}

static int recovery_hex_value(unsigned char ch)
{
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

static int recovery_hex_encode(const char *source, char *out, size_t out_size)
{
    static const char digits[] = "0123456789abcdef";
    size_t length;

    if (!source || !out || out_size == 0) return -1;
    length = strlen(source);
    if (length == 0) {
        if (out_size < 2) return -1;
        out[0] = '-';
        out[1] = '\0';
        return 0;
    }
    if (length > (out_size - 1) / 2) return -1;
    for (size_t i = 0; i < length; i++) {
        unsigned char ch = (unsigned char)source[i];
        out[i * 2] = digits[ch >> 4];
        out[i * 2 + 1] = digits[ch & 15];
    }
    out[length * 2] = '\0';
    return 0;
}

static int recovery_hex_decode(const char *encoded, char *out, size_t out_size)
{
    size_t length;

    if (!encoded || !out || out_size == 0) return -1;
    if (strcmp(encoded, "-") == 0) {
        out[0] = '\0';
        return 0;
    }
    length = strlen(encoded);
    if (length == 0 || (length & 1) != 0 || length / 2 >= out_size) return -1;
    for (size_t i = 0; i < length; i += 2) {
        int high = recovery_hex_value((unsigned char)encoded[i]);
        int low = recovery_hex_value((unsigned char)encoded[i + 1]);
        if (high < 0 || low < 0 || (high == 0 && low == 0)) return -1;
        out[i / 2] = (char)((high << 4) | low);
    }
    out[length / 2] = '\0';
    return 0;
}

static int editor_recovery_entry_valid(const EditorRecoveryEntry *entry)
{
    return entry && entry->id != 0 && entry->timestamp != 0 &&
           editor_path_fits(entry->source_path) &&
           editor_path_fits(entry->snapshot_path) &&
           editor_path_fits(entry->metadata_path) &&
           editor_path_is_recovery(entry->snapshot_path) &&
           editor_path_is_recovery_metadata(entry->metadata_path) &&
           entry->snapshot_path[0] != '\0';
}

static int editor_write_recovery_metadata(const EditorRecoveryEntry *entry)
{
    char temp_path[SERIALIZER_IO_PATH_MAX];
    char source_hex[EDITOR_PATH_MAX * 2];
    FILE *fp;

    if (!editor_recovery_entry_valid(entry) ||
        recovery_hex_encode(entry->source_path, source_hex,
                            sizeof(source_hex)) != 0 ||
        serializer_make_temp_path(entry->metadata_path,
                                  temp_path, sizeof(temp_path)) != 0) return -1;
    fp = serializer_open_temp(entry->metadata_path, temp_path,
                              sizeof(temp_path));
    if (!fp) return -1;
    if (fprintf(fp, "1\t%016llx\t%llu\t%s\n",
                (unsigned long long)entry->id,
                (unsigned long long)entry->timestamp, source_hex) < 0) {
        fclose(fp);
        serializer_remove_temp(temp_path);
        return -1;
    }
    {
        int stream_error = serializer_stream_has_error(fp);
        int flush_error = stream_error ? -1 : serializer_flush(fp);
        int close_error = fclose(fp);
        int replace_error = (stream_error || flush_error || close_error)
                          ? -1
                          : serializer_replace_file(temp_path,
                                                    entry->metadata_path);
        if (stream_error || flush_error || close_error || replace_error) {
            serializer_remove_temp(temp_path);
            return -1;
        }
    }
    return 0;
}

static int editor_recovery_name_id(const char *name, uint64_t *id,
                                   const char *suffix)
{
    char *end;
    unsigned long long parsed;
    size_t suffix_len;
    size_t name_len;

    if (!name || !id || !suffix) return -1;
    suffix_len = strlen(suffix);
    name_len = strlen(name);
    if (name_len != strlen(EDITOR_RECOVERY_PREFIX) + 16 + suffix_len ||
        strncmp(name, EDITOR_RECOVERY_PREFIX,
                strlen(EDITOR_RECOVERY_PREFIX)) != 0 ||
        strcmp(name + name_len - suffix_len, suffix) != 0) return -1;
    for (size_t i = 0; i < 16; i++) {
        unsigned char ch = (unsigned char)name[strlen(EDITOR_RECOVERY_PREFIX) + i];
        if (!((ch >= '0' && ch <= '9') ||
              (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'))) return -1;
    }
    errno = 0;
    parsed = strtoull(name + strlen(EDITOR_RECOVERY_PREFIX), &end, 16);
    if (errno == ERANGE || parsed == 0 || end != name + name_len - suffix_len)
        return -1;
    *id = (uint64_t)parsed;
    return 0;
}

static int editor_read_recovery_metadata(const EditorState *es,
                                         const char *name,
                                         EditorRecoveryEntry *entry)
{
    char metadata_path[EDITOR_PATH_MAX];
    char snapshot_path[EDITOR_PATH_MAX];
    char line[EDITOR_PATH_MAX * 2];
    char *version;
    char *id_text;
    char *timestamp_text;
    char *source_text;
    char *end;
    uint64_t id;
    unsigned long long timestamp;
    FILE *fp;

    if (!es || !name || !entry ||
        editor_recovery_name_id(name, &id, ".meta") != 0 ||
        editor_recovery_metadata_path(es, id, metadata_path,
                                      sizeof(metadata_path)) != 0 ||
        editor_recovery_snapshot_path(es, id, snapshot_path,
                                      sizeof(snapshot_path)) != 0) return 0;

    fp = serializer_fopen_utf8(metadata_path, "rb");
    if (!fp) {
        SerializerPathStatus status = serializer_probe_path_utf8(metadata_path);
        return status == SERIALIZER_PATH_MISSING ? 0 : -1;
    }
    if (!fgets(line, sizeof(line), fp) || strchr(line, '\n') == NULL ||
        fgets(line, sizeof(line), fp) != NULL || ferror(fp)) {
        fclose(fp);
        return 0;
    }
    fclose(fp);
    line[strcspn(line, "\r\n")] = '\0';
    version = strtok(line, "\t");
    id_text = strtok(NULL, "\t");
    timestamp_text = strtok(NULL, "\t");
    source_text = strtok(NULL, "\t");
    if (!version || !id_text || !timestamp_text || !source_text ||
        strtok(NULL, "\t") || strcmp(version, "1") != 0) return 0;
    errno = 0;
    id = strtoull(id_text, &end, 16);
    if (errno == ERANGE || id == 0 || end == id_text || *end != '\0') return 0;
    errno = 0;
    timestamp = strtoull(timestamp_text, &end, 10);
    if (errno == ERANGE || timestamp == 0 || end == timestamp_text ||
        *end != '\0') return 0;

    memset(entry, 0, sizeof(*entry));
    entry->id = (uint64_t)id;
    entry->timestamp = (uint64_t)timestamp;
    if (recovery_hex_decode(source_text, entry->source_path,
                            sizeof(entry->source_path)) != 0) return 0;
    memcpy(entry->snapshot_path, snapshot_path, strlen(snapshot_path) + 1);
    memcpy(entry->metadata_path, metadata_path, strlen(metadata_path) + 1);
    if (!editor_recovery_entry_valid(entry)) return 0;
    {
        SerializerPathStatus status = serializer_probe_path_utf8(entry->snapshot_path);
        if (status == SERIALIZER_PATH_ERROR) return -1;
        if (status != SERIALIZER_PATH_EXISTING) return 0;
    }
    return 1;
}

typedef struct {
    const EditorState *es;
    EditorRecoveryEntry entries[EDITOR_MAX_RECOVERY_ENTRIES];
    int count;
    int failed;
} EditorRecoveryDiscovery;

static int editor_discover_recovery_name(const char *name, void *context)
{
    EditorRecoveryDiscovery *discovery = (EditorRecoveryDiscovery *)context;
    EditorRecoveryEntry entry;
    int result;

    if (!name || !discovery) return -1;
    result = editor_read_recovery_metadata(discovery->es, name, &entry);
    if (result < 0) {
        discovery->failed = 1;
        return -1;
    }
    if (result == 0 || discovery->count >= EDITOR_MAX_RECOVERY_ENTRIES) return 0;
    for (int i = 0; i < discovery->count; i++) {
        if (discovery->entries[i].id == entry.id) return 0;
    }
    discovery->entries[discovery->count++] = entry;
    return 0;
}

static int editor_for_each_recovery_name(const EditorState *es,
                                         int (*callback)(const char *, void *),
                                         void *context)
{
    if (!es || !callback || es->recovery_root_path[0] == '\0') return -1;
#ifdef _WIN32
    {
        char pattern[EDITOR_PATH_MAX];
        wchar_t *wide_pattern;
        char *name;
        WIN32_FIND_DATAW data;
        HANDLE handle;
        int written = snprintf(pattern, sizeof(pattern), "%s\\%s*.meta",
                               es->recovery_root_path, EDITOR_RECOVERY_PREFIX);
        if (written < 0 || (size_t)written >= sizeof(pattern)) return -1;
        wide_pattern = serializer_utf8_to_wide(pattern);
        if (!wide_pattern) return -1;
        handle = FindFirstFileW(wide_pattern, &data);
        free(wide_pattern);
        if (handle == INVALID_HANDLE_VALUE) {
            DWORD error = GetLastError();
            return (error == ERROR_FILE_NOT_FOUND ||
                    error == ERROR_PATH_NOT_FOUND) ? 0 : -1;
        }
        do {
            if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                name = serializer_wide_to_utf8(data.cFileName);
                if (!name) {
                    FindClose(handle);
                    return -1;
                }
                if (callback(name, context) != 0) {
                    free(name);
                    FindClose(handle);
                    return -1;
                }
                free(name);
            }
        } while (FindNextFileW(handle, &data));
        {
            DWORD error = GetLastError();
            FindClose(handle);
            return error == ERROR_NO_MORE_FILES ? 0 : -1;
        }
    }
#else
    {
        DIR *directory = opendir(es->recovery_root_path);
        struct dirent *item;
        int read_error;
        if (!directory) return errno == ENOENT ? 0 : -1;
        for (;;) {
            /* Callbacks probe files and can set errno for benign stale entries.
             * Only errno from this particular readdir belongs to enumeration. */
            errno = 0;
            item = readdir(directory);
            if (!item) { read_error = errno; break; }
            if (callback(item->d_name, context) != 0) {
                closedir(directory);
                return -1;
            }
        }
        if (read_error != 0) {
            closedir(directory);
            return -1;
        }
        closedir(directory);
        return 0;
    }
#endif
}

int editor_discover_recoveries(EditorState *es)
{
    EditorRecoveryDiscovery discovery;

    if (!es || es->recovery_root_path[0] == '\0') return -1;
    memset(&discovery, 0, sizeof(discovery));
    discovery.es = es;
    if (editor_for_each_recovery_name(es, editor_discover_recovery_name,
                                      &discovery) != 0 || discovery.failed)
        return -1;
    memcpy(es->recovery_entries, discovery.entries, sizeof(discovery.entries));
    es->recovery_entry_count = discovery.count;
    return 0;
}

static int editor_add_recovery_entry(EditorState *es, const char *source_path)
{
    EditorRecoveryEntry entry;

    if (!es || !source_path || !editor_path_fits(source_path) ||
        !editor_path_fits(es->autosave_path)) return -1;
    if (es->recovery_root_path[0] == '\0') return 0;
    if (editor_discover_recoveries(es) != 0) return -1;
    if (es->recovery_entry_count >= EDITOR_MAX_RECOVERY_ENTRIES) {
        int current_entry = 0;
        for (int i = 0; i < es->recovery_entry_count; i++) {
            if (es->recovery_entries[i].id == es->recovery_document_id) {
                current_entry = 1;
                break;
            }
        }
        if (!current_entry) return -1;
    }
    memset(&entry, 0, sizeof(entry));
    entry.id = es->recovery_document_id;
    entry.timestamp = (uint64_t)time(NULL);
    memcpy(entry.source_path, source_path, strlen(source_path) + 1);
    memcpy(entry.snapshot_path, es->autosave_path,
           strlen(es->autosave_path) + 1);
    if (editor_recovery_metadata_path(es, entry.id, entry.metadata_path,
                                      sizeof(entry.metadata_path)) != 0 ||
        editor_write_recovery_metadata(&entry) != 0) return -1;
    if (editor_discover_recoveries(es) != 0) return -1;
    for (int i = 0; i < es->recovery_entry_count; i++) {
        if (es->recovery_entries[i].id == entry.id) return 0;
    }
    return -1;
}

static int editor_remove_recovery_entry(EditorState *es, int entry_index)
{
    EditorRecoveryEntry target;
    int current_index = -1;

    if (!es || entry_index < 0 || entry_index >= es->recovery_entry_count) return -1;
    target = es->recovery_entries[entry_index];
    if (es->recovery_root_path[0] == '\0') {
        (void)serializer_remove_utf8(target.snapshot_path);
        return 0;
    }
    if (editor_discover_recoveries(es) != 0) return -1;
    for (int i = 0; i < es->recovery_entry_count; i++) {
        if (es->recovery_entries[i].id == target.id) {
            current_index = i;
            break;
        }
    }
    if (current_index < 0) return 0;
    target = es->recovery_entries[current_index];
    {
        SerializerPathStatus metadata_status =
            serializer_probe_path_utf8(target.metadata_path);
        SerializerPathStatus snapshot_status =
            serializer_probe_path_utf8(target.snapshot_path);
        if (metadata_status == SERIALIZER_PATH_ERROR ||
            snapshot_status == SERIALIZER_PATH_ERROR) return -1;
        if (metadata_status == SERIALIZER_PATH_EXISTING &&
            serializer_remove_utf8(target.metadata_path) != 0) return -1;
        if (snapshot_status == SERIALIZER_PATH_EXISTING &&
            serializer_remove_utf8(target.snapshot_path) != 0) return -1;
    }
    return editor_discover_recoveries(es);
}

int editor_recover_entry(EditorState *es, int entry_index)
{
    uint64_t recovery_id;

    if (!es) return -1;
    recovery_id = es->pending_recovery_id;
    if (recovery_id == 0) {
        if (entry_index < 0 || entry_index >= es->recovery_entry_count) return -1;
        recovery_id = es->recovery_entries[entry_index].id;
    }
    return editor_recover_entry_by_id(es, recovery_id);
}

int editor_recover_entry_by_id(EditorState *es, uint64_t recovery_id)
{
    EditorRecoveryEntry entry;
    LevelDef recovered;
    int old_entry = -1;
    int entry_index = -1;

    if (!es || recovery_id == 0) return -1;
    if (es->recovery_root_path[0] != '\0' &&
        editor_discover_recoveries(es) != 0) {
        editor_set_status(es, "Recovery discovery failed");
        return -1;
    }
    for (int i = 0; i < es->recovery_entry_count; i++) {
        if (es->recovery_entries[i].id == recovery_id) {
            entry_index = i;
            break;
        }
    }
    if (entry_index < 0) {
        editor_set_status(es, "Recovery copy no longer exists");
        return -1;
    }
    entry = es->recovery_entries[entry_index];
    if (!editor_recovery_entry_valid(&entry) ||
        !editor_path_fits(entry.source_path) ||
        level_load_toml(entry.snapshot_path, &recovered) != 0) {
        editor_set_status(es, "Recovery failed");
        return -1;
    }
    if (es->autosave_path[0] != '\0') {
        for (int i = 0; i < es->recovery_entry_count; i++) {
            if (es->recovery_entries[i].id == es->recovery_document_id ||
                strcmp(es->recovery_entries[i].snapshot_path,
                       es->autosave_path) == 0) {
                old_entry = i;
                break;
            }
        }
    }
    if (old_entry >= 0 &&
        strcmp(es->recovery_entries[old_entry].snapshot_path,
               entry.snapshot_path) != 0) {
        if (editor_remove_recovery_entry(es, old_entry) != 0) {
            editor_set_status(es, "Recovery retirement blocked");
            return -1;
        }
    }

    editor_apply_loaded_level(es, &recovered,
                              entry.source_path[0] ? entry.source_path : NULL,
                              1, 0);
    es->recovery_document_id = entry.id;
    es->pending_recovery_id = 0;
    memcpy(es->autosave_path, entry.snapshot_path,
           strlen(entry.snapshot_path) + 1);
    memcpy(es->recovery_original_path, entry.source_path,
           strlen(entry.source_path) + 1);
    memset(&es->source_fingerprint, 0, sizeof(es->source_fingerprint));
    es->source_state = EDITOR_SOURCE_UNKNOWN;
    editor_set_recovered_dirty(es);
    editor_set_status(es, "Recovered unsaved changes");
    return 0;
}

int editor_choose_recovery(EditorState *es)
{
    int index = 0;

    if (!es) return -1;
    if (es->recovery_root_path[0] != '\0' && editor_discover_recoveries(es) != 0) {
        editor_set_status(es, "Recovery discovery failed");
        return -1;
    }
    if (es->recovery_entry_count == 0) return -1;
    for (;;) {
        const char *buttons[] = {"Cancel", "Recover", "Next"};
        char message[EDITOR_PATH_MAX + 128];
        char timestamp[64] = "unknown time";
        time_t raw_time = (time_t)es->recovery_entries[index].timestamp;
        struct tm time_value;
#ifdef _WIN32
        int time_valid = localtime_s(&time_value, &raw_time) == 0;
#else
        int time_valid = localtime_r(&raw_time, &time_value) != NULL;
#endif
        const char *source = es->recovery_entries[index].source_path[0]
                           ? es->recovery_entries[index].source_path
                           : "(untitled)";
        if (time_valid) strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S",
                                 &time_value);
        snprintf(message, sizeof(message), "Source: %s\nTimestamp: %s",
                 source, timestamp);
        {
            int button_id = 0;
            if (editor_test_recovery_choice >= 0) {
                button_id = editor_test_recovery_choice;
                editor_test_recovery_choice = -1;
            } else if (dialog_choice("Recover Editor Snapshot", message, buttons, 3, 1, 0, &button_id) != 0) {
                editor_set_status(es, "Recovery cancelled");
                return -1;
            }
            if (button_id == 0) {
                es->pending_recovery_id = 0;
                editor_set_status(es, "Recovery cancelled");
                return -1;
            }
            if (button_id == 1) {
                es->pending_recovery_id = es->recovery_entries[index].id;
                return index;
            }
            index = (index + 1) % es->recovery_entry_count;
        }
    }
}

int editor_prepare_playtest_level(EditorState *es, char *path, size_t path_size)
{
    if (!es || !path || path_size == 0 ||
        !editor_can_persist(es, "Playtest")) {
        if (es) editor_set_status(es, "Play failed: no private destination");
        return -1;
    }

    if (es->playtest_path[0] == '\0' && editor_make_playtest_path(es) != 0) {
        editor_set_status(es, "Play failed: no private destination");
        return -1;
    }
    if (es->playtest_path[0] == '\0' ||
        strcmp(es->playtest_path, es->file_path) == 0 ||
        strcmp(es->playtest_path, "levels/_playtest.toml") == 0) {
        editor_set_status(es, "Play failed: no private destination");
        return -1;
    }
    if (strlen(es->playtest_path) >= path_size) {
        editor_set_status(es, "Play failed: path too long");
        return -1;
    }
    if (level_save_toml(&es->level, es->playtest_path) != 0) {
        editor_set_status(es, "Play failed: save temporary level");
        return -1;
    }
    strcpy(path, es->playtest_path);
    return 0;
}

void editor_retire_playtest_level(EditorState *es)
{
    if (!es || es->playtest_path[0] == '\0') return;
    (void)serializer_remove_utf8(es->playtest_path);
    es->playtest_path[0] = '\0';
}

void editor_load_recent_files(EditorState *es)
{
    FILE *fp;
    char line[EDITOR_PATH_MAX + 1];

    if (!es) return;
    if (es->recent_path[0] == '\0' &&
        editor_preference_file_path(es, EDITOR_RECENT_NAME, es->recent_path,
                                    sizeof(es->recent_path)) != 0) {
        es->recent_file_count = 0;
        return;
    }

    es->recent_file_count = 0;
    fp = serializer_fopen_utf8(es->recent_path, "rb");
    if (!fp) return;

    while (es->recent_file_count < EDITOR_RECENT_MAX &&
           fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        if (!strchr(line, '\n')) {
            int ch = fgetc(fp);
            if (ch != EOF) {
                while ((ch = fgetc(fp)) != '\n' && ch != EOF) { }
                continue;
            }
        }
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        if (line[0] == '\0' || !editor_path_fits(line)) continue;
        memcpy(es->recent_files[es->recent_file_count], line, len + 1);
        es->recent_file_count++;
    }
    fclose(fp);
}

static void editor_save_recent_files(const EditorState *es)
{
    FILE *fp;

    if (!es || es->recent_path[0] == '\0') return;
    fp = serializer_fopen_utf8(es->recent_path, "wb");
    if (!fp) return;

    for (int i = 0; i < es->recent_file_count; i++) {
        fprintf(fp, "%s\n", es->recent_files[i]);
    }
    fclose(fp);
}

static void editor_add_recent_file(EditorState *es, const char *path)
{
    int existing = -1;

    if (!path || path[0] == '\0' || !editor_path_fits(path) ||
        editor_path_is_private(es, path)) return;
    for (int i = 0; i < es->recent_file_count; i++) {
        if (strcmp(es->recent_files[i], path) == 0) {
            existing = i;
            break;
        }
    }

    if (existing > 0) {
        char tmp[EDITOR_PATH_MAX];
        strncpy(tmp, es->recent_files[existing], sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';
        for (int i = existing; i > 0; i--) {
            strncpy(es->recent_files[i], es->recent_files[i - 1],
                    sizeof(es->recent_files[i]) - 1);
            es->recent_files[i][sizeof(es->recent_files[i]) - 1] = '\0';
        }
        strncpy(es->recent_files[0], tmp, sizeof(es->recent_files[0]) - 1);
        es->recent_files[0][sizeof(es->recent_files[0]) - 1] = '\0';
    } else if (existing < 0) {
        int limit = es->recent_file_count < EDITOR_RECENT_MAX
                  ? es->recent_file_count : EDITOR_RECENT_MAX - 1;
        for (int i = limit; i > 0; i--) {
            strncpy(es->recent_files[i], es->recent_files[i - 1],
                    sizeof(es->recent_files[i]) - 1);
            es->recent_files[i][sizeof(es->recent_files[i]) - 1] = '\0';
        }
        strncpy(es->recent_files[0], path, sizeof(es->recent_files[0]) - 1);
        es->recent_files[0][sizeof(es->recent_files[0]) - 1] = '\0';
        if (es->recent_file_count < EDITOR_RECENT_MAX) es->recent_file_count++;
    }

    editor_save_recent_files(es);
}

static int editor_preference_root_path(const EditorState *es, char *buf,
                                       size_t buf_size)
{
    char *pref_path;

    if (!es || !buf || buf_size == 0) return -1;
    buf[0] = '\0';
    if (es->preference_root[0] != '\0') {
        if (!editor_path_fits(es->preference_root)) return -1;
        memcpy(buf, es->preference_root, strlen(es->preference_root) + 1);
        return 0;
    }
    pref_path = preference_path(EDITOR_PREF_ORG, EDITOR_PREF_APP);
    if (!pref_path) return -1;
    if (strlen(pref_path) >= buf_size) {
        free(pref_path);
        return -1;
    }
    memcpy(buf, pref_path, strlen(pref_path) + 1);
    free(pref_path);
    return 0;
}

static int editor_preference_file_path(const EditorState *es, const char *name,
                                       char *buf,
                                        size_t buf_size)
{
    char pref_path[EDITOR_PATH_MAX];
    size_t length;
    int written;

    if (!es || !name || !buf || buf_size == 0 ||
        editor_preference_root_path(es, pref_path, sizeof(pref_path)) != 0) return -1;
    buf[0] = '\0';

    length = strlen(pref_path);
    if (length > 0 &&
        (pref_path[length - 1] == '/' || pref_path[length - 1] == '\\')) {
        written = snprintf(buf, buf_size, "%s%s", pref_path, name);
    } else {
#ifdef _WIN32
        written = snprintf(buf, buf_size, "%s\\%s", pref_path, name);
#else
        written = snprintf(buf, buf_size, "%s/%s", pref_path, name);
#endif
    }
    if (written < 0 || (size_t)written >= buf_size) {
        buf[0] = '\0';
        return -1;
    }
    return 0;
}

static uint64_t editor_new_recovery_id(const EditorState *es)
{
    uint64_t id;
    /* Seed once, then allocate monotonically within this process. Mixing a
     * changing clock with an incrementing counter using XOR can repeat an ID
     * even for consecutive documents that have not written snapshots yet. */
    if (!editor_recovery_seeded) {
#ifdef _WIN32
        if (BCryptGenRandom(NULL, (PUCHAR)&editor_recovery_sequence,
                            sizeof(editor_recovery_sequence), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) return 0;
#else
        FILE *random = fopen("/dev/urandom", "rb");
        if (!random) return 0;
        int read_ok = fread(&editor_recovery_sequence, sizeof(editor_recovery_sequence), 1, random) == 1;
        fclose(random);
        if (!read_ok) return 0;
#endif
        editor_recovery_seeded = 1;
    }
    do {
        id = ++editor_recovery_sequence;
        for (int i = 0; es && i < es->recovery_entry_count; i++) {
            if (es->recovery_entries[i].id == id) {
                id = 0;
                break;
            }
        }
    } while (id == 0);
    return id;
}

static int editor_set_recovery_path(EditorState *es, const char *document_path)
{
    char name[80];
    int written;

    if (!es || (document_path && !editor_path_fits(document_path))) return -1;
    if (es->recovery_root_path[0] == '\0' &&
        editor_preference_root_path(es, es->recovery_root_path,
                                    sizeof(es->recovery_root_path)) != 0) return -1;
    es->recovery_document_id = editor_new_recovery_id(es);
    if (!es->recovery_document_id) return -1;
    written = snprintf(name, sizeof(name), "editor_recovery_%016llx.toml",
                       (unsigned long long)es->recovery_document_id);
    if (written < 0 || (size_t)written >= sizeof(name) ||
        editor_preference_file_path(es, name, es->autosave_path,
                                     sizeof(es->autosave_path)) != 0) {
        es->autosave_path[0] = '\0';
        return -1;
    }
    es->recovery_original_path[0] = '\0';
    return 0;
}

static int editor_recovery_metadata_path(const EditorState *es, uint64_t id,
                                         char *path, size_t path_size)
{
    char name[80];
    int written;

    if (!es || !path || path_size == 0 || es->recovery_root_path[0] == '\0')
        return -1;
    written = snprintf(name, sizeof(name), "%s%016llx.meta",
                       EDITOR_RECOVERY_PREFIX, (unsigned long long)id);
    if (written < 0 || (size_t)written >= sizeof(name)) return -1;
    return editor_preference_file_path(es, name, path, path_size);
}

static int editor_recovery_snapshot_path(const EditorState *es, uint64_t id,
                                         char *path, size_t path_size)
{
    char name[80];
    int written;

    if (!es || !path || path_size == 0 || es->recovery_root_path[0] == '\0')
        return -1;
    written = snprintf(name, sizeof(name), "%s%016llx.toml",
                       EDITOR_RECOVERY_PREFIX, (unsigned long long)id);
    if (written < 0 || (size_t)written >= sizeof(name)) return -1;
    return editor_preference_file_path(es, name, path, path_size);
}

static int editor_make_playtest_path(EditorState *es)
{
    char name[96];
    unsigned long process_id;

    if (!es) return -1;
#ifdef _WIN32
    process_id = (unsigned long)GetCurrentProcessId();
#else
    process_id = (unsigned long)getpid();
#endif

    for (int attempt = 0; attempt < 100; attempt++) {
        int written;
        editor_playtest_sequence++;
        written = snprintf(name, sizeof(name),
                           "editor_playtest_%lu_%u_%lu.toml",
                           process_id, (unsigned)clock_millis(),
                           editor_playtest_sequence);
        if (written < 0 || (size_t)written >= sizeof(name)) return -1;
        if (editor_preference_file_path(es, name, es->playtest_path,
                                         sizeof(es->playtest_path)) != 0) {
            es->playtest_path[0] = '\0';
            return -1;
        }
        if (!editor_file_exists(es->playtest_path)) return 0;
    }

    es->playtest_path[0] = '\0';
    return -1;
}

static int editor_path_is_private(const EditorState *es, const char *path)
{
    if (!es || !path || path[0] == '\0') return 0;
    return editor_path_is_recovery(path) ||
           (es->autosave_path[0] != '\0' &&
            strcmp(path, es->autosave_path) == 0) ||
           (es->playtest_path[0] != '\0' &&
            strcmp(path, es->playtest_path) == 0);
}

static int editor_path_is_recovery(const char *path)
{
    const char *base;
    size_t length;

    if (!path || path[0] == '\0') return 0;
    base = strrchr(path, '/');
    {
        const char *backslash = strrchr(path, '\\');
        if (!base || (backslash && backslash > base)) base = backslash;
    }
    base = base ? base + 1 : path;
    length = strlen(base);
    if (length != 37 || strncmp(base, EDITOR_RECOVERY_PREFIX,
                                 strlen(EDITOR_RECOVERY_PREFIX)) != 0 ||
        strcmp(base + 32, ".toml") != 0) return 0;
    for (size_t i = 16; i < 32; i++) {
        unsigned char ch = (unsigned char)base[i];
        if (!((ch >= '0' && ch <= '9') ||
              (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'))) return 0;
    }
    return 1;
}

static int editor_path_is_recovery_metadata(const char *path)
{
    const char *base;
    size_t length;

    if (!path || path[0] == '\0') return 0;
    base = strrchr(path, '/');
    {
        const char *backslash = strrchr(path, '\\');
        if (!base || (backslash && backslash > base)) base = backslash;
    }
    base = base ? base + 1 : path;
    length = strlen(base);
    if (length != 37 || strncmp(base, EDITOR_RECOVERY_PREFIX,
                                 strlen(EDITOR_RECOVERY_PREFIX)) != 0 ||
        strcmp(base + 32, ".meta") != 0) return 0;
    for (size_t i = strlen(EDITOR_RECOVERY_PREFIX); i < 32; i++) {
        unsigned char ch = (unsigned char)base[i];
        if (!((ch >= '0' && ch <= '9') ||
              (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'))) return 0;
    }
    return 1;
}
