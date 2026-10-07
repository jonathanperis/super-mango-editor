/*
 * editor_recovery.c — Autosave snapshots and crash recovery for the editor.
 *
 * While a document has unsaved changes, editor_maybe_autosave writes a copy
 * of it every 30 seconds into the editor's preference folder.  Each open
 * document gets a random 64-bit id and two files named after it:
 *
 *     editor_recovery_<id>.toml   the level itself (a normal level file)
 *     editor_recovery_<id>.meta   one line: version, id, time, source path
 *
 * After a crash the next editor start finds the .meta files
 * (editor_discover_recoveries), offers them (editor_choose_recovery) and
 * loads the chosen snapshot as an unsaved document
 * (editor_recover_entry_by_id).  Saving, or explicitly discarding, the
 * document deletes both files again (editor_retire_*_recovery).
 *
 * Opening, saving and the recent-file list live in editor_files.c; this file
 * borrows its preference-folder and atomic-write helpers.
 */

#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "editor_recovery.h"

#include <stdio.h>      /* FILE, fgets, fprintf, snprintf */
#include <stdint.h>     /* uint64_t */
#include <time.h>       /* time, localtime_r, strftime */
#include <errno.h>      /* errno, ERANGE, ENOENT */
#include <stdlib.h>     /* strtoull, free */
#include <string.h>     /* memset, memcpy, strcmp, strlen, strtok */

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#include <windows.h>    /* FindFirstFileW */
#include <bcrypt.h>     /* BCryptGenRandom */
#else
#include <dirent.h>     /* directory discovery */
#endif

#include "file_dialog.h"       /* dialog_choice (native button dialog) */
#include "editor_files.h"      /* preference paths, atomic writes, loading */
#include "editor_session.h"    /* editor_set_status, editor_document_hash */
#include "editor_validation.h" /* editor_validate_level */
#include "../shared/serializer.h"    /* level_save_toml_recovery */
#include "../shared/serializer_io.h" /* UTF-8 file I/O */

#define EDITOR_AUTOSAVE_MS   30000u
#define EDITOR_STATUS_HOLD_MS 5000u  /* keep a fresh status message this long */
#define EDITOR_RECOVERY_PREFIX "editor_recovery_"

/*
 * A recovery metadata file holds one line:
 *     1 \t <id: 16 hex> \t <timestamp: decimal> \t <source path: hex> \n
 * The path is hex-encoded (2 characters per byte) so tabs and newlines in
 * a file name cannot break the format.  Buffers are sized from that layout
 * so the longest path the editor accepts (EDITOR_PATH_MAX - 1 bytes)
 * survives a write/read round trip:
 *     source hex : 2 * (EDITOR_PATH_MAX - 1) digits + NUL
 *     whole line : "1\t" (2) + id (16) + "\t" (1) + largest uint64 (20)
 *                  + "\t" (1) + hex digits + "\n" (1) + NUL (1)
 */
#define EDITOR_RECOVERY_SOURCE_HEX_MAX (2 * (EDITOR_PATH_MAX - 1) + 1)
#define EDITOR_RECOVERY_LINE_MAX \
    (2 + 16 + 1 + 20 + 1 + (EDITOR_RECOVERY_SOURCE_HEX_MAX - 1) + 1 + 1)

static uint64_t editor_recovery_sequence;
static int editor_recovery_seeded;
static int editor_test_recovery_choice = -1;

#ifdef MANGO_TESTING
void editor_test_set_recovery_choice(int button_id)
{
    editor_test_recovery_choice = button_id;
}
#endif

/* ------------------------------------------------------------------ */
/* Recovery file names                                                 */
/* ------------------------------------------------------------------ */

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

int editor_set_recovery_document(EditorState *es, const char *document_path)
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

int editor_path_is_recovery(const char *path)
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

/* ------------------------------------------------------------------ */
/* Metadata files                                                      */
/* ------------------------------------------------------------------ */

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
    char source_hex[EDITOR_RECOVERY_SOURCE_HEX_MAX];
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
    return editor_commit_temp_file(fp, temp_path, entry->metadata_path);
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
    char line[EDITOR_RECOVERY_LINE_MAX];
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

/* ------------------------------------------------------------------ */
/* Finding the recoveries on disk                                      */
/* ------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------ */
/* Adding and retiring entries                                         */
/* ------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------ */
/* Autosave                                                            */
/* ------------------------------------------------------------------ */

void editor_remember_valid_level(EditorState *es)
{
    if (!es) return;
    es->last_valid_level = es->level;
    es->last_valid_document_id = es->recovery_document_id;
    es->last_valid_level_set = 1;
}

/* Show a routine message only if the bar is not still showing a recent,
 * probably unread one (e.g. why a placement was refused). */
static void editor_set_background_status(EditorState *es, uint32_t now,
                                         const char *message)
{
    if (es->status_message[0] != '\0' &&
        now - es->status_set_ms < EDITOR_STATUS_HOLD_MS) return;
    editor_set_status(es, "%s", message);
}

void editor_maybe_autosave(EditorState *es)
{
    const LevelDef *snapshot;
    uint32_t now;

    if (!es || !es->modified) return;
    now = (uint32_t)clock_millis();
    if (now - es->last_autosave_ms < EDITOR_AUTOSAVE_MS) return;

    /*
     * Count every attempt, successful or not.  If only successes moved the
     * timestamp, a full disk or a level that stays invalid would retry (and
     * rewrite the status bar) on every frame; now it waits a full interval.
     */
    es->last_autosave_ms = now;

    /*
     * Which version to snapshot?  Recovery files are read back with
     * level_load_toml, which rejects levels that fail validation (and the
     * writer refuses them too), so a snapshot of an invalid draft could
     * never be recovered.  The safe choice is the newest *valid* version of
     * this document: after a crash the designer loses only the edits made
     * since the level last validated, instead of everything since the last
     * save.
     */
    editor_validate_level(&es->level, &es->validation_report);
    if (es->validation_report.error_count == 0) {
        editor_remember_valid_level(es);
        snapshot = &es->level;
    } else if (es->last_valid_level_set &&
               es->last_valid_document_id == es->recovery_document_id &&
               (!es->saved_document_hash_valid ||
                editor_document_hash(&es->last_valid_level) !=
                es->saved_document_hash)) {
        snapshot = &es->last_valid_level;
    } else {
        /* Nothing newer than the saved file is valid yet. */
        editor_set_background_status(es, now,
                                     "Autosave skipped: level has validation errors");
        return;
    }

    if (es->autosave_path[0] != '\0' &&
        level_save_toml_recovery(snapshot, es->autosave_path,
                                 es->file_path) == 0 &&
        editor_add_recovery_entry(es, es->file_path) == 0) {
        memcpy(es->recovery_original_path, es->file_path,
               strlen(es->file_path) + 1);
        editor_set_background_status(es, now, snapshot == &es->level
            ? "Autosaved recovery copy"
            : "Autosaved last valid version (current level has errors)");
    } else {
        /* A failure matters more than whatever was shown; it repeats at
         * most once per interval, so it cannot flood the bar. */
        editor_set_status(es, "Autosave failed; retrying in %u s",
                          EDITOR_AUTOSAVE_MS / 1000u);
    }
}

/* ------------------------------------------------------------------ */
/* Recovering a snapshot                                               */
/* ------------------------------------------------------------------ */

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
