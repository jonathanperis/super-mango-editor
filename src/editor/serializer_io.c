/*
 * serializer_io.c — Internal file I/O helpers for TOML serialization.
 */

#include "serializer_io.h"

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h> /* open, O_WRONLY, O_CREAT, O_TRUNC */
#include <unistd.h>   /* close */
#endif

FILE *serializer_open_write(const char *path)
{
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return NULL;

    FILE *fp = fdopen(fd, "w");
    if (!fp) {
        int saved_errno = errno;
        close(fd);
        errno = saved_errno;
        return NULL;
    }
    return fp;
#else
    return fopen(path, "w");
#endif
}
