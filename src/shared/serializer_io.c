#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700  /* realpath is an XSI extension */
#endif

/*
 * serializer_io.c — Internal file I/O helpers for TOML serialization.
 */

#include "serializer_io.h"

#include <errno.h>  /* errno, EEXIST */
#include <stdio.h>  /* fprintf, remove, snprintf */
#include <stdlib.h> /* free, malloc, realpath */
#include <string.h> /* memcpy, memset, strlen, strrchr */
#include <stdint.h> /* uint64_t */

#ifdef _WIN32
#include <windows.h> /* GetCurrentProcessId, MoveFileExW, UTF-8 conversion */
#include <io.h>      /* _commit, _fileno, _open */
#include <fcntl.h>   /* _O_CREAT, _O_EXCL */
#include <sys/stat.h> /* _S_IREAD, _S_IWRITE */
#else
#include <fcntl.h>    /* open, O_CREAT, O_EXCL */
#include <sys/stat.h> /* stat, lstat, fchmod */
#include <unistd.h>   /* getpid, fsync, fileno, close, read, write */
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

#ifdef MANGO_TESTING
void serializer_test_set_failure(int failure)
{
    serializer_test_failure = failure;
}
#endif

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

/*
 * serializer_sync_parent_dir — Make a completed rename/link durable.
 *
 * fsync(file) persists the bytes, but the new directory entry created by
 * rename() or link() lives in the parent directory.  Without syncing that
 * directory, a power loss right after "Saved" can bring back the old file.
 * Some filesystems cannot fsync a directory and report EINVAL; nothing more
 * can be done there, so that one error is treated as success.
 *
 * Callers run this after the new file is already in place, so a failure
 * here is only a warning: the save happened and reads will see it.
 * Reporting it as a failed save would leave the editor believing the old
 * bytes are on disk, and a create-only save would then fail forever
 * because its target now exists.
 */
#ifndef _WIN32   /* Windows replaces files with MOVEFILE_WRITE_THROUGH. */
static int serializer_sync_parent_dir(const char *path)
{
#if defined(__EMSCRIPTEN__)
    /* The browser's in-memory filesystem has no real disk to flush. */
    (void)path;
    return 0;
#else
    char dir[SERIALIZER_IO_PATH_MAX];
    const char *slash = strrchr(path, '/');
    size_t length;
    int fd;
    int result = 0;

    if (serializer_test_failure == SERIALIZER_TEST_FAILURE_DIR_SYNC) {
        serializer_test_failure = SERIALIZER_TEST_FAILURE_NONE;
        return -1;
    }
    if (!slash) {
        memcpy(dir, ".", 2);            /* "level.toml" lives in "." */
    } else if (slash == path) {
        memcpy(dir, "/", 2);            /* "/level.toml" lives in "/" */
    } else {
        length = (size_t)(slash - path);
        if (length >= sizeof(dir)) return -1;
        memcpy(dir, path, length);
        dir[length] = '\0';
    }

    fd = open(dir, O_RDONLY);
    if (fd < 0) return -1;
    if (fsync(fd) != 0 && errno != EINVAL) result = -1;
    if (close(fd) != 0) result = -1;
    return result;
#endif
}

/* The file is already installed; a failed directory sync only weakens
 * crash durability, so warn and let the save succeed. */
static void serializer_sync_parent_dir_or_warn(const char *path)
{
    if (serializer_sync_parent_dir(path) != 0) {
        fprintf(stderr, "serializer: warning: saved '%s' but could not sync its "
                "folder; a power loss now could undo the save\n", path);
    }
}
#endif /* !_WIN32 */

int serializer_path_is_symlink(const char *path)
{
    if (!path || path[0] == '\0') return 0;
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
    {
        struct stat link_stat;
        /* lstat describes the link itself; stat would describe its target. */
        return lstat(path, &link_stat) == 0 && S_ISLNK(link_stat.st_mode);
    }
#else
    return 0;
#endif
}

int serializer_same_file_utf8(const char *a, const char *b)
{
    if (!a || !b || a[0] == '\0' || b[0] == '\0') return 0;
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
    {
        /* Two spellings of one file (a symlinked folder, "./x" against "x")
         * share a device and inode number; comparing strings would miss it. */
        struct stat a_stat;
        struct stat b_stat;
        return stat(a, &a_stat) == 0 && stat(b, &b_stat) == 0 &&
               a_stat.st_dev == b_stat.st_dev && a_stat.st_ino == b_stat.st_ino;
    }
#else
    return 0;
#endif
}

int serializer_resolve_save_target(const char *path, char *buf, size_t buf_size)
{
    if (!path || !buf || buf_size == 0) return -1;

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
    {
        struct stat link_stat;
        if (lstat(path, &link_stat) == 0 && S_ISLNK(link_stat.st_mode)) {
            char *resolved = realpath(path, NULL);
            if (resolved) {
                size_t length = strlen(resolved);
                int fits = length < buf_size;
                if (fits) memcpy(buf, resolved, length + 1);
                free(resolved);
                return fits ? 0 : -1;
            }
            /* A dangling link has no target to update; replace the link. */
        }
    }
#endif

    if (strlen(path) >= buf_size) return -1;
    memcpy(buf, path, strlen(path) + 1);
    return 0;
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
     * compare-and-replace operation; callers recheck fingerprints first.
     * rename() replaces a symlink itself, never the file it points to, so a
     * link planted at the destination cannot redirect the write.  The editor
     * resolves a link first only when it saves back the file it opened. */
    if (rename(temp_path, target_path) != 0) return -1;
    serializer_sync_parent_dir_or_warn(target_path);
    return 0;
#endif
}

#ifndef _WIN32
/*
 * serializer_link_unsupported — Did link() fail because this filesystem
 * has no hard links (FAT/exFAT drives, many network shares), rather than
 * because the target exists or the folder is not writable?
 */
static int serializer_link_unsupported(int error)
{
    if (error == EPERM || error == EXDEV) return 1;
#ifdef ENOTSUP
    if (error == ENOTSUP) return 1;
#endif
#ifdef EOPNOTSUPP
    if (error == EOPNOTSUPP) return 1;
#endif
#ifdef ENOSYS
    if (error == ENOSYS) return 1;
#endif
    return 0;
}

/*
 * serializer_create_without_link — Create-only install without link().
 *
 * O_EXCL claims the target name: it fails if anything, a dangling symlink
 * included, is already there, so an existing file is never replaced.  The
 * finished temporary file is then copied into the claimed file through the
 * descriptor we hold, never through the name again, so nothing that appears
 * at that name afterwards can be written to or replaced.  The copy is
 * flushed before the temporary file goes away.
 *
 * Unlike link(), a reader opening the file during the copy can see it
 * partly written.  This path only runs on drives without hard links
 * (FAT/exFAT, many network shares), and a create-only save there has no
 * atomic option that also refuses to replace an existing file.
 */
static int serializer_create_without_link(const char *temp_path,
                                          const char *target_path)
{
    char buffer[8192];
    int failed = 0;
    int in = open(temp_path, O_RDONLY);
    if (in < 0) return -1;
    int out = open(target_path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (out < 0) {
        (void)close(in);
        return -1;
    }

    while (!failed) {
        ssize_t got = read(in, buffer, sizeof(buffer));
        if (got == 0) break;
        if (got < 0) {
            if (errno != EINTR) failed = 1;
            continue;
        }
        for (ssize_t done = 0; done < got && !failed;) {
            ssize_t put = write(out, buffer + done, (size_t)(got - done));
            if (put < 0) {
                if (errno != EINTR) failed = 1;
                continue;
            }
            done += put;
        }
    }
    if (!failed && fsync(out) != 0) failed = 1;
    (void)close(in);
    if (close(out) != 0) failed = 1;
    if (failed) {
        (void)unlink(target_path);   /* our own half-written claim */
        return -1;
    }
    (void)unlink(temp_path);
    serializer_sync_parent_dir_or_warn(target_path);
    return 0;
}
#endif

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
    {
        int link_result;
        /* link() gives create-only installation: an appearing target wins. */
        if (serializer_test_failure == SERIALIZER_TEST_FAILURE_NO_HARD_LINKS) {
            serializer_test_failure = SERIALIZER_TEST_FAILURE_NONE;
            errno = EPERM;
            link_result = -1;
        } else {
            link_result = link(temp_path, target_path);
        }
        if (link_result != 0) {
            if (!serializer_link_unsupported(errno)) return -1;
            return serializer_create_without_link(temp_path, target_path);
        }
    }
    if (unlink(temp_path) != 0) {
        (void)unlink(target_path);
        return -1;
    }
    serializer_sync_parent_dir_or_warn(target_path);
    return 0;
#endif
}

void serializer_remove_temp(const char *path)
{
    if (path) (void)serializer_remove_utf8(path);
}
