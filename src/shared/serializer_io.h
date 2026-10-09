/*
 * serializer_io.h — Internal file I/O helpers for TOML serialization.
 */

#pragma once

#include <stdio.h>  /* FILE */
#include <stddef.h> /* size_t */
#include <stdint.h> /* uint64_t */
#ifdef _WIN32
#include <wchar.h>  /* wchar_t */
#endif

#define SERIALIZER_IO_PATH_MAX 4096

typedef enum {
    SERIALIZER_PATH_MISSING = 0,
    SERIALIZER_PATH_EXISTING = 1,
    SERIALIZER_PATH_ERROR = -1
} SerializerPathStatus;

typedef struct {
    uint64_t size;
    uint64_t content_hash;
    int valid;
} SerializerFileFingerprint;

#ifdef _WIN32
/* Convert validated UTF-8 strings for Windows wide-character path APIs. */
wchar_t *serializer_utf8_to_wide(const char *value);
char *serializer_wide_to_utf8(const wchar_t *value);
#endif

/* Open a UTF-8 path with platform-correct path conversion. */
FILE *serializer_fopen_utf8(const char *path, const char *mode);

/* Remove a UTF-8 path with platform-correct path conversion. */
int serializer_remove_utf8(const char *path);

/* Probe a UTF-8 path without treating unreadable existing files as missing. */
SerializerPathStatus serializer_probe_path_utf8(const char *path);

/* Return non-zero when a UTF-8 path names a readable file. */
int serializer_file_exists_utf8(const char *path);

/* Fingerprint bytes read from an opened UTF-8 path. */
int serializer_fingerprint_utf8(const char *path,
                                SerializerFileFingerprint *fingerprint);
int serializer_fingerprint_equal(const SerializerFileFingerprint *a,
                                 const SerializerFileFingerprint *b);

/* Build the first candidate temporary sibling path for a target file. */
int serializer_make_temp_path(const char *path, char *buf, size_t buf_size);

/*
 * Create an exclusive temporary sibling.  Collision candidates are retried;
 * returned path is the file actually opened.
 *
 * serializer_open_temp makes a private file (POSIX mode 0600): recovery
 * snapshots, playtest copies, profiles and editor preferences are nobody
 * else's business.  serializer_open_temp_shared is for files the user
 * saves on purpose, such as a level: it asks for 0666 and lets the
 * process umask (usually 022) decide, exactly like a text editor would.
 * Either way, replacing an existing regular file keeps that file's
 * permission bits, and its owner and group where the OS allows it.
 */
FILE *serializer_open_temp(const char *target_path, char *temp_path,
                           size_t temp_path_size);
FILE *serializer_open_temp_shared(const char *target_path, char *temp_path,
                                  size_t temp_path_size);

/* Flush stdio buffers and request an OS-level file flush where supported. */
int serializer_flush(FILE *fp);

/* Non-zero when path itself is a symbolic link (POSIX; always 0 elsewhere). */
int serializer_path_is_symlink(const char *path);

/* Non-zero when both paths exist and name the same file (POSIX only). */
int serializer_same_file_utf8(const char *a, const char *b);

/*
 * Resolve a symlinked path to the file it points to (POSIX).  Other paths,
 * and dangling links, are copied unchanged.  Saves never call this on their
 * own: following a link planted at a destination would write a file the
 * user never chose.  The editor uses it for the document it opened, so
 * saving that document updates the real file and the link survives.
 */
int serializer_resolve_save_target(const char *path, char *buf, size_t buf_size);

/* Replace target with a completed sibling temporary file, then sync its
 * directory entry where the platform supports it.  A failed directory sync
 * after the file is in place is a stderr warning, not a failed save.
 *
 * Returns 0 on success and -1 when the target was left as it was (the
 * caller may delete the temporary file).  It returns
 * SERIALIZER_REPLACE_TEMP_KEPT when the target may already be gone and the
 * temporary file is the only complete copy: the caller must NOT delete it,
 * and should tell the user where it is. */
#define SERIALIZER_REPLACE_TEMP_KEPT (-3)
int serializer_replace_file(const char *temp_path, const char *target_path);

/* Install a completed sibling only when target is still absent (also synced).
 * POSIX uses link(); on filesystems without hard links (FAT/exFAT, some
 * network shares) it claims the name with O_EXCL and renames over that. */
int serializer_create_file(const char *temp_path, const char *target_path);

/* Remove a temporary file after an incomplete save. */
void serializer_remove_temp(const char *path);

/* Internal deterministic failure seam.  Zero is normal production behavior. */
#define SERIALIZER_TEST_FAILURE_NONE  0
#define SERIALIZER_TEST_FAILURE_WRITE 1
#define SERIALIZER_TEST_FAILURE_FLUSH 2
#define SERIALIZER_TEST_FAILURE_TARGET_APPEARED 3
#define SERIALIZER_TEST_FAILURE_DIR_SYNC 4 /* POSIX parent-folder fsync */
#define SERIALIZER_TEST_FAILURE_NO_HARD_LINKS 5 /* POSIX link() reports EPERM */
/* serializer_replace_file acts as if Windows had moved the original away and
 * then failed to move the new file in: it returns SERIALIZER_REPLACE_TEMP_KEPT
 * and leaves both files untouched. */
#define SERIALIZER_TEST_FAILURE_REPLACE_STRANDED 6
/* A checked save's last fingerprint check sees a changed file (as if another
 * program wrote it during the save), so the save returns -2. */
#define SERIALIZER_TEST_FAILURE_SOURCE_CHANGED 7
#ifdef MANGO_TESTING
void serializer_test_set_failure(int failure);  /* test builds only */
#endif

/* 1 when the injected test failure is `failure` (it is then used up);
 * always 0 in normal runs, where no failure is ever injected. */
int serializer_test_take_failure(int failure);

/* Check stdio output state, including the internal write-failure seam. */
int serializer_stream_has_error(FILE *fp);
