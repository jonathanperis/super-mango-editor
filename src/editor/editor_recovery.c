/*
 * editor_recovery.c — Autosave snapshots and crash recovery for the editor.
 *
 * While a document has unsaved changes, editor_maybe_autosave writes a copy
 * of it every 30 seconds into the editor's preference folder.  Each open
 * document gets a random 64-bit id and two files named after it:
 *
 *     editor_recovery_<id>.toml   the level itself (a normal level file)
 *     editor_recovery_<id>.meta   one line: version, id, time, owner, source
 *
 * After a crash the next editor start finds the .meta files
 * (editor_discover_recoveries), offers them (editor_choose_recovery) and
 * loads the chosen snapshot as an unsaved document
 * (editor_recover_entry_by_id), or deletes one the designer discards.
 * Saving, or explicitly discarding, the document deletes both files again
 * (editor_retire_*_recovery).
 *
 * The owner is the process id of the editor that wrote the snapshot, and
 * when the system tells, the time that process started.  A snapshot whose
 * owner is another editor that is still running is that editor's live
 * work, not a crash leftover, so it is never offered here.
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
#include <sys/stat.h>   /* stat: how old an orphan snapshot is */
#include <signal.h>     /* kill(pid, 0): is the owning editor still running? */
#include <unistd.h>     /* getpid */
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
/* An orphan snapshot younger than this may belong to an editor that is
 * between writing its .toml and its .meta; it is left alone. */
#define EDITOR_ORPHAN_MIN_AGE_S (5 * 60)

/*
 * A recovery metadata file holds one line:
 *     3 \t <id: 16 hex> \t <timestamp> \t <owner pid> \t <owner start>
 *       \t <source path: hex> \n
 * Older files are still read: version 2 has no owner start (the pid alone
 * decides), and version 1 has no owner at all.
 * The path is hex-encoded (2 characters per byte) so tabs and newlines in
 * a file name cannot break the format.  Buffers are sized from that layout
 * so the longest path the editor accepts (EDITOR_PATH_MAX - 1 bytes)
 * survives a write/read round trip:
 *     source hex : 2 * (EDITOR_PATH_MAX - 1) digits + NUL
 *     whole line : "3\t" (2) + id (16) + "\t" (1) + largest uint64 (20)
 *                  + "\t" (1) + largest uint64 pid (20) + "\t" (1)
 *                  + largest uint64 start (20) + "\t" (1)
 *                  + hex digits + "\n" (1) + NUL (1)
 */
#define EDITOR_RECOVERY_SOURCE_HEX_MAX (2 * (EDITOR_PATH_MAX - 1) + 1)
#define EDITOR_RECOVERY_LINE_MAX \
    (2 + 16 + 1 + 20 + 1 + 20 + 1 + 20 + 1 + (EDITOR_RECOVERY_SOURCE_HEX_MAX - 1) + 1 + 1)

static uint64_t editor_recovery_sequence;
static int editor_recovery_seeded;

/* Canned picker answers for tests, used first to last (see
 * editor_test_set_recovery_choice); -1 entries mean "no answer". */
#define EDITOR_TEST_RECOVERY_ANSWERS 4
static int editor_test_recovery_answers[EDITOR_TEST_RECOVERY_ANSWERS] = {-1, -1, -1, -1};

#ifdef MANGO_TESTING
void editor_test_set_recovery_choice(int action)
{
    for (int i = 0; i < EDITOR_TEST_RECOVERY_ANSWERS; i++)
        editor_test_recovery_answers[i] = -1;
    editor_test_recovery_answers[0] = action;
}

void editor_test_queue_recovery_choice(int action)
{
    for (int i = 0; i < EDITOR_TEST_RECOVERY_ANSWERS; i++) {
        if (editor_test_recovery_answers[i] < 0) {
            editor_test_recovery_answers[i] = action;
            return;
        }
    }
}
#endif

/* Take the next canned answer, or -1 when none is waiting. */
static int editor_take_test_recovery_answer(void)
{
    int answer = editor_test_recovery_answers[0];
    for (int i = 0; i + 1 < EDITOR_TEST_RECOVERY_ANSWERS; i++)
        editor_test_recovery_answers[i] = editor_test_recovery_answers[i + 1];
    editor_test_recovery_answers[EDITOR_TEST_RECOVERY_ANSWERS - 1] = -1;
    return answer;
}

/* ------------------------------------------------------------------ */
/* Which editor owns a snapshot                                        */
/* ------------------------------------------------------------------ */

static unsigned long editor_current_process_id(void)
{
#ifdef _WIN32
    return (unsigned long)GetCurrentProcessId();
#else
    return (unsigned long)getpid();
#endif
}

#ifdef _WIN32
/* The creation time of an open process handle, or 0. */
static uint64_t editor_windows_start_time(HANDLE process)
{
    FILETIME created, exited, kernel, user;
    if (!GetProcessTimes(process, &created, &exited, &kernel, &user)) return 0;
    return ((uint64_t)created.dwHighDateTime << 32) | created.dwLowDateTime;
}
#endif

uint64_t editor_process_start_time(unsigned long pid)
{
    if (pid == 0) return 0;
#if defined(_WIN32)
    {
        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                                     (DWORD)pid);
        uint64_t start;
        if (!process) return 0;
        start = editor_windows_start_time(process);
        CloseHandle(process);
        return start;
    }
#elif defined(__linux__)
    {
        /* /proc/<pid>/stat is one line of space-separated fields; field 22
         * is the start time in clock ticks after boot.  Field 2 is the
         * program name in parentheses, which may itself hold spaces and
         * ')', so counting starts after the LAST ')'. */
        char path[48];
        char line[1024];
        const char *field;
        char *end;
        unsigned long long start;
        FILE *fp;

        snprintf(path, sizeof(path), "/proc/%lu/stat", pid);
        fp = fopen(path, "r");
        if (!fp) return 0;
        field = fgets(line, sizeof(line), fp);
        fclose(fp);
        if (!field || !(field = strrchr(line, ')'))) return 0;
        for (int number = 2; number < 22; number++) {   /* field 2 ends here */
            field = strchr(field, ' ');
            if (!field) return 0;
            field++;
        }
        errno = 0;
        start = strtoull(field, &end, 10);
        if (errno == ERANGE || end == field) return 0;
        return (uint64_t)start;
    }
#else
    /* No portable way to ask (macOS would need proc_pidinfo); the pid
     * alone decides, as it did before start times were recorded. */
    return 0;
#endif
}

/*
 * editor_process_is_running — Is the process that was `pid`, started at
 * `start` (0 = unknown), still alive?
 *
 * POSIX: kill() with signal 0 sends nothing; it only checks that the process
 * exists.  EPERM means it exists but belongs to another user, which still
 * counts as running.  Windows: open the process and ask for its exit code;
 * STILL_ACTIVE means it has not exited.
 *
 * A process id is reused after its process ends.  When the start time was
 * recorded and the system still knows the pid's start time, the two must
 * match: an unrelated program that got the old number started later, so a
 * crash snapshot is not hidden behind it.  When either is unknown, the pid
 * alone decides, and the snapshot reappears once that program ends.
 */
static int editor_process_is_running(unsigned long pid, uint64_t start)
{
    uint64_t now_start;

    if (pid == 0) return 0;
#ifdef _WIN32
    {
        DWORD code = 0;
        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                                     (DWORD)pid);
        int running;
        if (!process) return GetLastError() == ERROR_ACCESS_DENIED;
        running = GetExitCodeProcess(process, &code) && code == STILL_ACTIVE;
        now_start = running && start != 0 ? editor_windows_start_time(process) : 0;
        CloseHandle(process);
        if (!running) return 0;
    }
#else
    if ((unsigned long)(pid_t)pid != pid) return 0;   /* not a valid pid_t */
    if (kill((pid_t)pid, 0) != 0 && errno != EPERM) return 0;
    now_start = start != 0 ? editor_process_start_time(pid) : 0;
#endif
    return start == 0 || now_start == 0 || now_start == start;
}

/* A snapshot that another, still running, editor is keeping up to date. */
static int editor_recovery_entry_is_live_elsewhere(const EditorRecoveryEntry *entry)
{
    return entry->owner_pid != 0 &&
           entry->owner_pid != editor_current_process_id() &&
           editor_process_is_running(entry->owner_pid, entry->owner_start);
}

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
    if (fprintf(fp, "3\t%016llx\t%llu\t%lu\t%llu\t%s\n",
                (unsigned long long)entry->id,
                (unsigned long long)entry->timestamp,
                entry->owner_pid, (unsigned long long)entry->owner_start,
                source_hex) < 0) {
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
    char *pid_text = NULL;
    char *start_text = NULL;
    char *source_text;
    char *end;
    uint64_t id;
    unsigned long long timestamp;
    unsigned long owner_pid = 0;
    unsigned long long owner_start = 0;
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
    /* Version 2 adds the owner's process id before the source path, and
     * version 3 the owner's start time after it. */
    if (version && (strcmp(version, "2") == 0 || strcmp(version, "3") == 0))
        pid_text = strtok(NULL, "\t");
    if (version && strcmp(version, "3") == 0) start_text = strtok(NULL, "\t");
    source_text = strtok(NULL, "\t");
    if (!version || !id_text || !timestamp_text || !source_text ||
        strtok(NULL, "\t") ||
        !(strcmp(version, "1") == 0 || (strcmp(version, "2") == 0 && pid_text) ||
          (strcmp(version, "3") == 0 && pid_text && start_text)))
        return 0;
    if (pid_text) {
        errno = 0;
        owner_pid = strtoul(pid_text, &end, 10);
        if (errno == ERANGE || end == pid_text || *end != '\0') return 0;
    }
    if (start_text) {
        errno = 0;
        owner_start = strtoull(start_text, &end, 10);
        if (errno == ERANGE || end == start_text || *end != '\0') return 0;
    }
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
    entry->owner_pid = owner_pid;
    entry->owner_start = (uint64_t)owner_start;
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
    /* Another running editor's live work is not ours to offer or delete. */
    if (editor_recovery_entry_is_live_elsewhere(&entry)) return 0;
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
        /* Every recovery file (.meta and .toml); callbacks pick by suffix. */
        int written = snprintf(pattern, sizeof(pattern), "%s\\%s*",
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

/*
 * Orphan snapshots: an editor_recovery_<id>.toml without its .meta is never
 * offered (the .meta is what discovery reads), so it would only take up
 * space forever.  Older editors could leave one behind when autosave hit
 * the entry limit after writing the level.
 *
 * But every editor writes a new snapshot's .toml first and its .meta right
 * after, so another editor starting in that moment sees an "orphan" that
 * is about to get its .meta.  Only a snapshot last written more than
 * EDITOR_ORPHAN_MIN_AGE_S ago is a real leftover; autosave rewrites a live
 * one every 30 seconds, so a live snapshot is never that old.
 */

/* Seconds since path was last written, or -1 when that is unknown. */
static long long editor_file_age_seconds(const char *path)
{
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA data;
    FILETIME now_time;
    ULARGE_INTEGER written, now;
    wchar_t *wide = serializer_utf8_to_wide(path);
    BOOL ok;

    if (!wide) return -1;
    ok = GetFileAttributesExW(wide, GetFileExInfoStandard, &data);
    free(wide);
    if (!ok) return -1;
    GetSystemTimeAsFileTime(&now_time);
    written.LowPart = data.ftLastWriteTime.dwLowDateTime;
    written.HighPart = data.ftLastWriteTime.dwHighDateTime;
    now.LowPart = now_time.dwLowDateTime;
    now.HighPart = now_time.dwHighDateTime;
    /* FILETIME counts 100-nanosecond steps. */
    return now.QuadPart > written.QuadPart
         ? (long long)((now.QuadPart - written.QuadPart) / 10000000ull) : 0;
#else
    struct stat info;
    time_t now = time(NULL);
    if (stat(path, &info) != 0) return -1;
    return now > info.st_mtime ? (long long)(now - info.st_mtime) : 0;
#endif
}

typedef struct {
    const EditorState *es;
    int removed;
} EditorOrphanSweep;

static int editor_sweep_orphan_name(const char *name, void *context)
{
    EditorOrphanSweep *sweep = (EditorOrphanSweep *)context;
    char metadata_path[EDITOR_PATH_MAX];
    char snapshot_path[EDITOR_PATH_MAX];
    uint64_t id;

    if (editor_recovery_name_id(name, &id, ".toml") != 0) return 0;
    /* This editor's own snapshot may be between its two writes. */
    if (id == sweep->es->recovery_document_id) return 0;
    if (editor_recovery_metadata_path(sweep->es, id, metadata_path,
                                      sizeof(metadata_path)) != 0 ||
        editor_recovery_snapshot_path(sweep->es, id, snapshot_path,
                                      sizeof(snapshot_path)) != 0) return 0;
    if (serializer_probe_path_utf8(metadata_path) == SERIALIZER_PATH_MISSING &&
        editor_file_age_seconds(snapshot_path) >= EDITOR_ORPHAN_MIN_AGE_S &&
        serializer_remove_utf8(snapshot_path) == 0)
        sweep->removed++;
    return 0;
}

int editor_clean_orphan_recoveries(EditorState *es)
{
    EditorOrphanSweep sweep;

    if (!es || es->recovery_root_path[0] == '\0') return 0;
    sweep.es = es;
    sweep.removed = 0;
    (void)editor_for_each_recovery_name(es, editor_sweep_orphan_name, &sweep);
    return sweep.removed;
}

/* ------------------------------------------------------------------ */
/* Adding and retiring entries                                         */
/* ------------------------------------------------------------------ */

/* editor_add_recovery_entry's answer when the folder already holds
 * EDITOR_MAX_RECOVERY_ENTRIES other snapshots (other failures are -1). */
#define EDITOR_RECOVERY_FOLDER_FULL (-2)

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
        if (!current_entry) return EDITOR_RECOVERY_FOLDER_FULL;
    }
    memset(&entry, 0, sizeof(entry));
    entry.id = es->recovery_document_id;
    entry.timestamp = (uint64_t)time(NULL);
    entry.owner_pid = editor_current_process_id();
    entry.owner_start = editor_process_start_time(entry.owner_pid);
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

    int added = -1;
    if (es->autosave_path[0] != '\0' &&
        level_save_toml_recovery(snapshot, es->autosave_path,
                                 es->file_path) == 0) {
        added = editor_add_recovery_entry(es, es->file_path);
        if (added != 0) {
            /* Without its .meta the snapshot is never offered; do not leave
             * it behind as an orphan.  A snapshot registered by an earlier
             * autosave keeps its (still valid) files. */
            char metadata_path[EDITOR_PATH_MAX];
            if (editor_recovery_metadata_path(es, es->recovery_document_id,
                                              metadata_path,
                                              sizeof(metadata_path)) == 0 &&
                serializer_probe_path_utf8(metadata_path) == SERIALIZER_PATH_MISSING)
                (void)serializer_remove_utf8(es->autosave_path);
        }
    }
    if (added == 0) {
        memcpy(es->recovery_original_path, es->file_path,
               strlen(es->file_path) + 1);
        editor_set_background_status(es, now, snapshot == &es->level
            ? "Autosaved recovery copy"
            : "Autosaved last valid version (current level has errors)");
    } else if (added == EDITOR_RECOVERY_FOLDER_FULL) {
        /* Say what is wrong and how to fix it: old crash copies fill every
         * slot, and only the designer can decide to recover or drop them. */
        (void)editor_clean_orphan_recoveries(es);
        editor_set_status(es, "Autosave paused: %d old recovery copies fill the "
                          "folder; Ctrl+R to recover or discard them",
                          EDITOR_MAX_RECOVERY_ENTRIES);
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

/*
 * editor_ask_recovery_action — One native dialog about entry `index`.
 *
 * Native dialogs have at most three buttons (macOS allows no more), and the
 * picker needs four actions.  With one snapshot the buttons are
 * Cancel / Recover / Discard.  With several, the third button is "More...",
 * which asks a second question: Back / Discard / Next.
 * Returns an EditorRecoveryAction, or -1 when the dialog failed.
 */
static int editor_ask_recovery_action(const EditorState *es, int index)
{
    char message[EDITOR_PATH_MAX + 160];
    char timestamp[64] = "unknown time";
    time_t raw_time = (time_t)es->recovery_entries[index].timestamp;
    struct tm time_value;
    int several = es->recovery_entry_count > 1;
    int button_id = 0;
    int answer = editor_take_test_recovery_answer();
#ifdef _WIN32
    int time_valid = localtime_s(&time_value, &raw_time) == 0;
#else
    int time_valid = localtime_r(&raw_time, &time_value) != NULL;
#endif
    const char *source = es->recovery_entries[index].source_path[0]
                       ? es->recovery_entries[index].source_path
                       : "(untitled)";

    if (answer >= 0) return answer;
    if (time_valid) strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S",
                             &time_value);
    snprintf(message, sizeof(message), "Copy %d of %d\nSource: %s\nTimestamp: %s",
             index + 1, es->recovery_entry_count, source, timestamp);
    {
        const char *buttons[] = {"Cancel", "Recover", several ? "More..." : "Discard"};
        if (dialog_choice("Recover Editor Snapshot", message, buttons, 3, 1, 0,
                          &button_id) != 0) return -1;
    }
    if (button_id == 0) return EDITOR_RECOVERY_CANCEL;
    if (button_id == 1) return EDITOR_RECOVERY_RECOVER;
    if (!several) return EDITOR_RECOVERY_DISCARD;
    {
        const char *buttons[] = {"Back", "Discard", "Next"};
        snprintf(message, sizeof(message),
                 "Discard copy %d of %d (deleting it), or look at the next one?\n"
                 "Source: %s\nTimestamp: %s",
                 index + 1, es->recovery_entry_count, source, timestamp);
        if (dialog_choice("Recover Editor Snapshot", message, buttons, 3, 2, 0,
                          &button_id) != 0) return -1;
    }
    if (button_id == 1) return EDITOR_RECOVERY_DISCARD;
    if (button_id == 2) return EDITOR_RECOVERY_NEXT;
    return EDITOR_RECOVERY_BACK;
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
        int action = editor_ask_recovery_action(es, index);
        if (action < 0) {
            editor_set_status(es, "Recovery cancelled");
            return -1;
        }
        if (action == EDITOR_RECOVERY_CANCEL) {
            es->pending_recovery_id = 0;
            editor_set_status(es, "Recovery cancelled");
            return -1;
        }
        if (action == EDITOR_RECOVERY_RECOVER) {
            es->pending_recovery_id = es->recovery_entries[index].id;
            return index;
        }
        if (action == EDITOR_RECOVERY_DISCARD) {
            /* Delete both files of this copy; the list is re-read from
             * disk, so the next copy (if any) moves into this position. */
            if (editor_remove_recovery_entry(es, index) != 0) {
                editor_set_status(es, "Could not discard the recovery copy");
                return -1;
            }
            if (es->recovery_entry_count == 0) {
                es->pending_recovery_id = 0;
                editor_set_status(es, "Recovery copy discarded");
                return -1;
            }
            if (index >= es->recovery_entry_count) index = 0;
            continue;
        }
        if (action == EDITOR_RECOVERY_NEXT)
            index = (index + 1) % es->recovery_entry_count;
        /* EDITOR_RECOVERY_BACK asks about the same copy again. */
    }
}
