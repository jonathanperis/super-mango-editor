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
 */
FILE *serializer_open_temp(const char *target_path, char *temp_path,
                           size_t temp_path_size);

/* Flush stdio buffers and request an OS-level file flush where supported. */
int serializer_flush(FILE *fp);

/* Replace target with a completed sibling temporary file. */
int serializer_replace_file(const char *temp_path, const char *target_path);

/* Install a completed sibling only when target is still absent. */
int serializer_create_file(const char *temp_path, const char *target_path);

/* Remove a temporary file after an incomplete save. */
void serializer_remove_temp(const char *path);

/* Internal deterministic failure seam.  Zero is normal production behavior. */
#define SERIALIZER_TEST_FAILURE_NONE  0
#define SERIALIZER_TEST_FAILURE_WRITE 1
#define SERIALIZER_TEST_FAILURE_FLUSH 2
#define SERIALIZER_TEST_FAILURE_TARGET_APPEARED 3
void serializer_test_set_failure(int failure);

/* Check stdio output state, including the internal write-failure seam. */
int serializer_stream_has_error(FILE *fp);
