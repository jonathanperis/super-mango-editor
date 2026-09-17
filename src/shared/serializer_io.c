#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

/*
 * serializer_io.c — Internal file I/O helpers for TOML serialization.
 */

#include "serializer_io.h"

#include <errno.h>  /* errno, EEXIST */
#include <stdio.h>  /* remove, snprintf */
#include <stdlib.h> /* free, malloc */
#include <string.h> /* strlen, memset */
#include <stdint.h> /* uint64_t */

#ifdef _WIN32
#include <windows.h> /* GetCurrentProcessId, MoveFileExW, UTF-8 conversion */
#include <io.h>      /* _commit, _fileno, _open */
#include <fcntl.h>   /* _O_CREAT, _O_EXCL */
#include <sys/stat.h> /* _S_IREAD, _S_IWRITE */
#else
#include <fcntl.h>    /* open, O_CREAT, O_EXCL */
#include <sys/stat.h> /* stat, fchmod */
#include <unistd.h>   /* getpid, fsync, fileno */
#endif

static int serializer_test_failure = SERIALIZER_TEST_FAILURE_NONE;

#ifdef _WIN32
wchar_t *serializer_utf8_to_wide(const char *path)
{
    int length;
    wchar_t *wide;

    if (!path) return NULL;
    length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                 path, -1, NULL, 0);
    if (length <= 0) return NULL;

    wide = (wchar_t *)malloc((size_t)length * sizeof(*wide));
    if (!wide) return NULL;
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                            path, -1, wide, length) <= 0) {
        free(wide);
        return NULL;
    }
    return wide;
}

char *serializer_wide_to_utf8(const wchar_t *value)
{
    int length;
    char *utf8;

    if (!value) return NULL;
    length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                 value, -1, NULL, 0, NULL, NULL);
    if (length <= 0) return NULL;

    utf8 = (char *)malloc((size_t)length);
    if (!utf8) return NULL;
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                            value, -1, utf8, length, NULL, NULL) <= 0) {
        free(utf8);
        return NULL;
    }
    return utf8;
}

static wchar_t *serializer_ascii_to_wide(const char *value)
{
    size_t length;
    wchar_t *wide;

    if (!value) return NULL;
    length = strlen(value);
    wide = (wchar_t *)malloc((length + 1) * sizeof(*wide));
    if (!wide) return NULL;
    for (size_t i = 0; i <= length; i++) wide[i] = (wchar_t)(unsigned char)value[i];
    return wide;
}
#endif

FILE *serializer_fopen_utf8(const char *path, const char *mode)
{
    if (!path || !mode) return NULL;

#ifdef _WIN32
    {
        wchar_t *wide_path = serializer_utf8_to_wide(path);
        wchar_t *wide_mode = serializer_ascii_to_wide(mode);
        FILE *fp = NULL;

        if (wide_path && wide_mode) fp = _wfopen(wide_path, wide_mode);
        free(wide_path);
        free(wide_mode);
        return fp;
    }
#else
    return fopen(path, mode);
#endif
}

int serializer_remove_utf8(const char *path)
{
    if (!path) return -1;

#ifdef _WIN32
    {
        wchar_t *wide_path = serializer_utf8_to_wide(path);
        int result = wide_path ? _wremove(wide_path) : -1;
        free(wide_path);
        return result;
    }
#else
    return remove(path);
#endif
}

SerializerPathStatus serializer_probe_path_utf8(const char *path)
{
    if (!path || path[0] == '\0') return SERIALIZER_PATH_ERROR;

#ifdef _WIN32
    {
        wchar_t *wide_path = serializer_utf8_to_wide(path);
        DWORD error;
        if (!wide_path) return SERIALIZER_PATH_ERROR;
        if (GetFileAttributesW(wide_path) != INVALID_FILE_ATTRIBUTES) {
            free(wide_path);
            return SERIALIZER_PATH_EXISTING;
        }
        error = GetLastError();
        free(wide_path);
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ||
            error == ERROR_INVALID_NAME) return SERIALIZER_PATH_MISSING;
        return SERIALIZER_PATH_ERROR;
    }
#else
    {
        struct stat st;
        if (stat(path, &st) == 0) return SERIALIZER_PATH_EXISTING;
        if (errno == ENOENT || errno == ENOTDIR) return SERIALIZER_PATH_MISSING;
        return SERIALIZER_PATH_ERROR;
    }
#endif
}

int serializer_file_exists_utf8(const char *path)
{
    FILE *fp;

    if (serializer_probe_path_utf8(path) != SERIALIZER_PATH_EXISTING) return 0;
    fp = serializer_fopen_utf8(path, "rb");
    if (!fp) return 0;
    fclose(fp);
    return 1;
}

int serializer_fingerprint_utf8(const char *path,
                                SerializerFileFingerprint *fingerprint)
{
    FILE *fp;
    unsigned char buffer[4096];
    size_t count;
    uint64_t hash = UINT64_C(1469598103934665603);
    uint64_t size = 0;

    if (!fingerprint) return -1;
    memset(fingerprint, 0, sizeof(*fingerprint));
    if (!path || path[0] == '\0') return -1;

    fp = serializer_fopen_utf8(path, "rb");
    if (!fp) {
        return serializer_probe_path_utf8(path) == SERIALIZER_PATH_MISSING ? 0 : -1;
    }

    while (!ferror(fp) && !feof(fp)) {
        count = fread(buffer, 1, sizeof(buffer), fp);
        for (size_t i = 0; i < count; i++) {
            hash ^= buffer[i];
            hash *= UINT64_C(1099511628211);
        }
        size += (uint64_t)count;
    }
    {
        int read_error = ferror(fp);
        int close_error = fclose(fp);
        if (read_error || close_error) return -1;
    }

    fingerprint->size = size;
    fingerprint->content_hash = hash;
    fingerprint->valid = 1;
    return 1;
}

int serializer_fingerprint_equal(const SerializerFileFingerprint *a,
                                 const SerializerFileFingerprint *b)
{
    return a && b && a->valid && b->valid &&
           a->size == b->size && a->content_hash == b->content_hash;
}

int serializer_make_temp_path(const char *path, char *buf, size_t buf_size)
{
    int written;

    if (!path || !buf || buf_size == 0) return -1;

#ifdef _WIN32
    written = snprintf(buf, buf_size, "%s.tmp.%lu", path,
                       (unsigned long)GetCurrentProcessId());
#else
    written = snprintf(buf, buf_size, "%s.tmp.%ld", path, (long)getpid());
#endif

    if (written < 0 || (size_t)written >= buf_size) {
        if (buf_size > 0) buf[0] = '\0';
        return -1;
    }
    return 0;
}

void serializer_test_set_failure(int failure)
{
    serializer_test_failure = failure;
}

int serializer_stream_has_error(FILE *fp)
{
    if (fp && ferror(fp)) return 1;
    if (serializer_test_failure == SERIALIZER_TEST_FAILURE_WRITE) {
        serializer_test_failure = SERIALIZER_TEST_FAILURE_NONE;
        return 1;
    }
    return 0;
}

FILE *serializer_open_temp(const char *target_path, char *temp_path,
                           size_t temp_path_size)
{
    unsigned long attempt;
    unsigned long process_id;

    if (!target_path || !temp_path || temp_path_size == 0) return NULL;

#ifdef _WIN32
    process_id = (unsigned long)GetCurrentProcessId();
#else
    process_id = (unsigned long)getpid();
#endif

    for (attempt = 0; attempt < 1000; attempt++) {
        int written;
        int fd;

        if (attempt == 0) {
            written = snprintf(temp_path, temp_path_size, "%s.tmp.%lu",
                               target_path, process_id);
        } else {
            written = snprintf(temp_path, temp_path_size, "%s.tmp.%lu.%lu",
                               target_path, process_id,
                               attempt);
        }
        if (written < 0 || (size_t)written >= temp_path_size) {
            temp_path[0] = '\0';
            return NULL;
        }

#ifdef _WIN32
        {
            wchar_t *wide_path = serializer_utf8_to_wide(temp_path);
            if (!wide_path) return NULL;
            fd = _wopen(wide_path, _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY,
                        _S_IREAD | _S_IWRITE);
            free(wide_path);
        }
#else
        fd = open(temp_path, O_WRONLY | O_CREAT | O_EXCL, 0600);
#endif

        if (fd < 0) {
            if (errno == EEXIST) continue;
            temp_path[0] = '\0';
            return NULL;
        }

#ifndef _WIN32
        {
            struct stat target_stat;
            if (lstat(target_path, &target_stat) == 0 &&
                S_ISREG(target_stat.st_mode)) {
                (void)fchmod(fd, target_stat.st_mode & 07777);
            }
        }
#endif

#ifdef _WIN32
        {
            FILE *fp = _fdopen(fd, "wb");
            if (!fp) {
                _close(fd);
                serializer_remove_temp(temp_path);
                temp_path[0] = '\0';
            }
            return fp;
        }
#else
        {
            FILE *fp = fdopen(fd, "wb");
            if (!fp) {
                close(fd);
                serializer_remove_temp(temp_path);
                temp_path[0] = '\0';
            }
            return fp;
        }
#endif
    }

    temp_path[0] = '\0';
    return NULL;
}

int serializer_flush(FILE *fp)
{
    if (!fp || serializer_stream_has_error(fp)) return -1;
    if (serializer_test_failure == SERIALIZER_TEST_FAILURE_FLUSH) {
        serializer_test_failure = SERIALIZER_TEST_FAILURE_NONE;
        return -1;
    }
    if (fflush(fp) != 0 || serializer_stream_has_error(fp)) return -1;

#ifdef _WIN32
    if (_commit(_fileno(fp)) != 0) return -1;
#elif defined(__APPLE__) || defined(__linux__) || defined(__unix__)
    if (fsync(fileno(fp)) != 0) return -1;
#endif

    return serializer_stream_has_error(fp) ? -1 : 0;
}

int serializer_replace_file(const char *temp_path, const char *target_path)
{
    if (!temp_path || !target_path) return -1;

#ifdef _WIN32
    {
        wchar_t *wide_temp = serializer_utf8_to_wide(temp_path);
        wchar_t *wide_target = serializer_utf8_to_wide(target_path);
        int result = -1;

        if (wide_temp && wide_target) {
            DWORD attributes = GetFileAttributesW(wide_target);

            if (attributes != INVALID_FILE_ATTRIBUTES) {
                if (ReplaceFileW(wide_target, wide_temp, NULL,
                                 0, NULL, NULL)) {
                    result = 0;
                }
            } else if (GetLastError() == ERROR_FILE_NOT_FOUND) {
                if (MoveFileExW(wide_temp, wide_target,
                                MOVEFILE_WRITE_THROUGH)) {
                    result = 0;
                }
            }
        }
        free(wide_temp);
        free(wide_target);
        return result;
    }
#else
    /* POSIX rename is atomic within one filesystem, but this is not a
     * compare-and-replace operation; callers recheck fingerprints first. */
    return rename(temp_path, target_path) == 0 ? 0 : -1;
#endif
}

int serializer_create_file(const char *temp_path, const char *target_path)
{
    if (!temp_path || !target_path) return -1;
    if (serializer_test_failure == SERIALIZER_TEST_FAILURE_TARGET_APPEARED) {
        serializer_test_failure = SERIALIZER_TEST_FAILURE_NONE;
        return -1;
    }

#ifdef _WIN32
    {
        wchar_t *wide_temp = serializer_utf8_to_wide(temp_path);
        wchar_t *wide_target = serializer_utf8_to_wide(target_path);
        int result = -1;

        if (wide_temp && wide_target &&
            MoveFileExW(wide_temp, wide_target, MOVEFILE_WRITE_THROUGH)) {
            result = 0;
        }
        free(wide_temp);
        free(wide_target);
        return result;
    }
#else
    /* link() gives create-only installation: an appearing target wins. */
    if (link(temp_path, target_path) != 0) return -1;
    if (unlink(temp_path) != 0) {
        (void)unlink(target_path);
        return -1;
    }
    return 0;
#endif
}

void serializer_remove_temp(const char *path)
{
    if (path) (void)serializer_remove_utf8(path);
}
